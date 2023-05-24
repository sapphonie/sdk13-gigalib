#include <cbase.h>

#ifdef CLIENT_DLL
#include <helpers/steam_helpers.h>

// run our ctor
CSteamHelpers g_SteamHelpers;
CSteamHelpers::CSteamHelpers()
{
    Warning("CSteamHelpers->\n");

    // spin off a thread and wait until steam is up before we call any of our funcs
    SteamSpin = std::thread( &SpinUntilSteamIsAlive );
    SteamSpin.detach();
}

void CSteamHelpers::SpinUntilSteamIsAlive()
{
    // 30sec
    const int maxtries = 30;
    for (size_t tries = 0; tries <= maxtries; tries++)
    {
        if (steamapicontext && steamapicontext->SteamHTTP())
        {
            g_SteamHelpers.iterFuncsToRun();
            // exits the thread
            return;
        }
        Warning("Failed getting Steam interfaces! Attempt #%i...\n", tries);
        std::this_thread::sleep_for(std::chrono::seconds(1));
        continue;
    }
    Error("Failed getting Steam interfaces! Try restarting Steam!");
}

void CSteamHelpers::iterFuncsToRun()
{
    // loop thru our funcptr vec
    for (auto& element : funcPtrVector)
    {
        // call it up
        element();
        Warning("-> Called %p after SteamInit\n", element);
    }
    funcPtrVector.clear();
}

// other functions will include this file and then use this to call their funcs on steam init
/*
// We need to do this so we're not passing a member func ptr to our init func
// To do so is undefined behavior because callconv etc
void DoThingTrampoline( )
{
    g_Thing.DoThing();
}

// ctor
CThing g_Thing;
CThing::CThing()
{
    RUN_THIS_FUNC_WHEN_STEAM_INITS( &DoThingTrampoline );
}
*/
void RUN_THIS_FUNC_WHEN_STEAM_INITS(fPtr yourFunctionPointer)
{
    g_SteamHelpers.funcPtrVector.push_back(yourFunctionPointer);
}

#endif // CLIENT_DLL
