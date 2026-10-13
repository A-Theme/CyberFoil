#include "util/remote_connection.hpp"
#include <curl/curl.h>

namespace inst::remote {
    std::string ResolveEndpoint(const config::RemoteProfile& profile)
    {
        return ChooseEndpoint(profile, [](const std::string& url) {
            struct Handle {
                CURL* value = curl_easy_init();
                ~Handle() { if (value) curl_easy_cleanup(value); }
            } handle;
            if (!handle.value) return false;
            curl_easy_setopt(handle.value, CURLOPT_URL, url.c_str());
            curl_easy_setopt(handle.value, CURLOPT_CONNECT_ONLY, 1L);
            curl_easy_setopt(handle.value, CURLOPT_CONNECTTIMEOUT_MS, 1800L);
            curl_easy_setopt(handle.value, CURLOPT_TIMEOUT_MS, 2200L);
            curl_easy_setopt(handle.value, CURLOPT_NOSIGNAL, 1L);
            // Match the TLS policy of existing CyberFoil Remote requests. No
            // credentials or HTTP requests are sent during the connectivity check.
            curl_easy_setopt(handle.value, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(handle.value, CURLOPT_SSL_VERIFYHOST, 0L);
            return curl_easy_perform(handle.value) == CURLE_OK;
        });
    }
}
