#pragma once
#include "stdafx.h"

// cfg_var declarations used by other modules
extern cfg_string cfg_ag_save_filename;
extern cfg_int cfg_ag_overwrite;
extern cfg_int cfg_ag_jpeg_quality;
extern cfg_int cfg_ag_max_results;
extern cfg_int cfg_ag_http_timeout;
extern cfg_int cfg_ag_retry_count;

// Cache folder cfg_var referenced by async_io_manager
extern cfg_string cfg_cache_folder;
extern cfg_bool cfg_ag_include_back_covers;
extern cfg_bool cfg_ag_include_artist_images;
extern cfg_string cfg_ag_back_cover_filename;
extern cfg_string cfg_ag_artist_image_filename;
extern cfg_int cfg_ag_save_format;
