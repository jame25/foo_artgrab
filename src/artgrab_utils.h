#pragma once
#include "stdafx.h"
#include <string>

namespace artgrab {
    std::string normalize_diacritics(const std::string& s);
    pfc::string8 url_encode(const char* str);
    bool is_valid_image_data(const t_uint8* data, size_t size);
    pfc::string8 detect_mime_type(const t_uint8* data, size_t size);
    pfc::string8 get_file_directory(const char* file_path);
}
