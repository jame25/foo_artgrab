#pragma once
#include "stdafx.h"
#include "async_io_manager.h"
#include "artgrab_utils.h"
#include <functional>
#include <vector>
#include <string>
#include <atomic>
#include <mutex>
#include <nlohmann/json.hpp>

namespace artgrab {

struct artwork_entry {
    pfc::array_t<t_uint8> data;
    pfc::string8 source;        // "iTunes", "Deezer", etc.
    pfc::string8 image_url;
    int width;
    int height;
    pfc::string8 mime_type;
    artwork_entry() : width(0), height(0) {}
};

typedef std::function<void(const artwork_entry& entry)> on_result_callback;
typedef std::function<void(const char* api_name, bool had_results)> on_api_done_callback;
typedef std::function<void()> on_all_done_callback;

class artwork_search : public std::enable_shared_from_this<artwork_search> {
public:
    artwork_search(
        const char* artist,
        const char* album,
        int max_results_per_api,
        bool include_back_covers,
        bool include_artist_images,
        on_result_callback on_result,
        on_api_done_callback on_api_done,
        on_all_done_callback on_all_done
    );

    void start();
    void cancel();

private:
    pfc::string8 m_artist;
    pfc::string8 m_album;
    int m_max_results;
    on_result_callback m_on_result;
    on_api_done_callback m_on_api_done;
    on_all_done_callback m_on_all_done;
    std::atomic<int> m_apis_remaining;
    std::atomic<bool> m_cancelled;
    bool m_include_back_covers;
    bool m_include_artist_images;

    void search_itunes();
    void search_deezer();
    void search_lastfm();
    void search_musicbrainz();
    void search_discogs();
    void fetch_back_covers(const std::vector<pfc::string8>& release_ids);
    void fetch_discogs_back_covers(const std::vector<pfc::string8>& release_ids);
    void fetch_artist_images(const std::vector<pfc::string8>& artist_ids);

    void api_finished(const char* api_name, bool had_results);
    void download_and_deliver(const char* url, const char* source, std::shared_ptr<std::atomic<int>> pending_downloads, std::shared_ptr<std::atomic<bool>> had_any_results, const char* api_name);

    // JSON parsing -- returns multiple URLs instead of single
    static bool parse_itunes_json_multi(const char* artist, const char* album,
        const pfc::string8& json, std::vector<pfc::string8>& urls, int max_results);
    static bool parse_deezer_json_multi(const char* artist, const char* album,
        const pfc::string8& json, std::vector<pfc::string8>& urls, int max_results,
        std::vector<pfc::string8>* artist_ids = nullptr);
    static bool parse_lastfm_json_multi(const pfc::string8& json,
        std::vector<pfc::string8>& urls, int max_results);
    static bool parse_discogs_json_multi(const char* artist, const char* album,
        const pfc::string8& json, std::vector<pfc::string8>& urls, int max_results,
        std::vector<pfc::string8>* release_ids = nullptr);
    static bool parse_musicbrainz_json_multi(const pfc::string8& json,
        std::vector<pfc::string8>& release_ids, const char* artist, int max_results);
};

} // namespace artgrab
