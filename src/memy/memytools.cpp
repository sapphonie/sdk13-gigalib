#include <cbase.h>

#include <memy/memytools.h>
#include <helpers/misc_helpers.h>

// #define memydbg yep

memy_init _memy_init;
memy_init::memy_init() : CAutoGameSystem("")
{
}

// shared between client and server
modbin* engine_bin          = new modbin();
modbin* server_bin          = new modbin();
modbin* tier0_bin           = new modbin();

// client only
#ifdef CLIENT_DLL
    modbin* vgui_bin        = new modbin();
    modbin* client_bin      = new modbin();
    modbin* gameui_bin      = new modbin();
#endif


char bins_list[][MAX_PATH] =
{
    {},             // engine
    {},             // server
    {},             // tier0
#ifdef CLIENT_DLL
    {},             // vgui
    {},             // client
    {},             // gameui
#endif
};

modbin* modbins_list[]
{
    engine_bin,
    server_bin,
    tier0_bin,
#ifdef CLIENT_DLL
    vgui_bin,
    client_bin,
    gameui_bin,
#endif
};

memy _memy;
memy::memy()
{
    V_strncpy(bins_list[0], FORCE_OBFUSCATE("engine"),         32);
    V_strncpy(bins_list[1], FORCE_OBFUSCATE("server"),         32);
    V_strncpy(bins_list[2], FORCE_OBFUSCATE("tier0"),          32);
#ifdef CLIENT_DLL
    V_strncpy(bins_list[3], FORCE_OBFUSCATE("vguimatsurface"), 32);
    V_strncpy(bins_list[4], FORCE_OBFUSCATE("client"),         32);
    V_strncpy(bins_list[5], FORCE_OBFUSCATE("GameUI"),         32);
#endif
}

#include <sdkCURL/sdkCURL.h>

bool memy_init::Init()
{
    // memy();
    memy::InitAllBins();

#ifdef SDKCURL
    new sdkCURL;
#endif

    return true;
}


bool memy::InitAllBins()
{
    // memy();
    size_t sbin_size = sizeof(bins_list) / sizeof(bins_list[0]);

    // loop thru our bins
    for (size_t ibin = 0; ibin < sbin_size; ibin++)
    {
        if (!InitSingleBin(bins_list[ibin], modbins_list[ibin]))
        {
            Error("[MEMY] Couldn't init %s!", bins_list[ibin]);
        }
    }

#ifdef CLIENT_DLL
    char* clipath = client_bin->binpath;
    V_StripFilename(clipath);
    V_StripLastDir(clipath, MAX_PATH);
    V_StripTrailingSlash(clipath);

    ConVarRef modpath = ConVarRef("_modpath", false);
    if (modpath.IsValid())
    {
        modpath.SetValue(clipath);
    }
    else
    {
        Error("Failed getting mod path!");
    }

#endif

    char* engpath = engine_bin->binpath;
    V_StripFilename(engpath);
    V_StripLastDir(engpath, MAX_PATH);
    V_StripTrailingSlash(engpath);

    ConVarRef sdkpath = ConVarRef("_sdkpath", false);
    if (sdkpath.IsValid())
    {
        sdkpath.SetValue(engpath);
    }
    else
    {
        Error("Failed getting SDK2013 path!");
    }
    return true;
}

