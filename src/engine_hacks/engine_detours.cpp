//
//
// Basically all of this by https://sappho.io ! <3

#include <cbase.h>

#ifdef _WIN32
    #pragma once
#endif

#ifdef ENGINE_DETOURS
#include <engine_hacks/engine_detours.h>

#ifdef _WIN32
    // it's a thiscall [member function] so we need a dummy edx and to make it fastcall
    #define dummyType uintptr_t*
    #define dummyVar  dummyRegister
    #define dummyComma ,
    #define mbrcallconv __fastcall
#else
    #define dummyType
    #define dummyVar
    #define dummyComma
    #define mbrcallconv __cdecl
#endif



/*
    TODO:

    MAKE_CALLBACK_CLASS_IMPL(__thiscall, __thiscall, etc)

    HOOK_CALLBACK etc
*/
CEngineDetours g_CEngineDetours;

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
#include <netadr.h>
#include <engine_memutils.h>


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

/*
* From VSES 2007
typedef struct netpacket_s
{
// 0
    netadr_t		    from;		// sender IP
// 12
    int				    source;		// received source 
// 16
    double			    received;	// received time
// 24
    unsigned char*      data;		// pointer to raw packet data
// 48
    bf_read			    message;	// easy bitbuf data access
// 52
    int				    size;		// size in bytes
    int				    wiresize;   // size in bytes before decompression
    bool			    stream;		// was send as stream
    struct netpacket_s* pNext;	// for internal use, should be NULL in public
} netpacket_t;
*/


sdkdetour* CNetChan__ProcessPacket = {};

#define CNetChan__ProcessPacket_vars             uintptr_t* _this, dummyType dummyVar dummyComma uintptr_t* netpacket, bool a3
#define CNetChan__ProcessPacket_varsnotype       _this, dummyVar dummyComma netpacket, a3
#define CNetChan__ProcessPacket_origfunc         PLH::FnCast(CNetChan__ProcessPacket->detourTrampoline, CNetChan__ProcessPacket_CB)(CNetChan__ProcessPacket_varsnotype);

void mbrcallconv CNetChan__ProcessPacket_CB(CNetChan__ProcessPacket_vars)
{
    // CNetChan__ProcessPacket_origfunc;
    // this is in ms
    // this is also, technically,
    // "max processing time per tick", in ms
    float max_proc_time = net_chan_proctime_limit_ms.GetFloat();

    if (max_proc_time <= 0.1)
    {
        CNetChan__ProcessPacket_origfunc;
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
    /*
    byte* rawdata = new byte[size]{};
    V_memcpy(rawdata, (void*)(*(netpacket + 6)), size);
    Warning("-%p\n", rawdata);
    delete[] rawdata;
    */
    // maybe a nullcheck??
    if (size <= 0 && size > toobig || size < toosmall)
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

    /*
    if (pnetchan->HasPendingReliableData())
    {
        bf_read& r = (bf_read&)(*(netpacket + 12));
        Warning("r = %p\n", r);
        // const char* dbg = r.GetDebugName();
        const char* dbg2 = (const char*)r.GetBasePointer();
        if (dbg2 && dbg2[0])
        {
            Warning("dbg = %s\n", dbg2);
        }

        bf_read read = *(netpacket + 12);
        Warning("%p\n", read);
        const char* dbg = read->GetDebugName();
        if ( dbg && dbg[0] )
        {
            Warning("dbg \n", dbg);
        }
         read->
         delete[] rawdata;
        int bufsize = pnetchan->GetBufferSize();
        if (bufsize != 64)
        {
            Warning("bsize = %i\n", bufsize);
        }
    }
    */

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
        // CNetChan__ProcessPacket_origfunc;
        return;
    }

    CBasePlayer* bPlayer = UTIL_PlayerByIndex(client);
    if (!bPlayer)
    {
        if (spew_procpacket)
        {
            Warning("bplayer == null??\n");
        }

        CNetChan__ProcessPacket_origfunc;
        return;
    }

    uint64_t start  = nanos();

    CNetChan__ProcessPacket_origfunc;

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
        if
        (
            bPlayer
            && UTIL_IsFullySignedOn(bPlayer)
            && bPlayer->IsConnected()
            && !bPlayer->IsDisconnecting()
        )
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
    CNetChan__ProcessPacket = new sdkdetour{};

    // unique string: "corrupted packet"
    // that gets you ProcessPacketHeader, which only xrefs with ProcessPacket
    #ifdef _WIN32

        // Signature for sub_101C9DF0:
        // 55 8B EC 51 53 56 8B F1 57 8B 7D 08
        CNetChan__ProcessPacket->patternSize    = 12;
        CNetChan__ProcessPacket->pattern        = "\x55\x8B\xEC\x51\x53\x56\x8B\xF1\x57\x8B\x7D\x08";

    #else

        // Signature for ProcessPacket_sub_4DF310:
        // 55 89 E5 57 31 FF 56 53 83 EC 7C C7 45 D8 00 00 00 00
        CNetChan__ProcessPacket->patternSize    = 18;
        CNetChan__ProcessPacket->pattern        = "\x55\x89\xE5\x57\x31\xFF\x56\x53\x83\xEC\x7C\xC7\x45\xD8\x00\x00\x00\x00";

    #endif

    populateAndInitDetour(CNetChan__ProcessPacket, (void*)CNetChan__ProcessPacket_CB);
}


