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

        // Check for UTF-8 two-byte sequences starting with 0xC4 (Latin Extended-A, first half)
        if (c == 0xC4 && i + 1 < s.size()) {
            unsigned char next = static_cast<unsigned char>(s[i + 1]);
            char replacement = 0;

            if (next == 0x80 || next == 0x81) replacement = (next == 0x80) ? 'A' : 'a';
            else if (next == 0x82 || next == 0x83) replacement = (next == 0x82) ? 'A' : 'a';
            else if (next == 0x84 || next == 0x85) replacement = (next == 0x84) ? 'A' : 'a';
            else if (next == 0x86 || next == 0x87) replacement = (next == 0x86) ? 'C' : 'c';
            else if (next == 0x88 || next == 0x89) replacement = (next == 0x88) ? 'C' : 'c';
            else if (next == 0x8A || next == 0x8B) replacement = (next == 0x8A) ? 'C' : 'c';
            else if (next == 0x8C || next == 0x8D) replacement = (next == 0x8C) ? 'C' : 'c';
            else if (next == 0x8E || next == 0x8F) replacement = (next == 0x8E) ? 'D' : 'd';
            else if (next == 0x90 || next == 0x91) replacement = (next == 0x90) ? 'D' : 'd';
            else if (next == 0x92 || next == 0x93) replacement = (next == 0x92) ? 'E' : 'e';
            else if (next == 0x94 || next == 0x95) replacement = (next == 0x94) ? 'E' : 'e';
            else if (next == 0x96 || next == 0x97) replacement = (next == 0x96) ? 'E' : 'e';
            else if (next == 0x98 || next == 0x99) replacement = (next == 0x98) ? 'E' : 'e';
            else if (next == 0x9A || next == 0x9B) replacement = (next == 0x9A) ? 'E' : 'e';
            else if (next == 0x9C || next == 0x9D) replacement = (next == 0x9C) ? 'G' : 'g';
            else if (next == 0x9E || next == 0x9F) replacement = (next == 0x9E) ? 'G' : 'g';
            else if (next == 0xA0 || next == 0xA1) replacement = (next == 0xA0) ? 'G' : 'g';
            else if (next == 0xA2 || next == 0xA3) replacement = (next == 0xA2) ? 'G' : 'g';
            else if (next == 0xA4 || next == 0xA5) replacement = (next == 0xA4) ? 'H' : 'h';
            else if (next == 0xA6 || next == 0xA7) replacement = (next == 0xA6) ? 'H' : 'h';
            else if (next == 0xA8 || next == 0xA9) replacement = (next == 0xA8) ? 'I' : 'i';
            else if (next == 0xAA || next == 0xAB) replacement = (next == 0xAA) ? 'I' : 'i';
            else if (next == 0xAC || next == 0xAD) replacement = (next == 0xAC) ? 'I' : 'i';
            else if (next == 0xAE || next == 0xAF) replacement = (next == 0xAE) ? 'I' : 'i';
            else if (next == 0xB0 || next == 0xB1) replacement = (next == 0xB0) ? 'I' : 'i';
            else if (next == 0xB4 || next == 0xB5) replacement = (next == 0xB4) ? 'J' : 'j';
            else if (next == 0xB6 || next == 0xB7) replacement = (next == 0xB6) ? 'K' : 'k';
            else if (next == 0xB9 || next == 0xBA) replacement = (next == 0xB9) ? 'L' : 'l';
            else if (next == 0xBB || next == 0xBC) replacement = (next == 0xBB) ? 'L' : 'l';
            else if (next == 0xBD || next == 0xBE) replacement = (next == 0xBD) ? 'L' : 'l';
            else if (next == 0xB2 || next == 0xB3) {
                result += (next == 0xB2) ? "IJ" : "ij";
                i++;
                continue;
            }

            if (replacement) {
                result += replacement;
                i++;
                continue;
            }
        }

        // Check for UTF-8 two-byte sequences starting with 0xC5 (Latin Extended-A, second half)
        if (c == 0xC5 && i + 1 < s.size()) {
            unsigned char next = static_cast<unsigned char>(s[i + 1]);
            char replacement = 0;

            if (next == 0x80 || next == 0x81) replacement = (next == 0x80) ? 'L' : 'l';
            else if (next == 0x82 || next == 0x83) replacement = (next == 0x82) ? 'L' : 'l';
            else if (next == 0x84 || next == 0x85) replacement = (next == 0x84) ? 'N' : 'n';
            else if (next == 0x86 || next == 0x87) replacement = (next == 0x86) ? 'N' : 'n';
            else if (next == 0x88 || next == 0x89) replacement = (next == 0x88) ? 'N' : 'n';
            else if (next == 0x8B || next == 0x8C) replacement = (next == 0x8B) ? 'N' : 'n';
            else if (next == 0x8D || next == 0x8E) replacement = (next == 0x8D) ? 'O' : 'o';
            else if (next == 0x8F || next == 0x90) replacement = (next == 0x8F) ? 'O' : 'o';
            else if (next == 0x90 || next == 0x91) replacement = (next == 0x90) ? 'O' : 'o';
            else if (next == 0x92 || next == 0x93) {
                result += (next == 0x92) ? "OE" : "oe";
                i++;
                continue;
            }
            else if (next == 0x94 || next == 0x95) replacement = (next == 0x94) ? 'R' : 'r';
            else if (next == 0x96 || next == 0x97) replacement = (next == 0x96) ? 'R' : 'r';
            else if (next == 0x98 || next == 0x99) replacement = (next == 0x98) ? 'R' : 'r';
            else if (next == 0x9A || next == 0x9B) replacement = (next == 0x9A) ? 'S' : 's';
            else if (next == 0x9C || next == 0x9D) replacement = (next == 0x9C) ? 'S' : 's';
            else if (next == 0x9E || next == 0x9F) replacement = (next == 0x9E) ? 'S' : 's';
            else if (next == 0xA0 || next == 0xA1) replacement = (next == 0xA0) ? 'S' : 's';
            else if (next == 0xA2 || next == 0xA3) replacement = (next == 0xA2) ? 'T' : 't';
            else if (next == 0xA4 || next == 0xA5) replacement = (next == 0xA4) ? 'T' : 't';
            else if (next == 0xA6 || next == 0xA7) replacement = (next == 0xA6) ? 'T' : 't';
            else if (next == 0xA8 || next == 0xA9) replacement = (next == 0xA8) ? 'U' : 'u';
            else if (next == 0xAA || next == 0xAB) replacement = (next == 0xAA) ? 'U' : 'u';
            else if (next == 0xAC || next == 0xAD) replacement = (next == 0xAC) ? 'U' : 'u';
            else if (next == 0xAE || next == 0xAF) replacement = (next == 0xAE) ? 'U' : 'u';
            else if (next == 0xB0 || next == 0xB1) replacement = (next == 0xB0) ? 'U' : 'u';
            else if (next == 0xB2 || next == 0xB3) replacement = (next == 0xB2) ? 'U' : 'u';
            else if (next == 0xB4 || next == 0xB5) replacement = (next == 0xB4) ? 'W' : 'w';
            else if (next == 0xB6 || next == 0xB7) replacement = (next == 0xB6) ? 'Y' : 'y';
            else if (next == 0xB8) replacement = 'Y';
            else if (next == 0xB9 || next == 0xBA) replacement = (next == 0xB9) ? 'Z' : 'z';
            else if (next == 0xBB || next == 0xBC) replacement = (next == 0xBB) ? 'Z' : 'z';
            else if (next == 0xBD || next == 0xBE) replacement = (next == 0xBD) ? 'Z' : 'z';

            if (replacement) {
                result += replacement;
                i++;
                continue;
            }
        }

        // Check for UTF-8 two-byte sequences starting with 0xD0 (Cyrillic)
        if (c == 0xD0 && i + 1 < s.size()) {
            unsigned char next = static_cast<unsigned char>(s[i + 1]);

            // Multi-char uppercase replacements
            if (next == 0x96) { result += "Zh"; i++; continue; }
            else if (next == 0xA5) { result += "Kh"; i++; continue; }
            else if (next == 0xA6) { result += "Ts"; i++; continue; }
            else if (next == 0xA7) { result += "Ch"; i++; continue; }
            else if (next == 0xA8) { result += "Sh"; i++; continue; }
            else if (next == 0xA9) { result += "Shch"; i++; continue; }
            else if (next == 0xAE) { result += "Yu"; i++; continue; }
            else if (next == 0xAF) { result += "Ya"; i++; continue; }

            char replacement = 0;
            if (next == 0x90) replacement = 'A';
            else if (next == 0x91) replacement = 'B';
            else if (next == 0x92) replacement = 'V';
            else if (next == 0x93) replacement = 'G';
            else if (next == 0x94) replacement = 'D';
            else if (next == 0x95) replacement = 'E';
            else if (next == 0x97) replacement = 'Z';
            else if (next == 0x98) replacement = 'I';
            else if (next == 0x99) replacement = 'I';
            else if (next == 0x9A) replacement = 'K';
            else if (next == 0x9B) replacement = 'L';
            else if (next == 0x9C) replacement = 'M';
            else if (next == 0x9D) replacement = 'N';
            else if (next == 0x9E) replacement = 'O';
            else if (next == 0x9F) replacement = 'P';
            else if (next == 0xA0) replacement = 'R';
            else if (next == 0xA1) replacement = 'S';
            else if (next == 0xA2) replacement = 'T';
            else if (next == 0xA3) replacement = 'U';
            else if (next == 0xA4) replacement = 'F';
            else if (next == 0xAB) replacement = 'Y';
            else if (next == 0xAD) replacement = 'E';

            // Lowercase Cyrillic (0xD0 0xB0-0xBF)
            else if (next == 0xB0) replacement = 'a';
            else if (next == 0xB1) replacement = 'b';
            else if (next == 0xB2) replacement = 'v';
            else if (next == 0xB3) replacement = 'g';
            else if (next == 0xB4) replacement = 'd';
            else if (next == 0xB5) replacement = 'e';
            else if (next == 0xB6) { result += "zh"; i++; continue; }
            else if (next == 0xB7) replacement = 'z';
            else if (next == 0xB8) replacement = 'i';
            else if (next == 0xB9) replacement = 'i';
            else if (next == 0xBA) replacement = 'k';
            else if (next == 0xBB) replacement = 'l';
            else if (next == 0xBC) replacement = 'm';
            else if (next == 0xBD) replacement = 'n';
            else if (next == 0xBE) replacement = 'o';
            else if (next == 0xBF) replacement = 'p';

            if (replacement) {
                result += replacement;
                i++;
                continue;
            }

            // Skip hard/soft signs
            if (next == 0xAA || next == 0xAC) {
                i++;
                continue;
            }
        }

        // Check for UTF-8 two-byte sequences starting with 0xD1 (Cyrillic lowercase continued)
        if (c == 0xD1 && i + 1 < s.size()) {
            unsigned char next = static_cast<unsigned char>(s[i + 1]);

            // Multi-char replacements
            if (next == 0x85) { result += "kh"; i++; continue; }
            else if (next == 0x86) { result += "ts"; i++; continue; }
            else if (next == 0x87) { result += "ch"; i++; continue; }
            else if (next == 0x88) { result += "sh"; i++; continue; }
            else if (next == 0x89) { result += "shch"; i++; continue; }
            else if (next == 0x8E) { result += "yu"; i++; continue; }
            else if (next == 0x8F) { result += "ya"; i++; continue; }

            char replacement = 0;
            if (next == 0x80) replacement = 'r';
            else if (next == 0x81) replacement = 's';
            else if (next == 0x82) replacement = 't';
            else if (next == 0x83) replacement = 'u';
            else if (next == 0x84) replacement = 'f';
            else if (next == 0x8B) replacement = 'y';
            else if (next == 0x8D) replacement = 'e';
            else if (next == 0x91) replacement = 'e';
            else if (next == 0x90) replacement = 'E';

            if (replacement) {
                result += replacement;
                i++;
                continue;
            }

            // Skip hard/soft signs
            if (next == 0x8A || next == 0x8C) {
                i++;
                continue;
            }
        }

        // Check for UTF-8 two-byte sequences starting with 0xCE (Greek)
        if (c == 0xCE && i + 1 < s.size()) {
            unsigned char next = static_cast<unsigned char>(s[i + 1]);

            // Multi-char replacements
            if (next == 0x98) { result += "Th"; i++; continue; }
            else if (next == 0xA7) { result += "Ch"; i++; continue; }
            else if (next == 0xA8) { result += "Ps"; i++; continue; }

            char replacement = 0;
            if (next == 0x91) replacement = 'A';
            else if (next == 0x92) replacement = 'B';
            else if (next == 0x93) replacement = 'G';
            else if (next == 0x94) replacement = 'D';
            else if (next == 0x95) replacement = 'E';
            else if (next == 0x96) replacement = 'Z';
            else if (next == 0x97) replacement = 'I';
            else if (next == 0x99) replacement = 'I';
            else if (next == 0x9A) replacement = 'K';
            else if (next == 0x9B) replacement = 'L';
            else if (next == 0x9C) replacement = 'M';
            else if (next == 0x9D) replacement = 'N';
            else if (next == 0x9E) replacement = 'X';
            else if (next == 0x9F) replacement = 'O';
            else if (next == 0xA0) replacement = 'P';
            else if (next == 0xA1) replacement = 'R';
            else if (next == 0xA3) replacement = 'S';
            else if (next == 0xA4) replacement = 'T';
            else if (next == 0xA5) replacement = 'Y';
            else if (next == 0xA6) replacement = 'F';
            else if (next == 0xA9) replacement = 'O';

            // Lowercase Greek (0xCE 0xB1-0xBF)
            else if (next == 0xB1) replacement = 'a';
            else if (next == 0xB2) replacement = 'b';
            else if (next == 0xB3) replacement = 'g';
            else if (next == 0xB4) replacement = 'd';
            else if (next == 0xB5) replacement = 'e';
            else if (next == 0xB6) replacement = 'z';
            else if (next == 0xB7) replacement = 'i';
            else if (next == 0xB8) { result += "th"; i++; continue; }
            else if (next == 0xB9) replacement = 'i';
            else if (next == 0xBA) replacement = 'k';
            else if (next == 0xBB) replacement = 'l';
            else if (next == 0xBC) replacement = 'm';
            else if (next == 0xBD) replacement = 'n';
            else if (next == 0xBE) replacement = 'x';
            else if (next == 0xBF) replacement = 'o';

            if (replacement) {
                result += replacement;
                i++;
                continue;
            }
        }

        // Check for UTF-8 two-byte sequences starting with 0xCF (Greek lowercase continued)
        if (c == 0xCF && i + 1 < s.size()) {
            unsigned char next = static_cast<unsigned char>(s[i + 1]);

            // Multi-char replacements
            if (next == 0x86) { result += "ph"; i++; continue; }
            else if (next == 0x87) { result += "ch"; i++; continue; }
            else if (next == 0x88) { result += "ps"; i++; continue; }

            char replacement = 0;
            if (next == 0x80) replacement = 'p';
            else if (next == 0x81) replacement = 'r';
            else if (next == 0x82 || next == 0x83) replacement = 's';
            else if (next == 0x84) replacement = 't';
            else if (next == 0x85) replacement = 'y';
            else if (next == 0x89) replacement = 'o';

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
