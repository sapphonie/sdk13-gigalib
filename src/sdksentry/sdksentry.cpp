#include <cbase.h>

#ifdef CLIENT_DLL

#include "tier0/valve_minmax_off.h"
#include <sstream>
#include <string>
#include "tier0/valve_minmax_on.h"

#include <sdksentry/sdksentry.h>

CSentry g_Sentry;
void SentryInitTrampoline()
{
    g_Sentry.SentryInit();
}

CSentry::CSentry()
{
    didinit     = false;
    didshutdown = false;
    RUN_THIS_FUNC_WHEN_STEAM_INITS(&SentryInitTrampoline);
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
    engine->ExecuteClientCmd("host_writeconfig");
}

ConVar cl_send_error_reports("cl_send_error_reports", "-1", FCVAR_ARCHIVE,
    "Enables/disables sending error reports to the developers to help improve the game.\n"
    "Error reports will include your SteamID, and any pertinent game info (class, loadout, current map, etc.) - we do not store any personally identifiable information.\n"
    "Read more at " V_STRINGIFY(SENTRY_PRIVACY_POLICY_URL) "\n"
    "-1 asks you again on game boot and disables reporting, 0 disables reporting and does not ask you again, 1 enables reporting.\n",
    sentry_callback
);

void CSentry::PostInit()
{
    Msg("Sentry Postinit!\n");
    GetURL();
}

void CSentry::GetURL()
{
    SteamAPICall_t hCallServer;
    if (!steamapicontext->SteamHTTP())
    {
        Error("Couldn't get SteamHTTP interface! Try restarting Steam.\n");
    }

    const char* sentry_dl = FORCE_OBFUSCATE( V_STRINGIFY(SENTRY_URL) );

    HTTPRequestHandle httphandle = steamapicontext->SteamHTTP()->CreateHTTPRequest(k_EHTTPMethodGET, sentry_dl);
    steamapicontext->SteamHTTP()->SetHTTPRequestAbsoluteTimeoutMS(httphandle, 5000);
    steamapicontext->SteamHTTP()->SendHTTPRequest(httphandle, &hCallServer);
    steamapicontext->SteamHTTP()->PrioritizeHTTPRequest(httphandle);

    SentryURLCallResult.Set(hCallServer, this, &CSentry::SentryURLCB);
}

void CSentry::SentryURLCB(HTTPRequestCompleted_t* arg, bool bFailed)
{
    if (bFailed || arg->m_eStatusCode < 200 || arg->m_eStatusCode > 299)
    {
        Warning("Failed to get Sentry DSN. Sentry integration disabled.\n");

        steamapicontext->SteamHTTP()->ReleaseHTTPRequest(arg->m_hRequest);
        return;
    }
    uint32 size;
    steamapicontext->SteamHTTP()->GetHTTPResponseBodySize(arg->m_hRequest, &size);

    char buffer[256] = {};

    if (size > 0)
    {
        steamapicontext->SteamHTTP()->GetHTTPResponseBodyData(arg->m_hRequest, (unsigned char*)buffer, size);
    }

    // strip newlines
    buffer[strcspn(buffer, "\n")] = 0;
    buffer[strcspn(buffer, "\r")] = 0;

    V_strncpy(real_sentry_url, buffer, sizeof(buffer) /* strlen(buffer + 1) */);

    steamapicontext->SteamHTTP()->ReleaseHTTPRequest(arg->m_hRequest);

    SentryInit();
}

// We ignore any crashes whenever the engine shuts down on linux.
// We shouldn't, but we do. Feel free to fix all the shutdown crashes if you want,
// then you can remove this
void CSentry::Shutdown()
{
#ifndef _WIN32
    didshutdown = true;
#endif

    // Warning("-> CSentry::Shutdown <-\n");
    // printf("-> CSentry::Shutdown <-\n");
}

#ifndef _WIN32
     #include <SDL.h>
#endif
// DO NOT THREAD THIS OR ANY FUNCTIONS CALLED BY IT UNDER ANY CIRCUMSTANCES
sentry_value_t SENTRY_CRASHFUNC(const sentry_ucontext_t* uctx, sentry_value_t event, void* closure)
{
    Warning("[Warning] SENTRY CAUGHT A CRASH!\n");
    printf("[printf] SENTRY CAUGHT A CRASH!\n");


    
    // MessageBoxW(NULL, crashdialogue, crashtitle, MB_OK);
    // sentry_set_context(CSentry::SentryInstance().nowFUNC, CSentry::SentryInstance().nowCTX);

    sentry_flush(9999);

    #ifndef _WIN32
    if (g_Sentry.didshutdown)
    {
        // Warning("NOT SENDING CRASH TO SENTRY.IO BECAUSE THIS IS A NORMAL SHUTDOWN! BUHBYE!\n");
        sentry_value_decref(event);
        return sentry_value_new_null();
    }
    #endif

    const char* crashdialogue =
    "\"BONK!\"\n\n"
    "TF2 Classic has crashed. Sorry about that.\n"
    "If you've enabled crash reporting,\n"
    "we'll get right on that.\n";

    const char* crashtitle = 
    "TF2Classic crash handler";

#ifdef _WIN32
    MessageBoxA(NULL, crashdialogue, crashtitle, MB_OK);
#else
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, crashtitle, crashdialogue, NULL);
#endif

    // sentry_value_t ctxinfo = sentry_value_new_object();
    // sentry_value_set_by_key(ctxinfo, "test", sentry_value_new_string("test str"));
    // SentryEvent("info", __FUNCTION__, "testEvent", ctxinfo);

    if (cl_send_error_reports.GetInt() <= 0 || !g_Sentry.didinit)
    {
        // Warning("NOT SENDING CRASH TO SENTRY.IO DUE TO USER NONCONSENT OR INIT FAILING! BUHBYE!\n");
        sentry_value_decref(event);
        return sentry_value_new_null();
    }

    sentry_flush(9999);

    // sentry_close();
    return event;
}

