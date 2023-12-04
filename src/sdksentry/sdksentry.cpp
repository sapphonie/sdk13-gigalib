#include <cbase.h>

#if defined(CLIENT_DLL) && defined(SDKCURL) && defined(SDKSENTRY)
#include <misc_helpers.h>
#include <sdkCURL/sdkCURL.h>
#include <sdksentry/sdksentry.h>
#include <engine_memutils.h>
#include <engine_detours.h>


#include "errhandlingapi.h"


__forceinline __declspec(noreturn) void TERMINATE_RIGHT_NOW()
{
#ifdef _WIN32
    TerminateProcess( GetCurrentProcess(), 0 );
#else
    _exit(2);
#endif
}



CSentry g_Sentry;


CSentry::CSentry()
{
    didinit.store(false);
    didshutdown.store(false);
    crashed.store(false);

    sentryLogFilePtr    = NULL;
    conFileFilePtr      = NULL;
#ifdef _WIN32
    mainWindowHandle    = NULL;
#endif
}

// #ifdef _DEBUG
void CC_SentryTest(const CCommand& args)
{
    sentry_value_t ctxinfo = sentry_value_new_object();
    sentry_value_set_by_key(ctxinfo, "test", sentry_value_new_string("test str"));
    SentryEvent("info", __FUNCTION__, "testEvent", ctxinfo);
}

ConCommand sentry_test("sentry_test", CC_SentryTest, "\n", FCVAR_NONE );
// #endif

void sentry_callback(IConVar* var, const char* pOldValue, float flOldValue)
{
    int consent = ((ConVar*)(var))->GetInt();

    if (consent == 1)
    {
        sentry_user_consent_give();
    }
    else if (consent == 0)
    {
        sentry_user_consent_revoke();
    }
    else
    {
        sentry_user_consent_reset();
    }

    engine->ExecuteClientCmd("host_writeconfig");
}

ConVar cl_send_error_reports("cl_send_error_reports", "-1", FCVAR_ARCHIVE,
    "Enables/disables sending error reports to the developers to help improve the game.\n"
    "Error reports will include your SteamID, and any pertinent game info (class, loadout, current map, convars, console spew, etc.) - we do not store any personally identifiable information.\n"
    "Read more at " VPC_QUOTE_STRINGIFY(SENTRY_PRIVACY_POLICY_URL) "\n"
    "-1 asks you again on game boot and disables reporting, 0 disables reporting and does not ask you again, 1 enables reporting.\n",
    sentry_callback
);

void CSentry__SentryURLCB__THUNK(const curlResponse* curlRepsonseStruct)
{
    g_Sentry.SentryURLCB(curlRepsonseStruct);
}

void CSentry::PostInit()
{
    DevMsg(2, "Sentry Postinit!\n");

    g_sdkCURL->CURLGet(VPC_QUOTE_STRINGIFY(SENTRY_URL), CSentry__SentryURLCB__THUNK);
}


void CSentry::SentryURLCB(const curlResponse* resp)
{
    if (!resp->completed || resp->failed || resp->respCode != 200)
    {
        Warning("Failed to get Sentry DSN. Sentry integration disabled.\n");
        Warning("completed = %i, failed = %i, statuscode = %i, bodylen = %i\n",
            resp->completed, resp->failed, resp->respCode, resp->bodyLen);
        return;
    }

    if (resp->bodyLen >= 256 || !resp->bodyLen)
    {
        Warning("sentry url response is >= 256chars || or is 0, bailing! len = %i\n", resp->bodyLen);
        return;
    }

    std::string body = resp->body;
    body = UTIL_StripCharsFromSTDString(body, '\n');
    body = UTIL_StripCharsFromSTDString(body, '\r');
    real_sentry_url = body;

    DevMsg(2, "sentry_dns %s\n", real_sentry_url.c_str());
    SentryInit();
}


