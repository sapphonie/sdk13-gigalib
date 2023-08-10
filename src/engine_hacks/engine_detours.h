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

#ifdef GAME_DLL


struct GIGAplayerStruct
{
    int                     entindex                    = -1;
    CBasePlayer*            basePlayerParent            = nullptr;
    int 					m_iLastProcessPacketTick    = 0;
    int 					m_iThisProcessPacketTick    = 0;
    double 					m_dflProcessPacketTime      = 0.0;
};


GIGAplayerStruct thePlayers[MAX_PLAYERS] = {};
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
    void                Shutdown() override;

};


#endif
