
#include <cbase.h>
#ifdef SDKCURL
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

    for (auto& thisheader : curlRepsonseStruct->headers)
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
}

void recurl_f(const CCommand& command)
{
    std::string url( command.Arg(1) );

    g_sdkCURL->CURLGet(url, curlcurlcurl);
}
#ifdef CLIENT_DLL
ConCommand curl_test_client("curl_test_client", recurl_f);
#endif
#ifdef GAME_DLL
ConCommand curl_test_server("curl_test_server", recurl_f);
#endif

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
    DevMsg(2, "%s\n", curl_version());

    CURLcode ccode = {};
    curlSetSSL = false;
    curlInited = false;

    if (!curlSetSSL)
    {
        // use the system ssl certs
        CURLsslset sslset = curl_global_sslset(CURLSSLBACKEND_OPENSSL, NULL, NULL);
        if (sslset != CURLSSLSET_OK)
        {
            Warning("curl_global_sslset failed: %i\n", sslset);
            return false;
        }
        curlSetSSL = true;
    }

    if (!curlInited)
    {
        ccode = curl_global_init(CURL_GLOBAL_ALL);
        if (ccode != CURLE_OK)
        {
            Warning("curl_global_init() failed: %s\n", curl_easy_strerror(ccode));
            return false;
        }
        curlInited = true;
    }

    reqs.clear();

    return true;
}


#define setopt_errwrap(chand, opt, etc)                                 \
    ccode = curl_easy_setopt(chand, opt, etc);                          \
    if (ccode != CURLE_OK)                                              \
    {                                                                   \
        resp->failed    = true;                                         \
        resp->completed = true;                                         \
        const curl_easyoption* ezoptinfo = curl_easy_option_by_id(opt); \
        Warning("curl_easy_setopt (%s) failed: %s\n",                   \
        curl_easy_strerror(ccode), ezoptinfo->name);                    \
        curl_easy_cleanup(chand);                                       \
        return false;                                                   \
    }

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
    // If you're crashing around here it's because you didn't remove the old libs from client_base/server_base vpcs
    CURL* curl = {};
    curl = curl_easy_init();
    DevMsg(2, "curl ptr = %p\n", curl);

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

    // setopt_errwrap(curl, CURLOPT_DNS_SERVERS, "1.1.1.1,1.0.0.1,8.8.8.8,8.8.4.4");

    setopt_errwrap(curl, CURLOPT_DOH_URL, "https://cloudflare-dns.com/dns-query");

    setopt_errwrap(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_3);

    setopt_errwrap(curl, CURLOPT_URL, inURL.c_str());
    setopt_errwrap(curl, CURLOPT_FOLLOWLOCATION, 1L);

    // send all response data to this func
    setopt_errwrap(curl, CURLOPT_WRITEFUNCTION, response_callback);
    setopt_errwrap(curl, CURLOPT_WRITEDATA, resp);

    // send all headers to this func
    setopt_errwrap(curl, CURLOPT_HEADERFUNCTION, header_callback);
    setopt_errwrap(curl, CURLOPT_HEADERDATA, resp);

    /* enable progress meter */
    setopt_errwrap(curl, CURLOPT_NOPROGRESS, 0L);

    // setopt_errwrap(curl, CURLOPT_XFERINFODATA, canary);
    // setopt_errwrap(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);

    setopt_errwrap(curl, CURLOPT_USERAGENT, "sdk2013 CURL implementation by sappho.io - github.com/sapphonie/sdk13-sappholib");

    ccode = curl_easy_perform(curl);
    if (ccode != CURLE_OK)
    {
        Warning("curl_easy_perform() failed: %s\n", curl_easy_strerror(ccode));
        curl_easy_cleanup(curl);
        return false;
    }

    curl_easy_cleanup(curl);

    // helpers
    for (auto& thisheader : resp->headers)
    {
        if (thisheader.find("HTTP/") != std::string::npos)
        {
            std::vector<std::string> svec = UTIL_SplitSTDString(thisheader, " ");

            resp->httpType = std::string( svec.at(0) );
            resp->respCode = stoi(svec.at(1));
        }
        else if (thisheader.find("content-length") != std::string::npos)
        {
            resp->contentLen = std::string(thisheader);
        }
    }

    resp->bodyLen = resp->body.length();

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
            DevMsg(2, "CURL REQUEST SUCCESSFUL!\n");

            reqs.erase(reqs.begin() + i);
            delete thisReq;
            continue;
        }

        else if (!thisReq)
        {
            AssertMsg(0, "How the hell did we not get a curl request???");
            Warning("How the hell did we not get a curl request???");
            continue;
        }

        else if (thisReq->failed && thisReq->completed)
        {
            Warning("CURL REQUEST %p FAILED\n", thisReq);
            reqs.erase(reqs.begin() + i);
            continue;
        }
    }
}

#endif