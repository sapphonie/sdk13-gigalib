#ifndef FLUSH_DLS_H
#define FLUSH_DLS_H
#include <helpers/misc_helpers.h>

#include <sdksentry/sdksentry.h>

class flushDLS : public CAutoGameSystem
{
public:
    flushDLS();
    bool Init() override;
    void PostInit() override;
};


// #define Debug_FlushDLs yep

#ifdef Debug_FlushDLs
Color grn(0, 255, 0, 255);
Color ylw(255, 255, 0, 255);
Color red(255, 0, 0, 255);
#endif

#endif // FLUSH_DLS_H