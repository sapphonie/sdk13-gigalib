#include <cbase.h>


#if defined(CLIENT_DLL) && defined(BLACKLISTS) && defined(BLACKLISTS_URL) && defined(ENGINE_DETOURS)

#include <qol/blacklists.h>

// #define blacklists_debug

// ctor
CBlacklists g_CBlacklists;

// We need to do this so we're not passing a member func ptr to our init func
// To do so is undefined behavior because callconv etc
void GetBlacklistTrampoline( )
{
    g_CBlacklists.GetBlacklist();
}

CBlacklists::CBlacklists()
{
    RUN_THIS_FUNC_WHEN_STEAM_INITS( &GetBlacklistTrampoline );
}

#define timeoutms 5000
// Thread this someday
void CBlacklists::GetBlacklist()
{
    SteamAPICall_t hCallServer;

    HTTPRequestHandle httphandle = steamapicontext->SteamHTTP()->CreateHTTPRequest(k_EHTTPMethodGET, FORCE_OBFUSCATE(BLACKLISTS_URL));
    steamapicontext->SteamHTTP()->SetHTTPRequestAbsoluteTimeoutMS(httphandle, timeoutms);
    steamapicontext->SteamHTTP()->SendHTTPRequest(httphandle, &hCallServer);
    steamapicontext->SteamHTTP()->PrioritizeHTTPRequest(httphandle);

    BlacklistsCallResult.Set(hCallServer, this, &CBlacklists::BlacklistDownloadCallback);
}

void CBlacklists::BlacklistDownloadCallback(HTTPRequestCompleted_t* arg, bool bFailed)
{
    if (bFailed || arg->m_eStatusCode < 200 || arg->m_eStatusCode > 299)
    {
        #ifdef blacklists_debugging
        Warning("REQUEST EXPLODED UH OH\n");
        #endif

        steamapicontext->SteamHTTP()->ReleaseHTTPRequest(arg->m_hRequest);
        return;
    }
    uint32 size;
    steamapicontext->SteamHTTP()->GetHTTPResponseBodySize(arg->m_hRequest, &size);

    unsigned char buffer[1024] = {};

    if (size > 0)
    {
        FileHandle_t filehandle = g_pFullFileSystem->Open("cfg/badips.txt", "w+", "GAME");
        if (!filehandle)
        {
            #ifdef blacklists_debugging
            Warning("FILE COULDN'T BE CREATED UH OH\n");
            #endif

            steamapicontext->SteamHTTP()->ReleaseHTTPRequest(arg->m_hRequest);
            return;
        }
        steamapicontext->SteamHTTP()->GetHTTPResponseBodyData(arg->m_hRequest, buffer, size);
        g_pFullFileSystem->Write(buffer, size, filehandle);
        g_pFullFileSystem->Flush(filehandle);
        g_pFullFileSystem->Close(filehandle);
    }

    #ifdef blacklists_debugging
    Warning("char %s, size %i\n", buffer, size);
    #endif

    steamapicontext->SteamHTTP()->ReleaseHTTPRequest(arg->m_hRequest);
}

// Return true if client should connect and false if they shouldn't
bool CBlacklists::CompareServerBlacklist(const char* ipaddr)
{
    #ifdef blacklists_debugging
    Warning("CBlacklists::CompareServerBlacklist->\n");
    #endif

    FileHandle_t filehandle = g_pFullFileSystem->Open("cfg/badips.txt", "r", "GAME");
    if (!filehandle)
    {
        #ifdef blacklists_debugging
        Warning("FILE COULDN'T BE CREATED OR OPENED UH OH\n");
        #endif
        return true;
    }

    char thisline[64];
    while (g_pFullFileSystem->ReadLine(thisline, sizeof(thisline), filehandle))
    {
        // ignore fake shit
        if
        (
                strlen(thisline) < 7    // smallest = "1.1.1.1\n"           = 8,  give it a bit of leeway just in case
            ||  strlen(thisline) > 17   // biggest  = "255.255.255.255\n"   = 16, ^
            || !strstr(thisline, ".")   // no period? it's not an IP address.
            ||  strstr(thisline, "#")   // comments!
        )
        {
            continue;
        }

        #ifdef blacklists_debugging
        Warning("strlen = %i\n", strlen(thisline));
        Warning("ipaddr = %s\n", ipaddr);
        Warning("thisip = %s\n", thisline);
        #endif

        // Strip newlines. Probably
        thisline[strcspn(thisline, "\n")] = 0;
        thisline[strcspn(thisline, "\r")] = 0;

        // Match!
        if ( Q_strcmp(ipaddr, thisline) == 0 )
        {
            #ifdef blacklists_debugging
            Warning("Uh oh! %s is a blacklisted server...\n", ipaddr);
            #endif

            g_pFullFileSystem->Close(filehandle);
            return false;
        }
    }

    g_pFullFileSystem->Close(filehandle);

    return true;
}
#endif // defined(CLIENT_DLL) && defined(BLACKLISTS) && defined(BLACKLISTS_URL) && defined(ENGINE_DETOURS)
