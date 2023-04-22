
#ifndef SDKCURL_H
#define SDKCURL_H

#ifdef CLIENT_DLL
#    define CURL_EXTERN  __declspec(dllimport)

#include <sdkCURL/vendored/curl.h>
#include <sdkCURL/vendored/easy.h>
#include <helpers/misc_helpers.h>

#include <string>
#include <vector>
#include <thread>
#undef SetPort
#undef PlaySound


#include <unordered_map>
#include <helpers/steam_helpers.h>


struct curlResponse
{
    std::string originalURL             = {};
    void* callback                      = {};
    std::string body                    = {};
    std::vector<std::string> headers    = {};
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

    void* ptr_curl_global_init        = {};
    void* ptr_curl_version            = {};
    void* ptr_curl_global_sslset      = {};
    void* ptr_curl_easy_init          = {};
    void* ptr_curl_easy_cleanup       = {};
    void* ptr_curl_easy_setopt        = {};
    void* ptr_curl_easy_perform       = {};
    void* ptr_curl_easy_strerror      = {};
    void* ptr_curl_easy_option_by_id  = {};


    typedef CURLcode                        FUNC_curl_global_init(long flags);
    typedef char*                           FUNC_curl_version(void);
    typedef CURLsslset                      FUNC_curl_global_sslset(curl_sslbackend id, const char* name, const curl_ssl_backend*** avail);
    typedef CURL*                           FUNC_curl_easy_init(void);
    typedef void                            FUNC_curl_easy_cleanup(CURL* curl);
    typedef CURLcode                        FUNC_curl_easy_setopt(CURL* curl, CURLoption option, ...);
    typedef CURLcode                        FUNC_curl_easy_perform(CURL* curl);
    typedef const char*                     FUNC_curl_easy_strerror(CURLcode);
    typedef const struct curl_easyoption*   FUNC_curl_easy_option_by_id(CURLoption);

    FUNC_curl_global_init*          f_curl_global_init              = {};
    FUNC_curl_version*              f_curl_version                  = {};
    FUNC_curl_global_sslset*        f_curl_global_sslset            = {};
    FUNC_curl_easy_init*            f_curl_easy_init                = {};
    FUNC_curl_easy_cleanup*         f_curl_easy_cleanup             = {};
    FUNC_curl_easy_setopt*          f_curl_easy_setopt              = {};
    FUNC_curl_easy_perform*         f_curl_easy_perform             = {};
    FUNC_curl_easy_strerror*        f_curl_easy_strerror            = {};
    FUNC_curl_easy_option_by_id*    f_curl_easy_option_by_id        = {};
};

#endif
#endif