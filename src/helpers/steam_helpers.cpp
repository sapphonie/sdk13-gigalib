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

#if 0

#include <cstdio>
#include <windows.h>
#include <tlhelp32.h>

// You need to call CloseHandle on this handle if it isn't null!
HANDLE FindProcessByName(const std::string& name)
{
    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (Process32First(snapshot, &entry))
    {
        while (Process32Next(snapshot, &entry))
        {
            if (stricmp(entry.szExeFile, name.c_str()) == 0)
            {
                HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, entry.th32ProcessID);
                return hProcess;
            }
        }
    }
    return 0;
}


void CheckSteamExeSpin()
{
    // 10sec
    const int maxtries = 10000;
    for (size_t tries = 0; tries <= maxtries; tries++)
    {
        HANDLE steamHandle = FindProcessByName("steam.exe");
        if (steamHandle)
        {
            FILETIME createdAt = {};
            FILETIME exitedAt = {};
            FILETIME kernTime = {};
            FILETIME userTime = {};

            SYSTEMTIME systime = {};
            GetProcessTimes(steamHandle, &createdAt, &exitedAt, &kernTime, &userTime);
            FileTimeToSystemTime(&createdAt, &systime); //and now, start time of process at sProcessTime variable at usefull format.
            CloseHandle(steamHandle);
        }
        else
        {
            PrintBomb();
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
#endif

// CURRENTLY UNUSED.
// ret's true if we rewrote something
bool RewriteLocalSteamConfigIfNeeded()
{
    std::string SteamKey("SOFTWARE\\Valve\\Steam");
    std::string SMInstallPathKey("SourceModInstallPath");
    std::string SteamPathKey("SteamPath");

    const char* moddir = HACK_COM_GetModDirectory();


    // 248356383
    // localconfig.vdf
    std::string appid = "243750";
    CRC32_t crc = CRC32_ProcessSingleBuffer(moddir, strlen(moddir));
    std::string crcStr = fmt::format(FMT_STRING("{:s}_{:d}"), appid, (unsigned int)crc);
    Warning("%s\n", crcStr.c_str());


    // Wouldn't it be funny if there was a race condition here?
    // Oh wait, there is.
    // You can fix it if you want, but this will basically never be hit since these regkeys are almost never modified.
    // https://learn.microsoft.com/en-us/archive/msdn-magazine/2015/july/c-using-stl-strings-at-win32-api-boundaries#handling-a-race-condition
    // get a large enough size to allocate our destination string buffer
    DWORD dataSize = 0;

    RegGetValueA(HKEY_CURRENT_USER, SteamKey.c_str(), SMInstallPathKey.c_str(), RRF_RT_REG_SZ, nullptr, nullptr, &dataSize);
    auto smPathStr = std::make_unique<char[]>(dataSize);
    RegGetValueA(HKEY_CURRENT_USER, SteamKey.c_str(), SMInstallPathKey.c_str(), RRF_RT_REG_SZ, nullptr, smPathStr.get(), &dataSize);
    Warning("%s\n", smPathStr.get());


    RegGetValueA(HKEY_CURRENT_USER, SteamKey.c_str(), SteamPathKey.c_str(), RRF_RT_REG_SZ, nullptr, nullptr, &dataSize);
    auto steamPathStr = std::make_unique<char[]>(dataSize);
    RegGetValueA(HKEY_CURRENT_USER, SteamKey.c_str(), SteamPathKey.c_str(), RRF_RT_REG_SZ, nullptr, steamPathStr.get(), &dataSize);
    Warning("%s\n", steamPathStr.get());

#if 0

    auto ourSourcemodPath = std::filesystem::path(smPathStr.get());
    ourSourcemodPath = ourSourcemodPath.lexically_normal();
    ourSourcemodPath = ourSourcemodPath / moddir / "bin" / "SteamEdit";

    auto steamexespin = std::thread(&CheckSteamExeSpin);
    steamexespin.detach();


    //  & ping 127.0.0.1 -t
    std::stringstream cmd;
    cmd << "cd /D " << "\"" << ourSourcemodPath.string().c_str() << "\"" << " & " << ".\\SteamEdit.exe -autofix -forcestart -silent ";

    Warning("%s\n", cmd.str().c_str());
    system(cmd.str().c_str());



    return true;
}
#endif



    auto localcfgPath = std::filesystem::path(steamPathStr.get());
    localcfgPath = localcfgPath.lexically_normal();
    localcfgPath = localcfgPath / "userdata";
    for (const auto& dir_entry : std::filesystem::directory_iterator{ localcfgPath })
    {
        const std::filesystem::path usercfgPath = localcfgPath / dir_entry.path() / "config" / "localconfig.vdf";

        std::fstream localcfgFile( usercfgPath, std::ios::in | std::ios::out );

        if (!localcfgFile.is_open())
        {
            continue;
        }

        bool rewriteLocalCfg = false;
        std::string thisline;
        std::vector<std::string> lineVec;


        /*
                Here's what this should look like, roughly
                ...
                "243750_3661242217"
                {
                    "LaunchOptions"     "-sw -w 1600 -h 900 -otherlaunchoptions"
                }
                ...
        */

        while (std::getline(localcfgFile, thisline))
        {
            // Doesn't match our CRC string. Add this line to our vector, and go to the next line
            if (thisline.find(crcStr) == std::string::npos)
            {
                lineVec.push_back(thisline);
                continue;
            }
            // If we get here, our CRC string MUST match!
            // Add it to our vector
            // We are on:
            //         "243750_3661242217"
            lineVec.push_back(thisline);

            // Advance to next line
            std::getline(localcfgFile, thisline);

            // We are hopefully on
            //         {
            {
                // make sure we have an opening {
                if (thisline.find("{") == std::string::npos)
                {
                    Warning("Messed up cfg!\n");
                    break;
                }
                // Add it to our vector
                lineVec.push_back(thisline);
            }
            // Advance to next line
            std::getline(localcfgFile, thisline);

            // We are hopefully on
            // The spaces are actually tabs but shut up
            //             "LaunchOptions"     "-sw -w 1600 -h 900 -otherlaunchoptions"
            const static size_t launchOptionsStrlen = strlen("\"LaunchOptions\"");
            auto pos = thisline.find("\"LaunchOptions\"");
            if (pos == std::string::npos)
            {
                Warning("Messed up cfg!\n");
                break;
            }
            // If we made it here, pos should be at the start of the "LaunchOptions" string
            //             "LaunchOptions"     "-sw -w 1600 -h 900 -otherlaunchoptions"
            //             ^
            pos += launchOptionsStrlen;
            //             "LaunchOptions"     "-sw -w 1600 -h 900 -otherlaunchoptions"
            //                            ^


            auto startPos = thisline.find('"', pos);
            if (startPos == std::string::npos)
            {
                Warning("Messed up cfg!\n");
                break;
            }
            // If we made it here, this character should be a quote
            //             "LaunchOptions"     "-sw -w 1600 -h 900 -otherlaunchoptions"
            //                                 ^


            Warning("%i, %i\n", pos, startPos);

            std::string launchOptionsStringLiteral = thisline.substr(0, startPos);
            //             "LaunchOptions"     "-sw -w 1600 -h 900 -otherlaunchoptions"
            //^-------------------------------^


            std::string userLaunchOptions = thisline.substr(startPos, thisline.length());
            //             "LaunchOptions"     "-sw -w 1600 -h 900 -otherlaunchoptions"
            //                                 ^--------------------------------------^

            // Strip off the quotes
            UTIL_ReplaceAll(userLaunchOptions, "\"", "");

            // This should be pretty self explanatory, add the quotes back manually, then manually add launch options if we need to
            std::stringstream fixedUpLaunchOptions;

            fixedUpLaunchOptions << userLaunchOptions;
            {
                // todo: check if the launch options are all smushed together
                if (userLaunchOptions.find("-nobreakpad") == std::string::npos)
                {
                    fixedUpLaunchOptions << " -nobreakpad ";
                    rewriteLocalCfg = true; // we only rewrite the file if this bool is set
                }
                if (userLaunchOptions.find("-nominidumps") == std::string::npos)
                {
                    fixedUpLaunchOptions << " -nominidumps ";
                    rewriteLocalCfg = true; // ^
                }
            }

            std::string fixedUpLaunchOptionsTrimmed(fixedUpLaunchOptions.str());
            trim(fixedUpLaunchOptionsTrimmed);
            UTIL_ReplaceAll(fixedUpLaunchOptionsTrimmed, "  ", " ");
            trim(fixedUpLaunchOptionsTrimmed);

            std::stringstream fullLaunchOptions;
            fullLaunchOptions << launchOptionsStringLiteral << '"' << fixedUpLaunchOptionsTrimmed << '"';

            // add the recombined line to our vector
            lineVec.push_back(fullLaunchOptions.str());
        }


        if (rewriteLocalCfg)
        {
            HWND hand = FindWindowA("Valve001", NULL);

            if (hand)
            {
                // hide the window since we can be in fullscreen and if so it'll just hang forever
                ShowWindow(hand, SW_HIDE);
            }
            auto hi = \
                "Hiya! In order to have things run smoothly, we need to restart steam and add some launch options.\n"
                "You'll have to restart Steam and start the game again, but don't worry - this should be a one time thing!";
            MessageBoxA(NULL, hi, "yo",
                MB_OK | MB_SETFOREGROUND | MB_TOPMOST);


            std::string bakPath = usercfgPath.string();
            bakPath.append(".bak");
            std::filesystem::copy_file(usercfgPath, bakPath, std::filesystem::copy_options::skip_existing);


            std::fstream MutableLocalCfg( usercfgPath, std::ios::binary | std::ios::out | std::ios::trunc );

            for (const auto& line : lineVec)
            {
                // steam has \ns in this file for some reason?
                MutableLocalCfg << line << "\n" << std::flush;
                MutableLocalCfg.std::fstream::sync();
            }
            MutableLocalCfg.std::fstream::sync();

            system("taskkill /f /im Steam.exe >nul 2>nul");


            // std::filesystem::path steamexe(steamPathStr.get());
            // steamexe = steamexe.lexically_normal();
            // steamexe = steamexe / "steam.exe";
            // 
            // std::stringstream steamcmdline;
            // steamcmdline << "start \"\"" << " " << "\"" << steamexe.string().c_str() << "\"";
            // 
            // system(steamcmdline.str().c_str());
            TerminateProcess(GetCurrentProcess(), 0);


            return true;
        }
    }

    return false;
}


CON_COMMAND_F(testRewrite, "", 0)
{
    RewriteLocalSteamConfigIfNeeded();
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
        ( !needToFixSourceTest || !allowedToFixSourceTest )
#ifdef NOBREAKPAD_NOMINIDUMPS_HACK
#ifdef SDKSENTRY
        // and we have the sentry stuff already set up
        && V_stristr(Win32CmdLine, "-nobreakpad")
        && V_stristr(Win32CmdLine, "-nominidumps")
#endif
#endif
    )
    {
        // don't relaunch
        return false;
    }

    // if (V_stristr(Win32CmdLine, "hijack"))
    // {
    //     return false;
    // }
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
#ifdef NOBREAKPAD_NOMINIDUMPS_HACK
        newCmdLine << "cmd /C echo Hey! We're restarting your game to enable crash reporting and the Steam overlay! Just sit tight... & ";
#else
        newCmdLine << "cmd /C echo Hey! We're restarting your game to enable the Steam overlay! Just sit tight... & ";
#endif
        newCmdLine << "taskkill /T /F /PID " << mypid << " & ";
        // start "" C:/<>/hl2.exe <launch options> our launch options
        newCmdLine << "start \"\" ";
        newCmdLine << StrWin32CmdLine << " -novid -isrelaunching";

#ifdef NOBREAKPAD_NOMINIDUMPS_HACK
#ifdef SDKSENTRY
        newCmdLine << " -nobreakpad -nominidumps";
#endif
#endif
        // Not worth relaunching the whole game over...
        //if (checkWine())
        //{
        //    newCmdLine << " -iswine";
        //}
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
    if (RelaunchIfNeeded())
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
