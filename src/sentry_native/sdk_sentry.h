#ifndef SDK_SENTRY_H
#define SDK_SENTRY_H
#ifdef CLIENT_DLL

#include "sentry.h"
#include <helpers/misc_helpers.h>
#include <helpers/steam_helpers.h>

class CSentry : public CAutoGameSystem
{
public:
    CSentry();

    bool                didinit;
    bool                didshutdown;

    void                PostInit() override;
    void                Shutdown() override;
    void                SentryInit();

    char                real_sentry_url[256] = {};

    CCallResult <CSentry, HTTPRequestCompleted_t> SentryURLCallResult;

    static sentry_value_t nowCTX;
    static const char*    nowFUNC;
private:
    void                GetURL();
    void                SentryURLCB(HTTPRequestCompleted_t* arg, bool bFailed);
};

void SetSteamID();
void SentryMsg(const char* logger, const char* text, bool forcesend = false);
void SentryEvent(const char* level, const char* logger, const char* message, sentry_value_t ctx, bool forcesend = false);
// void sdk13_version_callback(IConVar* var, const char* pOldValue, float flOldValue);
void SentrySetTags();


void _SentryEventThreaded(const char* level, const char* logger, const char* message, sentry_value_t ctx, bool forcesend = false);
void _SentryMsgThreaded(const char* logger, const char* text, bool forcesend);

void SentryAddressBreadcrumb(void* address, const char* optionalName = NULL);

#ifdef GetObject
#undef GetObject
#endif

#ifdef Yield
#undef Yield
#endif

#ifdef CompareString
#undef CompareString
#endif

#ifdef CreateEvent
#undef CreateEvent
#endif

#endif // CLIENT_DLL

#endif /* SDK_SENTRY_H */
