#include <cbase.h>

#ifdef CLIENT_DLL
#include <helpers/steam_helpers.h>




#ifdef _WIN32
#include <icommandline.h>
#include <Windows.h>
void restartWithFixedCmdline()
{
    const char* cmdline = CommandLine()->GetCmdLine();
    DevMsg(2, "%s\n", cmdline);

    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // Start the child process. 
    if
        (
            !CreateProcess
            (
                NULL,				// No module name (use command line)
                (LPSTR)cmdline,		// Command line
                NULL,				// Process handle not inheritable
                NULL,				// Thread handle not inheritable
                FALSE,				// Set handle inheritance to FALSE
                0,					// No creation flags
                NULL,				// Use parent's environment block
                NULL,				// Use parent's starting directory 
                &si,				// Pointer to STARTUPINFO structure
                &pi					// Pointer to PROCESS_INFORMATION structure
            )
            )
    {
        Warning("CreateProcess failed (%d).\n", GetLastError());
        return;
    }


    exit(0);
}



void rmSourceTest()
{
    if (V_stristr(GetCommandLineA(), "sourcetest"))
    {
        CommandLine()->RemoveParm("-game sourcetest");
        CommandLine()->AppendParm("-novid",         "");
        CommandLine()->AppendParm("-multirun", "");

        Sleep(1);
        restartWithFixedCmdline();
    }
}
#endif // WIN32


// run our ctor
CSteamHelpers g_SteamHelpers;
CSteamHelpers::CSteamHelpers()
{
    DevMsg(2, "CSteamHelpers CTOR->\n");

    #ifdef CLIENT_DLL
        rmSourceTest();
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
        if (steamapicontext && steamapicontext->SteamHTTP())
        {
            g_SteamHelpers.iterFuncsToRun();
            // exits the thread
            return;
        }
        DevMsg(2, "Failed getting Steam interfaces! Attempt #%i...\n", tries);
        std::this_thread::sleep_for(std::chrono::seconds(1));
        continue;
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
