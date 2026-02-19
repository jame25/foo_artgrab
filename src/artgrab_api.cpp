#include "stdafx.h"
#include "artgrab_api.h"
#include <algorithm>
#include <set>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

// API key getters (implemented in artgrab_entry.cpp, in artgrab:: namespace)
#include "artgrab_entry.h"

// Global-namespace wrappers for use in this file
static pfc::string8 get_effective_lastfm_key() { return artgrab::get_effective_lastfm_key(); }
static pfc::string8 get_effective_discogs_key() { return artgrab::get_effective_discogs_key(); }
static pfc::string8 get_effective_discogs_consumer_key() { return artgrab::get_effective_discogs_consumer_key(); }
static pfc::string8 get_effective_discogs_consumer_secret() { return artgrab::get_effective_discogs_consumer_secret(); }

// --- String matching helpers (from foo_artwork's artwork_manager.cpp) ---

static std::string normalize_for_matching(const std::string& s) {
    std::string diacritic_normalized = artgrab::normalize_diacritics(s);
    std::string result;
    result.reserve(diacritic_normalized.size());
    for (size_t i = 0; i < diacritic_normalized.size(); ++i) {
        char c = diacritic_normalized[i];
        if (c == '.' || c == ',' || c == '\'' || c == '!' || c == '?' || c == '-') continue;
        result += std::tolower(static_cast<unsigned char>(c));
    }
    // Normalize " and " and " & " to single space
    std::string normalized;
    normalized.reserve(result.size());
    for (size_t i = 0; i < result.size(); ++i) {
        if (i + 4 < result.size() && result[i] == ' ' && result[i+1] == 'a' && result[i+2] == 'n' && result[i+3] == 'd' && result[i+4] == ' ') {
            normalized += ' '; i += 4; continue;
        }
        if (i + 2 < result.size() && result[i] == ' ' && result[i+1] == '&' && result[i+2] == ' ') {
            normalized += ' '; i += 2; continue;
        }
        normalized += result[i];
    }
    // Collapse spaces, trim
    result.clear();
    bool last_was_space = false;
    for (char c : normalized) {
        if (c == ' ') { if (!last_was_space) { result += c; last_was_space = true; } }
        else { result += c; last_was_space = false; }
    }
    size_t start = result.find_first_not_of(' ');
    if (start == std::string::npos) return "";
    size_t end = result.find_last_not_of(' ');
    return result.substr(start, end - start + 1);
}

static bool strings_equal_ignore_case(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) return false;
    }
    return true;
}

static bool strings_match_fuzzy(const std::string& a, const std::string& b) {
    if (a.size() == b.size() && strings_equal_ignore_case(a, b)) return true;
    return normalize_for_matching(a) == normalize_for_matching(b);
}

static std::string strip_the_prefix(const std::string& s) {
    if (s.size() > 4 && (s[0]=='T'||s[0]=='t') && (s[1]=='H'||s[1]=='h') && (s[2]=='E'||s[2]=='e') && s[3]==' ')
        return s.substr(4);
    return s;
}

static bool artists_match(const std::string& a, const std::string& b) {
    if (strings_equal_ignore_case(a, b)) return true;
    if (strings_match_fuzzy(a, b)) return true;
    std::string a_s = strip_the_prefix(a), b_s = strip_the_prefix(b);
    if (strings_equal_ignore_case(a_s, b_s)) return true;
    return strings_match_fuzzy(a_s, b_s);
}

// Helper to unescape JSON slashes
static pfc::string8 unescape_json_slashes(const pfc::string8& url) {
    pfc::string8 result;
    const char* src = url.get_ptr();
    while (*src) {
        if (*src == '\\' && *(src + 1) == '/') { result += "/"; src += 2; }
        else { char c[2] = { *src, '\0' }; result += c; src++; }
    }
    return result;
}

