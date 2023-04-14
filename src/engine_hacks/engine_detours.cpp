//
//
// Basically all of this by https://sappho.io ! <3

#include <cbase.h>

#ifdef _WIN32
    #pragma once
#endif

#ifdef ENGINE_DETOURS
#include <engine_hacks/engine_detours.h>

// ctorCEngineDetours g_CEngineDetours;

CEngineDetours::CEngineDetours() : CAutoGameSystem("CEngineDetours")
{
}

// server detours
#if defined (GAME_DLL)
#include "player.h"
#include "util.h"
#include <iclient.h>
#include <inetchannel.h>
#include <inetmsghandler.h>

ConVar net_chan_proctime_limit_ms("net_chan_proctime_limit_ms", "128", FCVAR_NONE,
    "Max amount of time per tick a client is allowed to make the server spend processing network packets, in msec.\n"
    "Similar to TF2's net_chan_limit_msec, but uses a different calculation method.");

bool spew_procpacket = false;

void procpack_dbg_cb(IConVar* var, const char* pOldValue, float flOldValue)
{
    ConVarRef cvr(var);
    if (cvr.GetBool())
    {
        spew_procpacket = true;
        return;
    }
    spew_procpacket = false;
}

ConVar net_chan_debug_proctime_limit("net_chan_debug_proctime_limit", "0", FCVAR_NONE,
    "For debugging net_chan_proctime_limit_ms.", procpack_dbg_cb);


#define toobig 2048
#define toosmall 16
// How does this work?

// CNetChan::ProcessPacket calls CNetChan::ProcessPacketHeader as a subroutine
// CNetChan::ProcessPacketHeader seems to return packetflags.
// if flags == -1 the packet is invalid and will be ignored
//
// int __cdecl  ProcessPacket_sub_4DF310(int* this, _DWORD* a2, char a3)
// pretty sure a2 is a netpacket*
// pretty sure that char is a bool
// Unique string: "CNetChan::ProcessPacket"

////////////////////////////////////////////////
// CNetChan::ProcessPacket Detour
// For our net_chan_limit_msec reimplementation
// TL:DR; stops lagbots by limiting the amount
// of time clients are allowed to make the server
// spend processing packets
//
// No, we can't use INetChan::ProcessPacket;
// because we need to be able to Not Call The Function
// if clients are being dumb and stinky
////////////////////////////////////////////////

// https://stackoverflow.com/a/49066369
#include <chrono>

// NB: ALL OF THESE 3 FUNCTIONS BELOW USE SIGNED VALUES INTERNALLY AND WILL
// EVENTUALLY OVERFLOW (AFTER 200+ YEARS OR SO), AFTER WHICH POINT THEY WILL
// HAVE *SIGNED OVERFLOW*, WHICH IS UNDEFINED BEHAVIOR (IE: A BUG) FOR C/C++.
// But...that's ok...this "bug" is designed into the C++11 specification, so
// whatever. Your machine won't run for 200 years anyway...

// Get time stamp in milliseconds.
uint64_t millis()
{
    uint64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch())
            .count();
    return ms; 
}

// Get time stamp in microseconds.
uint64_t micros()
{
    uint64_t us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch())
            .count();
    return us; 
}

// Get time stamp in nanoseconds.
uint64_t nanos()
{
    uint64_t ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch())
            .count();
    return ns; 
}

static DetourHook Detour_ProcPacket{};

#ifdef _WIN32

    #define ProcessPacket_vars uintptr_t* _this, uintptr_t* dummy, uintptr_t* netpacket, bool a3
    #define ProcessPacket_varsnotype _this, dummy, netpacket, a3

    // it's a thiscall [member function] so we need a dummy edx and to make it fastcall
    typedef
        void                                                            // return
        (__fastcall *TypeDef_ProcessPacket)                             // callconv + typedef
        (ProcessPacket_vars);                                           // args

    #define ProcPacket_origfunc                                         \
        ((TypeDef_ProcessPacket)Detour_ProcPacket.GetOriginalFunc())    \
            (ProcessPacket_varsnotype);                                 \
            Detour_ProcPacket.RestorePatch();

    void  __fastcall ProcPacket
        (ProcessPacket_vars)
