// stub.cpp — minimal self-extracting installer stub.
// Layout of the final .exe:  [this stub][appended .zip][8-byte LE zip size]
// On run: carve out the appended zip -> %TEMP%\UntilThenThaiMod\data.zip,
// extract it with the built-in Windows `tar` (handles .zip on Win10 1803+),
// then run the extracted INSTALL.bat in a fresh console and wait.
#include <windows.h>
#include <string>
#include <vector>
#include <fstream>
#include <cstring>

static int fail(const char* msg){ MessageBoxA(NULL, msg, "Until Then - Thai Mod", MB_ICONERROR); return 1; }

static DWORD run(const std::string& cmd, const char* cwd, DWORD flags){
    std::vector<char> c(cmd.begin(), cmd.end()); c.push_back(0);
    STARTUPINFOA si{}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if(!CreateProcessA(NULL, c.data(), NULL, NULL, FALSE, flags, NULL, cwd, &si, &pi))
        return (DWORD)-1;
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0; GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    return code;
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int){
    char self[MAX_PATH]; GetModuleFileNameA(NULL, self, MAX_PATH);
    std::ifstream f(self, std::ios::binary);
    if(!f) return fail("Cannot open installer.");
    std::vector<char> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    f.close();
    if(data.size() < 8) return fail("Installer is corrupt.");

    unsigned long long zipsize = 0;
    std::memcpy(&zipsize, data.data() + data.size() - 8, 8);
    if(zipsize == 0 || zipsize > data.size() - 8) return fail("Installer payload is invalid.");
    size_t zipstart = data.size() - 8 - (size_t)zipsize;

    char tmp[MAX_PATH]; GetTempPathA(MAX_PATH, tmp);
    std::string dir = std::string(tmp) + "UntilThenThaiMod";
    CreateDirectoryA(dir.c_str(), NULL);
    std::string zippath = dir + "\\data.zip";
    { std::ofstream o(zippath, std::ios::binary);
      if(!o) return fail("Cannot write temp files.");
      o.write(data.data() + zipstart, (std::streamsize)zipsize); }

    // unpack with built-in tar (bsdtar understands .zip)
    if(run("tar -xf \"" + zippath + "\" -C \"" + dir + "\"", dir.c_str(), CREATE_NO_WINDOW) != 0)
        return fail("Failed to unpack. Your Windows may be too old; use the .zip version instead.");

    std::string bat = dir + "\\INSTALL.bat";
    if(GetFileAttributesA(bat.c_str()) == INVALID_FILE_ATTRIBUTES)
        return fail("INSTALL.bat not found after unpacking.");

    // run the installer in a visible console and wait for it to finish
    run("cmd /c \"\"" + bat + "\"\"", dir.c_str(), CREATE_NEW_CONSOLE);
    return 0;
}
