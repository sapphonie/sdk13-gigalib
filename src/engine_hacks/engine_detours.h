#ifndef ENGINEDETOURS_H
#define ENGINEDETOURS_H
#ifdef _WIN32
#pragma once
#endif
#include <helpers/misc_helpers.h>
#include <memy/memytools.h>

#include <memy/detourhook.hpp>


#ifdef SDKSENTRY
    #include <sdksentry/sdksentry.h>
#endif

class CEngineDetours : public CAutoGameSystem
{
public:
                        CEngineDetours();

    void                PostInit() override;

};
#endif