#else
    #define ProcessPacket_vars uintptr_t* _this, uintptr_t* netpacket, bool a3
    #define ProcessPacket_varsnotype _this, netpacket, a3

    // no dummy needed
    // technically don't need the cdecl but i'd rather be explicit
    typedef
        void                                                            // ret
        (__cdecl * TypeDef_ProcessPacket)                               // callconv + typedef
        (ProcessPacket_vars);                                           // args

    #define ProcPacket_origfunc                                         \
        ((TypeDef_ProcessPacket)Detour_ProcPacket.GetOriginalFunc())    \
            (ProcessPacket_varsnotype);                                 \
            Detour_ProcPacket.RestorePatch();

    void __cdecl ProcPacket
        (ProcessPacket_vars)
#endif
{
    // this is in ms
    // this is also, technically,
    // "max processing time per tick", in ms
    float max_proc_time = net_chan_proctime_limit_ms.GetFloat();

    if (max_proc_time <= 0.1)
    {
        ProcPacket_origfunc;
        return;
    }

    // a3 should never be false, it seems...
    if (!_this || !netpacket || !a3)
    {
        if (spew_procpacket)
        {
            Warning("CNetChan::ProcessPacket -> this or netpacket or a3 = null\n");
        }
        return;
    }


    /*
    this is where we get the size of the packet

    in ProcessPacket...

        [ida pseudo]

        v29 = a2[14];
        *(double *)&v26 = Plat_FloatTime();
        v22 = this[4];
        v19 = *(double *)&qword_C976A0;
        v21 = this[3];
      -->v18<-- = a2[13]; == *(a2 + 13) !!!!
        v6 = (const char *)(*(int (__cdecl **)(int *))*this)(this);
        v7 = _mm_cvtpd_ps((__m128d)v26);
        ConMsg(
          "UDP <- %s: sz=%i seq=%i ack=%i rel=%i ch=%d, tm=%f rt=%f wire=%i\n", // unique string
                  v6,
                    -->v18<--,
                            v21 & 0x3F,
                                v22 & 0x3F,
                                        v32 & 1,
                                                (v32 & 0x20) != 0,
                                                        v19,
                                                    _mm_unpacklo_ps(v7, v7).m128_f32[0],
                                                                    v29);
    */
    int offs = 13;
    // somethow this is the same on both win and linux, for now
    int size = *(netpacket + offs);

    if (spew_procpacket)
    {
        Warning("packetsize = %i\n", size);
    }

    // maybe a nullcheck??
    if (size > toobig || size < toosmall)
    {
        if (spew_procpacket)
        {
            Warning("CNetChan::ProcessPacket:: PACKET WAS A WACKY SIZE = %i\n", size);
        }
        // DON'T call original function here.
        return;
    }


    INetChannel* pnetchan   = reinterpret_cast<INetChannel*>(_this);
    IClient* iclient        = static_cast<IClient*>( pnetchan->GetMsgHandler() );

    if (!iclient)
    {
        if (spew_procpacket)
        {
            Warning("No valid iclient...\n");
        }
        return;
    }

    int client = -1;
    client = iclient->GetPlayerSlot() + 1;

    if (client < 1)
    {
        if (spew_procpacket)
        {
            Warning("client is invalid??? idx = %i\n", client);
        }
        ProcPacket_origfunc;
        return;
    }

    CBasePlayer* bPlayer = UTIL_PlayerByIndex(client);
    if (!bPlayer)
    {
        if (spew_procpacket)
        {
            Warning("bplayer == null??\n");
        }

        ProcPacket_origfunc;
        return;
    }

    uint64_t start  = nanos();

    ProcPacket_origfunc;

    uint64_t end    = nanos();

    double proctime_ms =
        (end - start)   /* time in ns */
        / 1000.0        /* nano -> micro */ 
        / 1000.0        /* micro -> milli */;

    if (spew_procpacket)
    {
        Warning("time = %fms lasttick=%i thistick=%i\n", proctime_ms, bPlayer->m_iLastProcessPacketTick, bPlayer->m_iThisProcessPacketTick);
    }

    bPlayer->m_iLastProcessPacketTick = bPlayer->m_iThisProcessPacketTick;
    bPlayer->m_iThisProcessPacketTick = gpGlobals->tickcount;

    int delta = 0;
    // 1 or more ticks ahead
    if (bPlayer->m_iThisProcessPacketTick > bPlayer->m_iLastProcessPacketTick)
    {
        delta = bPlayer->m_iThisProcessPacketTick - bPlayer->m_iLastProcessPacketTick;
        // scale our processing time by the delta
        bPlayer->m_dflProcessPacketTime = (proctime_ms * (1.0/delta));

        if (spew_procpacket)
        {
            Warning("ahead. delta %i, proctime = %f\n", delta, bPlayer->m_dflProcessPacketTime);
        }
    }
    // same tick (monkaS)
    else if (bPlayer->m_iThisProcessPacketTick == bPlayer->m_iLastProcessPacketTick)
    {
        // scale our processing time by adding each "same tick" time to our running total
        bPlayer->m_dflProcessPacketTime += proctime_ms;
    
        if (spew_procpacket)
        {
            Warning("same. delta = %i, proctime = %f\n", 0, bPlayer->m_dflProcessPacketTime);
        }
    }
    // something has gone bad
    else
    {
        if (spew_procpacket)
        {
            Warning("negative tick delta? uh oh.\n");
        }
        return;
    }

    if (bPlayer->m_dflProcessPacketTime >= max_proc_time)
    {
        //if (spew_procpacket)
        //{
        Warning("Client %s exceeded processing time. (Theirs) %.2fms >= %.2fms (Max)\n", bPlayer->GetPlayerName(), bPlayer->m_dflProcessPacketTime, max_proc_time);
        //}

        // could maybe be abused if clients send a CHONKY packet and then immediately dc?
        if ( !bPlayer->IsDisconnecting() )
        {
            const char* kickcmd = UTIL_VarArgs( "kickid %i %s;", bPlayer->GetUserID(), "Exceeded processing time" );
            engine->ServerCommand(kickcmd);
            engine->ServerExecute();
        }
    }

    return;
}

