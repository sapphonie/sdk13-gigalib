#include <cbase.h>

#ifdef CLIENT_DLL
#include <helpers/misc_helpers.h>
#include <helpers/steam_helpers.h>

#ifdef _WIN32
#include <icommandline.h>
#include <Windows.h>
#include <engine_hacks/engine_detours.h>

void restartWithFixedCmdline(std::stringstream &newCmdLine)
{

    STARTUPINFO si          = {};
    PROCESS_INFORMATION pi  = {};

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // Start the child process. 
    if
    (
        !CreateProcess
        (
            nullptr,                            // No module name (use command line)
            (LPSTR)newCmdLine.str().c_str(),    // Command line
            nullptr,                            // Process handle not inheritable
            nullptr,                            // Thread handle not inheritable
            FALSE,                              // Set handle inheritance to FALSE
            CREATE_NO_WINDOW,                   // No creation flags
            nullptr,                            // Use parent's environment block
            nullptr,                            // Use parent's starting directory 
            &si,                                // Pointer to STARTUPINFO structure
            &pi                                 // Pointer to PROCESS_INFORMATION structure
        )
    )
    {
        Warning("CreateProcess failed, Steam overlay and tons of other things probably won't work! Error: %i.\n", GetLastError());
        return;
    }
    CloseHandle( pi.hProcess );
    CloseHandle( pi.hThread );

    TerminateProcess(GetCurrentProcess(), 0);
}


// return true to abort running the rest of the steam ctors
// aka we need to restart
bool RelaunchIfNeeded()
{
    const char* Win32CmdLine = GetCommandLineA();
    std::string StrWin32CmdLine(Win32CmdLine);

    bool allowedToFixSourceTest = false;
    bool needToFixSourceTest    = false;
    if (V_stristr(Win32CmdLine, "-fixoverlay"))
    {
        allowedToFixSourceTest = true;
    }

    if (V_stristr(Win32CmdLine, "-game sourcetest"))
    {
        needToFixSourceTest = true;
    }

    if
    (
        // we dont need to or we arent allowed to
        !needToFixSourceTest
        ||
        !allowedToFixSourceTest
    )
    {
        // don't relaunch
        return false;
    }

    if (V_stristr(Win32CmdLine, "-norelaunch_iknowwhatimdoing"))
    {
        return false;
    }
    if (!V_stristr(Win32CmdLine, "-isrelaunching"))
    {
        DWORD mypid = GetProcessId(GetCurrentProcess());
        UTIL_ReplaceAll(StrWin32CmdLine, "-game sourcetest", "");
        std::stringstream newCmdLine;
        // hack of the millenium
        newCmdLine << "cmd /C echo Hey! We're restarting your game to enable the Steam overlay! Just sit tight... & ";
        newCmdLine << "taskkill /T /F /PID " << mypid << " & ";
        // start "" C:/<>/hl2.exe <launch options> our launch options
        newCmdLine << "start \"\" ";
        newCmdLine << StrWin32CmdLine << " -novid -isrelaunching";

        restartWithFixedCmdline(newCmdLine);
        return true;
    }
    return false;
}

#endif // WIN32


// run our ctor
CSteamHelpers g_SteamHelpers;
CSteamHelpers::CSteamHelpers()
{
    DevMsg(2, "CSteamHelpers CTOR->\n");
#ifdef _WIN32
    // Don't bother with proton.
    if (!checkWine() && RelaunchIfNeeded())
    {
        return;
    }
#endif

    // spin off a thread and wait until steam is up before we call any of our funcs
    SteamSpin = std::thread( &SpinUntilSteamIsAlive );
    SteamSpin.detach();
}

void CSteamHelpers::SpinUntilSteamIsAlive()
{
    // 30sec
    const int maxtries = 30;
    for (size_t tries = 0; tries <= maxtries; tries++)
    {
        if (steamapicontext)
        {
            g_SteamHelpers.iterFuncsToRun();
            // exits the thread
            return;
        }
        DevMsg(2, "Failed getting Steam interfaces! Attempt #%i...\n", tries);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    Error("Failed getting Steam interfaces! Try restarting Steam!");
}

void CSteamHelpers::iterFuncsToRun()
{
    // loop thru our funcptr vec
    for (auto& element : funcPtrVector)
    {
        // call it up
        element();
        DevMsg(2, "-> Called %p after SteamInit\n", element);
    }
    funcPtrVector.clear();
}

// other functions will include this file and then use this to call their funcs on steam init
/*
// We need to do this so we're not passing a member func ptr to our init func
// To do so is undefined behavior because callconv etc
void DoThingTrampoline( )
{
    g_Thing.DoThing();
}

// ctor
CThing g_Thing;
CThing::CThing()
{
    RUN_THIS_FUNC_WHEN_STEAM_INITS( &DoThingTrampoline );
}
*/
void RUN_THIS_FUNC_WHEN_STEAM_INITS(fPtr yourFunctionPointer)
{
    g_SteamHelpers.funcPtrVector.push_back(yourFunctionPointer);
}

#endif // CLIENT_DLL
