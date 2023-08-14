#ifndef ENGINEDETOURS_H
#define ENGINEDETOURS_H


#ifdef _WIN32
#pragma once
#endif

#undef NOINLINE
#undef FASTCALL
#define __thiscall
#include <valve_minmax_off.h>
#define ZYDIS_DEPRECATED
#include <polyhook2/IHook.hpp>
#include <polyhook2/Detour/x86Detour.hpp>

#include <valve_minmax_on.h>

#ifdef _WIN32
// hack because theres no macro for DEBUG that i can find
// feel free to PR to add it to vpc if you know of one
#ifdef DEBUG
    #pragma comment( lib, "../shared/sdk13-gigalib/src/polyhook/bin/debug/PolyHook_2.lib" )
    #pragma comment( lib, "../shared/sdk13-gigalib/src/polyhook/bin/debug/Zydis.lib" )
#else
    #pragma comment( lib, "../shared/sdk13-gigalib/src/polyhook/bin/release/PolyHook_2.lib" )
    #pragma comment( lib, "../shared/sdk13-gigalib/src/polyhook/bin/release/Zydis.lib" )
#endif
#endif

#include <helpers/misc_helpers.h>
#include <memy/memytools.h>
#ifdef CLIENT_DLL
#ifdef BLACKLISTS
    #include <qol/blacklists.h>
#endif
#endif


#ifdef SDKSENTRY
    #include <sdksentry/sdksentry.h>
#endif


class sdkdetour
{
public:
    void whackVars()
    {
        patternSize         = {};
        pattern             = {};
        patternAddr         = {};
        detourPtr           = {};
        // callbackAddr     = {};
        detourTrampoline    = {};
    };

    sdkdetour()
    {
        //Warning("test");
        whackVars();
    };
    ~sdkdetour()
    {
        //Warning("untest");
        //detourPtr->unHook(); // broken by dynamic code policy on windows...?
        //whackVars();
    };
    size_t patternSize          = {};
    const char* pattern         = {};
    uintptr_t patternAddr       = {};
    PLH::x86Detour* detourPtr   = {};
    //uint64_t callbackAddr       = {};
    uint64_t detourTrampoline   = {};
};

static void populateAndInitDetour(sdkdetour* detour, void* callback)
{
    detour->patternAddr = memy::FindPattern
    (
        engine_bin,
        detour->pattern,
        detour->patternSize,
        0
    );
    if (!detour->patternAddr)
    {
        Error("Could not get address for detour!");
    }
    detour->detourPtr = new PLH::x86Detour
    (
        (const uint64_t)detour->patternAddr,
        (const uint64_t)(callback),
        &detour->detourTrampoline
    );
    detour->detourPtr->hook();
}


class CEngineDetours : public CAutoGameSystem
{
public:
                        CEngineDetours();

    void                Shutdown() override;

};

CEngineDetours* gCEngineDetours = nullptr;

#endif