void CNetChan__ProcessPacket_Init()
{
    size_t sizeof_ProcPacket = 0;

    // unique string: "corrupted packet"
    // that gets you ProcessPacketHeader, which only xrefs with ProcessPacket
    #ifdef _WIN32

        // Signature for sub_101C9DF0:
        // 55 8B EC 51 53 56 8B F1 57 8B 7D 08
        sizeof_ProcPacket = 12;
        const char* pattern_ProcPacket = "\x55\x8B\xEC\x51\x53\x56\x8B\xF1\x57\x8B\x7D\x08";

    #else

        // Signature for ProcessPacket_sub_4DF310:
        // 55 89 E5 57 31 FF 56 53 83 EC 7C C7 45 D8 00 00 00 00
        sizeof_ProcPacket = 18;
        const char* pattern_ProcPacket = "\x55\x89\xE5\x57\x31\xFF\x56\x53\x83\xEC\x7C\xC7\x45\xD8\x00\x00\x00\x00";

    #endif

    uintptr_t addr_ProcPacket = (uintptr_t)memy::FindPattern
    (
        engine_bin->addr,
        engine_bin->size,
        pattern_ProcPacket,
        sizeof_ProcPacket,
        0
    );

    Detour_ProcPacket.Init(addr_ProcPacket, (void*)ProcPacket);
}


// SteamID spoof prevention / CBaseServer::ConnectClient hardening
// Many many many thanks to Asherkin's Connect extension:
// https://github.com/asherkin/connect/blob/master/extension/extension.cpp
// ...which saved me several hours of painful and annoying reversing
// #define STEAMIDSPOOF_DEBUGGING yep

#include <netadr.h>

