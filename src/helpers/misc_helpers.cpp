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
    ipadr[0] = '\0';
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