// #define sentry_id_debug
// #define sentry_id_spewhashes

// Why? Well, some steam changes require that we manually hook into the minidump sys and override it.
// handle_exception tosses it over to the sentry crash handler.
#ifdef _WIN32
#include <minidump.h>
void mini(unsigned int uStructuredExceptionCode, _EXCEPTION_POINTERS* pExceptionInfo, const char* pszFilenameSuffix)
{
    sentry_handle_exception( (sentry_ucontext_t*)pExceptionInfo);
    sentry_flush(9999);
}
#endif

void CSentry::SentryInit()
{
    Msg("Sentry init!\n");
    const char* mpath = ConVarRef("_modpath", false).GetString();

#ifdef _WIN32
    // location of the crashpad handler (in moddir/bin)
    char crash_exe[MAX_PATH] = {};
    V_snprintf(crash_exe, MAX_PATH, "%sbin%ccrashpad_handler.exe", mpath, CORRECT_PATH_SEPARATOR);
    AssertMsg1(NULL, "crash_exe = %s", crash_exe);
#endif

    // location of the sentry workdir (we're just gonna stick it in mod dir/cache)
    char sentry_db[MAX_PATH] =  {};
    V_snprintf(sentry_db, MAX_PATH, "%scache", mpath);
    AssertMsg1(NULL, "cache = %s", sentry_db);

    // Suprisingly, this just works to disable built in Valve crash stuff for linux
    CommandLine()->AppendParm("-nominidumps", "");
    CommandLine()->AppendParm("-nobreakpad", "");


    sentry_options_t* options           = sentry_options_new();

    sentry_options_set_traces_sample_rate(options, 1);
    sentry_options_set_on_crash         (options, SENTRY_CRASHFUNC, (void*)NULL);
    sentry_options_set_dsn              (options, real_sentry_url);
    sentry_options_set_release          (options, __TIME__ __DATE__);
    sentry_options_set_debug            (options, 1);
    // only windows needs the crashpad exe
#ifdef _WIN32
    sentry_options_set_handler_path     (options, crash_exe);
    SetMiniDumpFunction(mini);
#endif
    sentry_options_set_database_path    (options, sentry_db);
    sentry_options_set_shutdown_timeout (options, 9999);

#ifndef _WIN32
    // Just in case
    // sentry_reinstall_backend();
#endif
    

    /*
    // attachments !!
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


    if (sentry_init(options) != 0)
    {
        Warning("Sentry initialization failed!\n");
        CSentry::didinit = false;
        return;
    }

    SetSteamID();

    CSentry::didinit = true;

    sentry_add_breadcrumb(sentry_value_new_breadcrumb(NULL, "SentryInit"));
    Msg("Sentry initialization success!\n");

    SentryMsg("info", __FUNCTION__);
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

#include <thread>
#include <util_shared.h>
void SentryMsg(const char* logger, const char* text, bool forcesend /* = false */)
{
    if ( (!forcesend && cl_send_error_reports.GetInt() <= 0) || !g_Sentry.didinit )
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
    if ( (!forcesend && cl_send_error_reports.GetInt() <= 0) || !g_Sentry.didinit )
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

    /*
    for (int i = 0; i < 1000000; i++)
    {
        void* junk = malloc(100);
        memset(junk, 0x42, 100);
        // free(junk);
    }
    */

    void* junk = malloc(100);
    memset(junk, 0x42, 100);
    junk = malloc(100);

    return;
}

void SentrySetTags()
{
    char mapname[128] = {};
    V_FileBase(engine->GetLevelName(), mapname, sizeof(mapname));
    V_strlower(mapname);
    if (mapname && mapname[0])
    {
        sentry_set_tag("current map", mapname);
    }
    else
    {
        sentry_set_tag("current map", "none");
    }

    char ipadr[64] = {};
    if (UTIL_GetRealRemoteAddr(ipadr) && ipadr && ipadr[0])
    {
        sentry_set_tag("server ip", ipadr);
    }
    else
    {
        sentry_set_tag("server ip", "none");
    }
}

void SentryAddressBreadcrumb(void* address, const char* optionalName )
{
    char addyString[11] = {};
    UTIL_AddrToString(address, addyString);
    sentry_value_t addr_crumb = sentry_value_new_breadcrumb(NULL, NULL);

    sentry_value_t address_object = sentry_value_new_object();
    sentry_value_set_by_key(address_object, optionalName ? optionalName : _OBFUSCATE("variable address"), sentry_value_new_string(addyString));
    sentry_value_set_by_key
    (
        addr_crumb, "data", address_object
    );
    sentry_add_breadcrumb(addr_crumb);
}
#endif