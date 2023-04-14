#ifndef BLACKLISTS_H
#define BLACKLISTS_H

#ifdef CLIENT_DLL

#include <helpers/misc_helpers.h>
#include <helpers/steam_helpers.h>

#include <thread>

class CBlacklists
{
public:

    CBlacklists();


    static bool         CompareServerBlacklist(const char* ipaddr);
    
    CCallResult<CBlacklists, HTTPRequestCompleted_t> BlacklistsCallResult;

    void                GetBlacklist();


private:
    void                BlacklistDownloadCallback(HTTPRequestCompleted_t* arg, bool bFailed);
};

#endif // BLACKLISTS_H
#endif