////////////////////////////////////////////////
// CBaseServer::RejectConnection Hook[ish]
// For rejecting clients in our
// CBaseServer::ConnectClient Detour
////////////////////////////////////////////////
uintptr_t addr_RejectConnection = NULL;
// CBaseServer::RejectConnection ( [const] netadr_t &[*]address, int iClientChallenge, [const] char *pchReason )
void CBaseServer__RejectConnection_Init()
{
    size_t sizeof_RejectConnection = 0;

#ifdef _WIN32
    /*

        Windows:

        Signature for sub_1015DFA0:
        55 8B EC 81 EC 04 05 00 00 56 6A FF
        \x55\x8B\xEC\x81\xEC\x04\x05\x00\x00\x56\x6A\xFF

        Got the original signature from
        https://github.com/asherkin/connect/blob/0c6274058e718d142189d331aae9b288b351360a/connect.games.txt#L71


        sub_10155B50+70   220 mov     eax, [edi]
        sub_10155B50+72   220 mov     ecx, edi
        sub_10155B50+74   220 call    dword ptr [eax+48h]
        sub_10155B50+77   220 push    offset aGameuiServerre_0 ; "#GameUI_ServerRejectBanned" (not unique)
        sub_10155B50+7C   224 push    dword ptr [esi+98h]
        sub_10155B50+82   228 mov     ecx, eax
        sub_10155B50+84   228 mov     edx, [eax]
        sub_10155B50+86   228 call    dword ptr [edx+0C0h]
        sub_10155B50+8C   228 push    eax
        sub_10155B50+8D   22C mov     ecx, offset dword_105EC6C8
        sub_10155B50+92   22C call    --->sub_1015DFA0<----------------------------------------------------
        sub_10155B50+97   220 mov     esi, [edi]
        sub_10155B50+99   220 mov     ecx, edi
        sub_10155B50+9B   220 call    dword ptr [esi+54h]
        sub_10155B50+9E   220 push    eax             ; ArgList
        sub_10155B50+9F   224 push    offset aSteamUseridSIs ; "STEAM UserID %s is banned" (unique)
        sub_10155B50+A4   228 call    sub_1017CF00
        sub_10155B50+A9   228 push    eax
        sub_10155B50+AA   22C push    edi
        sub_10155B50+AB   230 call    dword ptr [esi+34h]
        sub_10155B50+AE   230 add     esp, 10h
        sub_10155B50+B1   220 jmp     loc_10155CC6
    */
    sizeof_RejectConnection = 12;
    const char* pattern_RejectConnection = "\x55\x8B\xEC\x81\xEC\x04\x05\x00\x00\x56\x6A\xFF";

#else

    /*
        Linux:

        Signature for sub_370310:
        55 89 E5 53 8D 85 F4 FA FF FF
        \x55\x89\xE5\x53\x8D\x85\xF4\xFA\xFF\xFF

        sub_56ED90+65   25C mov     edx, [ebx+98h]
        sub_56ED90+6B   25C mov     eax, [ebx]
        sub_56ED90+6D   25C mov     [esp], ebx
        sub_56ED90+70   25C mov     [ebp+var_230], edx
        sub_56ED90+76   25C call    dword ptr [eax+1Ch]
        sub_56ED90+79   25C mov     ecx, [eax]
        sub_56ED90+7B   25C mov     [esp], eax
        sub_56ED90+7E   25C call    dword ptr [ecx+0C4h]
        sub_56ED90+84   25C mov     edx, [ebp+var_230]
        sub_56ED90+8A   25C mov     dword ptr [esp+0Ch], offset aGameuiServerre_9 ; "#GameUI_ServerRejectBanned" (not unique)
        sub_56ED90+92   25C mov     [esp+4], eax
        sub_56ED90+96   25C mov     dword ptr [esp], offset dword_CD7980
        sub_56ED90+9D   25C mov     [esp+8], edx
        sub_56ED90+A1   25C call    --->sub_370310<--------------------------------------------------------------------------
        sub_56ED90+A6   25C mov     eax, [ebx]
        sub_56ED90+A8   25C mov     edx, [eax+38h]
        sub_56ED90+AB   25C mov     [esp], ebx
        sub_56ED90+AE   25C mov     [ebp+var_22C], edx
        sub_56ED90+B4   25C call    dword ptr [eax+28h]
        sub_56ED90+B7   25C mov     dword ptr [esp], offset aSteamUseridSIs ; "STEAM UserID %s is banned" (unique)
        sub_56ED90+BE   25C mov     [esp+4], eax
        sub_56ED90+C2   25C call    sub_3F7BB0
        sub_56ED90+C7   25C mov     [esp], ebx
        sub_56ED90+CA   25C mov     [esp+4], eax
        sub_56ED90+CE   25C call    [ebp+var_22C]
    */
    sizeof_RejectConnection = 10;
    const char* pattern_RejectConnection = "\x55\x89\xE5\x53\x8D\x85\xF4\xFA\xFF\xFF";

#endif

    addr_RejectConnection = (uintptr_t)memy::FindPattern
    (
        engine_bin->addr,
        engine_bin->size,
        pattern_RejectConnection,
        sizeof_RejectConnection,
        0
    );
}

#ifdef _WIN32
    #define RejectConnection_vars uintptr_t* _this, uintptr_t* dummy, netadr_t* netadr, int clichallenge, const char* reason
    typedef
        void                                    // ret
        (__fastcall *TypeDef_RejectConnection)  // callconv + typedef
        (RejectConnection_vars);                // vars
#else
    #define RejectConnection_vars uintptr_t* _this, netadr_t* netadr, int clichallenge, const char* reason
    typedef
        void                                    // ret
        (__cdecl *TypeDef_RejectConnection)     // callconv + typedef
        (RejectConnection_vars);                // vars
