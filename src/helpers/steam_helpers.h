#ifndef STEAMHELPERS_H
#define STEAMHELPERS_H
#ifdef _WIN32
#pragma once
#endif


typedef void (*fPtr)(void);
#ifdef CLIENT_DLL

#include <dbg.h>
#include <steam/isteamutils.h>
#include "clientsteamcontext.h"
#include <steam/isteamhttp.h>
#include <steam/isteamutils.h>



class CSteamHelpers
{
public:

    CSteamHelpers();

private:
    std::thread SteamSpin;
    static void SpinUntilSteamIsAlive();

public:
    // vector of `void function(void)` ptrs
    std::vector<fPtr> funcPtrVector;
    void iterFuncsToRun();
};

void RUN_THIS_FUNC_WHEN_STEAM_INITS(fPtr yourFunctionPointer);

#endif // inc guard
#endif
