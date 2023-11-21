#ifndef MISC_HELPERS_H
#define MISC_HELPERS_H

#ifdef _WIN32
#pragma once
#endif

#include <cbase.h>
// If you use $QUOTE in VPCs, linux needs stringified to get the value as a string, windows does not
#ifdef _WIN32
	#define VPC_QUOTE_STRINGIFY(x) x

#else
	#define VPC_QUOTE_STRINGIFY(x) V_STRINGIFY(x)
#endif


#include <inc_cpp_stdlib.h>

#include <obfuscate.h>
#include <vtf/vtf.h>
#include <netadr.h>
#include <inetchannelinfo.h>
#include <inetchannel.h>
#include <eiface.h>
#include <cdll_int.h>

#ifdef CLIENT_DLL
#include <cdll_client_int.h>
#include <icommandline.h>
#include <filesystem.h>

bool UTIL_CheckRealRemoteAddr(netadr_t netaddr);
bool UTIL_GetRealRemoteAddr(char* ipadr);
#endif // clientdll
bool UTIL_IsFakePlayer(CBasePlayer* inplayer);
bool UTIL_IsVTFValid(std::string& instr);
std::unique_ptr<byte> UTIL_SmartLoadFileForMe(std::string& infile, int* pLength);
std::string UTIL_AddrToString(void* inAddr);


// https://stackoverflow.com/a/57346888
//
std::vector<std::string> UTIL_SplitSTDString(const std::string& i_str, const std::string& i_delim);
std::string UTIL_StripCharsFromSTDString(std::string i_str, char char_to_strip);
std::string& UTIL_ReplaceAll(std::string& context, std::string const& from, std::string const& to);

// https://stackoverflow.com/a/49066369
// Get time stamp in milliseconds.
uint64_t millis();
// Get time stamp in microseconds.
uint64_t micros();
// Get time stamp in nanoseconds.
uint64_t nanos();

#ifdef CLIENT_DLL
void UTIL_GetMap(char mapname[128]);
#endif

#ifdef GNUC
#undef offsetof
//#define offsetof( type, var ) __builtin_offsetof( type, var )
#define offsetof(s,m)	(size_t)&(((s *)0)->m)
#else
#undef offsetof
#define offsetof(s,m)	(size_t)&(((s *)0)->m)
#endif



// trim from start (in place)
static inline void ltrim(std::string& s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
        }));
}

// trim from end (in place)
static inline void rtrim(std::string& s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
        }).base(), s.end());
}

// trim from both ends (in place)
static inline void trim(std::string& s) {
    rtrim(s);
    ltrim(s);
}

// trim from start (copying)
static inline std::string ltrim_copy(std::string s) {
    ltrim(s);
    return s;
}

// trim from end (copying)
static inline std::string rtrim_copy(std::string s) {
    rtrim(s);
    return s;
}

// trim from both ends (copying)
static inline std::string trim_copy(std::string s) {
    trim(s);
    return s;
}







// Reimplementation of sdk13 convar class so we can poke at internals
// please use with caution as this invalidates the entire point of some of this stuff being private
// example use:
//
// ConVarRef mat_picmip("mat_picmip");
// if (mat_picmip.IsValid())
// {
//     ConVar*     realVar = static_cast<ConVar*>(mat_picmip.GetLinkedConVar());
//     FakeConVar* fakeVar = reinterpret_cast<FakeConVar*>(realVar);
//     FakeConVar__SetMax(fakeVar, 10.0);
//     FakeConVar__SetMin(fakeVar, -10.0);
// }
//
class FakeConVar
{
public:
    friend class ConVar;
    friend class CCvar;
    friend class ConVarRef;

    // no ctor/dtor
    // FakeConVar()    = delete;
    // ~FakeConVar()   = delete;
    // FakeConVar(FakeConVar const&) = delete;
    // FakeConVar& operator=(FakeConVar const&) = delete;

    // these are the actual functions
    char bullshit[28];

    // This either points to "this" or it points to the original declaration of a ConVar.
    // This allows ConVars to exist in separate modules, and they all use the first one to be declared.
    // m_pParent->m_pParent must equal m_pParent (ie: m_pParent must be the root, or original, ConVar).
    ConVar* m_pParent;

    // Static data
    const char* m_pszDefaultValue;

    // Value
    // Dynamically allocated
    char* m_pszString;
    int							m_StringLength;

    // Values
    float						m_fValue;
    int							m_nValue;

    // Min/Max values
    bool						m_bHasMin;
    float						m_fMinVal;
    bool						m_bHasMax;
    float						m_fMaxVal;

    // Call this function when ConVar changes
    FnChangeCallback_t			m_fnChangeCallback;

    // ANYTHING PAST HERE NEEDS TO BE FORCEINLINED
    FORCEINLINE_CVAR void SetMin(float min)
    {
        reinterpret_cast<FakeConVar*>(m_pParent)->m_bHasMin = true;
        reinterpret_cast<FakeConVar*>(m_pParent)->m_fMinVal = min;

    }

    FORCEINLINE_CVAR void SetMax(float max)
    {
        reinterpret_cast<FakeConVar*>(m_pParent)->m_bHasMax = true;
        reinterpret_cast<FakeConVar*>(m_pParent)->m_fMaxVal = max;
    }
};

static_assert( sizeof(FakeConVar) == sizeof(ConVar) );


bool checkWine();


// need to clean this up
const char* HACK_COM_GetModDirectory();

#endif
