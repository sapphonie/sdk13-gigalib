
#include <cbase.h>
#ifdef CLIENT_DLL
#include <sdkCURL/sdkCURL.h>
sdkCURL* g_sdkCURL;

// sdkCURL g_sdkCURL;
sdkCURL::sdkCURL()
{
    g_sdkCURL = this;
    InitCURL();
}



void curlcurlcurl(const curlResponse* curlRepsonseStruct)
{

    Warning("SUCCESS! %p / %.512s\n\n\n", curlRepsonseStruct, curlRepsonseStruct->body.c_str());

}

void recurl_f(const CCommand& command)
{
    std::string url( command.Arg(1) );

    // bool ret =
    g_sdkCURL->CURLGet(url, curlcurlcurl);
    //if (!ret)
    //{
    //    Warning("CURL req failed!\n");
    //    return;
    //}
}

ConCommand recurl("recurl", recurl_f);


size_t sdkCURL::response_callback(void* ptr, size_t size, size_t nmemb, void* stream)
{
    size_t respsize = size * nmemb;
    // might not be null term'd so we have to specify a size
    reinterpret_cast<curlResponse*>(stream)->body.append((const char*)ptr, respsize);
    return respsize;
}


size_t sdkCURL::header_callback(char* buffer, size_t size, size_t nitems, void* userdata)
{
    size_t headersize = nitems * size;
    // might not be null term'd so we have to specify nitems as the size (size is always 1, what a footgun)
    reinterpret_cast<curlResponse*>(userdata)->headers.push_back(std::string(buffer, nitems));
    return headersize;
}

// called in memy
bool sdkCURL::InitCURL()
{
    // set in sentry initialization. probably should move that someday
    ConVar* __modpath = cvar->FindVar("_modpath");
    char* libcurlLocation = new char[MAX_PATH]{};
#ifdef _WIN32
    V_snprintf(libcurlLocation, MAX_PATH, "%sbin%slibcurl.dll", __modpath->GetString(), CORRECT_PATH_SEPARATOR_S);

    HMODULE libcurldll = LoadLibrary(libcurlLocation);
#else
    //
#endif
    delete [] libcurlLocation;
    if (!libcurldll)
    {
        Warning("Couldn't load libcurl!\n");
        return false;
    }
    ptr_curl_global_init        = GetProcAddress(libcurldll, "curl_global_init");
    ptr_curl_global_init        = GetProcAddress(libcurldll, "curl_global_init");
    ptr_curl_version            = GetProcAddress(libcurldll, "curl_version");
    ptr_curl_global_sslset      = GetProcAddress(libcurldll, "curl_global_sslset");
    ptr_curl_easy_init          = GetProcAddress(libcurldll, "curl_easy_init");
    ptr_curl_easy_cleanup       = GetProcAddress(libcurldll, "curl_easy_cleanup");
    ptr_curl_easy_setopt        = GetProcAddress(libcurldll, "curl_easy_setopt");
    ptr_curl_easy_perform       = GetProcAddress(libcurldll, "curl_easy_perform");
    ptr_curl_easy_strerror      = GetProcAddress(libcurldll, "curl_easy_strerror");
    ptr_curl_easy_option_by_id  = GetProcAddress(libcurldll, "curl_easy_option_by_id");

    f_curl_global_init          = (FUNC_curl_global_init*)          ptr_curl_global_init;
    f_curl_version              = (FUNC_curl_version*)              ptr_curl_version;
    f_curl_global_sslset        = (FUNC_curl_global_sslset*)        ptr_curl_global_sslset;
    f_curl_easy_init            = (FUNC_curl_easy_init*)            ptr_curl_easy_init;
    f_curl_easy_cleanup         = (FUNC_curl_easy_cleanup*)         ptr_curl_easy_cleanup;
    f_curl_easy_setopt          = (FUNC_curl_easy_setopt*)          ptr_curl_easy_setopt;
    f_curl_easy_perform         = (FUNC_curl_easy_perform*)         ptr_curl_easy_perform;
    f_curl_easy_strerror        = (FUNC_curl_easy_strerror*)        ptr_curl_easy_strerror;
    f_curl_easy_option_by_id    = (FUNC_curl_easy_option_by_id*)    ptr_curl_easy_option_by_id;


    CURLcode ccode = {};
    curlSetSSL = false;
    curlInited = false;

    if (!curlSetSSL)
    {
        // use the system ssl certs
        CURLsslset sslset = f_curl_global_sslset(CURLSSLBACKEND_OPENSSL, NULL, NULL);
        if (sslset != CURLSSLSET_OK)
        {
            Warning("curl_global_sslset failed: %i\n", sslset);
            return false;
        }
        curlSetSSL = true;
    }


    if (!curlInited)
    {
        ccode = f_curl_global_init(CURL_GLOBAL_ALL);
        if (ccode != CURLE_OK)
        {
            Warning("curl_global_init() failed: %s\n", f_curl_easy_strerror(ccode));
            return false;
        }
        curlInited = true;
    }
    Warning("vers = %s\n", f_curl_version());

    reqs.clear();

    return true;

}