FORCEINLINE void DoDyingStuff()
{
    // do our logging no matter what
    // global char[256000]
    char* spew = Engine_GetSpew();
    const size_t spewSize = 256000;

    if (!spew || spew[0] == 0x0)
    {
        fflush((FILE*)g_Sentry.sentryLogFilePtr);
        return;
    }

    // guaruntee that there's a nul at the end
    spew[spewSize - 1] = 0x0;

    // iterate starting from the end of the array
    // we want to find the first non null character
    size_t lastNonNullCharPos = spewSize - 1;
    while (spew[lastNonNullCharPos] == 0x0)
    {
        lastNonNullCharPos--;
    }

    // now we're starting from the beginning of the array
    // and putting each char directly into our file,
    // sanitizing nulls as we go
    for (size_t c = 0; c <= lastNonNullCharPos; c++)
    {
        char thisChar = spew[c];
        if (thisChar == 0x0)
        {
            fputs("<NULL>", (FILE*)g_Sentry.conFileFilePtr);
        }
        else
        {
            fputc(thisChar, (FILE*)g_Sentry.conFileFilePtr);
        }
    }

    fflush( (FILE*)g_Sentry.conFileFilePtr );
    fclose( (FILE*)g_Sentry.conFileFilePtr );

    fflush( (FILE*)g_Sentry.sentryLogFilePtr );

    // we want this open no matter what so DONT close it
    // fclose((FILE*)g_Sentry.sentryLogFilePtr);
    return;
}

// We ignore any crashes whenever the engine shuts down
// We shouldn't, but we do. Feel free to fix all the shutdown crashes if you want,
// then you can remove the ugly atomic part of this
void CSentry::Shutdown()
{
    sentry_add_breadcrumb(sentry_value_new_breadcrumb(NULL, __FUNCTION__));

    DoDyingStuff();

    int flushRet = sentry_flush(5000);
    if (flushRet != 0)
    {
        Warning("-> Could not fully flush sentry queue, ret = %i\n", flushRet);
    }
    sentry_close();
    didshutdown.store(true);

#ifdef _WIN32
    SetUnhandledExceptionFilter(NULL);
#endif
}

#ifndef _WIN32
     #include <SDL.h>
#endif


void AttachExtraCtxToSentryCrash(sentry_value_t event)
{
    const char* gamePaths   = (const char*)g_Sentry.gamePaths;
    const char* cmdline     = (const char*)g_Sentry.cmdline;
    sentry_value_t xtraInfo = sentry_value_new_object();
    sentry_value_set_by_key(xtraInfo, "gamePaths", sentry_value_new_string(gamePaths));
    sentry_value_set_by_key(xtraInfo, "startupCmdline", sentry_value_new_string(cmdline));

    sentry_value_t contexts = sentry_value_new_object();
    sentry_value_set_by_key(contexts, "xtraInfo", xtraInfo);
    sentry_value_set_by_key(event, "contexts", contexts);
}



// DO NOT THREAD THIS OR ANY FUNCTIONS CALLED BY IT UNDER ANY CIRCUMSTANCES
// THIS NEEDS TO BE SIGNAL SAFE ALSO
// I MEAN NOT REALLY ON WINDOWS ITS CALLED IN A SEH BUT
// PLEASE DONT TOUCH UNLESS YOU KNOW WHAT YOU'RE DOING
sentry_value_t SENTRY_CRASHFUNC(const sentry_ucontext_t* uctx, sentry_value_t event, void* closure)
{
    AssertMsg(0, "CRASHED - WE'RE IN THE SIGNAL HANDLER NOW SO BREAK IF YOU WANT");
    

    AttachExtraCtxToSentryCrash(event);
    

    // reentry guard - we already crashed, just bail
    if ( g_Sentry.crashed.load() )
    {
        TERMINATE_RIGHT_NOW();
        sentry_value_decref(event);
        return sentry_value_new_null();
    }

    g_Sentry.crashed.store(true);
    DoDyingStuff();

    // we already shutdown, just bail
    if ( g_Sentry.didshutdown.load() )
    {
        TERMINATE_RIGHT_NOW();
        sentry_value_decref(event);
        return sentry_value_new_null();
    }

    const char* crashdialogue =
    "\"BONK!\"\n\n"
    "We crashed. Sorry about that.\n"
    "If you've enabled crash reporting,\n"
    "we'll get right on that.\n";

    const char* crashtitle = 
    "SDK13Mod crash handler - written by sappho.io";


#ifdef _WIN32
    // hide the window since we can be in fullscreen and if so it'll just hang forever
    ShowWindow((HWND)g_Sentry.mainWindowHandle, SW_HIDE);

    MessageBoxA(NULL, crashdialogue, crashtitle, \
        MB_OK | MB_SETFOREGROUND | MB_TOPMOST);
#else
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, crashtitle, crashdialogue, NULL);
#endif
    // we never properly loaded, just bail
    if ( !g_Sentry.didinit.load() )
    {
        TERMINATE_RIGHT_NOW();
        sentry_value_decref(event);
        return sentry_value_new_null();
    }
    return event;
}