#endif

void RejectConnection_origfunc(uintptr_t* _this, netadr_t* netadr, int clichallenge, const char* reason)
{
    #ifdef _WIN32
        ((TypeDef_RejectConnection)addr_RejectConnection)(_this, 0x0, netadr, clichallenge, reason);
    #else
        ((TypeDef_RejectConnection)addr_RejectConnection)(_this, netadr, clichallenge, reason);
    #endif
}


////////////////////////////////////////////////
// CBaseServer::ConnectClient Detour
// For SteamID spoofing prevention/hardening
////////////////////////////////////////////////
static DetourHook Detour_ConnectClient{};

typedef enum EAuthProtocol
{
    k_EAuthProtocolWONCertificate   = 1,
    k_EAuthProtocolHashedCDKey      = 2,
    k_EAuthProtocolSteam            = 3,
} EAuthProtocol;

#ifdef _WIN32
/*
    CBaseServer__ConnectClient, // func
    IClient *,                  // ret
    netadr_t &, address,        // netadr ref that i'm calling a pointer because it's easier
    int, nProtocol,
    int, iChallenge,
    int, iClientChallenge,
    int, nAuthProtocol,
    const char *, pchName,
    const char *, pchPassword,
    const char *, pCookie,
    int, cbCookie)
*/
    // it's a thiscall [member function]  we need a dummy edx and to make it fastcall
    #define ConnectClient_vars uintptr_t* _this, uintptr_t* dummy, netadr_t* netadr, int proto, int challenge, int clichallenge, int authproto, const char* clname, const char* clpasswd, const char* clcookie, int callbackcookie
    #define ConnectClient_varsnotype _this, dummy, netadr, proto, challenge, clichallenge, authproto, clname, clpasswd, clcookie, callbackcookie
    typedef
        uintptr_t*                                          // ret [iclient*]
        (__fastcall *TypeDef_ConnectClient)                 // callconv + typedef
        (ConnectClient_vars);                               // vars

    uintptr_t* ConnectClient_origfunc(ConnectClient_vars)
    {
        void* origfunc = Detour_ConnectClient.GetOriginalFunc();
        uintptr_t* iclient = ((TypeDef_ConnectClient)origfunc)(ConnectClient_varsnotype);
        Detour_ConnectClient.RestorePatch();
        return iclient;
    }

    uintptr_t* __fastcall ConnectClient(ConnectClient_vars)
#else
    // it's a __cdecl, no dummy needed
    #define ConnectClient_vars uintptr_t* _this, netadr_t* netadr, int proto, int challenge, int clichallenge, int authproto, char* clname, char* clpasswd, char* clcookie, int callbackcookie
    #define ConnectClient_varsnotype _this, netadr, proto, challenge, clichallenge, authproto, clname, clpasswd, clcookie, callbackcookie

    typedef
        uintptr_t*                          // ret [iclient*]
        (__cdecl *TypeDef_ConnectClient)    // callconv + typedef
        (ConnectClient_vars);               // vars

    uintptr_t* ConnectClient_origfunc(ConnectClient_vars)
    {
        void* origfunc = Detour_ConnectClient.GetOriginalFunc();
        uintptr_t* iclient = ((TypeDef_ConnectClient)origfunc)(ConnectClient_varsnotype);
        Detour_ConnectClient.RestorePatch();
        return iclient;
    }

    uintptr_t* __cdecl ConnectClient(ConnectClient_vars)