// Helper to extract iTunes artwork URL with resolution upgrade
static pfc::string8 get_itunes_artwork_url(const json& item) {
    pfc::string8 url;
    if (item.contains("artworkUrl600")) url = item["artworkUrl600"].get<std::string>().c_str();
    else if (item.contains("artworkUrl512")) url = item["artworkUrl512"].get<std::string>().c_str();
    else if (item.contains("artworkUrl100")) url = item["artworkUrl100"].get<std::string>().c_str();
    else if (item.contains("artworkUrl60")) url = item["artworkUrl60"].get<std::string>().c_str();
    else if (item.contains("artworkUrl30")) url = item["artworkUrl30"].get<std::string>().c_str();
    else return pfc::string8();

    // Upgrade to 1200x1200
    url.replace_string("600x600", "1200x1200");
    url.replace_string("512x512", "1200x1200");
    url.replace_string("100x100", "1200x1200");
    url.replace_string("60x60", "1200x1200");
    url.replace_string("30x30", "1200x1200");

    // Quality suffix
    if (url.find_first(".png") != pfc_infinite) {
        if (url.find_first("bb.png") != pfc_infinite) url.replace_string("bb.png", "bb-80.png");
        else if (url.find_first("bf.png") != pfc_infinite) url.replace_string("bf.png", "bb-80.png");
    } else if (url.find_first(".jpg") != pfc_infinite || url.find_first(".jpeg") != pfc_infinite) {
        if (url.find_first("bb.jpg") != pfc_infinite) url.replace_string("bb.jpg", "bb-90.jpg");
        else if (url.find_first("bf.jpg") != pfc_infinite) url.replace_string("bf.jpg", "bb-90.jpg");
    }

    return url;
}

// ============================================================================
// artwork_search implementation
// ============================================================================

