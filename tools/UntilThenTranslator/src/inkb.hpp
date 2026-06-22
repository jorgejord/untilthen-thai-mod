// inkb.hpp — Until Then .inkb (compiled Ink binary) core: parse / rebuild / safe offset-patch / validate.
// Header-only. Ports the proven Python pipeline (inkb_core) to C++ — including the marker-safe
// offset patch that fixes the marker-corruption crash bug.
//
// Format: [10-byte header] [string section: null-terminated UTF-8 strings] [\xff\xff\xff\xff marker]
//         [bytecode tail: references string offsets as little-endian uint32, relative to section start]
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstring>

namespace inkb {

constexpr size_t STRING_SECTION_START = 10;
static const std::string MARKER = std::string("\xff\xff\xff\xff", 4);

struct InkString {
    uint32_t section_offset;   // offset from start of string section
    std::string text;          // raw UTF-8 bytes (no null terminator)
};

struct Parsed {
    std::string header;            // 10 bytes
    std::vector<InkString> strings;
    std::string binary_tail;       // from the marker (inclusive) to end
};

// --- minimal UTF-8 validity check (mirrors Python's decode-or-break behaviour) ---
inline bool is_valid_utf8(const char* s, size_t n) {
    size_t i = 0;
    while (i < n) {
        unsigned char c = (unsigned char)s[i];
        size_t extra;
        if (c < 0x80) extra = 0;
        else if ((c >> 5) == 0x6) extra = 1;
        else if ((c >> 4) == 0xE) extra = 2;
        else if ((c >> 3) == 0x1E) extra = 3;
        else return false;
        if (i + extra >= n) return false;
        for (size_t k = 1; k <= extra; ++k)
            if (((unsigned char)s[i + k] >> 6) != 0x2) return false;
        i += extra + 1;
    }
    return true;
}

inline size_t find_marker(const std::string& data, size_t from) {
    size_t p = data.find(MARKER, from);
    return p == std::string::npos ? data.size() : p;
}

inline Parsed parse(const std::string& data) {
    Parsed out;
    out.header = data.substr(0, STRING_SECTION_START);
    size_t binary_start = find_marker(data, STRING_SECTION_START);
    size_t pos = STRING_SECTION_START;
    while (pos < binary_start) {
        size_t end = data.find('\0', pos);
        if (end == std::string::npos || end > binary_start) break;
        std::string chunk = data.substr(pos, end - pos);
        if (!is_valid_utf8(chunk.data(), chunk.size())) break;  // stop like the Python parser
        InkString s;
        s.section_offset = (uint32_t)(pos - STRING_SECTION_START);
        s.text = chunk;
        out.strings.push_back(std::move(s));
        pos = end + 1;
    }
    out.binary_tail = data.substr(binary_start);
    return out;
}

// build the string section bytes + map old_offset -> new_offset
inline std::string build_string_section(const std::vector<InkString>& strings,
                                        std::unordered_map<uint32_t, uint32_t>& offset_map) {
    std::string result;
    offset_map.clear();
    for (const auto& s : strings) {
        offset_map[s.section_offset] = (uint32_t)result.size();
        result += s.text;
        result += '\0';
    }
    return result;
}

// marker-SAFE offset patch: never modify bytes that belong to a \xff\xff\xff\xff marker.
inline std::string patch_binary_offsets_safe(const std::string& binary_tail,
                                             const std::unordered_map<uint32_t, uint32_t>& offset_map) {
    std::string result = binary_tail;
    std::unordered_map<uint32_t, uint32_t> patchable;
    for (auto& kv : offset_map)
        if (kv.first != 0 && kv.first != kv.second) patchable[kv.first] = kv.second;
    if (patchable.empty()) return result;

    // collect protected byte positions (every marker occupies 4 bytes)
    std::vector<bool> protectedB(binary_tail.size(), false);
    for (size_t m = binary_tail.find(MARKER); m != std::string::npos; m = binary_tail.find(MARKER, m + 1))
        for (size_t j = m; j < m + 4 && j < protectedB.size(); ++j) protectedB[j] = true;

    if (binary_tail.size() < 4) return result;
    for (size_t i = 0; i + 4 <= binary_tail.size(); ++i) {
        if (protectedB[i] || protectedB[i+1] || protectedB[i+2] || protectedB[i+3]) continue;
        uint32_t val;
        std::memcpy(&val, binary_tail.data() + i, 4);   // x86 = little-endian
        auto it = patchable.find(val);
        if (it != patchable.end()) {
            uint32_t nv = it->second;
            std::memcpy(&result[i], &nv, 4);
        }
    }
    return result;
}

inline std::string rebuild(const Parsed& p) {
    std::unordered_map<uint32_t, uint32_t> off;
    std::string section = build_string_section(p.strings, off);
    std::string tail = patch_binary_offsets_safe(p.binary_tail, off);
    return p.header + section + tail;
}

// idempotent safety check: parse->rebuild must reproduce the bytes exactly.
inline bool is_safe(const std::string& bytes) {
    Parsed p = parse(bytes);
    for (auto& s : p.strings)
        if (!is_valid_utf8(s.text.data(), s.text.size())) return false;
    return rebuild(p) == bytes;
}

} // namespace inkb