#endif
{
    #ifdef STEAMIDSPOOF_DEBUGGING
    Warning("CBaseServer::ConnectClient->       \n");
    Warning("_this                          = %p\n", _this);
    Warning("netadr                         = %p\n", netadr);
    Warning("proto                          = %i\n", proto);
    Warning("challenge                      = %i\n", challenge);
    Warning("clichallenge                   = %i\n", clichallenge);
    Warning("authproto                      = %i\n", authproto);
    Warning("clname                         = %s\n", clname);
    Warning("clpasswd                       = %s\n", clpasswd);
    Warning("clcookie                       = %s\n", clcookie);
    Warning("callbackcookie                 = %x\n", callbackcookie);
    #endif

    // stv clients use k_EAuthProtocolHashedCDKey...!
    // don't interfere with them, for now
    if (authproto != k_EAuthProtocolSteam)
    {
        uintptr_t* iclient = ConnectClient_origfunc(ConnectClient_varsnotype);
        return iclient;
    }

    if (!clichallenge || !challenge)
    {
        RejectConnection_origfunc(_this, netadr, clichallenge, "Invalid challenge(s).\n");
        return nullptr;
    }

    // this is almost certainly defined somewhere, i just need to figure out where...
    int curproto = 24;
    if (proto != curproto)
    {
        RejectConnection_origfunc(_this, netadr, clichallenge, "Invalid protocol.\n");
        return nullptr;
    }

    if (clcookie == nullptr || (size_t)callbackcookie < sizeof(uint64))
    {
        RejectConnection_origfunc(_this, netadr, clichallenge, "#GameUI_ServerRejectInvalidSteamCertLen");
        return nullptr;
    }

    uint64 ull_steamid  = *(uint64*)clcookie;

    #ifdef STEAMIDSPOOF_DEBUGGING
    Warning("ull_steamid %llu\n", ull_steamid);
    #endif

    CSteamID realsteamid = CSteamID(ull_steamid);
    bool isvalidsteamid = realsteamid.IsValid();

    #ifdef STEAMIDSPOOF_DEBUGGING
    Warning("isvalidsteamid = %i\n", isvalidsteamid);
    #endif

    if (!isvalidsteamid)
    {
        RejectConnection_origfunc(_this, netadr, clichallenge, "Invalid SteamID.\n");
        return nullptr;
    }

    /*
    void* pvTicket      = (void*)((intptr_t)clcookie + sizeof(uint64));
    int cbTicket        = callbackcookie - sizeof(uint64);

    Warning("pvticket = %p\n", pvTicket);
    Warning("cbTicket = %i\n", cbTicket);

    EBeginAuthSessionResult result = BeginAuthSession(pvTicket, cbTicket, realsteamid);
    if (result != k_EBeginAuthSessionResultOK)
    {
        RejectConnection_origfunc(_this, 0x0, netadr, clichallenge, "#GameUI_ServerRejectSteam");
        return NULL;
    }
    */

    uintptr_t* iclient = ConnectClient_origfunc(ConnectClient_varsnotype);
    return iclient;
}


void CBaseServer__ConnectClient_Init()
{
    size_t sizeof_ConnectClient = 0;

    // unique string: "CBaseServer::ConnectClient"
    #ifdef _WIN32

        // Signature for sub_1015BB80:
        // 55 8B EC 81 EC 04 05 00 00 56 68 ? ? ? ?
        sizeof_ConnectClient = 15;
        const char* pattern_ConnectClient = "\x55\x8B\xEC\x81\xEC\x04\x05\x00\x00\x56\x68\x2A\x2A\x2A\x2A";

    #else

        // Signature for sub_372660:
        // 55 89 E5 83 EC 48 8B 55 0C 89 5D F4 8B 45 14
        sizeof_ConnectClient = 15;
        const char* pattern_ConnectClient = "\x55\x89\xE5\x83\xEC\x48\x8B\x55\x0C\x89\x5D\xF4\x8B\x45\x14";

    #endif

    uintptr_t addr_ConnectClient = memy::FindPattern
    (
        engine_bin,
        pattern_ConnectClient,
        sizeof_ConnectClient,
        0
    );

    Detour_ConnectClient.Init(addr_ConnectClient, (void*)ConnectClient);
}


////////////////////////////////////////////////
// Init
////////////////////////////////////////////////
void CEngineDetours::PostInit()
{
    // ONLY run these on dedis!
    if (engine->IsDedicatedServer())
    {
        CNetChan__ProcessPacket_Init();
        CBaseServer__RejectConnection_Init();
        CBaseServer__ConnectClient_Init();
    }
}

// client detours
#elif defined(CLIENT_DLL)
#include <filesystem.h>
#include <netadr.h>
#ifdef BLACKLISTS
    #include <qol/blacklists.h>
#endif
#include <inetchannel.h>
#include <iclient.h>

#ifdef SENTRY
    #include <sentry_native/sdk_sentry.h>
#endif

#ifdef BLACKLISTS
const char* malicious_server_dc =
"Sorry, this server has been marked by developers as malicious";

const char* malicious_spew =
"!!\n"
"!! Sorry, the server you tried to connect to has been marked by developers as malicious.\n"
"!! If you believe this to be an error, please contact the developers at\n"
"!! %s\n"
"!!\n";
#endif

