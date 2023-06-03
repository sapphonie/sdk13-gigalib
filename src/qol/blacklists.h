#ifndef BLACKLISTS_H
#define BLACKLISTS_H

#ifdef CLIENT_DLL

#include <helpers/misc_helpers.h>
#include <helpers/steam_helpers.h>
#include <sdkCURL/sdkCURL.h>
#include "tier0/valve_minmax_off.h"
#include <thread>
#include "tier0/valve_minmax_on.h"

bool         CompareServerBlacklist(const char* ipaddr);

class CBlacklists : public CAutoGameSystem
{
public:

    CBlacklists();
    void                PostInit() override;

// bool CBlacklists::CompareServerBlacklist(const char* ipaddr)

    // static bool         CompareServerBlacklist(std::string);
    void                BlacklistsURLCB(const curlResponse* resp);


};

#endif // BLACKLISTS_H
#endif // CLIENT_DLL