bool memy::InitSingleBin(const char* binname, modbin* mbin)
{
    // binname + .dll
    char realbinname[256] = {};

    #ifdef _WIN32
        V_snprintf(realbinname, sizeof(realbinname), "%s.dll", binname);

        HMODULE mhandle;
        mhandle = GetModuleHandleA(realbinname);
        if (!mhandle)
        {
            Error("[MEMY] Couldn't init %s!\n", realbinname);
            return false;
        }

        MODULEINFO minfo;

        GetModuleInformation(GetCurrentProcess(), mhandle, &minfo, sizeof(minfo));

        mbin->addr = reinterpret_cast<uintptr_t>(minfo.lpBaseOfDll);
        mbin->size = minfo.SizeOfImage;
        mbin->end  = mbin->addr + mbin->size;
        GetModuleFileName(mhandle, mbin->binpath, MAX_PATH);

        if (!mbin->addr || !mbin->size || !mbin->binpath)
        {
            Error("[MEMY] Couldn't init %s; addr = %x, size = %i, path = %p!\n", realbinname, mbin->addr, mbin->size, mbin->binpath);

            return false;
        }
        #ifdef memydbg
            Warning("memy::InitSingleBin -> name %s, mbase %x, msize %i, path = %s\n", realbinname, mbin->addr, mbin->size, mbin->binpath);
        #endif

    #else
        // binname + .so

        // funny special cases
        if (strcmp(binname, "engine") == 0)
        {
            // client only
            #ifdef CLIENT_DLL
                V_snprintf(realbinname, sizeof(realbinname), "%s.so", binname);
            // server only
            #else
                if (engine->IsDedicatedServer())
                {
                    V_snprintf(realbinname, sizeof(realbinname), "%s_srv.so", binname);
                }
                else
                {
                    V_snprintf(realbinname, sizeof(realbinname), "%s.so", binname);
                }
            #endif
        }
        // linux loads libtier0.so and libtier0_srv.so, and they are different. Yay!
        else if (strcmp(binname, "tier0") == 0)
        {
            // client only
            #ifdef CLIENT_DLL
                V_snprintf(realbinname, sizeof(realbinname), "lib%s.so", binname);
            // server only
            #else
                if (engine->IsDedicatedServer())
                {
                    V_snprintf(realbinname, sizeof(realbinname), "lib%s_srv.so", binname);
                }
                else
                {
                    V_snprintf(realbinname, sizeof(realbinname), "lib%s.so", binname);
                }
            #endif
        }
        else
        {
            V_snprintf(realbinname, sizeof(realbinname), "%s.so", binname);
        }

        void*          mbase = nullptr;
        size_t         msize = 0;
        char path[MAX_PATH]  = {};
        if (GetModuleInformation(realbinname, &mbase, &msize, path))
        {
            Error("memy::InitSingleBin -> GetModuleInformation failed for %s!\n", realbinname);
            return false;
        }

        mbin->addr      = reinterpret_cast<uintptr_t>(mbase);
        mbin->size      = msize;
		mbin->end       = mbin->addr + mbin->size;

        V_strncpy(mbin->binpath, path, MAX_PATH);

        #ifdef memydbg
            Warning("memy::InitSingleBin -> name %s, mbase %x, msize %i, path = %s\n", realbinname, mbin->addr, mbin->size, mbin->binpath);
        #endif

    #endif

    return true;
}

inline bool memy::comparedata(const byte* data, const char* pattern, const size_t sigsize)
{
    if (!data || !pattern || !sigsize)
    {
        #ifdef memydbg
            Warning("memy::DataCompare -> Couldn't grab data %p, pattern %p, nor patternsize %i\n", data, pattern, sigsize);
        #endif
        return false;
    }

    for
    (
        size_t head = 0;
        head < sigsize; // sigsize doesn't start from 0 so we don't need to <=
        (head++, pattern++, data++)
    )
    {
        // char at this pos in our pattern
        byte pattern_byte = *(pattern);

        #ifdef memydbg
        if (head >= sigsize - 6)
        {
            Warning("memy::DataCompare -> head = %i; char = %.2x\n", head, pattern_byte);
        }
        #endif

        // if it's a wildcard just skip it
        if ( pattern_byte == '\x2A' )
        {
            continue;
        }

        // char at this pos in our memory
        byte data_byte = *(data);

        // if it doesn't match it's bunk; go to the next byte
        if ( pattern_byte != data_byte )
        {
            return false;
        }
    }

    return true;
}