static DetourHook Detour_FullConnect        = {};
static DetourHook Detour_NetChanShutdown    = {};

#ifdef _WIN32

    #define FullConnect_vars uintptr_t* _this, uintptr_t* dummy, netadr_t& netadr
    #define FullConnect_varsnotype _this, dummy, netadr

    // it's a thiscall [member function] so we need a dummy edx and to make it fastcall
    typedef
        void                                        // return
        (__fastcall *TypeDef__FullConnect)          // callconv + typedef
        (FullConnect_vars);                         // args

    #define FullConnect_origfunc                                        \
        ((TypeDef__FullConnect)Detour_FullConnect.GetOriginalFunc())    \
            (FullConnect_varsnotype);                                   \
            Detour_FullConnect.RestorePatch();


    void    __fastcall CClientSomething__FullConnect
    (FullConnect_vars)
#else

    #define FullConnect_vars uintptr_t* _this, netadr_t& netadr
    #define FullConnect_varsnotype _this, netadr

    // it's a thiscall [member function] so we need a dummy edx and to make it fastcall
    typedef
        void                                        // return
        (__cdecl *TypeDef__FullConnect)             // callconv + typedef
        (FullConnect_vars);                         // args

    #define FullConnect_origfunc                                        \
        ((TypeDef__FullConnect)Detour_FullConnect.GetOriginalFunc())    \
            (FullConnect_varsnotype);                                   \
            Detour_FullConnect.RestorePatch();


    void    __cdecl CClientSomething__FullConnect
    (FullConnect_vars)
#endif
{
    FullConnect_origfunc

    if (!UTIL_CheckRealRemoteAddr(netadr))
    {
        return;
    }

    const char* ipaddr = netadr.ToString(true);
    if (!ipaddr || !ipaddr[0])
    {
        return;
    }

    // if this cmpserverblacklist returns true, the server is not malicious.
    // otherwise...
    if ( CBlacklists::CompareServerBlacklist(ipaddr) )
    {
#ifdef SENTRY
        sentry_value_t ctxinfo = sentry_value_new_object();
        sentry_value_set_by_key(ctxinfo, "malicious ip address", sentry_value_new_string(ipaddr));
        SentryEvent("info", __FUNCTION__, "Malicious IP connect attempt", ctxinfo);
#endif

        Color dullyello(255, 136, 78, 255);
        const int spewlen = strlen(malicious_spew);
        const int MAX_URL_SIZE = 128;
        char* finalspew = new char[spewlen + MAX_URL_SIZE + 1];
        V_snprintf(finalspew, (spewlen + MAX_URL_SIZE + 1), "%s%s", malicious_spew, BLACKLISTS_CONTACT_URL);
        ConColorMsg(dullyello, "%s", malicious_spew);
        delete [] finalspew;

        INetChannel* netchan = static_cast<INetChannel*>(engine->GetNetChannelInfo());
        if (netchan)
        {
            netchan->Shutdown(malicious_server_dc);
        }
        else
        {
            //SentryEvent("info", __FUNCTION__, "No netchan for malicious server dc?!", ctxinfo);

            engine->ExecuteClientCmd("disconnect\n");
        }

        // beep beep beep! yes, this has to be here or it will not work fsr
        engine->ExecuteClientCmd("play ui/vote_failure\n");
    }
}

void CClientSomething__FullConnect_Init()
{
    size_t sizeof_FullConnect = 0;

    // unique string: "Connected to %s\n"
    #ifdef _WIN32

        // Signature for sub_100D0450:
        // 55 8B EC 53 8B 5D 08 56 57 53 8B F9 E8 ? ? ? ? 8B 4F 10
        sizeof_FullConnect = 20;
        const char* pattern_FullConnect = "\x55\x8B\xEC\x53\x8B\x5D\x08\x56\x57\x53\x8B\xF9\xE8\x2A\x2A\x2A\x2A\x8B\x4F\x10";

    #else

        // Signature for sub_38A7C0:
        // 55 89 E5 83 EC 28 89 5D F4 8B 5D 0C 89 75 F8 8B 75 08 89 7D FC 89 5C 24 04 89 34 24 E8 ? ? ? ? 8B 46 10
        sizeof_FullConnect = 36;
        const char* pattern_FullConnect = "\x55\x89\xE5\x83\xEC\x28\x89\x5D\xF4\x8B\x5D\x0C\x89\x75\xF8\x8B\x75\x08\x89\x7D\xFC\x89\x5C\x24\x04\x89\x34\x24\xE8\x2A\x2A\x2A\x2A\x8B\x46\x10";

    #endif

    uintptr_t addr_FullConnect = memy::FindPattern
    (
        engine_bin,
        pattern_FullConnect,
        sizeof_FullConnect,
        0
    );

    Detour_FullConnect.Init(addr_FullConnect, (void*)CClientSomething__FullConnect);
}