void sentry_logger(sentry_level_t level, const char* message, va_list args, void* userdata)
{
    constexpr const size_t bufSize = 1024;
    char* buffer = (char*)(calloc(1, bufSize + 1));
    vsnprintf(buffer, bufSize, message, args);

    fprintf((FILE*)g_Sentry.sentryLogFilePtr, "%i - %s\n", level, buffer);
    // if we don't flush we can lose data so we're taking the perf hit and doing it every message
    fflush((FILE*)g_Sentry.sentryLogFilePtr);


/*

typedef enum sentry_level_e {
    SENTRY_LEVEL_DEBUG      = -1,
    SENTRY_LEVEL_INFO       = 0,
    SENTRY_LEVEL_WARNING    = 1,
    SENTRY_LEVEL_ERROR      = 2,
    SENTRY_LEVEL_FATAL      = 3,
} sentry_level_t;


*/
    // don't do valve functions if we already crashed
    if ( g_Sentry.crashed.load() )
    {
        free(buffer);
        return;
    }
    switch (level)
    {
        case SENTRY_LEVEL_DEBUG:
        {
            DevMsg(1, "[SENTRY] DEBUG - %s\n", buffer);
            break;
        }
        case SENTRY_LEVEL_INFO:
        {
            Msg("[SENTRY] INFO - %s\n", buffer);
            break;
        }
        case SENTRY_LEVEL_WARNING:
        {
            Warning("[SENTRY] WARNING - %s\n", buffer);
            break;
        }
        case SENTRY_LEVEL_ERROR:
        {
            Warning("[SENTRY] ERROR - %s\n", buffer);
            break;
        }
        case SENTRY_LEVEL_FATAL:
        {
            Warning("[SENTRY] FATAL - %s\n", buffer);
            break;
        }
        default:
        {
            Warning("[SENTRY] UNKNOWN LOGGING LEVEL %i - %s\n", level, buffer);
            break;
        }
    }

    free(buffer);
}






