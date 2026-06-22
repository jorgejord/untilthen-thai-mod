// main.cpp — UntilThen Translator CLI (Phase 1: prove the .inkb core)
//   selftest <file.inkb>        round-trip one file, report byte-identical
//   selftest-dir <dir>          round-trip every .inkb under dir (recursive)
//   extract <file.inkb>         list strings (index : bytes)
#include "inkb.hpp"
#include <cstdio>
#include <string>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

static std::string read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

static int cmd_selftest(const std::string& path) {
    std::string data = read_file(path);
    if (data.empty()) { printf("ERROR: cannot read %s\n", path.c_str()); return 1; }
    inkb::Parsed p = inkb::parse(data);
    std::string rb = inkb::rebuild(p);
    bool ok = (rb == data);
    printf("%s : strings=%zu tail=%zu  round-trip=%s  safe=%s\n",
           path.c_str(), p.strings.size(), p.binary_tail.size(),
           ok ? "BYTE-IDENTICAL" : "DIFF", inkb::is_safe(data) ? "yes" : "NO");
    return ok ? 0 : 2;
}

static int cmd_selftest_dir(const std::string& dir) {
    size_t total = 0, ident = 0, diff = 0;
    for (auto& e : fs::recursive_directory_iterator(dir)) {
        if (!e.is_regular_file() || e.path().extension() != ".inkb") continue;
        std::string data = read_file(e.path().string());
        inkb::Parsed p = inkb::parse(data);
        std::string rb = inkb::rebuild(p);
        ++total;
        if (rb == data) ++ident;
        else { ++diff; if (diff <= 15) printf("  DIFF: %s (base=%zu out=%zu)\n",
                                              e.path().string().c_str(), data.size(), rb.size()); }
    }
    printf("round-trip %zu files: %zu byte-identical, %zu diff\n", total, ident, diff);
    return diff ? 2 : 0;
}

static int cmd_extract(const std::string& path) {
    std::string data = read_file(path);
    inkb::Parsed p = inkb::parse(data);
    printf("header=%zuB strings=%zu tail=%zuB\n", p.header.size(), p.strings.size(), p.binary_tail.size());
    for (size_t i = 0; i < p.strings.size(); ++i)
        printf("[%3zu] %s\n", i, p.strings[i].text.c_str());
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("UntilThen Translator (core test)\n"
               "  selftest <file.inkb>\n  selftest-dir <dir>\n  extract <file.inkb>\n");
        return 1;
    }
    std::string cmd = argv[1], arg = argv[2];
    if (cmd == "selftest")     return cmd_selftest(arg);
    if (cmd == "selftest-dir") return cmd_selftest_dir(arg);
    if (cmd == "extract")      return cmd_extract(arg);
    printf("unknown command: %s\n", cmd.c_str());
    return 1;
}