namespace artgrab {

artwork_search::artwork_search(const char* artist, const char* album, int max_results_per_api,
    on_result_callback on_result, on_api_done_callback on_api_done, on_all_done_callback on_all_done)
    : m_artist(artist), m_album(album), m_max_results(max_results_per_api),
      m_on_result(on_result), m_on_api_done(on_api_done), m_on_all_done(on_all_done),
      m_apis_remaining(0), m_cancelled(false) {}

void artwork_search::start() {
    struct api_entry { const char* name; void (artwork_search::*fn)(); };
    api_entry apis[] = {
        { "iTunes",      &artwork_search::search_itunes },
        { "Deezer",      &artwork_search::search_deezer },
        { "Last.fm",     &artwork_search::search_lastfm },
        { "MusicBrainz", &artwork_search::search_musicbrainz },
        { "Discogs",     &artwork_search::search_discogs },
    };

    // Count enabled APIs first
    int count = 0;
    for (auto& api : apis) {
        if (artgrab::is_api_enabled(api.name)) count++;
    }

    if (count == 0) {
        // No APIs enabled -- signal immediate completion
        m_apis_remaining = 0;
        m_on_all_done();
        return;
    }

    m_apis_remaining = count;
    for (auto& api : apis) {
        if (artgrab::is_api_enabled(api.name)) {
            (this->*api.fn)();
        }
    }
}

void artwork_search::cancel() { m_cancelled = true; }

void artwork_search::api_finished(const char* api_name, bool had_results) {
    if (!m_cancelled) {
        m_on_api_done(api_name, had_results);
    }
    if (--m_apis_remaining == 0) {
        // Always fire on_all_done when all APIs finish, even after cancellation.
        // The caller needs this to know all async work is done (for cleanup/lifetime).
        if (!m_cancelled) {
            m_on_all_done();
        }
    }
}

// ============================================================================
// download_and_deliver -- Downloads an image, gets dimensions via GDI+, calls on_result
// ============================================================================

void artwork_search::download_and_deliver(const char* url, const char* source,
    std::shared_ptr<std::atomic<int>> pending_downloads,
    std::shared_ptr<std::atomic<bool>> had_any_results,
    const char* api_name) {
    pfc::string8 url_copy = url;
    pfc::string8 source_copy = source;
    pfc::string8 api_copy = api_name;
    auto self = shared_from_this();

    async_io_manager::instance().http_get_binary_async(url_copy,
        [self, url_copy, source_copy, pending_downloads, had_any_results, api_copy](bool success, const pfc::array_t<t_uint8>& data, const pfc::string8& error) {
            if (!self->m_cancelled && success && data.get_size() > 0 && artgrab::is_valid_image_data(data.get_ptr(), data.get_size())) {
                artwork_entry entry;
                entry.data = data;
                entry.source = source_copy;
                entry.image_url = url_copy;
                entry.mime_type = artgrab::detect_mime_type(data.get_ptr(), data.get_size());

                // Get dimensions via GDI+
                IStream* stream = SHCreateMemStream(data.get_ptr(), (UINT)data.get_size());
                if (stream) {
                    Gdiplus::Image img(stream);
                    if (img.GetLastStatus() == Gdiplus::Ok) {
                        entry.width = img.GetWidth();
                        entry.height = img.GetHeight();
                    }
                    stream->Release();
                }

                self->m_on_result(entry);
                had_any_results->store(true);
            }

            // When all downloads for this API are done, signal api_finished
            int remaining = --(*pending_downloads);
            if (remaining == 0) {
                self->api_finished(api_copy.get_ptr(), had_any_results->load());
            }
        });
}

// ============================================================================
// search_itunes -- iTunes Search API (no key required)
// ============================================================================

void artwork_search::search_itunes() {
    pfc::string8 url = "https://itunes.apple.com/search?term=";
    url << artgrab::url_encode(m_artist) << "+" << artgrab::url_encode(m_album);
    url << "&entity=album&limit=10";

    pfc::string8 artist_str = m_artist;
    pfc::string8 album_str = m_album;
    int max_results = m_max_results;

    auto self = shared_from_this();
    async_io_manager::instance().http_get_async(url,
        [self, artist_str, album_str, max_results](bool success, const pfc::string8& response, const pfc::string8& error) {
            if (self->m_cancelled) { self->api_finished("iTunes", false); return; }

            if (!success) {
                self->api_finished("iTunes", false);
                return;
            }

            std::vector<pfc::string8> urls;
            if (!parse_itunes_json_multi(artist_str, album_str, response, urls, max_results) || urls.empty()) {
                self->api_finished("iTunes", false);
                return;
            }

            auto pending = std::make_shared<std::atomic<int>>((int)urls.size());
            auto had_any = std::make_shared<std::atomic<bool>>(false);
            for (const auto& img_url : urls) {
                self->download_and_deliver(img_url.get_ptr(), "iTunes", pending, had_any, "iTunes");
            }
        });
}

// ============================================================================
// search_deezer -- Deezer API (no key required)
// ============================================================================

void artwork_search::search_deezer() {
    pfc::string8 search_query;
    search_query << "artist:\"" << m_artist << "\" track:\"" << m_album << "\"";

    pfc::string8 url = "https://api.deezer.com/search?q=";
    url << artgrab::url_encode(search_query) << "&limit=15";

    pfc::string8 artist_str = m_artist;
    pfc::string8 album_str = m_album;
    int max_results = m_max_results;

    auto self = shared_from_this();
    async_io_manager::instance().http_get_async(url,
        [self, artist_str, album_str, max_results](bool success, const pfc::string8& response, const pfc::string8& error) {
            if (self->m_cancelled) { self->api_finished("Deezer", false); return; }

            if (!success) {
                self->api_finished("Deezer", false);
                return;
            }

            std::vector<pfc::string8> urls;
            if (!parse_deezer_json_multi(artist_str, album_str, response, urls, max_results) || urls.empty()) {
                self->api_finished("Deezer", false);
                return;
            }

            auto pending = std::make_shared<std::atomic<int>>((int)urls.size());
            auto had_any = std::make_shared<std::atomic<bool>>(false);
            for (const auto& img_url : urls) {
                self->download_and_deliver(img_url.get_ptr(), "Deezer", pending, had_any, "Deezer");
            }
        });
}

// ============================================================================
// search_lastfm -- Last.fm API (requires key)
// ============================================================================

void artwork_search::search_lastfm() {
    pfc::string8 key = get_effective_lastfm_key();
    if (key.is_empty()) {
        api_finished("Last.fm", false);
        return;
    }

    pfc::string8 url = "http://ws.audioscrobbler.com/2.0/?method=track.getinfo&api_key=";
    url << artgrab::url_encode(key) << "&artist=" << artgrab::url_encode(m_artist);
    url << "&track=" << artgrab::url_encode(m_album);
    url << "&autocorrect=1&format=json";

    int max_results = m_max_results;

    auto self = shared_from_this();
    async_io_manager::instance().http_get_async(url,
        [self, max_results](bool success, const pfc::string8& response, const pfc::string8& error) {
            if (self->m_cancelled) { self->api_finished("Last.fm", false); return; }

            if (!success) {
                self->api_finished("Last.fm", false);
                return;
            }

            std::vector<pfc::string8> urls;
            if (!parse_lastfm_json_multi(response, urls, max_results) || urls.empty()) {
                self->api_finished("Last.fm", false);
                return;
            }

            auto pending = std::make_shared<std::atomic<int>>((int)urls.size());
            auto had_any = std::make_shared<std::atomic<bool>>(false);
            for (const auto& img_url : urls) {
                self->download_and_deliver(img_url.get_ptr(), "Last.fm", pending, had_any, "Last.fm");
            }
        });
}

// ============================================================================
// search_musicbrainz -- MusicBrainz + CoverArtArchive (no key required)
// ============================================================================

void artwork_search::search_musicbrainz() {
    pfc::string8 search_query;
    search_query << "artist:\"" << m_artist << "\" AND recording:\"" << m_album << "\"";

    pfc::string8 url = "http://musicbrainz.org/ws/2/recording/?query=";
    url << artgrab::url_encode(search_query);
    url << "&fmt=json&limit=10&inc=releases";

    pfc::string8 artist_str = m_artist;
    int max_results = m_max_results;

    auto self = shared_from_this();
    async_io_manager::instance().http_get_async(url,
        [self, artist_str, max_results](bool success, const pfc::string8& response, const pfc::string8& error) {
            if (self->m_cancelled) { self->api_finished("MusicBrainz", false); return; }

            if (!success) {
                self->api_finished("MusicBrainz", false);
                return;
            }

            std::vector<pfc::string8> release_ids;
            if (!parse_musicbrainz_json_multi(response, release_ids, artist_str.get_ptr(), max_results) || release_ids.empty()) {
                self->api_finished("MusicBrainz", false);
                return;
            }

            // Each release ID becomes a CoverArtArchive download
            auto pending = std::make_shared<std::atomic<int>>((int)release_ids.size());
            auto had_any = std::make_shared<std::atomic<bool>>(false);
            for (const auto& id : release_ids) {
                pfc::string8 coverart_url = "http://coverartarchive.org/release/";
                coverart_url << id << "/front";
                self->download_and_deliver(coverart_url.get_ptr(), "MusicBrainz", pending, had_any, "MusicBrainz");
            }
        });
}

// ============================================================================
// search_discogs -- Discogs API (requires token or consumer key+secret)
// ============================================================================

void artwork_search::search_discogs() {
    pfc::string8 token = get_effective_discogs_key();
    pfc::string8 consumer_key = get_effective_discogs_consumer_key();
    pfc::string8 consumer_secret = get_effective_discogs_consumer_secret();

    bool has_token = !token.is_empty();
    bool has_consumer_creds = !consumer_key.is_empty() && !consumer_secret.is_empty();

    if (!has_token && !has_consumer_creds) {
        api_finished("Discogs", false);
        return;
    }

    pfc::string8 search_query;
    search_query << m_artist << " " << m_album;

    pfc::string8 url = "https://api.discogs.com/database/search?q=";
    url << artgrab::url_encode(search_query);
    url << "&type=release";

    if (has_token) {
        url << "&token=" << artgrab::url_encode(token);
    } else {
        url << "&key=" << artgrab::url_encode(consumer_key);
        url << "&secret=" << artgrab::url_encode(consumer_secret);
    }

    pfc::string8 artist_str = m_artist;
    pfc::string8 album_str = m_album;
    int max_results = m_max_results;

    auto self = shared_from_this();
    async_io_manager::instance().http_get_async(url,
        [self, artist_str, album_str, max_results](bool success, const pfc::string8& response, const pfc::string8& error) {
            if (self->m_cancelled) { self->api_finished("Discogs", false); return; }

            if (!success) {
                self->api_finished("Discogs", false);
                return;
            }

            std::vector<pfc::string8> urls;
            if (!parse_discogs_json_multi(artist_str, album_str, response, urls, max_results) || urls.empty()) {
                self->api_finished("Discogs", false);
                return;
            }

            auto pending = std::make_shared<std::atomic<int>>((int)urls.size());
            auto had_any = std::make_shared<std::atomic<bool>>(false);
            for (const auto& img_url : urls) {
                self->download_and_deliver(img_url.get_ptr(), "Discogs", pending, had_any, "Discogs");
            }
        });
}

// ============================================================================
// Multi-result JSON parsers
// ============================================================================

bool artwork_search::parse_itunes_json_multi(const char* artist, const char* album,
    const pfc::string8& json_in, std::vector<pfc::string8>& urls, int max_results) {
    try {
        std::string json_data(json_in.get_ptr());
        json data = json::parse(json_data);

        if (!data.contains("resultCount") || data["resultCount"].get<int>() == 0)
            return false;

        json results = data["results"];
        std::string artist_str(artist);
        std::string album_str(album);
        std::set<std::string> seen_urls;

        // Pass 1: exact artist+album match
        for (const auto& item : results) {
            if ((int)urls.size() >= max_results) break;

            // For album entity, use collectionName; for song entity, use trackName
            std::string result_name;
            if (item.contains("collectionName"))
                result_name = item["collectionName"].get<std::string>();
            else if (item.contains("trackName"))
                result_name = item["trackName"].get<std::string>();
            else
                continue;

            std::string result_artist = item.contains("artistName") ? item["artistName"].get<std::string>() : "";

            if (strings_match_fuzzy(result_name, album_str) && artists_match(result_artist, artist_str)) {
                pfc::string8 art_url = get_itunes_artwork_url(item);
                if (!art_url.is_empty()) {
                    std::string url_key(art_url.get_ptr());
                    if (seen_urls.find(url_key) == seen_urls.end()) {
                        seen_urls.insert(url_key);
                        urls.push_back(art_url);
                    }
                }
            }
        }

        // Pass 2: artist-only fallback
        for (const auto& item : results) {
            if ((int)urls.size() >= max_results) break;

            std::string result_artist = item.contains("artistName") ? item["artistName"].get<std::string>() : "";
            if (!artists_match(result_artist, artist_str)) continue;

            pfc::string8 art_url = get_itunes_artwork_url(item);
            if (!art_url.is_empty()) {
                std::string url_key(art_url.get_ptr());
                if (seen_urls.find(url_key) == seen_urls.end()) {
                    seen_urls.insert(url_key);
                    urls.push_back(art_url);
                }
            }
        }

        return !urls.empty();
    } catch (const std::exception&) {
        return false;
    }
}

bool artwork_search::parse_deezer_json_multi(const char* artist, const char* album,
    const pfc::string8& json_in, std::vector<pfc::string8>& urls, int max_results) {
    try {
        std::string json_data(json_in.get_ptr());
        json data = json::parse(json_data);

        if (!data.contains("total") || data["total"].get<int>() == 0)
            return false;

        // Sort by rank descending
        std::sort(data["data"].begin(), data["data"].end(),
            [](const json& a, const json& b) {
                return a["rank"].get<int>() > b["rank"].get<int>();
            });

        json results = data["data"];
        std::string artist_str(artist);
        std::string album_str(album);
        std::set<std::string> seen_urls;

        // Pass 1: exact artist+title match
        for (const auto& item : results) {
            if ((int)urls.size() >= max_results) break;

            std::string result_title = item["title"].get<std::string>();
            std::string result_artist = item["artist"]["name"].get<std::string>();

            if (strings_match_fuzzy(result_title, album_str) && artists_match(result_artist, artist_str)) {
                pfc::string8 art_url;
                if (item.contains("album") && item["album"].contains("cover_xl")) {
                    art_url = item["album"]["cover_xl"].get<std::string>().c_str();
                } else if (item.contains("album") && item["album"].contains("cover_big")) {
                    art_url = item["album"]["cover_big"].get<std::string>().c_str();
                }

                if (!art_url.is_empty()) {
                    art_url = unescape_json_slashes(art_url);
                    art_url.replace_string("1000x1000", "1200x1200");

                    std::string url_key(art_url.get_ptr());
                    if (seen_urls.find(url_key) == seen_urls.end()) {
                        seen_urls.insert(url_key);
                        urls.push_back(art_url);
                    }
                }
            }
        }

        // Pass 2: artist-only fallback
        for (const auto& item : results) {
            if ((int)urls.size() >= max_results) break;

            std::string result_artist = item["artist"]["name"].get<std::string>();
            if (!artists_match(result_artist, artist_str)) continue;

            pfc::string8 art_url;
            if (item.contains("album") && item["album"].contains("cover_xl")) {
                art_url = item["album"]["cover_xl"].get<std::string>().c_str();
            } else if (item.contains("album") && item["album"].contains("cover_big")) {
                art_url = item["album"]["cover_big"].get<std::string>().c_str();
            }

            if (!art_url.is_empty()) {
                art_url = unescape_json_slashes(art_url);
                art_url.replace_string("1000x1000", "1200x1200");

                std::string url_key(art_url.get_ptr());
                if (seen_urls.find(url_key) == seen_urls.end()) {
                    seen_urls.insert(url_key);
                    urls.push_back(art_url);
                }
            }
        }

        return !urls.empty();
    } catch (const std::exception&) {
        return false;
    }
}

bool artwork_search::parse_lastfm_json_multi(const pfc::string8& json_in,
    std::vector<pfc::string8>& urls, int max_results) {
    try {
        std::string json_data(json_in.get_ptr());
        json data = json::parse(json_data);

        if (data.contains("message") && data["message"].get<std::string>() == "Track not found")
            return false;

        if (!data.contains("track") || !data["track"].contains("album") || !data["track"]["album"].contains("image"))
            return false;

        json images = data["track"]["album"]["image"];

        // Last.fm track.getinfo only returns one set of images (different sizes of the same image).
        // We take the best available -- max 1 result from this API.
        // Try extralarge first, then large
        for (const auto& item : images) {
            if (item["size"].get<std::string>() == "extralarge") {
                pfc::string8 art_url = item["#text"].get<std::string>().c_str();
                if (!art_url.is_empty()) {
                    // Upgrade resolution: remove size constraint for original quality
                    art_url.replace_string("u/300x300", "u/");
                    urls.push_back(art_url);
                    return true;
                }
            }
        }

        for (const auto& item : images) {
            if (item["size"].get<std::string>() == "large") {
                pfc::string8 art_url = item["#text"].get<std::string>().c_str();
                if (!art_url.is_empty()) {
                    art_url.replace_string("u/174s", "u/");
                    urls.push_back(art_url);
                    return true;
                }
            }
        }

        return false;
    } catch (const std::exception&) {
        return false;
    }
}

bool artwork_search::parse_discogs_json_multi(const char* artist, const char* album,
    const pfc::string8& json_in, std::vector<pfc::string8>& urls, int max_results) {
    try {
        std::string json_data(json_in.get_ptr());
        json data = json::parse(json_data);

        if (!data.contains("pagination") || data["pagination"]["items"].get<int>() == 0)
            return false;

        json results = data["results"];
        std::string artist_str(artist);
        std::string album_str(album);
        std::set<std::string> seen_urls;

        // Discogs title format: "Artist - Album"
        std::string artist_title = artist_str + " - " + album_str;

        // Pass 1: exact artist+title match
        for (const auto& item : results) {
            if ((int)urls.size() >= max_results) break;

            std::string result_title = item["title"].get<std::string>();
            if (strings_match_fuzzy(result_title, artist_title)) {
                pfc::string8 art_url;
                if (item.contains("cover_image") && !item["cover_image"].get<std::string>().empty()) {
                    art_url = item["cover_image"].get<std::string>().c_str();
                } else if (item.contains("thumb") && !item["thumb"].get<std::string>().empty()) {
                    art_url = item["thumb"].get<std::string>().c_str();
                }

                if (!art_url.is_empty()) {
                    std::string url_key(art_url.get_ptr());
                    if (seen_urls.find(url_key) == seen_urls.end()) {
                        seen_urls.insert(url_key);
                        urls.push_back(art_url);
                    }
                }
            }
        }

        // Pass 2: artist-only fallback (title starts with artist name)
        for (const auto& item : results) {
            if ((int)urls.size() >= max_results) break;

            std::string result_title = item["title"].get<std::string>();

            // Check if title starts with artist name
            bool artist_matches = false;
            std::string artist_stripped = strip_the_prefix(artist_str);
            std::string title_stripped = strip_the_prefix(result_title);

            if (result_title.size() >= artist_str.size()) {
                artist_matches = strings_equal_ignore_case(
                    result_title.substr(0, artist_str.size()), artist_str);
            }
            if (!artist_matches && title_stripped.size() >= artist_stripped.size()) {
                artist_matches = strings_equal_ignore_case(
                    title_stripped.substr(0, artist_stripped.size()), artist_stripped);
            }

            if (!artist_matches) continue;

            pfc::string8 art_url;
            if (item.contains("cover_image") && !item["cover_image"].get<std::string>().empty()) {
                art_url = item["cover_image"].get<std::string>().c_str();
            } else if (item.contains("thumb") && !item["thumb"].get<std::string>().empty()) {
                art_url = item["thumb"].get<std::string>().c_str();
            }

            if (!art_url.is_empty()) {
                std::string url_key(art_url.get_ptr());
                if (seen_urls.find(url_key) == seen_urls.end()) {
                    seen_urls.insert(url_key);
                    urls.push_back(art_url);
                }
            }
        }

        return !urls.empty();
    } catch (const std::exception&) {
        return false;
    }
}

bool artwork_search::parse_musicbrainz_json_multi(const pfc::string8& json_in,
    std::vector<pfc::string8>& release_ids, const char* artist, int max_results) {
    try {
        std::string json_data(json_in.get_ptr());
        json data = json::parse(json_data);

        if (!data.contains("recordings") || !data.contains("count") || data["count"].get<int>() == 0)
            return false;

        std::string artist_str(artist);
        std::set<std::string> seen_ids;

        for (const auto& rec : data["recordings"]) {
            if ((int)release_ids.size() >= max_results) break;

            // Check if recording's artist-credit matches the requested artist
            bool artist_matches = false;
            if (rec.contains("artist-credit")) {
                for (const auto& ac : rec["artist-credit"]) {
                    if (ac.contains("name") && ac["name"].is_string()) {
                        std::string credit_name = ac["name"].get<std::string>();
                        if (artists_match(credit_name, artist_str)) {
                            artist_matches = true;
                            break;
                        }
                    }
                    if (ac.contains("artist") && ac["artist"].contains("name")) {
                        std::string nested_name = ac["artist"]["name"].get<std::string>();
                        if (artists_match(nested_name, artist_str)) {
                            artist_matches = true;
                            break;
                        }
                    }
                }
            }

            if (!artist_matches) continue;

            if (!rec.contains("releases")) continue;
            for (const auto& rel : rec["releases"]) {
                if ((int)release_ids.size() >= max_results) break;

                if (rel.contains("id") && rel["id"].is_string()) {
                    std::string id = rel["id"].get<std::string>();
                    if (seen_ids.find(id) == seen_ids.end()) {
                        seen_ids.insert(id);
                        release_ids.push_back(id.c_str());
                    }
                }
            }
        }

        return !release_ids.empty();
    } catch (const std::exception&) {
        return false;
    }
}

} // namespace artgrab