// SteamID spoof prevention / CBaseServer::ConnectClient hardening
// Many many many thanks to Asherkin's Connect extension:
// https://github.com/asherkin/connect/blob/master/extension/extension.cpp
// ...which saved me several hours of painful and annoying reversing
// #define STEAMIDSPOOF_DEBUGGING yep


////////////////////////////////////////////////
// CBaseServer::RejectConnection Hook[ish]
// For rejecting clients in our
// CBaseServer::ConnectClient Detour
////////////////////////////////////////////////

sdkdetour* CBaseServer__RejectConnection = {};

#define CBaseServer__RejectConnection_vars             uintptr_t* _this, dummyType dummyVar dummyComma netadr_t* netadr, int clichallenge, const char* reason
#define CBaseServer__RejectConnection_varsnotype       _this, dummyVar dummyComma netadr, clichallenge, reason
#define CBaseServer__RejectConnection_origfunc         PLH::FnCast(CBaseServer__RejectConnection->detourTrampoline, CBaseServer__RejectConnection_CB)(CBaseServer__RejectConnection_varsnotype);

void mbrcallconv CBaseServer__RejectConnection_CB(CBaseServer__RejectConnection_vars)
{
    return CBaseServer__RejectConnection_origfunc;
}

// CBaseServer::RejectConnection ( [const] netadr_t &[*]address, int iClientChallenge, [const] char *pchReason )
void CBaseServer__RejectConnection_Init()
{
    CBaseServer__RejectConnection = new sdkdetour{};
#ifdef _WIN32
    /*

        Windows:

        Signature for sub_1015DFA0:
        55 8B EC 81 EC 04 05 00 00 56 6A FF
        \x55\x8B\xEC\x81\xEC\x04\x05\x00\x00\x56\x6A\xFF

        Got the original signature from
        https://github.com/asherkin/connect/blob/0c6274058e718d142189d331aae9b288b351360a/connect.games.txt#L71


        ...
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
        ...
    */
    CBaseServer__RejectConnection->patternSize  = 12;
    CBaseServer__RejectConnection->pattern      = "\x55\x8B\xEC\x81\xEC\x04\x05\x00\x00\x56\x6A\xFF";

#else

    /*
        Linux:

        Signature for sub_370310:
        55 89 E5 53 8D 85 F4 FA FF FF
        \x55\x89\xE5\x53\x8D\x85\xF4\xFA\xFF\xFF

        ...
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
        ...
    */
    CBaseServer__RejectConnection->patternSize  = 10;
    CBaseServer__RejectConnection->pattern      = "\x55\x89\xE5\x53\x8D\x85\xF4\xFA\xFF\xFF";

#endif
    populateAndInitDetour(CBaseServer__RejectConnection, (void*)CBaseServer__RejectConnection_CB);
}




////////////////////////////////////////////////
// CBaseServer::ConnectClient Detour
// For SteamID spoofing prevention/hardening
////////////////////////////////////////////////
typedef enum EAuthProtocol
{
    k_EAuthProtocolWONCertificate   = 1,
    k_EAuthProtocolHashedCDKey      = 2,
    k_EAuthProtocolSteam            = 3,
} EAuthProtocol;


sdkdetour* CBaseServer__ConnectClient = {};
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

