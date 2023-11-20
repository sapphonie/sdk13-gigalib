#ifndef BLACKLISTS_H
#define BLACKLISTS_H


#if defined(CLIENT_DLL) && defined(BLACKLISTS) && defined(BLACKLISTS_URL) && defined(ENGINE_DETOURS) && defined(SDKCURL)


#include <helpers/misc_helpers.h>
#include <helpers/steam_helpers.h>
#include <sdkCURL/sdkCURL.h>

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
#endif // #if defined(CLIENT_DLL) && defined(BLACKLISTS) && defined(BLACKLISTS_URL) && defined(ENGINE_DETOURS) && defined(SDKCURL)