//---------------------------------------------------------------------------------------------------------
// Finds a pattern of bytes in the engine memory given a signature and a mask
// Returns the address of the first (and hopefully only) match with an optional offset, otherwise nullptr
//---------------------------------------------------------------------------------------------------------    
uintptr_t memy::FindPattern(const uintptr_t startaddr, const size_t searchsize, const char* pattern, const size_t sigsize, const size_t offset)
{
    #ifdef memydbg
        char hexstr[128] = {};
        V_binarytohex
        (
            reinterpret_cast<const byte*>(pattern),
            (sigsize * 2) + 1, // sigsize -> bytes + nullterm
            hexstr,
            (sigsize * 2) + 1
        );
    #endif

    if (!startaddr || !searchsize || !pattern)
    {
        #ifdef memydbg
            Warning("memy::FindPattern -> Couldn't grab modbase %x, modsize %i, or pattern %p = %s\n", startaddr, searchsize, pattern, hexstr);
        #endif

        return NULL;
    }

    // iterate thru memory, starting at modbase + i, up to (modsize + searchsize) - sigsize
    for (size_t i = 0; i <= (searchsize - sigsize); i++)
    {
        byte* addr = reinterpret_cast<byte*>(startaddr) + i;

        if (comparedata(addr, pattern , sigsize))
        {
            #ifdef memydbg
                Warning("memy::FindPattern -> found pattern %s, %i, %i!\n", hexstr, sigsize, offset);
            #endif

            return reinterpret_cast<uintptr_t>(addr + offset);
        }
    }

    #ifdef memydbg
        Warning("memy::FindPattern -> Failed, pattern %s, %i, %i!\n", hexstr, sigsize, offset);
    #endif

    return NULL;
}


bool memy::SetMemoryProtection(void* addr, size_t protlen, int wantprot)
{
    #ifdef _WIN32
        // VirtualProtect requires a valid pointer to store the old protection value
        DWORD tmp;
        DWORD prot;

        switch (wantprot)
        {
            case (MEM_READ):
            {
                prot = PAGE_READONLY;
                break;
            }
            case (MEM_READ | MEM_WRITE):
            {
                prot = PAGE_READWRITE;
                break;
            }
            case (MEM_READ | MEM_EXEC):
            {
                prot = PAGE_EXECUTE_READ;
                break;
            }
            case (MEM_READ | MEM_WRITE | MEM_EXEC):
            default:
            {
                prot = PAGE_EXECUTE_READWRITE;
                break;
            }
        }

        // BOOL is typedef'd as an int on Windows, sometimes (lol), bang bang it to convert it to a bool proper
        return !!(VirtualProtect(addr, protlen, prot, &tmp));
    #else
    // POSIX
        return mprotect( LALIGN(addr), protlen + LALDIF(addr), wantprot) == 0;
    #endif
}


#if defined (POSIX)
//returns 0 if successful
int memy::GetModuleInformation(const char *name, void **base, size_t *length, char path[MAX_PATH])
{
    // this is the only way to do this on linux, lol
    FILE *f = fopen("/proc/self/maps", "r");
    if (!f)
    {
        Warning("memy::GetModInfo -> Couldn't get proc->self->maps\n");
        return 1;
    }

    char buf[PATH_MAX+100];
    while (!feof(f))
    {
        if (!fgets(buf, sizeof(buf), f))
            break;

        char *tmp = strrchr(buf, '\n');
        if (tmp)
            *tmp = '\0';

        char *mapname = strchr(buf, '/');
        if (!mapname)
            continue;

        char perm[5];
        unsigned long begin, end;
        sscanf(buf, "%lx-%lx %4s", &begin, &end, perm);

        if (strcmp(basename(mapname), name) == 0 && perm[0] == 'r' && perm[2] == 'x')
        {
            #ifdef memydbg
                Warning("perm = %s\n", perm);
            #endif
            *base = (void*)begin;
            *length = (size_t)end-begin;
            V_strncpy(path, mapname, MAX_PATH);
            fclose(f);
            return 0;
        }
    }

    fclose(f);
    Warning("memy::GetModInfo -> Couldn't find info for modname %s\n", name);
    return 2;
}
#endif