#ifdef _DEBUG
CON_COMMAND_F(triggerError, "test", 0)
{
    Error("error message %i", RandomInt(34,46));
}
#endif
// int __cdecl sub_7920CAA0(char a1, char *Format, va_list ArgList)
sdkdetour* InternalError{};
#define InternalError_vars        bool makeDump, char* fmt, va_list arglist
#define InternalError_novars      makeDump, fmt, arglist
#define InternalError_origfunc    PLH::FnCast(InternalError->detourTrampoline, InternalError_CB)(InternalError_novars);
void InternalError_CB(InternalError_vars)
{
    char errBuf[1024] = {};
    vsnprintf(errBuf, sizeof(errBuf), fmt, arglist);
    Warning("err at %s\n", errBuf);
    // Maybe don't...?
    // SentrySetTags();

    DoDyingStuff();

    std::string errFmt = fmt::format( FMT_STRING( "Error(): {:s}" ), errBuf);
    sentry_value_t event = sentry_value_new_event();
    sentry_value_set_by_key(event, "level", sentry_value_new_string("error"));
    sentry_value_set_by_key(event, "logger", sentry_value_new_string(__FUNCTION__));
    sentry_value_set_by_key(event, "message", sentry_value_new_string(errFmt.c_str()));

    sentry_value_t thread = sentry_value_new_thread(GetCurrentThreadId(), "nada");
    sentry_value_set_stacktrace(thread, NULL, 16);
    sentry_event_add_thread(event, thread);
    AttachExtraCtxToSentryCrash(event);
    sentry_capture_event(event);

    sentry_flush(5000);
    sentry_close();

    // don't re-crash
#ifdef _WIN32
    SetUnhandledExceptionFilter(NULL);
#endif
    // SentryEvent("fatal", "Error()", errBuf, NULL, false);

    const char* crashdialogue =
    "\"KABOOM!\"\n\n"
    "The engine has politely asked us to explode.\n"
    "If you've enabled error reporting,\n"
    "we'll take a look.\n";

    const char* crashtitle = 
    "Engine Error(!)";

    std::string errWindow = fmt::format( FMT_STRING( "{:s}\nThe error was:\n{:s}" ), crashdialogue, errBuf);

#ifdef _WIN32
    // hide the window since we can be in fullscreen and if so it'll just hang forever
    ShowWindow((HWND)g_Sentry.mainWindowHandle, SW_HIDE);

    MessageBoxA(NULL, errWindow.c_str(), crashtitle, \
        MB_OK | MB_SETFOREGROUND | MB_TOPMOST);
#else
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, crashtitle, crashdialogue, NULL);
#endif
    TERMINATE_RIGHT_NOW();
    return;
}

void InternalError_Init()
{

    InternalError = new sdkdetour{};

    // Unique string: "NetChannel removed.", which calls Shutdown immediately after
    #ifdef _WIN32
        // Signature for sub_7920CAA0:
        // 55 8B EC 6A FF 68 ? ? ? ? 68 ? ? ? ? 
        InternalError->patternSize = 15;
        InternalError->pattern     = "\x55\x8B\xEC\x6A\xFF\x68\x2A\x2A\x2A\x2A\x68\x2A\x2A\x2A\x2A";
    #else
        // Signature for sub_4DFAF0:
        // 55 89 E5 57 56 53 83 EC 3C 8B 5D 08 8B 75 0C 8B 8B 8C 00 00 00
        InternalError->patternSize = 1;
        InternalError->pattern = "";
    #endif

    populateAndInitDetour(InternalError, (void*)InternalError_CB);
}






#ifdef _WIN32
#include <minidump.h>
#endif

#ifdef TF2C_BETA
void BetaWindowPopup(std::string_view windowtext)
{
#ifdef _WIN32
    // hide the window since we can be in fullscreen and if so it'll just hang forever
    ShowWindow( (HWND)g_Sentry.mainWindowHandle, SW_HIDE);

    // DO NOT USE .data() ON A STRING VIEW! IT'S NOT REQUIRED TO BE NULL TERM'D, UNLIKE std::string's .c_str()!
    // Because of that, we generate a temp std::string
    MessageBoxA(NULL, std::string(windowtext).c_str(), "Beta window popup!", \
        MB_OK | MB_SETFOREGROUND | MB_TOPMOST);
#else
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, std::string(windowtext).c_str(), windowtext.c_str(), NULL);
#endif

}
#else
void BetaWindowPopup(std::string windowtext) {};
#endif

// Replaces Valve's stupid built in minidump handler.
// This is only here until we make sure that our method
// of using sentry without relaunching is bug free.
void MINI(unsigned int uStructuredExceptionCode, _EXCEPTION_POINTERS* pExceptionInfo, const char* pszFilenameSuffix)
{
    BetaWindowPopup(
        "WARNING - somehow we exploded and ended up in the minidump handler.\n"
        "***This should never happen, unless your game crashed before sentry could init...***\n"
        "Please harass sappho and azzy about this.\n"
        "Thanks!" );

    sentry_ucontext_t* uctx = new sentry_ucontext_t{ *pExceptionInfo };
    sentry_handle_exception(uctx);
}

