#ifndef BLACKLISTS_H
#define BLACKLISTS_H

#ifdef CLIENT_DLL

#include <helpers/misc_helpers.h>
#include <helpers/steam_helpers.h>

#include "tier0/valve_minmax_off.h"
#include <thread>
#include "tier0/valve_minmax_on.h"

bool         CompareServerBlacklist(const char* ipaddr);

class CBlacklists
{
public:

    CBlacklists();

// bool CBlacklists::CompareServerBlacklist(const char* ipaddr)

    // static bool         CompareServerBlacklist(std::string);

    CCallResult<CBlacklists, HTTPRequestCompleted_t> BlacklistsCallResult;

    void                GetBlacklist();


private:
    void                BlacklistDownloadCallback(HTTPRequestCompleted_t* arg, bool bFailed);
};

#endif // BLACKLISTS_H
#endif // CLIENT_DLL
