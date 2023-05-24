#ifndef ENGINEDETOURS_H
#define ENGINEDETOURS_H
#ifdef _WIN32
#pragma once
#endif



#include <helpers/misc_helpers.h>
#include <memy/memytools.h>
#ifdef CLIENT_DLL
#ifdef BLACKLISTS
#include <qol/blacklists.h>
#endif
#endif

// #include <memy/detourhook.hpp>


#ifdef SDKSENTRY
    #include <sdksentry/sdksentry.h>
#endif

class CEngineDetours : public CAutoGameSystem
{
public:
                        CEngineDetours();

    void                PostInit() override;

    // int GetSignonState(CBasePlayer* basePlayer);
    // int GetSignonState(INetChannelInfo* info);
};
#endif
