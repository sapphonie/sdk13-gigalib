#ifndef MEMYTOOLS_H
#define MEMYTOOLS_H

#ifdef _WIN32
    #pragma once

    #define WIN32_LEAN_AND_MEAN
    #include "Windows.h"
    #include "Psapi.h"
    #pragma comment(lib, "psapi.lib")

    #define MEM_READ  1
    #define MEM_WRITE 2
    #define MEM_EXEC  4
#elif defined (POSIX)
    #include "tier0/valve_minmax_off.h"
    #include "sys/mman.h"

    #define MEM_READ  PROT_READ
    #define MEM_WRITE PROT_WRITE
    #define MEM_EXEC  PROT_EXEC

    // Addresses must be aligned to page size for linux
    #define LALIGN(addr) (void*)((uintptr_t)(addr) & ~(getpagesize() - 1))
    #define LALDIF(addr) ((uintptr_t)(addr) % getpagesize())

    #include "tier0/valve_minmax_on.h"
#endif
typedef unsigned char byte;

#ifdef PostMessage
	#undef PostMessage
#endif

struct modbin
{
    uintptr_t addr = NULL;
    size_t    size = 0;
    char      binpath[MAX_PATH] = {};
};


// shared between client and server
extern struct modbin* engine_bin;
extern struct modbin* server_bin;
extern struct modbin* tier0_bin;

// client only
extern struct modbin* vgui_bin;
extern struct modbin* client_bin;
extern struct modbin* gameui_bin;

#include <igamesystem.h>
class memy_init : public CAutoGameSystem
{
public:
    memy_init();
    bool                    Init() override;
};

class memy
{
    public:
        memy();

        static bool         InitAllBins();


        static uintptr_t    FindPattern(uintptr_t startaddr, size_t searchsize, const char* pattern, size_t sigsize, size_t offset = 0);
        static uintptr_t    FindPattern(modbin* mbin, const char* pattern, size_t sigsize, size_t offset = 0)
        {
            return          FindPattern(mbin->addr, mbin->size, pattern, sigsize, offset);
        }

        static bool         SetMemoryProtection(void* addr, size_t protlen, int wantprot);
        #if defined (POSIX)
        static int          GetModuleInformation(const char* name, void** base, size_t* length, char[MAX_PATH] path);
        #endif
    private:

        static bool         InitSingleBin(const char* binname, modbin* mbin);


        static inline bool  comparedata(const byte* addr, const char* pattern, size_t sigsize);

};



#endif // MEMYTOOLS_H