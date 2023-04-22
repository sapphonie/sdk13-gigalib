//---------------------------------------------------------------------------------------------
// Credits to Momentum Mod for this code and specifically xen-000
//---------------------------------------------------------------------------------------------
#ifdef BIN_PATCHES

#ifndef BINPATCH_H
#define BINPATCH_H
#ifdef _WIN32
    #pragma once
#endif

#include <helpers/misc_helpers.h>
#include <memy/memytools.h>

//------------------------------------------------------------------------------------
// Functions used to find patterns of bytes in the engine's memory to hook or patch
//-----------------------------------------------------------------------------------

class CBinary : public CAutoGameSystem
{
public:
                        CBinary();

    void                PostInit() override;

private:
    static bool         ApplyAllPatches();

};

enum PatchType
{
    PATCH_IMMEDIATE     = true,
    PATCH_REFERENCE     = false
};

class CBinPatch
{
public:
    CBinPatch(char*, size_t, size_t, bool);
    CBinPatch(char*, size_t, size_t, bool, int);
    CBinPatch(char*, size_t, size_t, bool, float);
    CBinPatch(char*, size_t, size_t, bool, char*);

    bool ApplyPatch(modbin* mbin);

private:
    char *m_pSignature;
    char *m_pPatch;
    size_t m_iSize;

    size_t m_iOffset;
    size_t m_iPatchLength;

    bool m_bImmediate;
};
#endif // BINPATCH_H

#endif // BINPATCH