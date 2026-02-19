#include "stdafx.h"
#include "artgrab_utils.h"
#include <string>
#include <cstring>

namespace artgrab {

std::string normalize_diacritics(const std::string& s) {
    std::string result;
    result.reserve(s.size());

    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);

        // Check for UTF-8 two-byte sequences starting with 0xC3 (Latin-1 Supplement)
        if (c == 0xC3 && i + 1 < s.size()) {
            unsigned char next = static_cast<unsigned char>(s[i + 1]);
            char replacement = 0;

            // Uppercase variants (0xC3 0x80-0x9F)
            if (next >= 0x80 && next <= 0x85) replacement = 'A';       // A A A A A A
            else if (next == 0x86) { result += "AE"; i++; continue; }  // AE
            else if (next == 0x87) replacement = 'C';                  // C
            else if (next >= 0x88 && next <= 0x8B) replacement = 'E';  // E E E E
            else if (next >= 0x8C && next <= 0x8F) replacement = 'I';  // I I I I
            else if (next == 0x90) replacement = 'D';                  // D
            else if (next == 0x91) replacement = 'N';                  // N
            else if (next >= 0x92 && next <= 0x96) replacement = 'O';  // O O O O O
            else if (next == 0x98) replacement = 'O';                  // O
            else if (next >= 0x99 && next <= 0x9C) replacement = 'U';  // U U U U
            else if (next == 0x9D) replacement = 'Y';                  // Y
            // Lowercase variants (0xC3 0xA0-0xBF)
            else if (next >= 0xA0 && next <= 0xA5) replacement = 'a';  // a a a a a a
            else if (next == 0xA6) { result += "ae"; i++; continue; }  // ae
            else if (next == 0xA7) replacement = 'c';                  // c
            else if (next >= 0xA8 && next <= 0xAB) replacement = 'e';  // e e e e
            else if (next >= 0xAC && next <= 0xAF) replacement = 'i';  // i i i i
            else if (next == 0xB0) replacement = 'd';                  // d
            else if (next == 0xB1) replacement = 'n';                  // n
            else if (next >= 0xB2 && next <= 0xB6) replacement = 'o';  // o o o o o
            else if (next == 0xB8) replacement = 'o';                  // o
            else if (next >= 0xB9 && next <= 0xBC) replacement = 'u';  // u u u u
            else if (next == 0xBD || next == 0xBF) replacement = 'y';  // y y
            else if (next == 0x9F) { result += "ss"; i++; continue; }  // ss (German eszett)

            if (replacement) {
                result += replacement;
                i++;  // Skip the second byte
                continue;
            }
        }

        // Check for UTF-8 two-byte sequences starting with 0xC5 (Latin Extended-A)
        if (c == 0xC5 && i + 1 < s.size()) {
            unsigned char next = static_cast<unsigned char>(s[i + 1]);
            char replacement = 0;

            if (next == 0x92 || next == 0x93) {  // OE oe
                result += (next == 0x92) ? "OE" : "oe";
                i++;
                continue;
            }
            else if (next == 0xA0 || next == 0xA1) replacement = (next == 0xA0) ? 'S' : 's';  // S s
            else if (next == 0xBD || next == 0xBE) replacement = (next == 0xBD) ? 'Z' : 'z';  // Z z

            if (replacement) {
                result += replacement;
                i++;
                continue;
            }
        }

        // Pass through other characters unchanged
        result += s[i];
    }

    return result;
}

pfc::string8 url_encode(const char* str) {
    pfc::string8 result;

    for (const char* p = str; *p; ++p) {
        char c = *p;
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            result.add_char(c);
        } else if (c == ' ') {
            result << "+";
        } else {
            result << "%" << pfc::format_hex((unsigned char)c, 2);
        }
    }

    return result;
}

bool is_valid_image_data(const t_uint8* data, size_t size) {
    if (size < 4) return false;

    // Check for common image format signatures
    // JPEG
    if (data[0] == 0xFF && data[1] == 0xD8) return true;

    // PNG
    if (data[0] == 0x89 && data[1] == 'P' && data[2] == 'N' && data[3] == 'G') return true;

    // GIF
    if (size >= 6 && memcmp(data, "GIF87a", 6) == 0) return true;
    if (size >= 6 && memcmp(data, "GIF89a", 6) == 0) return true;

    // BMP
    if (data[0] == 'B' && data[1] == 'M') return true;

    // WebP (RIFF....WEBP)
    if (size >= 12 &&
        data[0] == 'R' && data[1] == 'I' && data[2] == 'F' && data[3] == 'F' &&
        data[8] == 'W' && data[9] == 'E' && data[10] == 'B' && data[11] == 'P') return true;

    return false;
}

pfc::string8 detect_mime_type(const t_uint8* data, size_t size) {
    if (size < 4) return "application/octet-stream";

    // JPEG
    if (data[0] == 0xFF && data[1] == 0xD8) return "image/jpeg";

    // PNG
    if (data[0] == 0x89 && data[1] == 'P' && data[2] == 'N' && data[3] == 'G') return "image/png";

    // WebP (RIFF....WEBP)
    if (size >= 12 &&
        data[0] == 'R' && data[1] == 'I' && data[2] == 'F' && data[3] == 'F' &&
        data[8] == 'W' && data[9] == 'E' && data[10] == 'B' && data[11] == 'P') return "image/webp";

    // GIF
    if (size >= 6 && (memcmp(data, "GIF87a", 6) == 0 || memcmp(data, "GIF89a", 6) == 0)) return "image/gif";

    // BMP
    if (data[0] == 'B' && data[1] == 'M') return "image/bmp";

    return "application/octet-stream";
}

pfc::string8 get_file_directory(const char* file_path) {
    pfc::string8 directory = file_path;

    // Remove file:// prefix if present
    if (directory.find_first("file://") == 0) {
        directory = directory.get_ptr() + 7; // Remove "file://" by getting substring from position 7
    }

    // Find last backslash or forward slash
    t_size pos = directory.find_last('\\');
    if (pos == pfc_infinite) {
        pos = directory.find_last('/');
    }

    if (pos != pfc_infinite) {
        directory.truncate(pos);
        return directory;
    }

    return pfc::string8();
}

} // namespace artgrab
