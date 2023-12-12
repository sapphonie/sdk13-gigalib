#ifndef SDKCURL_H
#define SDKCURL_H

#ifdef SDKCURL
#ifdef _WIN32
extern "C" { FILE __iob_func[3] = { *stdin,*stdout,*stderr }; }
#endif


#ifdef _WIN32

    #define curlLibPath "../shared/sdk13-gigalib/bin/curl/lib/"

    //#pragma comment( lib, curlLibPath "libbrotlicommon.a"           )
    //#pragma comment( lib, curlLibPath "libbrotlidec.a"              )
    //#pragma comment( lib, curlLibPath "libcrypto.a"                 )
    //#pragma comment( lib, curlLibPath "libcurl.a"                   )
    #pragma comment( lib, curlLibPath "libcurl.dll.a"               )
    //#pragma comment( lib, curlLibPath "libnghttp2.a"                )
    //#pragma comment( lib, curlLibPath "libnghttp3.a"                )
    //#pragma comment( lib, curlLibPath "libngtcp2.a"                 )
    //#pragma comment( lib, curlLibPath "libngtcp2_crypto_quictls.a"  )
    //#pragma comment( lib, curlLibPath "libssh2.a"                   )
    //#pragma comment( lib, curlLibPath "libssl.a"                    )
    //#pragma comment( lib, curlLibPath "libz.a"                      )
    //#pragma comment( lib, curlLibPath "libzstd.a"                   )

#endif




#include <sdkCURL/vendored/curl.h>
#include <sdkCURL/vendored/easy.h>
#include <helpers/misc_helpers.h>

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

        actually jk idiot DONT it's a std::string for a reason, use fmt::format or snprintf
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

#ifdef CLIENT_DLL
    // Methods of IGameSystem
    virtual char const* Name() { return "sdkCURL_client"; }
#endif

#ifdef GAME_DLL
    // Methods of IGameSystem
    virtual char const* Name() { return "sdkCURL_server"; }
#endif

    void UpdateRequestVector();

    // Gets called "often" (client)
    virtual void Update(float frametime);

    // Gets called "often" (server)
    virtual void FrameUpdatePostEntityThink(void);
    virtual void FrameUpdatePreEntityThink(void);


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