LONG CALLBACK VecXceptionHandler(EXCEPTION_POINTERS* info)
{
    auto code = info->ExceptionRecord->ExceptionCode;

    //
    //    Error Codes in COM
    //
    //    Article
    //    04 / 27 / 2021
    //
    //    To indicate success or failure, COM methods and functions return a value of type HRESULT.
    //    An HRESULT is a 32-bit integer.The high-order bit of the HRESULT signals success or failure.
    //    Zero(0) indicates success and 1 indicates failure.
    //
    //    This produces the following numeric ranges:
    //
    //      Success codes : 0x00000000–0x7FFFFFFF.
    //      Error codes   : 0x80000000–0xFFFFFFFF.
    // TL;DR? If we don't do this, we can catch benign debug stuff as errors...
    if (code < 0x80000000 && code != 0)
    {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    // switch (info->ExceptionRecord->ExceptionCode)
    // {
    //     case EXCEPTION_ACCESS_VIOLATION:
    //     case EXCEPTION_INVALID_HANDLE:
    //     case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
    //     case EXCEPTION_DATATYPE_MISALIGNMENT:
    //     case EXCEPTION_ILLEGAL_INSTRUCTION:
    //     case EXCEPTION_INT_DIVIDE_BY_ZERO:
    //     case EXCEPTION_STACK_OVERFLOW:
    //     case 0xC0000409: // STATUS_STACK_BUFFER_OVERRUN
    //     case 0xC0000374: // STATUS_HEAP_CORRUPTION
    //         break;
    //     case 0: // Valve use this for Sys_Error.
    //         if ( (info->ExceptionRecord->ExceptionFlags & EXCEPTION_NONCONTINUABLE) == 0 )
    //             return EXCEPTION_CONTINUE_SEARCH;
    //         break;
    //     default:
    //         return EXCEPTION_CONTINUE_SEARCH;
    // }

    BetaWindowPopup( "Vectored exception handler called!" );

    sentry_ucontext_t* uctx = new sentry_ucontext_t{ *info };
    sentry_handle_exception(uctx);

    // TERMINATE_RIGHT_NOW();
    // theoretically, according to
    // https://github.com/asherkin/accelerator/blob/1f9ef1fa9d972bdd0a10e96776a0e7ee2c519d1b/extension/extension.cpp#L299-L302 ,
    // this should stop us from calling valve's handler.
    // it doesn't, though, it seems that sentry_handle_exception takes care of that for us.
    // REGARDLESS, maybe we should terminate here? I Don't Know Yet.
    info->ExceptionRecord->ExceptionCode = EXCEPTION_BREAKPOINT;
    return EXCEPTION_EXECUTE_HANDLER;
}

void CSentry::SentryInit()
{
    DevMsg(2, "Sentry init!\n");

    // Don't bother on Linux
    if (checkWine())
    {
        return;
    }

    SetMiniDumpFunction(MINI);
    AddVectoredExceptionHandler(1 /* first handler */, VecXceptionHandler);

    const char* mpath = ConVarRef("_modpath", false).GetString();
    if (!mpath)
    {
        Error("Couldn't get ConVarRef for _modpath!\n");
    }
    std::string modpath_ss( mpath );
#ifdef _WIN32
    // location of the crashpad handler (in moddir/bin)
    std::stringstream crash_exe;
    crash_exe << modpath_ss << CORRECT_PATH_SEPARATOR << "bin" << CORRECT_PATH_SEPARATOR << "crashpad_handler.exe";
#endif
    // location of the sentry workdir (we're just gonna stick it in mod dir/cache/sentry)
    std::stringstream sentry_db;
    sentry_db << modpath_ss << CORRECT_PATH_SEPARATOR << "cache" << CORRECT_PATH_SEPARATOR << "sentry";

    // prevent a crash if cache dir doesnt exist
    bool exists = std::filesystem::exists(sentry_db.str());
    if (!exists)
    {
        bool mkdir = std::filesystem::create_directories(sentry_db.str());
        if (mkdir)
        {
            Msg("Successfully created sentry cache folder at %s.\n", sentry_db.str().c_str());
        }
        else
        {
            Warning("Failed to create sentry cache folder at %s! Your game might crash.\n", sentry_db.str().c_str());
        }
    }

    sentry_options_t* options               = sentry_options_new();
    constexpr char releaseVers[256]         = VPC_QUOTE_STRINGIFY(SENTRY_RELEASE_VERSION);
    sentry_options_set_traces_sample_rate   (options, 1);
    sentry_options_set_on_crash             (options, SENTRY_CRASHFUNC, (void*)NULL);
    sentry_options_set_dsn                  (options, real_sentry_url.c_str());
    sentry_options_set_release              (options, releaseVers);
    sentry_options_set_debug                (options, 1);
    sentry_options_set_max_breadcrumbs      (options, 1024);
    sentry_options_set_require_user_consent (options, 1);
    sentry_options_set_logger               (options, sentry_logger, NULL);

    // only windows needs the crashpad exe
#ifdef _WIN32
    sentry_options_set_handler_path         (options, crash_exe.str().c_str());
#endif

    sentry_options_set_database_path        (options, sentry_db.str().c_str());
    sentry_options_set_shutdown_timeout     (options, 5000);

    sentry_conlog = {};
    sentry_conlog << sentry_db.str() << CORRECT_PATH_SEPARATOR << "last_crash_console_log.txt";
    sentry_options_add_attachment(options, sentry_conlog.str().c_str());


/*
    char inventorypath[MAX_PATH] = {};
    V_snprintf(inventorypath, MAX_PATH, "%stf_inventory.txt", last_element);
    sentry_options_add_attachment(options, inventorypath);

    char steaminfpath[MAX_PATH] = {};
    V_snprintf(steaminfpath, MAX_PATH, "%ssteam.inf", last_element);
    sentry_options_add_attachment(options, steaminfpath);

    char configpath[MAX_PATH] = {};
    V_snprintf(configpath, MAX_PATH, "%scfg/config.cfg", last_element);
    sentry_options_add_attachment(options, configpath);

    char badipspath[MAX_PATH] = {};
    V_snprintf(badipspath, MAX_PATH, "%scfg/badips.txt", last_element);
    sentry_options_add_attachment(options, badipspath);
*/


    
    // using C here because we have to set up for our signal handler later
    char conLogLoc[MAX_PATH] = {};
    snprintf(conLogLoc, MAX_PATH, "%s", sentry_conlog.str().c_str());
    FILE* conlog        = fopen(conLogLoc, "w+");
    conFileFilePtr      = (uintptr_t)conlog;


    std::stringstream sentry_loglog = {};
    sentry_loglog << sentry_db.str() << CORRECT_PATH_SEPARATOR << "_sentry.log";

    char sentryLogLoc[MAX_PATH] = {};
    snprintf(sentryLogLoc, MAX_PATH, "%s", sentry_loglog.str().c_str());
    FILE* senlog            = fopen(sentryLogLoc, "w+");
    sentryLogFilePtr        = (uintptr_t)senlog;


    int sentryinit = sentry_init(options);
    if (sentryinit != 0)
    {
        Warning("Sentry initialization failed!\n");
        didinit.store(false);
        return;
    }
    // sentry_reinstall_backend();
    didinit.store(true);


    // HAS TO RUN AFTER SENTRY INIT!!!
    SetSteamID();
    SentrySetTags();
    sentry_add_breadcrumb(sentry_value_new_breadcrumb(NULL, __FUNCTION__));
    DevMsg(2, "Sentry initialization success!\n");

#ifdef _WIN32
    mainWindowHandle = (sig_atomic_t)FindWindow("Valve001", NULL);
#endif
    InternalError_Init();

    ConVarRef cl_send_error_reports("cl_send_error_reports");
    // get the current version of our consent
    sentry_user_consent_reset();
    sentry_callback(cl_send_error_reports.GetLinkedConVar(), "", -2.0);
    
    // for adding cmdline to context on crash
    cmdline = new char[2048] {};
#ifdef _WIN32
    snprintf((char*)cmdline, 2048, "%s", GetCommandLineA());
#else
    snprintf( (char*)cmdline, 2048, "%s", CommandLine()->GetCmdLine());
#endif

    // for adding paths to context on crash
    char _[2] = {};
    const size_t len = filesystem->GetSearchPath("GAME", true, _, 1);
    char* paths = new char[len + 2] {};
    /*const size_t newlen = */filesystem->GetSearchPath("GAME", true, paths, len + 1);
    std::string strpaths(paths);
    delete [] paths;
    std::vector<std::string> vec = UTIL_SplitSTDString( strpaths, std::string(";") );

    //AssertAlways(len == newlen);

    std::stringstream path_ss = {};
    for ( auto& str : vec )
    {
        path_ss << str << "\n";
    }
    // path_ss << "\n";
    
    gamePathsSize = path_ss.str().size();
    gamePaths = new char[gamePathsSize + 2] {};
    memcpy( (void*)gamePaths, path_ss.str().c_str(), gamePathsSize );


    // already asked
    if (cl_send_error_reports.GetInt() >= 0)
    {
        return;
    }

    // localize these with translations eventually
    const char* windowText = \
        "Do you want to send error reports to the developers?\n"
        "Error reports will include your SteamID, and any pertinent game info (class, loadout, current map, convars, console spew, etc.)\n"
        "Check out our privacy policy at " VPC_QUOTE_STRINGIFY(SENTRY_PRIVACY_POLICY_URL) "\n"
        "You can change this later by changing the cl_send_error_reports cvar.";

    const char* windowTitle = \
        "Error reporting consent popup";

#ifdef _WIN32

    ShowWindow((HWND)mainWindowHandle, SW_HIDE);

    int msgboxID = MessageBoxA(
        NULL,
        windowText,
        windowTitle,
        MB_YESNO | MB_SETFOREGROUND | MB_TOPMOST
    );
    switch (msgboxID)
    {
    case IDYES:
        cl_send_error_reports.SetValue(1);
        break;
    case IDNO:
        cl_send_error_reports.SetValue(0);
        break;
    }
    ShowWindow((HWND)mainWindowHandle, SW_RESTORE);

#else

    SDL_MessageBoxData messageboxdata   = {};
    messageboxdata.flags                = SDL_MESSAGEBOX_INFORMATION;
    messageboxdata.window               = nullptr;
    messageboxdata.title                = windowTitle;
    messageboxdata.message              = windowText;

    SDL_MessageBoxButtonData buttons[2] = {};
    buttons[0].flags                    = SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
    buttons[0].buttonid                 = 0;
    buttons[0].text                     = "Yes";
    buttons[1].flags                    = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
    buttons[1].buttonid                 = 1;
    buttons[1].text                     = "No";

    messageboxdata.numbuttons           = 2;
    messageboxdata.buttons              = buttons;
    messageboxdata.colorScheme          = nullptr;

    int buttonid                        = {};
    SDL_ShowMessageBox(&messageboxdata, &buttonid);

    // yes
    if (buttonid == 0)
    {
        cl_send_error_reports.SetValue(1);
    }
    // no
    else if (buttonid == 1)
    {
        cl_send_error_reports.SetValue(0);
    }
    // fall thru
    else
    {
        cl_send_error_reports.SetValue(-1);
    }
#endif
}

void SetSteamID()
{
    sentry_value_t user = sentry_value_new_object();

    std::string steamid_string;
    if (!steamapicontext || !steamapicontext->SteamUser())
    {
        steamid_string = "none";
    }
    else
    {
        CSteamID c_steamid = steamapicontext->SteamUser()->GetSteamID();
        if (c_steamid.IsValid())
        {
            // i eated it all </3
            steamid_string = std::to_string(c_steamid.ConvertToUint64());

#if defined (sentry_id_debug)
            Warning("steamid_string -> %s\n", steamid_string.c_str());
#endif
        }
        else
        {
            steamid_string = "invalid";
        }
    }
    sentry_value_set_by_key(user, "id", sentry_value_new_string(steamid_string.c_str()));
    sentry_set_user(user);
}

void SentryMsg(const char* logger, const char* text, bool forcesend /* = false */)
{
    if ( (!forcesend && cl_send_error_reports.GetInt() <= 0) || !(g_Sentry.didinit.load(std::memory_order_relaxed)) )
    {
        // Warning("NOT SENDING!\n");
        return;
    }

    std::thread sentry_msg_thread(_SentryMsgThreaded, logger, text, forcesend);
    sentry_msg_thread.detach();
}

void _SentryMsgThreaded(const char* logger, const char* text, bool forcesend /* = false */)
{
    SentrySetTags();

    sentry_capture_event
    (
        sentry_value_new_message_event
        (
            SENTRY_LEVEL_INFO,
            logger,
            text
        )
    );

    return;
}

// level should ALWAYS be fatal / error / warning / info / debug - prefer info unless you know better
// logger should always be __FUNCTION__
// message should be a message of "what you are doing" / "what you are logging"
// context info? You need to read the docs: https://docs.sentry.io/platforms/native/usage/#manual-events
void SentryEvent(const char* level, const char* logger, const char* message, sentry_value_t ctxinfo, bool forcesend /* = false */)
{
    if ( (!forcesend && cl_send_error_reports.GetInt() <= 0) || !(g_Sentry.didinit.load(std::memory_order_relaxed)) )
    {
        // Warning("NOT SENDING!\n");
        return;
    }
    std::thread sentry_event_thread(_SentryEventThreaded, level, logger, message, ctxinfo, forcesend);
    sentry_event_thread.detach();
}

void _SentryEventThreaded(const char* level, const char* logger, const char* message, sentry_value_t ctxinfo, bool forcesend /* = false */)
{
    SentrySetTags();

    sentry_value_t event = sentry_value_new_event();
    sentry_value_set_by_key(event, "level", sentry_value_new_string(level));
    sentry_value_set_by_key(event, "logger", sentry_value_new_string(logger));
    sentry_value_set_by_key(event, "message", sentry_value_new_string(message));

    sentry_value_t sctx = sentry_value_new_object();
    sentry_value_set_by_key(sctx, "event context", ctxinfo);

    sentry_value_set_by_key(event, "contexts", sctx);
    sentry_capture_event(event);

    sentry_flush(9999);
}

const std::vector<std::string> cvarList =
{
    "mat_dxlevel",
    "host_timescale",
    "sv_cheats",
    "mat_picmip",
    "r_rootlod",
    "r_lod",
    "developer",
};


void SentrySetTags()
{
    char mapname[128] = {};
    UTIL_GetMap(mapname);
    if (mapname && mapname[0])
    {
        sentry_set_tag("current map", mapname);
    }
    else
    {
        sentry_set_tag("current map", "none");
    }

    char ipadr[64] = {};
    UTIL_GetRealRemoteAddr(ipadr);
    if (ipadr && ipadr[0])
    {
        sentry_set_tag("server ip", ipadr);
    }
    else
    {
        sentry_set_tag("server ip", "none");
    }

    static bool isWine = checkWine();
    if (isWine)
    {
        sentry_set_tag("wine", "yes");
    }
    else
    {
        sentry_set_tag("wine", "no");
    }

    if (!cvar)
    {
        return;
    }
	for (auto& element : cvarList)
	{
        ConVarRef cRef(element.c_str(), true);
        
        if (cRef.IsValid() && cRef.GetName() && cRef.GetString())
        {
            sentry_set_tag(cRef.GetName(), cRef.GetString());
        }
        else
        {
            Warning("Failed getting sentry tag for %s\n", element.c_str());
        }
	}
}

void SentryAddressBreadcrumb(void* address, const char* optionalName)
{
    std::string addr = UTIL_AddrToString(address);
    sentry_value_t addr_crumb = sentry_value_new_breadcrumb(NULL, NULL);

    sentry_value_t address_object = sentry_value_new_object();
    sentry_value_set_by_key(address_object, optionalName ? optionalName : _OBFUSCATE("variable address"), sentry_value_new_string(addr.c_str()));
    sentry_value_set_by_key
    (
        addr_crumb, "data", address_object
    );
    sentry_add_breadcrumb(addr_crumb);
}
#endif // defined(CLIENT_DLL) && defined(SDKCURL) && defined(SDKSENTRY)
