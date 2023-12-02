#include <cbase.h>


#include <helpers/misc_helpers.h>
#ifdef GAME_DLL
ConVar _sdkpath("_sdkpath_srv", "", FCVAR_HIDDEN | FCVAR_GAMEDLL);
#else
ConVar _sdkpath("_sdkpath_cli", "", FCVAR_HIDDEN | FCVAR_CLIENTDLL);
#endif

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

bool UTIL_IsVTFValid(std::string& fileloc)
{
    int len = 0;
    auto bytes = UTIL_SmartLoadFileForMe(fileloc, &len);
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
    auto vtfheader = std::make_unique<VTFFileHeader_t>();
    // will never happen
    if (!vtfheader)
    {
        return true;
    }
    memcpy(vtfheader.get(), bytes.get(), sizeof(VTFFileHeader_t));

    // Duh
    if (!vtfheader->headerSize)
    {
        AssertMsg(NULL, "[VTF] Invalid header/headerSize");
        return false;
    }

    // Magic
    if (memcmp(vtfheader->fileTypeString, "VTF\0", 4))
    {
        AssertMsg(NULL, "[VTF] Invalid header magic");
        return false;
    }

    // 7.5 or lower
    if (vtfheader->version[0] > 7 || vtfheader->version[1] > 5)
    {
        AssertMsg(NULL, "[VTF] Invalid vtf version");
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
        return false;
    }

    return true;
}

#include <filesystem.h>
#include <filesystem_helpers.h>
std::unique_ptr<byte> UTIL_SmartLoadFileForMe(std::string& filename, int* pLength)
{
    FileHandle_t file;
    file = filesystem->Open(filename.c_str(), "rb", "GAME");
    if (FILESYSTEM_INVALID_HANDLE == file)
    {
        if (pLength) *pLength = 0;
        return NULL;
    }

    int size = filesystem->Size(file);
    std::unique_ptr<byte> buffer(new byte[size + 1]);
    if (!buffer)
    {
        Warning( "UTIL_SmartLoadFileForMe:  Couldn't allocate buffer of size %i for file %s\n", size + 1, filename.c_str() );
        filesystem->Close(file);
        return NULL;
    }
    filesystem->Read(buffer.get(), size, file);
    filesystem->Close(file);

    // Ensure null terminator
    buffer.get()[size] = 0;

    if (pLength)
    {
        *pLength = size;
    }

    return buffer;
}

std::string UTIL_AddrToString(void* inAddr)
{
    std::string outStr;
    if (!inAddr)
	{
        outStr = "(nil)";
	}
	else
	{
        outStr = fmt::format( FMT_STRING("{}"), fmt::ptr(inAddr));
	}

    // this isn't dangling, this is an object and C++ returns by value
    // actually technically it's moved to this return but whatever
    return outStr;
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

std::string UTIL_StripCharsFromSTDString(std::string i_str, char char_to_strip)
{ 
	i_str.erase(std::remove(i_str.begin(), i_str.end(), char_to_strip), i_str.end());
	std::string o_str = i_str;
	return o_str;

}

//https://en.cppreference.com/w/cpp/string/basic_string/replace
std::string& UTIL_ReplaceAll(std::string& context, std::string const& from, std::string const& to)
{
	std::size_t lookHere = 0;
	std::size_t foundHere;
	while ((foundHere = context.find(from, lookHere)) != std::string::npos)
	{
		context.replace(foundHere, from.size(), to);
		lookHere = foundHere + to.size();
	}
	return context;
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


#include <Windows.h>
#include <engine_hacks/engine_detours.h>

bool checkWineInternal()
{
#ifdef _WIN32
    static const char* (__cdecl * pwine_get_version)(void);
    HMODULE hntdll = GetModuleHandle("ntdll.dll");
    if (!hntdll)
    {
        Error("Not running on NT.");
        return false;
    }
    FARPROC fp = GetProcAddress(hntdll, "wine_get_version");

    pwine_get_version = PLH::FnCast(fp, pwine_get_version);
    if (pwine_get_version)
    {
        Warning("Running on Wine... %s\n", pwine_get_version());
        return true;
    }
    else
    {
        Warning("did not detect Wine.\n");
        return false;
    }
#else
    return false;
#endif
}

// ret's true if we're running under wine/proton
bool checkWine()
{
    static bool iswine = checkWineInternal();
    return iswine;
}

// Hackily grabbed from other places in the sdk since this is for some reason undefined in places
const char* HACK_COM_GetModDirectory()
{
    static char modDir[MAX_PATH];
    if (Q_strlen(modDir) == 0)
    {
        const char* gamedir = CommandLine()->ParmValue("-game", CommandLine()->ParmValue("-defaultgamedir", "hl2"));
        Q_strncpy(modDir, gamedir, sizeof(modDir));
        if (strchr(modDir, '/') || strchr(modDir, '\\'))
        {
            Q_StripLastDir(modDir, sizeof(modDir));
            int dirlen = Q_strlen(modDir);
            Q_strncpy(modDir, gamedir + dirlen, sizeof(modDir) - dirlen);
        }
    }

    return modDir;
}

#endif
