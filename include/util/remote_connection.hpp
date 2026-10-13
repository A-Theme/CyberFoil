#pragma once
#include "util/config.hpp"
#include <functional>

namespace inst::remote {
    // The probe checks connectivity only. Authentication and catalogue errors
    // belong to the normal Remote request, and never trigger endpoint switching.
    inline std::string ChooseEndpoint(const config::RemoteProfile& profile,
        const std::function<bool(const std::string&)>& canConnect)
    {
        if (profile.connection != "auto" || profile.lanUrl.empty() || profile.wanUrl.empty())
            return config::BuildRemoteUrl(profile);
        return canConnect(profile.lanUrl) ? profile.lanUrl : profile.wanUrl;
    }
    std::string ResolveEndpoint(const config::RemoteProfile& profile);
}
