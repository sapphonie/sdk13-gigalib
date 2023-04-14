#ifndef ENGINEDETOURS_H
#define ENGINEDETOURS_H
#ifdef _WIN32
#pragma once
#endif
#include <helpers/misc_helpers.h>
#include <memy/memytools.h>

#include <memy/detourhook.hpp>


#ifdef SENTRY
    #include <sentry_native/sdk_sentry.h>
#endif

class CEngineDetours : public CAutoGameSystem
{
public:
                        CEngineDetours();

    void                PostInit() override;

};
#endif
