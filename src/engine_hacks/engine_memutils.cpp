#include <cbase.h>


#include <engine_memutils.h>
#ifdef GAME_DLL
#include <iclient.h>
#include <inetchannel.h>
#include <inetchannelinfo.h>

// uniquish string demorestart in ExecStringCmd, that function + 3, see here
// https://github.com/sapphonie/StAC-tf2/blob/a834bb1c2b4a0431de9b89998504684e3fc7af5f/gamedata/stac.txt#L86-L130
/*
 dd offset _ZN11CBaseClient20ExecuteStringCommandEPKc ; CBaseClient::ExecuteStringCommand(char const*)
 dd offset _ZN11CBaseClient10SendNetMsgER11INetMessageb ; CBaseClient::SendNetMsg(INetMessage &,bool)
 dd offset _ZN11CBaseClient12ClientPrintfEPKcz ; CBaseClient::ClientPrintf(char const*,...)
 dd offset _ZNK11CBaseClient11IsConnectedEv ; CBaseClient::IsConnected(void)
 dd offset _ZNK11CBaseClient9IsSpawnedEv ; CBaseClient::IsSpawned(void)
 dd offset _ZNK11CBaseClient8IsActiveEv ; CBaseClient::IsActive(void)
*/

/*
	sub_1009B4E0
	sub_1009B4E0
	sub_1009B4E0
	sub_1009B4E0          ; BOOL __thiscall sub_1009B4E0(_DWORD *this)
	sub_1009B4E0          sub_1009B4E0 proc near
	sub_1009B4E0      000 xor     eax, eax        ; Logical Exclusive OR
	sub_1009B4E0+2    000 cmp     dword ptr [ecx+0C4h], 6 ; Compare Two Operands (you want the 0xC4 here)
	sub_1009B4E0+9    000 setz    al              ; Set Byte if Zero (ZF=1)
	sub_1009B4E0+C    000 retn                    ; Return Near from Procedure
	sub_1009B4E0+C        sub_1009B4E0 endp
	sub_1009B4E0+C
*/

int GetSignOnStateFromNetChanInfo(INetChannelInfo* info)
{
	if (!info)
	{
		return -1;
	}
	INetChannel* netchan = static_cast<INetChannel*>(info);
	IClient* iclient = static_cast<IClient*>(netchan->GetMsgHandler());
	if (!iclient)
	{
		return -1;
	}
	uintptr_t iclient_addr = (uintptr_t)iclient;

#ifdef _WIN32
	uintptr_t offs = 0xC4;
#else
	uintptr_t offs = 0xC8;
#endif

	uint8 signonstate_byte = *(reinterpret_cast<uint8*>(iclient_addr + offs));
	int signonstate = signonstate_byte;

	return signonstate;
}


int UTIL_GetSignonState(CBasePlayer* basePlayer)
{
	if (!basePlayer)
	{
		Warning("Null baseplayer in GetClientSignonstate\n");
		return -1;
	}
	INetChannelInfo* info = engine->GetPlayerNetInfo(basePlayer->entindex());
	return GetSignOnStateFromNetChanInfo(info);
}


int UTIL_GetSignonState(INetChannelInfo* info)
{
	return GetSignOnStateFromNetChanInfo(info);
}


bool UTIL_IsFullySignedOn(CBasePlayer* basePlayer)
{
	int signonstate = UTIL_GetSignonState(basePlayer);
	if (signonstate != 6 && signonstate != 7)
	{
		return false;
	}
	return true;
}

#endif

/*
	if ( !UTIL_IsFullySignedOn(pPlayer) )
	{
		return TICK_INTERVAL;
	}
*/