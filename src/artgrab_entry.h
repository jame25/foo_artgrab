#pragma once
#include "stdafx.h"
#include <string>

namespace artgrab {
    pfc::string8 get_effective_lastfm_key();
    pfc::string8 get_effective_discogs_key();
    pfc::string8 get_effective_discogs_consumer_key();
    pfc::string8 get_effective_discogs_consumer_secret();
    bool is_api_enabled(const char* api_name);
    void initialize();
    void shutdown();
}