#ifdef _WIN32
    #define NetChanShutdown_vars uintptr_t* _this, uintptr_t* dummy, const char* reason
    #define NetChanShutdown_varsnotype _this, dummy, reason

    // it's a thiscall [member function] so we need a dummy edx and to make it fastcall
    typedef
        void                                        // return
        (__fastcall *TypeDef__NetChanShutdown)      // callconv + typedef
        (NetChanShutdown_vars);                     // args

    #define NetchanShutdown_origfunc                                            \
        ((TypeDef__NetChanShutdown)Detour_NetChanShutdown.GetOriginalFunc())    \
            (NetChanShutdown_varsnotype);                                       \
            Detour_NetChanShutdown.RestorePatch();

    void    __fastcall CNetChan__Shutdown(NetChanShutdown_vars)
#else
    #define NetChanShutdown_vars uintptr_t* _this, const char* reason
    #define NetChanShutdown_varsnotype _this, reason

    typedef
        void                                        // return
        (__cdecl *TypeDef__NetChanShutdown)         // callconv + typedef
        (NetChanShutdown_vars);                     // args

    #define NetchanShutdown_origfunc                                            \
        ((TypeDef__NetChanShutdown)Detour_NetChanShutdown.GetOriginalFunc())    \
            (NetChanShutdown_varsnotype);                                       \
            Detour_NetChanShutdown.RestorePatch();

    void    __cdecl CNetChan__Shutdown(NetChanShutdown_vars)
#endif
{
    INetChannelInfo* netchan = engine->GetNetChannelInfo();
    if (netchan && netchan->IsLoopback())
    {
        NetchanShutdown_origfunc;
        // Warning("-> ignoring netchan shutdown = '%s'\n", reason);
        return;
    }
    NetchanShutdown_origfunc;
    // Warning("-> Netchan shutdown = '%s'\n", reason);

#ifdef FLUSH_DLS
    // no cvar, sorry
    engine->ClientCmd_Unrestricted("flush_map_overrides\n");

    if (cvar->FindVar("cl_flush_downloads_on_dc")->GetBool())
    {
        engine->ClientCmd_Unrestricted("flush_dls\n");
    }
    else if (cvar->FindVar("cl_flush_sprays_on_dc")->GetBool())
    {
        engine->ClientCmd_Unrestricted("flush_sprays\n");
    }
#endif

    // Servers can "brick" clients by fucking with their host_timescale. Don't let 'em
    cvar->FindVar("host_timescale")->SetValue(1);
}

void CNetChan__Shutdown_Init()
{
    size_t sizeof_NetChanShutDown = 0;
    // Unique string: "NetChannel removed.", which calls Shutdown immediately after
    #ifdef _WIN32
        // Signature for sub_101CCA10:
        // 55 8B EC 83 EC 10 56 8B F1 83 BE 8C 00 00 00 00
        sizeof_NetChanShutDown = 16;
        const char* pattern_NetChanShutDown = "\x55\x8B\xEC\x83\xEC\x10\x56\x8B\xF1\x83\xBE\x8C\x00\x00\x00\x00";
    #else
        // Signature for sub_4DFAF0:
        // 55 89 E5 57 56 53 83 EC 3C 8B 5D 08 8B 75 0C 8B 8B 8C 00 00 00
        sizeof_NetChanShutDown = 21;
        const char* pattern_NetChanShutDown = "\x55\x89\xE5\x57\x56\x53\x83\xEC\x3C\x8B\x5D\x08\x8B\x75\x0C\x8B\x8B\x8C\x00\x00\x00";
    #endif

    uintptr_t addr_NetChanShutdown = memy::FindPattern
    (
        engine_bin,
        pattern_NetChanShutDown,
        sizeof_NetChanShutDown,
        0
    );

    Detour_NetChanShutdown.Init(addr_NetChanShutdown, (void*)CNetChan__Shutdown);
}

void CEngineDetours::PostInit()
{
    CClientSomething__FullConnect_Init();
    CNetChan__Shutdown_Init();
}
#endif


#endif