#define setopt_errwrap(chand, opt, etc)                                 \
    ccode = f_curl_easy_setopt(chand, opt, etc);                        \
    if (ccode != CURLE_OK)                                              \
    {                                                                   \
        resp->failed    = true;                                         \
        resp->completed = true;                                         \
        const curl_easyoption* ezoptinfo = f_curl_easy_option_by_id(opt); \
        Warning("curl_easy_setopt (%s) failed: %s\n",                   \
        f_curl_easy_strerror(ccode), ezoptinfo->name);                  \
        f_curl_easy_cleanup(chand);                                     \
        return false;                                                   \
    }


#include <functional>
bool sdkCURL::CURLGet_Thread(std::string inURL, curlResponse* resp)
{
    resp->originalURL = inURL;
    resp->failed    = true;
    resp->completed = false;

    if (!curlSetSSL || !curlInited)
    {
        resp->failed    = true;
        resp->completed = true;

        Warning("CURL COULDN'T INIT!\n");
        return false;
    }
    CURL* curl = {};
    curl = f_curl_easy_init();
    Warning("%p\n", curl);

    if (!curl)
    {
        resp->failed    = true;
        resp->completed = true;

        Warning("curl_easy_init returned NULL!\n");
        return false;
    }
    

    CURLcode ccode;
    setopt_errwrap(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
    setopt_errwrap(curl, CURLOPT_TRANSFER_ENCODING, 1L);

    setopt_errwrap(curl, CURLOPT_DOH_URL, "https://cloudflare-dns.com/dns-query");

    setopt_errwrap(curl, CURLOPT_URL, inURL.c_str());
    setopt_errwrap(curl, CURLOPT_FOLLOWLOCATION, 1L);

    // send all response data to this func
    setopt_errwrap(curl, CURLOPT_WRITEFUNCTION, response_callback);
    setopt_errwrap(curl, CURLOPT_WRITEDATA, resp);

    // send all headers to this func
    setopt_errwrap(curl, CURLOPT_HEADERFUNCTION, header_callback);
    setopt_errwrap(curl, CURLOPT_HEADERDATA, resp);

    setopt_errwrap(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    ccode = f_curl_easy_perform(curl);
    if (ccode != CURLE_OK)
    {
        Warning("curl_easy_perform() failed: %s\n", f_curl_easy_strerror(ccode));
        f_curl_easy_cleanup(curl);
        return false;
    }

    f_curl_easy_cleanup(curl);

    resp->failed    = false;
    resp->completed = true;
    return true;
}

bool sdkCURL::CURLGet(std::string inURL, curlCallback ccb)
{
    curlResponse* r = new curlResponse;
    r->callback = (void*)ccb;
    reqs.push_back(r);
    std::thread t(&sdkCURL::CURLGet_Thread, g_sdkCURL, inURL, r);
    t.detach();
    return true;
}




void sdkCURL::Update(float frametime)
{


    for (size_t i = 0; i < reqs.size(); ++i)
    {

        curlResponse* thisReq = reqs.at(i);


        if (thisReq && !thisReq->failed && thisReq->completed)
        {
            curlCallback callthis = (curlCallback)(thisReq->callback);
            callthis(thisReq);
            Warning("SUCC \n");

            reqs.erase(reqs.begin() + i);
            delete thisReq;
            continue;
        }

        else if (!thisReq)
        {
            AssertMsgAlways(0, "How the hell did we not get a curl request???");
            Warning("How the hell did we not get a curl request???");
            continue;
        }

        else if (thisReq->failed && thisReq->completed)
        {
            Warning("%p FAILED\n", thisReq);
            reqs.erase(reqs.begin() + i);
            continue;
        }
    }

    /*
    Warning("%.512s\n\n\n", r->body.c_str());

    for (auto& thisheader : r->headers)
    {
        // not the hdeaer we want
        if (thisheader.find("HTTP/") == std::string::npos)
        {
            //continue;
        }
        // redirects
        else if
            (
                thisheader.find("301")
                || thisheader.find("302")
                || thisheader.find("303")
                || thisheader.find("307")
                || thisheader.find("308")
                )
        {
            //continue;
        }
        const char* th = thisheader.c_str();
        Warning("header %s\n", th);
    }

    delete r;
*/
}



#endif
