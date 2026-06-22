// pck.hpp — minimal read-only Godot 4 .pck (PCK format v2) reader.
// Reads the directory, then extracts ONLY the files you ask for (by path prefix) — so we can pull
// out assets/story + assets/databases (~a few MB) without unpacking the whole 3 GB game.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <fstream>
namespace pck {

struct Entry { std::string path; uint64_t ofs; uint64_t size; };
struct Pack  { std::string file; std::vector<Entry> entries; bool ok=false; uint32_t version=0; };

inline uint32_t rd32(std::istream& f){ uint32_t v=0; f.read((char*)&v,4); return v; }
inline uint64_t rd64(std::istream& f){ uint64_t v=0; f.read((char*)&v,8); return v; }

// open + read the directory. `fileBaseRelative` controls whether entry offsets are relative to
// the v2 file_base field (true) or absolute (false) — open() auto-detects by validating a probe.
inline Pack openRaw(const std::string& path, bool addFileBase){
    Pack p; p.file=path; std::ifstream f(path, std::ios::binary); if(!f) return p;
    if(rd32(f)!=0x43504447u) return p;                 // magic "GDPC"
    p.version=rd32(f); rd32(f); rd32(f); rd32(f);        // version, major, minor, patch
    uint64_t fileBase=0;
    if(p.version>=2){ rd32(f); fileBase=rd64(f); }       // pack_flags, file_base
    for(int i=0;i<16;i++) rd32(f);                       // reserved
    uint32_t count=rd32(f);
    if(count==0 || count>5000000u) return p;
    p.entries.reserve(count);
    for(uint32_t i=0;i<count;i++){
        uint32_t sl=rd32(f); if(!f || sl>4096) return p;
        std::string raw(sl,'\0'); f.read(&raw[0], sl);
        size_t nul=raw.find('\0'); Entry e; e.path = (nul==std::string::npos)? raw : raw.substr(0,nul);
        e.ofs=rd64(f); e.size=rd64(f);
        f.seekg(16, std::ios::cur);                      // md5
        if(p.version>=2){ rd32(f); }                     // flags
        if(addFileBase) e.ofs += fileBase;
        if(!f) return p;
        p.entries.push_back(std::move(e));
    }
    p.ok=true; return p;
}

inline std::string readEntry(const Pack& p, const Entry& e){
    std::ifstream f(p.file, std::ios::binary); if(!f) return std::string();
    f.seekg((std::streamoff)e.ofs); std::string buf(e.size,'\0'); f.read(&buf[0],(std::streamsize)e.size);
    return f? buf : std::string();
}

// open with auto-detection of the offset convention (some packs store file-base-relative offsets).
inline Pack open(const std::string& path){
    Pack p = openRaw(path, true);
    if(!p.ok) return p;
    // probe: find a small known text file and check the first byte is plausible (not past EOF)
    std::ifstream f(path, std::ios::binary | std::ios::ate); uint64_t fsz = f? (uint64_t)f.tellg():0;
    for(const auto& e : p.entries){
        if(e.size>0 && e.size<1000000){
            if(e.ofs + e.size <= fsz){ std::string b=readEntry(p,e); if(!b.empty()) return p; }
            break;
        }
    }
    // fall back to absolute offsets
    Pack q = openRaw(path, false); return q.ok? q : p;
}

} // namespace pck
