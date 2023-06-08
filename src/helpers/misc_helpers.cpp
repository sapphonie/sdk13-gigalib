#include <cbase.h>


#include <helpers/misc_helpers.h>
ConVar _sdkpath("_sdkpath", "", FCVAR_HIDDEN);

#ifdef CLIENT_DLL
ConVar _modpath("_modpath", "", FCVAR_HIDDEN);

bool UTIL_CheckRealRemoteAddr(netadr_t netadr)
{
    if (netadr.IsValid() && !netadr.IsLocalhost() && !netadr.IsLoopback() && !netadr.IsReservedAdr())
    {
        return true;
    }
    return false;
}

bool UTIL_GetRealRemoteAddr(char* ipadr)
{
	// memset(ipadr, 0x0, sizeof(ipadr));
    INetChannel* netchan = static_cast<INetChannel*>(engine->GetNetChannelInfo());
    if (netchan)
    {
        netadr_t remaddr = netchan->GetRemoteAddress();
        if (UTIL_CheckRealRemoteAddr(remaddr))
        {
            // ToString is always a char[64];
            V_strncpy(ipadr, remaddr.ToString(), 64);
            return true;
        }
    }
	ipadr[0] = 0x0;
    return false;
}

#endif

// by default, returns if this CBasePlayer ptr is a real client or not. works on client and server
// DON'T PASS THIS A NULLPTR - there's no obvious answer to "is NULL a fake client or not?"
bool UTIL_IsFakePlayer(CBasePlayer* inplayer)
{
	if (!inplayer)
	{
		Error("UTIL_IsFakePlayer -> NULL CBasePlayer! Grab a programmer and tell them to fix their code!\n");
		return true;
	}

	player_info_t pinfo = {};
	if (!engine->GetPlayerInfo(inplayer->entindex(), &pinfo))
	{
		Warning("UTIL_IsFakePlayer -> COULDN'T GET PLAYER INFO FOR CBasePlayer = %p!\n", inplayer);
		// todo: sentry error
		return true;
	}

	if (pinfo.fakeplayer)
	{
		return true;
	}

	return false;
}

bool UTIL_IsVTFValid(const char* fileloc)
{
	int len = 0;
	byte* bytes = UTIL_LoadFileForMe(fileloc, &len);

	if (!len)
	{
		//AssertMsg(NULL, "Downloaded file is NULL!\n");
		return true;
	}

	// sanity check - smallest i have seen is 96 bytes
	if (len <= 64)
	{
		AssertMsg(NULL, "Downloaded file is too small!\n");
		return false;
	}

	// Get our header
	VTFFileHeader_t* vtfheader = (VTFFileHeader_t*)calloc(1, sizeof(VTFFileHeader_t));
	// will never happen
	if (!vtfheader)
	{
		return true;
	}
	memcpy(vtfheader, bytes, sizeof(VTFFileHeader_t));

	// Duh
	if (!vtfheader || !vtfheader->headerSize)
	{
		AssertMsg(NULL, "[VTF] Invalid header/headerSize");
		free(vtfheader);
		return false;
	}

	// Magic
	if (memcmp(vtfheader->fileTypeString, "VTF\0", 4))
	{
		AssertMsg(NULL, "[VTF] Invalid header magic");
		free(vtfheader);
		return false;
	}

	// 7.5 or lower
	if (vtfheader->version[0] > 7 || vtfheader->version[1] > 5)
	{
		AssertMsg(NULL, "[VTF] Invalid vtf version");
		free(vtfheader);
		return false;
	}

	// basic size checks
	if
		(
			vtfheader->width < 0
			|| vtfheader->width  > 8192
			|| vtfheader->height < 0
			|| vtfheader->height > 8192
			)
	{
		AssertMsg(NULL, "[VTF] Invalid vtf dimensions");
		free(vtfheader);
		return false;
	}

	// basic flag checks
	if
		(
			vtfheader->flags &
			(
				TEXTUREFLAGS_RENDERTARGET
				| TEXTUREFLAGS_DEPTHRENDERTARGET
				| TEXTUREFLAGS_NODEPTHBUFFER
				| TEXTUREFLAGS_UNUSED_00400000
				| TEXTUREFLAGS_UNUSED_01000000
				| TEXTUREFLAGS_UNUSED_10000000
				| TEXTUREFLAGS_UNUSED_40000000
				| TEXTUREFLAGS_UNUSED_80000000
				)
			)
	{
		AssertMsg1(NULL, "[VTF] Invalid vtf flags = %x", vtfheader->flags);
		free(vtfheader);
		return false;
	}

	free(vtfheader);
	return true;
}

void UTIL_AddrToString(void* inAddr, char outAddrStr[11])
{
	// sizeof doesn't work for params
	memset(outAddrStr, 0x0, 11);
	if (!inAddr)
	{
		V_snprintf(outAddrStr, 11, "(nil)");
	}
	else
	{
		V_snprintf(outAddrStr, 11, "%p", inAddr);
	}

	return;
}

std::vector<std::string> UTIL_SplitSTDString(const std::string& i_str, const std::string& i_delim)
{
	std::vector<std::string> result;
	size_t startIndex = 0;

	for (size_t found = i_str.find(i_delim); found != std::string::npos; found = i_str.find(i_delim, startIndex))
	{
		result.emplace_back(i_str.begin() + startIndex, i_str.begin() + found);
		startIndex = found + i_delim.size();
	}
	if (startIndex != i_str.size())
		result.emplace_back(i_str.begin() + startIndex, i_str.end());
	return result;
}


// https://stackoverflow.com/a/49066369
#include <valve_minmax_off.h>
#include <chrono>
#include <valve_minmax_on.h>


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

#ifdef CLIENT_DLL
void UTIL_GetMap(char mapname[128])
{
	// sizeof doesn't work for params
	memset(mapname, 0x0, 128);
	V_FileBase(engine->GetLevelName(), mapname, 128);
	V_strlower(mapname);
}
#endif