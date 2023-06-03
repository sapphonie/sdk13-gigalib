
#ifndef SDKCURL_H
#define SDKCURL_H

#ifdef SDKCURL
#ifdef _WIN32
extern "C" { FILE __iob_func[3] = { *stdin,*stdout,*stderr }; }
#endif

#include <sdkCURL/vendored/curl.h>
#include <sdkCURL/vendored/easy.h>
#include <helpers/misc_helpers.h>

#include "tier0/valve_minmax_off.h"
    #include <string>
    #include <vector>
    #include <thread>
#include "tier0/valve_minmax_on.h"

#undef SetPort
#undef PlaySound


#include <helpers/steam_helpers.h>
struct curlResponse
{
    std::string originalURL             = {};
    void* callback                      = {};
    /*
        if you want to copy this into a c str:

        char* buffer = new char[resp->body.size() + 1] {};
        strcpy(buffer, resp->body.c_str());

        ...

        delete [] buffer;
    */
    std::string body                    = {};
    std::vector<std::string> headers    = {};
    std::string httpType                = {};
    short respCode                      = {};
    // do not use this for allocating buffers
    std::string contentLen              = {};
    // use this for allocating buffers instead
    int bodyLen                         = {};
    bool completed                      = false;
    bool failed                         = false;
};

typedef void (*curlCallback)(const curlResponse* curlRepsonseStruct);

class sdkCURL : public CAutoGameSystem , public CAutoGameSystemPerFrame
{
public:

    std::vector<curlResponse*> reqs     = {};

    sdkCURL();
    bool InitCURL();
    bool CURLGet(std::string inURL, curlCallback );

    // Methods of IGameSystem
    virtual char const* Name() { return "sdkCURL"; }
    // Gets called each frame
    virtual void Update(float frametime);


private:
    static size_t response_callback(void* ptr, size_t size, size_t nmemb, void* stream);
    static size_t header_callback(char* buffer, size_t size, size_t nitems, void* userdata);

    bool curlSetSSL = false;
    bool curlInited = false;
    bool CURLGet_Thread(std::string inURL, curlResponse* resp);

};

extern sdkCURL* g_sdkCURL;

#endif // SDKCURL
#endif // SDKCURL_H
