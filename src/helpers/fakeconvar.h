#ifndef FAKECONVAR_H
#define FAKECONVAR_H

#include <convar.h>
#include <iconvar.h>
#include <inc_cpp_stdlib.h>

// Reimplementation of sdk13 ConCommandBase class so we can poke at internals
// please use with caution as this invalidates the entire point of some of this stuff being private
class alignas(4) FakeConCommandBase
{
    friend class CCvar;
    friend class ConVar;
    friend class ConCommandBase;
    friend class ConCommand;
    friend class CDefaultCvar;

public:
    // needed for class/struct memory alignment,
    // so we have a vftable ptr at 0x0 on the class
    // this should never be called
    virtual void                        doNothing() = delete;

    // Next ConVar in chain
    // Prior to register, it points to the next convar in the DLL.
    // Once registered, though, m_pNext is reset to point to the next
    // convar in the global list
    FakeConCommandBase*                 m_pNext;

    // Has the cvar been added to the global list?
    bool                                m_bRegistered;

    // Static data
    const char*                         m_pszName;
    const char*                         m_pszHelpString;

    // ConVar flags
    int                                 m_nFlags;



    // ConVars add themselves to this list for the executable. 
    // Then ConVar_Register runs through  all the console variables 
    // and registers them into a global list stored in vstdlib.dll
    static FakeConCommandBase*          s_pConCommandBases;

    // ConVars in this executable use this 'global' to access values.
    static IConCommandBaseAccessor*     s_pAccessor;


    FORCEINLINE_CVAR void Shutdown_HACK()
    {
        Assert(g_pCVar);
        if (g_pCVar)
        {
            g_pCVar->UnregisterConCommand( reinterpret_cast<ConCommandBase*>(this) );
        }
    }
};

// Reimplementation of sdk13 convar class so we can poke at internals
// please use with caution as this invalidates the entire point of some of this stuff being private
// example use:
//
// ConVarRef mat_picmip("mat_picmip");
// if (mat_picmip.IsValid())
// {
//     ConVar*     realVar = static_cast<ConVar*>(mat_picmip.GetLinkedConVar());
//     FakeConVar* fakeVar = reinterpret_cast<FakeConVar*>(realVar);
//     fakeVar->SetMax(fakeVar, 10.0);
//     fakeVar->SetMin(fakeVar, -10.0);
// }
//
class alignas(4) FakeConVar : public FakeConCommandBase, public IConVar // public classes needed for memory layout compatibility
{
public:
    friend class ConVar;
    friend class CCvar;
    friend class ConVarRef;

    typedef FakeConCommandBase BaseClass;

    // This either points to "this" or it points to the original declaration of a ConVar.
    // This allows ConVars to exist in separate modules, and they all use the first one to be declared.
    // m_pParent->m_pParent must equal m_pParent (ie: m_pParent must be the root, or original, ConVar).
    FakeConVar*                 m_pParent;

    // Static data
    const char*                 m_pszDefaultValue;

    // Value
    // Dynamically allocated
    char*                       m_pszString;
    int                         m_StringLength;

    // Values
    float                       m_fValue;
    int                         m_nValue;

    // Min/Max values
    bool                        m_bHasMin;
    float                       m_fMinVal;
    bool                        m_bHasMax;
    float                       m_fMaxVal;

    // Call this function when ConVar changes
    FnChangeCallback_t          m_fnChangeCallback;

    // ANYTHING PAST HERE NEEDS TO BE FORCEINLINED
    FORCEINLINE_CVAR void SetMin(float min)
    {
        m_pParent->m_bHasMin = true;
        m_pParent->m_fMinVal = min;
    }

    FORCEINLINE_CVAR void SetMax(float max)
    {
        m_pParent->m_bHasMax = true;
        m_pParent->m_fMaxVal = max;
    }

    // set flags of a convar manually without mucking about with AddFlags or bitflag logic
    // you should still use bitflag logic if you want to preserve one or more of the old flags of a cvar, though
    // stolen from Open Fortress - thanks kaydemon
    // -sappho
    FORCEINLINE_CVAR void SetFlags(int flags)
    {
        m_pParent->m_nFlags = flags;
        m_nFlags = flags;
    }

    // delete a convar off the face of the planet
    // never use this unless you know exactly what you're doing
    FORCEINLINE_CVAR void Nuke(void)
    {
        m_pParent->Shutdown_HACK();
        Shutdown_HACK();
    }

};

static_assert(sizeof(FakeConCommandBase)    == sizeof(ConCommandBase),  "size mismatch between Fake/ConCommandBase!");
static_assert(sizeof(FakeConVar)            == sizeof(ConVar),          "size mismatch between Fake/ConVar!");

static_assert(sizeof(ConCommandBase)        == 24, "ConCommandBase  size != expected size of 24! Did you change ConVar.h?");
static_assert(sizeof(ConVar)                == 72, "ConVar          size != expected size of 72! Did you change ConVar.h?");

static_assert(std::is_standard_layout<ConVar>::value        == false, "ConVar should never be a standard layout - aka Plain Old Data!");
static_assert(std::is_standard_layout<FakeConVar>::value    == false, "FakeConVar should never be a standard layout - aka Plain Old Data!");

static_assert(alignof(FakeConCommandBase)   == alignof(ConCommandBase), "alignof mismatch between Fake/ConCommandBase!");
static_assert(alignof(FakeConVar)           == alignof(ConVar),         "alignof mismatch between Fake/ConVar!");

static_assert(alignof(ConCommandBase)       == 4, "alignof Fake/ConCommandBase should be 4!");
static_assert(alignof(ConVar)               == 4, "alignof Fake/ConVar should be 4!");

#endif