#define CBaseServer__ConnectClient_vars             uintptr_t* _this, dummyType dummyVar dummyComma netadr_t* netadr, int proto, int challenge, int clichallenge, int authproto, const char* clname, const char* clpasswd, const char* clcookie, int callbackcookie
#define CBaseServer__ConnectClient_varsnotype       _this, dummyVar dummyComma netadr, proto, challenge, clichallenge, authproto, clname, clpasswd, clcookie, callbackcookie
#define CBaseServer__ConnectClient_origfunc         PLH::FnCast(CBaseServer__ConnectClient->detourTrampoline, CBaseServer__ConnectClient_CB)(CBaseServer__ConnectClient_varsnotype);
#include <steam/isteamgameserver.h>
#include <steam/isteamgameserverstats.h>
uintptr_t* mbrcallconv CBaseServer__ConnectClient_CB(CBaseServer__ConnectClient_vars)
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
        uintptr_t* iclient = CBaseServer__ConnectClient_origfunc;
        return iclient;
    }

    if (!clichallenge || !challenge)
    {
        const char* reason = "Invalid challenge(s).\n";
        CBaseServer__RejectConnection_origfunc(_this, netadr, clichallenge, reason);
        return nullptr;
    }

    // this is almost certainly defined somewhere, i just need to figure out where...
    int curproto = 24;
    if (proto != curproto)
    {
        const char* reason = "Invalid protocol.\n";
        CBaseServer__RejectConnection_origfunc(_this, netadr, clichallenge, reason);
        return nullptr;
    }

    if (clcookie == nullptr || (size_t)callbackcookie < sizeof(uint64))
    {
        const char* reason = "#GameUI_ServerRejectInvalidSteamCertLen";
        CBaseServer__RejectConnection_origfunc(_this, netadr, clichallenge, reason);
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
        const char* reason = "Invalid SteamID.\n";
        CBaseServer__RejectConnection_origfunc(_this, netadr, clichallenge, reason);
        return nullptr;
    }

    // void* pvTicket      = (void*)((intptr_t)clcookie + sizeof(uint64));
    // int cbTicket        = callbackcookie - sizeof(uint64);

    // Warning("pvticket = %p\n", pvTicket);
    // Warning("cbTicket = %i\n", cbTicket);
    /*
    Aggrevating nonsense with anonymous servers not being able to call steamgameserverapi funcs? 
    will eventually fuck around with it in steam helpers but right now i dont care
    
    if (g_pSteamHelpers->pSteamGameServer)
    {
        EBeginAuthSessionResult result = g_pSteamHelpers->pSteamGameServer->BeginAuthSession(pvTicket, cbTicket, realsteamid);
        if (result != k_EBeginAuthSessionResultOK)
        {
            const char* reason = "#GameUI_ServerRejectSteam";
            CBaseServer__RejectConnection_origfunc(_this, netadr, clichallenge, reason);
            return NULL;
        }
    }
    */
    uintptr_t* iclient = CBaseServer__ConnectClient_origfunc;
    return iclient;
}


void CBaseServer__ConnectClient_Init()
{
    CBaseServer__ConnectClient = new sdkdetour{};
    // unique string: "CBaseServer::ConnectClient"
    #ifdef _WIN32
        // Signature for sub_1015BB80:
        // 55 8B EC 81 EC 04 05 00 00 56 68 ? ? ? ?
        CBaseServer__ConnectClient->patternSize  = 15;
        CBaseServer__ConnectClient->pattern      = "\x55\x8B\xEC\x81\xEC\x04\x05\x00\x00\x56\x68\x2A\x2A\x2A\x2A";
    #else

        // Signature for sub_372660:
        // 55 89 E5 83 EC 48 8B 55 0C 89 5D F4 8B 45 14
        CBaseServer__ConnectClient->patternSize  = 15;
        CBaseServer__ConnectClient->pattern      = "\x55\x89\xE5\x83\xEC\x48\x8B\x55\x0C\x89\x5D\xF4\x8B\x45\x14";
    #endif

    populateAndInitDetour(CBaseServer__ConnectClient, (void*)CBaseServer__ConnectClient_CB);

}




/*
Signature for _ZN17CGameEventManager13RegisterEventEP9KeyValues:
55 89 E5 57 56 53 83 EC 2C 8B 5D 0C 8B 75 08 85 DB
\x55\x89\xE5\x57\x56\x53\x83\xEC\x2C\x8B\x5D\x0C\x8B\x75\x08\x85\xDB
*/





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

#ifdef SDKSENTRY
    #include <sdksentry/sdksentry.h>
#endif



#ifdef BLACKLISTS

const char* malicious_server_dc =
"Sorry, this server has been marked by developers as malicious";


sdkdetour* CClientState__FullConnect{};

#define CClientState__FullConnect_vars            uintptr_t* _this, dummyType dummyVar dummyComma netadr_t& netadr
#define CClientState__FullConnect_varsnotype      _this, dummyVar dummyComma netadr
#define CClientState__FullConnect_origfunc        PLH::FnCast(CClientState__FullConnect->detourTrampoline, CClientState__FullConnect_CB)(CClientState__FullConnect_varsnotype);

