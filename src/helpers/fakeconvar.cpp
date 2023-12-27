#include <cbase.h>
#include <inc_cpp_stdlib.h>
#include <fakeconvar.h>
#include <igamesystem.h>

constexpr static const char* ConVarTestString = __TIMESTAMP__;

#ifdef CLIENT_DLL
ConVar testConVar("testConVar_client", ConVarTestString, FCVAR_CHEAT);
#endif
#ifdef GAME_DLL
ConVar testConVar("testConVar_server", ConVarTestString, FCVAR_CHEAT);
#endif


// we can't do offsetof so instead we supply this macro with the member to check
// and what we think the offset of that member should be
#define CHECK_CONVAR_OFFSET_MEMBER(member, expectedOffsetFromConVar)                            \
{                                                                                               \
    int offsetFromFakeConVar = offsetof(FakeConVar, member);                                    \
    if (offsetFromFakeConVar != expectedOffsetFromConVar)                                       \
    {                                                                                           \
        std::string msg = fmt::format(                                                          \
            FMT_STRING( "Offset {:s} on FakeConVar == 0x{:x}, should be 0x{:x}! Exploding!" ),  \
            V_STRINGIFY(member),                                                                \
            offsetFromFakeConVar,                                                               \
            expectedOffsetFromConVar);                                                          \
        const char* str = msg.c_str();                                                          \
        AssertMsgAlways(offsetFromFakeConVar == expectedOffsetFromConVar, str);                 \
    }                                                                                           \
}


class FakeConVarUnitTests : public CAutoGameSystem
{
public:
    FakeConVarUnitTests() {};

    virtual bool Init()
    {
        return true;
    }

    virtual void PostInit()
    {
        static bool inited = false;
        if (inited)
        {
            return;
        }
        inited = true;

        // ConVarRef testConVarRef("testConVar", false);
        // AssertFatalMsg(testConVarRef.IsValid(), "Failed getting testConVar ref!");
        // ConVar* testConVarPtr       = static_cast<ConVar*>(testConVarRef.GetLinkedConVar());
        ConVar* testConVarPtr       = static_cast<ConVar*>(&testConVar);
        AssertMsgAlways(testConVarPtr, "Failed getting testConVarPtr!");

        FakeConVar* fakeTestConVar  = reinterpret_cast<FakeConVar*>(testConVarPtr);
        NOTE_UNUSED(fakeTestConVar);
        // BaseClass VFTable @ 0x0
        CHECK_CONVAR_OFFSET_MEMBER(m_pNext,            0x04);
        CHECK_CONVAR_OFFSET_MEMBER(m_bRegistered,      0x08);
        CHECK_CONVAR_OFFSET_MEMBER(m_pszName,          0x0C);
        CHECK_CONVAR_OFFSET_MEMBER(m_pszHelpString,    0x10);
        CHECK_CONVAR_OFFSET_MEMBER(m_nFlags,           0x14);
        // IConVar VFTable @ 0x18
        CHECK_CONVAR_OFFSET_MEMBER(m_pParent,          0x1C);
        CHECK_CONVAR_OFFSET_MEMBER(m_pszDefaultValue,  0x20);
        CHECK_CONVAR_OFFSET_MEMBER(m_pszString,        0x24);
        CHECK_CONVAR_OFFSET_MEMBER(m_StringLength,     0x28);
        CHECK_CONVAR_OFFSET_MEMBER(m_fValue,           0x2C);
        CHECK_CONVAR_OFFSET_MEMBER(m_nValue,           0x30);
        CHECK_CONVAR_OFFSET_MEMBER(m_bHasMin,          0x34);
        CHECK_CONVAR_OFFSET_MEMBER(m_fMinVal,          0x38);
        CHECK_CONVAR_OFFSET_MEMBER(m_bHasMax,          0x3C);
        CHECK_CONVAR_OFFSET_MEMBER(m_fMaxVal,          0x40);
        CHECK_CONVAR_OFFSET_MEMBER(m_fnChangeCallback, 0x44);

#if 1
        try
        {
            volatile const char* defaultValue =
            *reinterpret_cast<const char**>
            (
                reinterpret_cast<uintptr_t>(testConVarPtr) + offsetof(FakeConVar, m_pszDefaultValue)
            );

            AssertMsgAlways(strcmp((const char*)(defaultValue), fakeTestConVar->m_pszDefaultValue) == 0,
                "FakeConVar/ConVar mismatch on ->m_pszDefaultValue! #1");

            AssertMsgAlways(strcmp((const char*)(defaultValue), ConVarTestString) == 0,
                "FakeConVar/ConVar mismatch on ->m_pszDefaultValue! #2");

            AssertMsgAlways(strcmp(fakeTestConVar->m_pszDefaultValue, ConVarTestString) == 0,
                "FakeConVar/ConVar mismatch on ->m_pszDefaultValue! #3");

            // Misc
            fakeTestConVar->SetMin(0.0);
            fakeTestConVar->SetMax(0.0);

            // tests IConVar
            fakeTestConVar->SetValue("nada");
            AssertMsgAlways(strcmp(((ConVar*)(fakeTestConVar))->GetString(), "nada") == 0,
                "FakeConVar GetString test failed!");

            // tests ConCommandBase
            fakeTestConVar->SetFlags(FCVAR_HIDDEN | FCVAR_CHEAT);
            AssertMsgAlways(fakeTestConVar->IsFlagSet(FCVAR_CHEAT),
                "FakeConVar IsFlagSet test failed! #1");

            fakeTestConVar->SetFlags(FCVAR_HIDDEN);
            AssertMsgAlways(!fakeTestConVar->IsFlagSet(FCVAR_CHEAT),
                "FakeConVar IsFlagSet test failed! #2");

            // tests if we explode on Nuke()
            fakeTestConVar->Nuke();
        }
        catch (...)
        {
            Error("Exploded on FakeConVar set tests!");
        }
#endif

    }
};
static FakeConVarUnitTests g_FakeConVarUnitTests = {};
