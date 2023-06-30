#include <cbase.h>


#if defined(CLIENT_DLL) && defined(BLACKLISTS)
// && defined(BLACKLISTS_URL) && defined(ENGINE_DETOURS)

#include <qol/blacklists.h>
#include <sdkCURL/sdkCURL.h>

// #define blacklists_debug

// ctor
CBlacklists g_CBlacklists;


CBlacklists::CBlacklists()
{
}

void CBlacklists__SentryURLCB__THUNK(const curlResponse* curlRepsonseStruct)
{
    g_CBlacklists.BlacklistsURLCB(curlRepsonseStruct);
}

void CBlacklists::PostInit()
{
    DevMsg(2, "Blacklists Postinit!\n");

    g_sdkCURL->CURLGet(VPC_QUOTE_STRINGIFY(BLACKLISTS_URL), CBlacklists__SentryURLCB__THUNK);
}

#define maxBlacklistsLen 1024
void CBlacklists::BlacklistsURLCB(const curlResponse* resp)
{
    if (!resp->completed || resp->failed || resp->respCode != 200)
    {
        Warning("Failed to get server blacklist urls.\n");
        Warning("completed = %i, failed = %i, statuscode = %i, bodylen = %i\n", resp->completed, resp->failed, resp->respCode, resp->bodyLen);
        return;
    }
    
    if ( !resp->bodyLen || resp->bodyLen > maxBlacklistsLen )
    {
        Warning("blacklists bodyLen bad, == %i\n", resp->bodyLen);
        return;
    }

    char* buffer = new char[resp->body.size() + 1] {};
    strcpy(buffer, resp->body.c_str());

    // strip newlines
    buffer[strcspn(buffer, "\n")] = 0;
    buffer[strcspn(buffer, "\r")] = 0;

    FileHandle_t filehandle = g_pFullFileSystem->Open("cfg/badips.txt", "w+", "GAME");
    if (!filehandle)
    {
        #ifdef blacklists_debugging
        Warning("FILE COULDN'T BE CREATED UH OH\n");
        #endif

        delete[] buffer;

        return;
    }
    g_pFullFileSystem->Write(buffer, sizeof(buffer), filehandle);
    g_pFullFileSystem->Flush(filehandle);
    g_pFullFileSystem->Close(filehandle);

    delete[] buffer;
}


// Return true if client should connect and false if they shouldn't
bool CompareServerBlacklist(const char* ipaddr)
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