void mbrcallconv CClientState__FullConnect_CB(CClientState__FullConnect_vars)
{
    CClientState__FullConnect_origfunc

    if (!UTIL_CheckRealRemoteAddr(netadr))
    {
        return;
    }

    const char* ipaddr = netadr.ToString(true);

    if ( !ipaddr || !ipaddr[0] )
    {
        return;
    }
    // if this cmpserverblacklist returns true, the server is not malicious.
    // otherwise...
    //bool CBlacklists::CompareServerBlacklist(const char* ipaddr)

    if ( !CompareServerBlacklist(ipaddr) )
    {
#ifdef SDKSENTRY
        sentry_value_t ctxinfo = sentry_value_new_object();
        sentry_value_set_by_key(ctxinfo, "malicious ip address", sentry_value_new_string(ipaddr));
        SentryEvent("info", __FUNCTION__, "Malicious IP connect attempt", ctxinfo);
#endif


        Color dullyello(255, 136, 78, 255);
        static std::stringstream spew;
        if (spew.str().empty())
        {
            spew << "!!\n";
            spew << "!! Sorry, the server you tried to connect to has been marked by developers as malicious.\n";
            spew << "!! If you believe this to be an error, please contact the developers at\n";
            spew << "!! " << VPC_QUOTE_STRINGIFY(BLACKLISTS_CONTACT_URL) << "\n";
            spew << "!!\n";
        }

        ConColorMsg(dullyello, "%s", spew.str().c_str());

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

void CClientState__FullConnect_Init()
{
    CClientState__FullConnect = new sdkdetour{};

    // unique string: "Connected to %s\n"
    #ifdef _WIN32
        // Signature for sub_100D0450:
        // 55 8B EC 53 8B 5D 08 56 57 53 8B F9 E8 ? ? ? ? 8B 4F 10
        CClientState__FullConnect->patternSize   = 20;
        CClientState__FullConnect->pattern       = "\x55\x8B\xEC\x53\x8B\x5D\x08\x56\x57\x53\x8B\xF9\xE8\x2A\x2A\x2A\x2A\x8B\x4F\x10";
    #else

        // Signature for sub_38A7C0:
        // 55 89 E5 83 EC 28 89 5D F4 8B 5D 0C 89 75 F8 8B 75 08 89 7D FC 89 5C 24 04 89 34 24 E8 ? ? ? ? 8B 46 10
        CClientState__FullConnect->patternSize   = 36;
        CClientState__FullConnect->pattern       = "\x55\x89\xE5\x83\xEC\x28\x89\x5D\xF4\x8B\x5D\x0C\x89\x75\xF8\x8B\x75\x08\x89\x7D\xFC\x89\x5C\x24\x04\x89\x34\x24\xE8\x2A\x2A\x2A\x2A\x8B\x46\x10";
    #endif

    populateAndInitDetour(CClientState__FullConnect, (void*)CClientState__FullConnect_CB);
}
#endif







sdkdetour* CNetChan__Shutdown{};
#define CNetChan__Shutdown_vars        uintptr_t* _this, dummyType dummyVar dummyComma const char* reason
#define CNetChan__Shutdown_novars      _this, dummyVar dummyComma reason
#define CNetChan__Shutdown_origfunc    PLH::FnCast(CNetChan__Shutdown->detourTrampoline, CNetChan__Shutdown_CB)(CNetChan__Shutdown_novars);
void mbrcallconv CNetChan__Shutdown_CB(CNetChan__Shutdown_vars)
{
    // Pretty sure this calls twice but I do not care enough to fix it right now
    INetChannelInfo* netchan = engine->GetNetChannelInfo();
    if (netchan && netchan->IsLoopback())
    {
        CNetChan__Shutdown_origfunc;
        //Warning("-> ignoring netchan shutdown = '%s'\n", reason);
        return;
    }
    CNetChan__Shutdown_origfunc;
    //Warning("-> Netchan shutdown = '%s'\n", reason);

#ifdef FLUSH_DLS
    // no cvar, sorry
    engine->ClientCmd_Unrestricted("flush_map_overrides\n");

    if (cvar->FindVar("cl_auto_flush_downloads")->GetBool())
    {
        engine->ClientCmd_Unrestricted("flush_dls\n");
    }
    else if (cvar->FindVar("cl_auto_flush_sprays")->GetBool())
    {
        engine->ClientCmd_Unrestricted("flush_sprays\n");
    }
#endif

    // Servers can "brick" clients by fucking with their host_timescale. Don't let 'em
    cvar->FindVar("host_timescale")->SetValue(1);
}

void CNetChan__Shutdown_Init()
{
    CNetChan__Shutdown = new sdkdetour{};

    // Unique string: "NetChannel removed.", which calls Shutdown immediately after
    #ifdef _WIN32
        // Signature for sub_101CCA10:
        // 55 8B EC 83 EC 10 56 8B F1 83 BE 8C 00 00 00 00
        CNetChan__Shutdown->patternSize = 16;
        CNetChan__Shutdown->pattern     = "\x55\x8B\xEC\x83\xEC\x10\x56\x8B\xF1\x83\xBE\x8C\x00\x00\x00\x00";
    #else
        // Signature for sub_4DFAF0:
        // 55 89 E5 57 56 53 83 EC 3C 8B 5D 08 8B 75 0C 8B 8B 8C 00 00 00
        CNetChan__Shutdown->patternSize = 21;
        CNetChan__Shutdown->pattern     = "\x55\x89\xE5\x57\x56\x53\x83\xEC\x3C\x8B\x5D\x08\x8B\x75\x0C\x8B\x8B\x8C\x00\x00\x00";
    #endif

    populateAndInitDetour(CNetChan__Shutdown, (void*)CNetChan__Shutdown_CB);
}







#ifdef _WIN32
#include <Windows.h>
int win32_HARDENING() {
    /*
        DWORD ProhibitDynamicCode : 1;
        DWORD AllowThreadOptOut : 1;
        DWORD AllowRemoteDowngrade : 1;
        DWORD AuditProhibitDynamicCode : 1;
        DWORD ReservedFlags : 28;
    */
    PROCESS_MITIGATION_DYNAMIC_CODE_POLICY dynCode;
    dynCode.ProhibitDynamicCode             = 1;
    dynCode.AllowThreadOptOut               = 0;
    dynCode.AllowRemoteDowngrade            = 0;
    dynCode.AuditProhibitDynamicCode        = 0;
    dynCode.ReservedFlags                   = 0;

    if (!SetProcessMitigationPolicy(ProcessDynamicCodePolicy, &dynCode, sizeof(dynCode)))
    {
    }

    PROCESS_MITIGATION_CONTROL_FLOW_GUARD_POLICY cfGuard;
    cfGuard.EnableControlFlowGuard      = 1;
    cfGuard.EnableExportSuppression     = 0;
    cfGuard.StrictMode                  = 1;
    cfGuard.ReservedFlags               = 0;

    if (!SetProcessMitigationPolicy(ProcessControlFlowGuardPolicy, &cfGuard, sizeof(cfGuard)))
    {
    }

    PROCESS_MITIGATION_EXTENSION_POINT_DISABLE_POLICY exPtDisable;
    exPtDisable.DisableExtensionPoints  = 1;
    exPtDisable.ReservedFlags           = 0;

    if (!SetProcessMitigationPolicy(ProcessExtensionPointDisablePolicy, &exPtDisable, sizeof(exPtDisable)))
    {
    }

    PROCESS_MITIGATION_ASLR_POLICY aslr;
    memset(&aslr, 0, sizeof(aslr));
    aslr.EnableBottomUpRandomization     = 1;
    aslr.EnableForceRelocateImages       = 1;
    aslr.EnableHighEntropy               = 1;
    aslr.DisallowStrippedImages          = 1;
    aslr.ReservedFlags                   = 0;

    if (!SetProcessMitigationPolicy(ProcessASLRPolicy, &aslr, sizeof(aslr)))
    {
        Warning("-> Failed ASLR");
    }

    if (!SetProcessDEPPolicy(PROCESS_DEP_ENABLE | PROCESS_DEP_DISABLE_ATL_THUNK_EMULATION))
    {
    }

    return 1;
}
#endif

void CEngineDetours::PostInit()
{
    CClientState__FullConnect_Init();
    CNetChan__Shutdown_Init();
#ifdef _WIN32
    win32_HARDENING();
#endif
}
#endif // client

// dtors
void CEngineDetours::Shutdown()
{
#ifdef CLIENT_DLL
    delete CClientState__FullConnect;
    delete CNetChan__Shutdown;
#else
    if (engine->IsDedicatedServer())
    {
        delete CNetChan__ProcessPacket;
        delete CBaseServer__RejectConnection;
        delete CBaseServer__ConnectClient;
    }
#endif
}
#endif
