#pragma once

#ifdef GAME_DLL
int UTIL_GetSignonState(CBasePlayer* basePlayer);
int UTIL_GetSignonState(INetChannelInfo* info);
bool UTIL_IsFullySignedOn(CBasePlayer* basePlayer);
#endif
