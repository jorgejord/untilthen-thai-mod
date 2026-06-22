// app.cpp — UntilThen Thai Translator (SAO-themed Dear ImGui editor)
// Tabs: Story (.inkb sheets) | UI (.translation keys) | Databases (JSON) | Build (inject+pack)
// Live tag-safety warning (flags rows where [$]/[wave]/[shake]/brackets differ from English).
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <commdlg.h>
#include <d3d11.h>
#include <string>
#include <vector>
#include <fstream>
#include <thread>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <cstdio>
#include <functional>
#include <iterator>

#include "pck.hpp"
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"
#include "json.hpp"
#include "sao_theme.hpp"

using json = nlohmann::json;
// Find the project root by walking up from the .exe until we see a "translation_sheets" folder.
// This makes the tool PORTABLE: drop the whole bundle anywhere and it still finds its data.
// directory of the .exe itself (program + bundled font live here in the portable build)
static std::string ExeDir(){ char p[MAX_PATH]={0}; GetModuleFileNameA(nullptr,p,MAX_PATH); std::string s=p; for(char&c:s)if(c=='\\')c='/'; size_t q=s.find_last_of('/'); return q==std::string::npos? s : s.substr(0,q); }
static const std::string EXEDIR = ExeDir();
static std::string DetectRoot(){
    std::string cur=EXEDIR;
    for(int i=0;i<8;i++){
        DWORD a=GetFileAttributesA((cur+"/translation_sheets").c_str());
        if(a!=INVALID_FILE_ATTRIBUTES && (a&FILE_ATTRIBUTE_DIRECTORY)) return cur;
        size_t q=cur.find_last_of('/'); if(q==std::string::npos) break; cur=cur.substr(0,q);
    }
    return EXEDIR; // portable fallback: look for data next to the .exe
}
static const std::string ROOT = DetectRoot();
// where the GAME DATA lives (sheets / databases / UI). The program ships WITHOUT any game data;
// the user points this at their own copy. Defaults to the dev ROOT so the full project still works.
static std::string g_dataRoot = ROOT;

// ============================ DX11 ============================
static ID3D11Device*           g_dev=nullptr;
static ID3D11DeviceContext*    g_ctx=nullptr;
static IDXGISwapChain*         g_sc=nullptr;
static ID3D11RenderTargetView* g_rtv=nullptr;
void MkRT(){ ID3D11Texture2D* bb; g_sc->GetBuffer(0,IID_PPV_ARGS(&bb)); g_dev->CreateRenderTargetView(bb,nullptr,&g_rtv); bb->Release(); }
void RmRT(){ if(g_rtv){g_rtv->Release();g_rtv=nullptr;} }
bool MkDev(HWND h){ DXGI_SWAP_CHAIN_DESC sd{}; sd.BufferCount=2; sd.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator=60; sd.BufferDesc.RefreshRate.Denominator=1; sd.Flags=DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT; sd.OutputWindow=h; sd.SampleDesc.Count=1; sd.Windowed=TRUE; sd.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
    D3D_FEATURE_LEVEL fl; const D3D_FEATURE_LEVEL lv[]={D3D_FEATURE_LEVEL_11_0,D3D_FEATURE_LEVEL_10_0};
    if(D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,lv,2,D3D11_SDK_VERSION,&sd,&g_sc,&g_dev,&fl,&g_ctx)!=S_OK) return false;
    MkRT(); return true; }
void RmDev(){ RmRT(); if(g_sc)g_sc->Release(); if(g_ctx)g_ctx->Release(); if(g_dev)g_dev->Release(); }
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,UINT,WPARAM,LPARAM);
LRESULT WINAPI WndProc(HWND h,UINT m,WPARAM w,LPARAM l){ if(ImGui_ImplWin32_WndProcHandler(h,m,w,l)) return true;
    switch(m){ case WM_SIZE: if(g_dev&&w!=SIZE_MINIMIZED){ RmRT(); g_sc->ResizeBuffers(0,(UINT)LOWORD(l),(UINT)HIWORD(l),DXGI_FORMAT_UNKNOWN,0); MkRT(); } return 0;
    case WM_SYSCOMMAND: if((w&0xfff0)==SC_KEYMENU) return 0; break; case WM_DESTROY: PostQuitMessage(0); return 0; }
    return DefWindowProc(h,m,w,l); }

// ============================ shared ============================
static std::string g_status = "พร้อมใช้งาน";
static char  g_search[128]="";
static int   g_requestFocus=-1;
static int   g_gotoTab=-1;        // global-search jump: force-select this tab this frame
static int   g_filterMode=0;      // 0=ทั้งหมด 1=ยังไม่แปล 2=แท็กพัง  (applies to every edit table)
// ---- undo: capture pre-edit value when a field gains focus, push on commit ----
struct UndoEntry{ std::function<void()> restore; };
static std::vector<UndoEntry> g_undo; static std::string g_undoSnap; static bool g_haveSnap=false;
// ---- find & replace (Thai column) ----
static bool g_openReplace=false; static char g_frFind[256]="", g_frRepl[256]=""; static int g_frScope=1;
// ---- program UI language (0=English [default], 1=ไทย) + tiny inline translator ----
static int g_lang=0;
static inline const char* T(const char* en,const char* th){ return g_lang? th:en; }
// ---- theme presets + custom accent + light mode + custom font (all persisted) ----
struct ThemePreset{ const char* name; float r,g,b; };
static const ThemePreset THEMES[]={
    {"SAO Cyan",0.18f,0.83f,0.92f},{"Crimson",0.95f,0.30f,0.32f},{"Emerald",0.20f,0.86f,0.55f},
    {"Violet",0.62f,0.45f,0.95f},{"Amber",0.96f,0.70f,0.20f},{"Rose",0.96f,0.42f,0.66f},
    {"Azure",0.30f,0.58f,0.98f},{"Steel",0.55f,0.64f,0.74f},
};
static int g_themeIdx=0; static float g_accentCol[3]={0.18f,0.83f,0.92f}; static bool g_lightMode=false;
static std::string g_fontPath; static bool g_fontReload=false;   // custom .ttf (empty = bundled Sarabun)

// ---- settings (font size + UI scale, persisted) ----
static float g_fontSize=22.f, g_uiScale=1.f; static bool g_needFont=false; static ImGuiStyle g_baseStyle;
static std::string g_gamePath="C:\\Program Files (x86)\\Steam\\steamapps\\common\\Until Then";
// classic folder picker (no COM init needed without BIF_NEWDIALOGSTYLE)
std::string BrowseFolder(const char* title){
    std::string result; char disp[MAX_PATH]={0};
    BROWSEINFOA bi{}; bi.lpszTitle=title; bi.pszDisplayName=disp; bi.ulFlags=BIF_RETURNONLYFSDIRS|BIF_EDITBOX;
    LPITEMIDLIST pidl=SHBrowseForFolderA(&bi);
    if(pidl){ char path[MAX_PATH]; if(SHGetPathFromIDListA(pidl,path)) result=path; CoTaskMemFree(pidl); }
    return result;
}
// open-file dialog for picking a font (.ttf/.otf)
std::string BrowseFile(const char* title){
    char path[MAX_PATH]={0};
    OPENFILENAMEA ofn{}; ofn.lStructSize=sizeof(ofn); ofn.lpstrFile=path; ofn.nMaxFile=MAX_PATH;
    ofn.lpstrFilter="Fonts (*.ttf;*.otf)\0*.ttf;*.otf\0All files\0*.*\0"; ofn.lpstrTitle=title;
    ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    return GetOpenFileNameA(&ofn)? std::string(path) : std::string();
}
// auto-detect the Until Then game folder across all Steam libraries (same logic as the installer)
static std::string AutoFindGame(){
    std::vector<std::string> roots; char buf[1024]; DWORD sz;
    auto reg=[&](HKEY h,const char* sub,const char* val)->std::string{ sz=sizeof(buf);
        if(RegGetValueA(h,sub,val,RRF_RT_REG_SZ,nullptr,buf,&sz)==ERROR_SUCCESS) return buf; return std::string(); };
    std::string sp=reg(HKEY_CURRENT_USER,"Software\\Valve\\Steam","SteamPath");
    if(sp.empty()) sp=reg(HKEY_LOCAL_MACHINE,"SOFTWARE\\WOW6432Node\\Valve\\Steam","InstallPath");
    if(!sp.empty()) roots.push_back(sp);
    if(!sp.empty()){ std::ifstream f(sp+"\\steamapps\\libraryfolders.vdf"); std::string ln;
        while(std::getline(f,ln)){ size_t p=ln.find("\"path\""); if(p==std::string::npos) continue;
            size_t a=ln.find('"',p+6); if(a==std::string::npos) continue; size_t b=ln.find('"',a+1); if(b==std::string::npos) continue;
            std::string raw=ln.substr(a+1,b-a-1), out; for(size_t i=0;i<raw.size();++i){ if(raw[i]=='\\'&&i+1<raw.size()&&raw[i+1]=='\\'){ out+='\\'; ++i; } else out+=raw[i]; }
            roots.push_back(out); } }
    const char* fb[]={"C:\\Program Files (x86)\\Steam","C:\\Program Files\\Steam","D:\\Steam","D:\\SteamLibrary","E:\\Steam","E:\\SteamLibrary","F:\\Steam","F:\\SteamLibrary"};
    for(auto s:fb) roots.push_back(s);
    for(auto& c:roots){ if(c.empty()) continue; std::string g=c+"\\steamapps\\common\\Until Then";
        if(GetFileAttributesA((g+"\\UntilThen.pck").c_str())!=INVALID_FILE_ATTRIBUTES ||
           GetFileAttributesA((g+"\\UntilThen.pck.bak").c_str())!=INVALID_FILE_ATTRIBUTES) return g; }
    return std::string();
}
// is there EXTRACTED, editable data at this root? (the game ships everything PACKED in UntilThen.pck,
// so a raw game folder returns false until the .pck is unpacked.)
static bool HasData(const std::string& root){ return !root.empty() &&
    GetFileAttributesA((root+"/translation_sheets").c_str())!=INVALID_FILE_ATTRIBUTES; }
// ImGui 1.92 dynamic fonts: text size is style.FontSizeBase (re-rasterized on demand), not an atlas rebuild.
// ApplyUIScale must re-apply FontSizeBase because it resets the whole style from g_baseStyle.
void ApplyUIScale(){ ImGuiStyle& s=ImGui::GetStyle(); s=g_baseStyle; s.ScaleAllSizes(g_uiScale); s.FontSizeBase=g_fontSize; }
void RebuildFont(){ ImGui::GetStyle().FontSizeBase=g_fontSize; g_needFont=false; }
// re-apply the whole colour theme from the accent + light-mode state, then rescale.
void ApplyTheme(){ sao::g_accent=ImVec4(g_accentCol[0],g_accentCol[1],g_accentCol[2],1.f); sao::g_light=g_lightMode;
    sao::ApplyStyle(); g_baseStyle=ImGui::GetStyle(); ApplyUIScale(); }
// (re)build the font atlas: bundled Sarabun, or a custom .ttf with Sarabun merged in for Thai glyphs.
void BuildFonts(){ ImGuiIO& io=ImGui::GetIO(); io.Fonts->Clear();
    // Sarabun ships NEXT TO the .exe (portable); fall back to the dev project path.
    std::string sara=EXEDIR+"/Sarabun-Regular.ttf";
    { std::ifstream t(sara); if(!t) sara=ROOT+"/tools/fontproj/Sarabun-Regular.ttf"; }
    std::string base=g_fontPath.empty()? sara : g_fontPath;
    io.Fonts->AddFontFromFileTTF(base.c_str(),g_fontSize,nullptr,io.Fonts->GetGlyphRangesThai());
    if(!g_fontPath.empty()){ ImFontConfig m; m.MergeMode=true; io.Fonts->AddFontFromFileTTF(sara.c_str(),g_fontSize,&m,io.Fonts->GetGlyphRangesThai()); }
}
std::string CfgPath(){ std::ifstream a(EXEDIR+"/config.json"); if(a) return EXEDIR+"/config.json";
    std::ifstream b(ROOT+"/tools/UntilThenTranslator/config.json"); if(b) return ROOT+"/tools/UntilThenTranslator/config.json";
    return EXEDIR+"/config.json"; }   // new config saves next to the .exe (portable)
void SaveCfg(){ json j; j["font"]=g_fontSize; j["ui"]=g_uiScale; j["game"]=g_gamePath;
    j["lang"]=g_lang; j["theme"]=g_themeIdx; j["light"]=g_lightMode; j["fontpath"]=g_fontPath; j["dataroot"]=g_dataRoot;
    j["accent"]={g_accentCol[0],g_accentCol[1],g_accentCol[2]};
    std::ofstream o(CfgPath()); o<<j.dump(2); }
void LoadCfg(){ std::ifstream f(CfgPath()); if(f){ json j; try{ f>>j;
    g_fontSize=j.value("font",22.f); g_uiScale=j.value("ui",1.f); g_gamePath=j.value("game",g_gamePath);
    g_lang=j.value("lang",0); g_themeIdx=j.value("theme",0); g_lightMode=j.value("light",false); g_fontPath=j.value("fontpath",std::string());
    g_dataRoot=j.value("dataroot",g_dataRoot);
    if(j.contains("accent")&&j["accent"].is_array()&&j["accent"].size()==3) for(int i=0;i<3;i++) g_accentCol[i]=j["accent"][i].get<float>();
    }catch(...){} } }

// ---- tag-safety: th must keep same bracket-balance + same count of dangerous tags as en ----
static int countSub(const std::string& s,const std::string& sub){ int n=0; size_t p=0; while((p=s.find(sub,p))!=std::string::npos){n++;p+=sub.size();} return n; }
bool tagsOK(const std::string& en,const std::string& th){
    if(th.empty()) return true;
    int de=0,dt=0; for(char c:en){ if(c=='[')de++; else if(c==']')de--; } for(char c:th){ if(c=='[')dt++; else if(c==']')dt--; }
    if(de!=dt) return false;
    const char* k[]={"[wave]","[/wave]","[shake","[/shake]","[$","[color","[/color]","[img","[font","[/font]","[center]","[/center]"};
    for(auto s:k) if(countSub(en,s)!=countSub(th,s)) return false;
    return true;
}

// ============================ Story model ============================
struct Line{ int i; std::string speaker,full,en,th; };
struct FileE{ std::string rel; std::vector<Line> lines; };
struct Sheet{ std::string path; std::vector<FileE> files; bool dirty=false; int done=0,total=0;
    void recount(){ done=total=0; for(auto&f:files)for(auto&l:f.lines){ total++; if(!l.th.empty())done++; } } };
static Sheet g_sheet; static int g_ch=0,g_reg=0,g_selFile=-1; static bool g_allFiles=false;
static int g_jumpFile=-1;   // left-list click in All-files mode -> scroll editor to this file
static const char* CHAPS[]={"1","2","3","4","5","6","7","8","9","10","lo","rdvt"};
std::string SheetPath(int ch,int reg){ return g_dataRoot+"/translation_sheets/sheet_"+CHAPS[ch]+(reg?"_rough":"_clean")+".json"; }
void LoadSheet(int ch,int reg){ g_sheet=Sheet{}; g_selFile=-1; std::string p=SheetPath(ch,reg); g_sheet.path=p;
    std::ifstream f(p); if(!f){ g_status="โหลดไม่ได้: "+p; return; } json j; try{ f>>j; }catch(...){ g_status="JSON เสีย"; return; }
    for(auto it=j.begin();it!=j.end();++it){ FileE fe; fe.rel=it.key();
        const json& a = it.value().is_object()&&it.value().contains("lines")? it.value()["lines"]:it.value();
        for(auto& ln:a){ Line L; L.i=ln.value("i",0); L.speaker=ln.value("speaker",""); L.full=ln.value("full","");
            L.en=ln.value("en",""); L.th=ln.value("th",""); fe.lines.push_back(std::move(L)); } g_sheet.files.push_back(std::move(fe)); }
    g_sheet.recount(); g_sheet.dirty=false; if(!g_sheet.files.empty())g_selFile=0; g_status="โหลด story: "+std::to_string(g_sheet.files.size())+" ไฟล์"; }
void SaveSheet(){ if(g_sheet.path.empty())return; json j; for(auto&fe:g_sheet.files){ json a=json::array();
        for(auto&l:fe.lines) a.push_back({{"i",l.i},{"speaker",l.speaker},{"full",l.full},{"en",l.en},{"th",l.th}}); j[fe.rel]=a; }
    std::ofstream o(g_sheet.path); o<<j.dump(1); g_sheet.dirty=false; g_status="บันทึก story แล้ว"; }

// ============================ UI-keys model ============================
struct Kv{ std::string key,en,th; };
static std::vector<Kv> g_ui; static bool g_uiLoaded=false,g_uiDirty=false;
void LoadUI(){ g_ui.clear(); std::ifstream tsv(g_dataRoot+"/tools/transproj/en_real.tsv");
    std::vector<std::pair<std::string,std::string>> en; std::string ln;
    while(std::getline(tsv,ln)){ auto t=ln.find('\t'); if(t!=std::string::npos) en.push_back({ln.substr(0,t),ln.substr(t+1)}); }
    json th; { std::ifstream f(g_dataRoot+"/tools/transproj/combined_th.json"); if(f) try{ f>>th; }catch(...){} }
    for(auto& kv:en){ Kv r; r.key=kv.first; r.en=kv.second; if(th.contains(kv.first)&&th[kv.first].is_string()) r.th=th[kv.first].get<std::string>(); g_ui.push_back(r); }
    // also include keys that are in th but not in tsv
    g_uiLoaded=true; g_uiDirty=false; g_status="โหลด UI: "+std::to_string(g_ui.size())+" คีย์"; }
void SaveUI(){ json j; for(auto& r:g_ui) if(!r.th.empty()) j[r.key]=r.th; std::ofstream o(g_dataRoot+"/tools/transproj/combined_th.json"); o<<j.dump(0); g_uiDirty=false; g_status="บันทึก UI แล้ว (อย่าลืม Build text.th.translation ในแท็บ Build)"; }

// ============================ Databases model ============================
static const char* DBS[]={"email","text","fb","the_liamson_times","geddit","web","cc","matchy","landimu","minds_alike"};
static int g_dbIdx=0; static json g_dbFlatTh, g_dbFlatEn; static std::vector<std::string> g_dbKeys; static bool g_dbDirty=false;
void LoadDB(int idx){ g_dbKeys.clear(); g_dbFlatTh=json::object(); g_dbFlatEn=json::object();
    std::string nm=DBS[idx];
    { std::ifstream f(g_dataRoot+"/ThaiMod/payload/assets/databases/"+nm+"_th.json"); if(f){ json j; try{ f>>j; g_dbFlatTh=j.flatten(); }catch(...){} } }
    { std::ifstream f(g_dataRoot+"/UntilThenExtrallPCK/assets/databases/"+nm+".json"); if(f){ json j; try{ f>>j; g_dbFlatEn=j.flatten(); }catch(...){} } }
    for(auto it=g_dbFlatTh.begin(); it!=g_dbFlatTh.end(); ++it) if(it.value().is_string()) g_dbKeys.push_back(it.key());
    // fresh extraction (no Thai yet): fall back to the English keys so there's something to translate
    if(g_dbKeys.empty()) for(auto it=g_dbFlatEn.begin(); it!=g_dbFlatEn.end(); ++it) if(it.value().is_string()) g_dbKeys.push_back(it.key());
    g_dbDirty=false; g_status="โหลด database: "+nm+" ("+std::to_string(g_dbKeys.size())+" ช่อง)"; }
void SaveDB(){ json out = g_dbFlatTh.unflatten(); std::string nm=DBS[g_dbIdx];
    std::ofstream o(g_dataRoot+"/ThaiMod/payload/assets/databases/"+nm+"_th.json"); o<<out.dump(2); g_dbDirty=false;
    // keep _fil mirror in sync (databases shared)
    std::ofstream o2(g_dataRoot+"/ThaiMod/payload/assets/databases/"+nm+"_fil.json"); o2<<out.dump(2);
    g_status="บันทึก database: "+nm+" แล้ว"; }

// ============================ Build (shell, threaded) ============================
static std::string g_log; static std::mutex g_logMx; static std::atomic<bool> g_busy{false};
void logLine(const std::string& s){ std::lock_guard<std::mutex> lk(g_logMx); g_log+=s; g_log+="\n"; }
void runAsync(std::vector<std::string> cmds, std::string title){
    if(g_busy) return; g_busy=true; { std::lock_guard<std::mutex> lk(g_logMx); g_log.clear(); }
    std::thread([cmds,title](){ logLine("=== "+title+" ===");
        for(auto& c:cmds){ logLine("$ "+c); FILE* p=_popen((c+" 2>&1").c_str(),"r"); if(!p){ logLine("[run failed]"); continue; }
            char buf[512]; while(fgets(buf,sizeof(buf),p)){ std::string s=buf; if(!s.empty()&&s.back()=='\n')s.pop_back(); logLine(s); } _pclose(p); }
        logLine("=== DONE ==="); g_busy=false; }).detach();
}
static const std::string PY = "\"C:\\Users\\theze\\AppData\\Local\\Programs\\Python\\Python312\\python.exe\"";
static const std::string TOOLS = "\""+ROOT+"\\tools\"";
std::string injectCmd(const char* ch,bool rough){ // returns a python inject command
    std::string sheet = ROOT+"/translation_sheets/sheet_"+ch+(rough?"_rough":"_clean")+".json";
    std::string base = "set PYTHONUTF8=1 && "+PY+" \""+ROOT+"/tools/inject_inkb.py\" --sheet \""+sheet+"\"";
    if(rough) base += " --out \""+ROOT+"/ThaiMod/payload/assets/story/locales/fil\"";
    return base;
}

// ===== Load data from the user's OWN game (.pck): C++ pulls out story+databases, bundled Python builds sheets =====
static std::atomic<bool> g_reloadData{false};
static void mkdirsA(const std::string& path){ std::string cur; for(char c:path){ cur+=c; if(c=='/'||c=='\\') CreateDirectoryA(cur.c_str(),nullptr); } }
static std::string PyExe(){ std::string e=EXEDIR+"/python/python.exe"; if(GetFileAttributesA(e.c_str())!=INVALID_FILE_ATTRIBUTES) return "\""+e+"\""; return PY; }
static std::string PipeDir(){ std::string d=EXEDIR+"/pipeline"; if(GetFileAttributesA((d+"/extract_inkb.py").c_str())!=INVALID_FILE_ATTRIBUTES) return d; return ROOT+"/tools"; }
static bool KitMode(){ return GetFileAttributesA((EXEDIR+"/python/python.exe").c_str())!=INVALID_FILE_ATTRIBUTES
                          && GetFileAttributesA((EXEDIR+"/pipeline/extract_inkb.py").c_str())!=INVALID_FILE_ATTRIBUTES; }
static std::string PckTool(){ std::string e=EXEDIR+"/pcktool/GodotPCKExplorer.Console.exe"; if(GetFileAttributesA(e.c_str())!=INVALID_FILE_ATTRIBUTES) return "\""+e+"\"";
    return "\""+ROOT+"/godot-pck-explorer-dotnet-ui-console-win-linux-mac/GodotPCKExplorer.Console.exe\""; }
static bool KitBuildMode(){ return GetFileAttributesA((EXEDIR+"/pcktool/GodotPCKExplorer.Console.exe").c_str())!=INVALID_FILE_ATTRIBUTES
                              && GetFileAttributesA((EXEDIR+"/pipeline/inject_inkb.py").c_str())!=INVALID_FILE_ATTRIBUTES; }
static std::string bs(std::string s){ for(char&c:s) if(c=='/')c='\\'; return s; }   // forward-> back slashes for cmd
void LoadFromGame(const std::string& pckPath){
    if(g_busy) return; g_busy=true; { std::lock_guard<std::mutex> lk(g_logMx); g_log.clear(); }
    std::thread([pckPath](){
        logLine("=== Load from your game ===");
        logLine("reading: "+pckPath);
        pck::Pack p = pck::open(pckPath);
        if(!p.ok){ logLine("[cannot open .pck]"); logLine("=== DONE ==="); g_busy=false; return; }
        std::string dataRoot=EXEDIR, outRoot=dataRoot+"/UntilThenExtrallPCK"; int nd=0,ns=0;
        for(auto& e:p.entries){ std::string rp=e.path; if(rp.rfind("res://",0)==0) rp=rp.substr(6);
            bool isDB    = rp.rfind("assets/databases/",0)==0 && rp.size()>5 && rp.substr(rp.size()-5)==".json";
            bool isStory = rp.rfind("assets/story/",0)==0 && rp.size()>5 && rp.substr(rp.size()-5)==".inkb" && rp.find("/locales/")==std::string::npos;
            if(!isDB && !isStory) continue;
            std::string full=outRoot+"/"+rp; size_t sl=full.find_last_of('/'); if(sl!=std::string::npos) mkdirsA(full.substr(0,sl+1));
            std::string data=pck::readEntry(p,e); std::ofstream o(full,std::ios::binary); o.write(data.data(),(std::streamsize)data.size()); if(isDB)nd++; else ns++;
        }
        logLine("extracted "+std::to_string(nd)+" databases + "+std::to_string(ns)+" story files");
        std::string cmd = "set PYTHONUTF8=1 && "+PyExe()+" \""+PipeDir()+"/extract_inkb.py\" --story \""+outRoot+"/assets/story\" --out \""+dataRoot+"/translation_sheets\"";
        logLine("$ building sheets..."); FILE* pp=_popen((cmd+" 2>&1").c_str(),"r");
        if(pp){ char b[512]; while(fgets(b,sizeof(b),pp)){ std::string s=b; if(!s.empty()&&s.back()=='\n')s.pop_back(); logLine(s); } _pclose(pp); }
        std::string ts=dataRoot+"/translation_sheets/";
        for(auto ch:CHAPS){ std::ifstream t(ts+"sheet_"+std::string(ch)+".json",std::ios::binary); if(!t) continue;
            std::string body((std::istreambuf_iterator<char>(t)),std::istreambuf_iterator<char>());
            std::ofstream(ts+"sheet_"+ch+"_clean.json")<<body; std::ofstream(ts+"sheet_"+ch+"_rough.json")<<body; }
        logLine("=== DONE — your game's text is ready to translate ===");
        g_reloadData=true; g_busy=false;
    }).detach();
}

// ============================ SAO panel ============================
// Two NESTED layers: an OUTER frame child that draws the SAO cyan corner brackets, and an INNER content
// child inset ~10px inside it. The brackets live on the outer frame, the text on the inner panel, with a
// gap between -> the brackets can NEVER overlap or touch the text.
void BeginPanel(const char* id, ImVec2 sz){
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
    std::string oid = std::string(id) + "o";
    ImGui::BeginChild(oid.c_str(), sz, 0);
    ImVec2 a=ImGui::GetWindowPos(); ImVec2 ws=ImGui::GetWindowSize(); ImVec2 b=ImVec2(a.x+ws.x, a.y+ws.y);
    sao::CornerBrackets(ImGui::GetWindowDrawList(), ImVec2(a.x+2,a.y+2), ImVec2(b.x-2,b.y-2), sao::Cyan(sao::Pulse(1.1f,0.72f,1.f)), 17.f, 2.f);
    float m=10.f;
    ImGui::SetCursorPos(ImVec2(m,m));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(13,11));
    ImGui::BeginChild(id, ImVec2(ws.x-2*m, ws.y-2*m), ImGuiChildFlags_Borders|ImGuiChildFlags_AlwaysUseWindowPadding); }
void EndPanel(){ ImGui::EndChild(); ImGui::PopStyleVar(); ImGui::EndChild(); ImGui::PopStyleVar(); }

// generic editable table for {key|en|th}; vrefs: indices into provided getters
template<class GetEN,class GetTH,class SetTH,class Label>
void EditTable(const char* id,int n, GetEN getEN, GetTH getTH, SetTH setTH, Label label, bool enterFlow, float keyW, bool multilineMode=false){
    std::string q=g_search; std::transform(q.begin(),q.end(),q.begin(),::tolower);
    std::vector<int> view; view.reserve(n);
    for(int k=0;k<n;k++){ if(!q.empty()){ std::string e=getEN(k); std::transform(e.begin(),e.end(),e.begin(),::tolower);
        std::string t=getTH(k); if(e.find(q)==std::string::npos && t.find(g_search)==std::string::npos) continue; }
        if(g_filterMode==1 && !getTH(k).empty()) continue;                    // untranslated
        if(g_filterMode==2 && tagsOK(getEN(k),getTH(k))) continue;            // bad tags
        view.push_back(k); }
    ImGui::TextDisabled("(%d / %d)", (int)view.size(), n);
    if(multilineMode){
        // REAL ImGui table: drag ANY column border to resize, columns auto-stretch to fill width, clean look.
        // Small DBs render every row with auto-growing cells (no wasted space); the huge fb DB uses a fixed
        // row height + clipper so it stays perfectly smooth.
        ImGuiStyle& S=ImGui::GetStyle();
        auto enBox=[&](const std::string& en, float h){
            static std::vector<char> eb; if(eb.size()<en.size()+8) eb.resize(en.size()+8); strncpy(eb.data(),en.c_str(),eb.size()-1); eb[eb.size()-1]=0;
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0,0,0,0)); ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0,0,0,0)); ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0,0,0,0));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,0.f);
            ImGui::InputTextMultiline("##en",eb.data(),eb.size(),ImVec2(-FLT_MIN,h),ImGuiInputTextFlags_ReadOnly);
            ImGui::PopStyleVar(); ImGui::PopStyleColor(3); };
        auto thBox=[&](int k, const std::string& th, bool ok, float h){
            static std::vector<char> tb; size_t nd=th.size()+1024; if(tb.size()<nd) tb.resize(nd); strncpy(tb.data(),th.c_str(),tb.size()-1); tb[tb.size()-1]=0;
            bool warm=th.empty(); if(warm) ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.18f,0.10f,0.08f,0.7f));
            if(!ok) ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.95f,0.35f,0.25f,1.f));
            ImGui::InputTextMultiline("##th",tb.data(),tb.size(),ImVec2(-FLT_MIN,h));
            if(ImGui::IsItemActivated()){ g_undoSnap=th; g_haveSnap=true; }
            if(strcmp(tb.data(),th.c_str())!=0) setTH(k,tb.data());
            if(ImGui::IsItemDeactivatedAfterEdit()&&g_haveSnap){ std::string ov=g_undoSnap; auto st=setTH; int kk=k; g_undo.push_back({[st,kk,ov](){st(kk,ov.c_str());}}); g_haveSnap=false; if(g_undo.size()>300)g_undo.erase(g_undo.begin()); }
            if(!ok) ImGui::PopStyleColor(); if(warm) ImGui::PopStyleColor(); };
        auto cellH=[&](const std::string& s){ float w=ImGui::GetContentRegionAvail().x-S.FramePadding.x*2;
            float h=ImGui::CalcTextSize(s.c_str(),NULL,false,w<40?40:w).y+S.FramePadding.y*2; float mn=ImGui::GetTextLineHeight()+S.FramePadding.y*2; return h<mn?mn:h; };
        int N=(int)view.size();
        bool big = N>700;   // fb -> fixed-height + clipper; everything else -> auto-grow
        const float ROW_H = ImGui::GetTextLineHeight()*5.f + 12.f;
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(14,12));
        ImGuiTableFlags tf = ImGuiTableFlags_Resizable|ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_ScrollY|ImGuiTableFlags_SizingStretchProp;
        if(ImGui::BeginTable(id, 3, tf)){
            ImGui::TableSetupColumn(T("Key","คีย์"), ImGuiTableColumnFlags_WidthFixed, 175.f);
            ImGui::TableSetupColumn("English", ImGuiTableColumnFlags_WidthStretch, 1.f);
            ImGui::TableSetupColumn(T("Thai","ไทย"), ImGuiTableColumnFlags_WidthStretch, 1.15f);
            ImGui::TableSetupScrollFreeze(0,1); ImGui::TableHeadersRow();
            if(big){
                ImGuiListClipper clip; clip.Begin(N, ROW_H + S.CellPadding.y*2);
                while(clip.Step()) for(int vi=clip.DisplayStart; vi<clip.DisplayEnd; ++vi){ int k=view[vi];
                    std::string en=getEN(k), th=getTH(k); bool ok=tagsOK(en,th);
                    ImGui::TableNextRow(ImGuiTableRowFlags_None, ROW_H); ImGui::PushID(k);
                    ImGui::TableSetColumnIndex(0); ImGui::TextWrapped("%s", label(k).c_str());
                    ImGui::TableSetColumnIndex(1); enBox(en, ROW_H);
                    ImGui::TableSetColumnIndex(2); thBox(k, th, ok, ROW_H);
                    ImGui::PopID(); }
            } else {
                for(int vi=0; vi<N; ++vi){ int k=view[vi];
                    std::string en=getEN(k), th=getTH(k); bool ok=tagsOK(en,th);
                    ImGui::TableNextRow(); ImGui::PushID(k);
                    ImGui::TableSetColumnIndex(0); ImGui::TextWrapped("%s", label(k).c_str());
                    ImGui::TableSetColumnIndex(1); enBox(en, cellH(en));
                    ImGui::TableSetColumnIndex(2); thBox(k, th, ok, cellH(th));
                    ImGui::PopID(); }
            }
            ImGui::EndTable();
        }
        ImGui::PopStyleVar();
        return;
    }
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(16,12)); ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(11,9));
    if(ImGui::BeginTable(id,3, ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_ScrollY|ImGuiTableFlags_Resizable)){
        ImGui::TableSetupColumn(keyW<70?"#":T("Key","คีย์"), ImGuiTableColumnFlags_WidthFixed, keyW);
        ImGui::TableSetupColumn("English", ImGuiTableColumnFlags_WidthStretch, 1.f);
        ImGui::TableSetupColumn(T("Thai","ไทย"), ImGuiTableColumnFlags_WidthStretch, 1.f);
        ImGui::TableSetupScrollFreeze(0,1); ImGui::TableHeadersRow();
        ImGuiListClipper clip; clip.Begin((int)view.size());
        while(clip.Step()) for(int vi=clip.DisplayStart; vi<clip.DisplayEnd; ++vi){ int k=view[vi];
            std::string en=getEN(k), th=getTH(k); bool ok=tagsOK(en,th);
            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%s", label(k).c_str());
            ImGui::TableSetColumnIndex(1); ImGui::TextWrapped("%s", en.c_str());
            ImGui::TableSetColumnIndex(2);
            static std::vector<char> buf; size_t need=th.size()+512; if(buf.size()<need) buf.resize(need);
            strncpy(buf.data(),th.c_str(),buf.size()-1); buf[buf.size()-1]=0;
            ImGui::PushID(k);
            bool warm=th.empty(); if(warm) ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.18f,0.10f,0.08f,0.7f));
            if(!ok) ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.95f,0.35f,0.25f,1.f));
            ImGui::SetNextItemWidth(ok?-1.f:-26.f);
            if(enterFlow && g_requestFocus==vi){ ImGui::SetKeyboardFocusHere(); ImGui::SetScrollHereY(0.5f); g_requestFocus=-1; }
            bool ent=ImGui::InputText("##th",buf.data(),buf.size(), enterFlow?ImGuiInputTextFlags_EnterReturnsTrue:0);
            if(ImGui::IsItemActivated()){ g_undoSnap=th; g_haveSnap=true; }
            if(strcmp(buf.data(),th.c_str())!=0) setTH(k,buf.data());
            if(ImGui::IsItemDeactivatedAfterEdit()&&g_haveSnap){ std::string ov=g_undoSnap; auto st=setTH; int kk=k; g_undo.push_back({[st,kk,ov](){st(kk,ov.c_str());}}); g_haveSnap=false; if(g_undo.size()>300)g_undo.erase(g_undo.begin()); }
            if(ent && vi+1<(int)view.size()) g_requestFocus=vi+1;
            if(!ok){ ImGui::PopStyleColor(); ImGui::SameLine(0,4); ImGui::TextColored(ImVec4(0.95f,0.35f,0.25f,1.f),"\xe2\x9a\xa0");
                if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s",T("tags/brackets differ from English!\nbroken [$]/[wave]/[shake]/[ ] may crash the game","tag/วงเล็บไม่ตรงกับ English!\nถ้า [$]/[wave]/[shake]/[ ] เพี้ยน เกมอาจบัค")); }
            if(warm) ImGui::PopStyleColor();
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar(2);
}

// ============================ Story editor (file sections + jump-to-file) ============================
// refs = flat (fileIndex,lineIndex). In All-files mode a header row is drawn before each file's lines,
// and clicking a file on the left (g_jumpFile) scrolls the editor straight to that file's section.
void StoryEditTable(const std::vector<std::pair<int,int>>& refs){
    std::string q=g_search; std::transform(q.begin(),q.end(),q.begin(),::tolower);
    std::vector<int> view; view.reserve(refs.size());
    for(int k=0;k<(int)refs.size();k++){ if(!q.empty()){ const auto&L=g_sheet.files[refs[k].first].lines[refs[k].second];
        std::string e=L.en; std::transform(e.begin(),e.end(),e.begin(),::tolower);
        if(e.find(q)==std::string::npos && L.th.find(g_search)==std::string::npos) continue; } view.push_back(k); }
    struct VR{ bool hdr; int fi; int k; };
    static std::vector<VR> vr; vr.clear(); vr.reserve(view.size()+16);
    int lastFi=-1;
    for(int vi=0; vi<(int)view.size(); ++vi){ int k=view[vi]; int fi=refs[k].first;
        if(g_allFiles && fi!=lastFi){ vr.push_back({true,fi,-1}); lastFi=fi; }
        vr.push_back({false,fi,k}); }
    ImGui::TextDisabled("(%d / %d)", (int)view.size(), (int)refs.size());
    static int scrollTo=-1;
    if(g_jumpFile>=0){ scrollTo=-1; for(int i=0;i<(int)vr.size();++i){ if(vr[i].fi==g_jumpFile){ scrollTo=i; break; } } g_jumpFile=-1; }
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(16,12)); ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(11,9));
    if(ImGui::BeginTable("st",3, ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_ScrollY|ImGuiTableFlags_Resizable)){
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 55.f);
        ImGui::TableSetupColumn("English", ImGuiTableColumnFlags_WidthStretch, 1.f);
        ImGui::TableSetupColumn(T("Thai","ไทย"), ImGuiTableColumnFlags_WidthStretch, 1.f);
        ImGui::TableSetupScrollFreeze(0,1); ImGui::TableHeadersRow();
        ImGuiListClipper clip; clip.Begin((int)vr.size());
        if(scrollTo>=0) clip.IncludeItemByIndex(scrollTo);
        while(clip.Step()) for(int i=clip.DisplayStart;i<clip.DisplayEnd;++i){ VR& row=vr[i];
            ImGui::TableNextRow();
            if(row.hdr){ auto&fe=g_sheet.files[row.fi]; int dn=0; for(auto&l:fe.lines) if(!l.th.empty())dn++;
                ImGui::TableSetColumnIndex(1);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(sao::g_accent.x,sao::g_accent.y,sao::g_accent.z,1));
                ImGui::AlignTextToFramePadding(); ImGui::Text("\xe2\x96\xb8 %s  [%d/%d]", fe.rel.c_str(), dn, (int)fe.lines.size());
                ImGui::PopStyleColor();
                if(scrollTo==i) ImGui::SetScrollHereY(0.12f);
                continue; }
            int k=row.k; int fi=refs[k].first, li=refs[k].second; auto& L=g_sheet.files[fi].lines[li];
            std::string en=L.en; bool ok=tagsOK(en,L.th);
            ImGui::PushID(i);
            ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%d", L.i);
            ImGui::TableSetColumnIndex(1); ImGui::TextWrapped("%s", en.c_str());
            ImGui::TableSetColumnIndex(2);
            static std::vector<char> buf; size_t need=L.th.size()+512; if(buf.size()<need) buf.resize(need);
            strncpy(buf.data(),L.th.c_str(),buf.size()-1); buf[buf.size()-1]=0;
            bool warm=L.th.empty(); if(warm) ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.18f,0.10f,0.08f,0.7f));
            if(!ok) ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.95f,0.35f,0.25f,1.f));
            ImGui::SetNextItemWidth(ok?-1.f:-26.f);
            if(g_requestFocus==i){ ImGui::SetKeyboardFocusHere(); ImGui::SetScrollHereY(0.5f); g_requestFocus=-1; }
            else if(scrollTo==i){ ImGui::SetScrollHereY(0.12f); }
            bool ent=ImGui::InputText("##th",buf.data(),buf.size(), ImGuiInputTextFlags_EnterReturnsTrue);
            if(ImGui::IsItemActivated()){ g_undoSnap=L.th; g_haveSnap=true; }
            if(strcmp(buf.data(),L.th.c_str())!=0){ g_sheet.files[fi].lines[li].th=buf.data(); g_sheet.dirty=true; g_sheet.recount(); }
            if(ImGui::IsItemDeactivatedAfterEdit()&&g_haveSnap){ std::string ov=g_undoSnap; g_undo.push_back({[fi,li,ov](){ g_sheet.files[fi].lines[li].th=ov; g_sheet.dirty=true; g_sheet.recount(); }}); g_haveSnap=false; if(g_undo.size()>300)g_undo.erase(g_undo.begin()); }
            if(ent){ for(int j=i+1;j<(int)vr.size();++j) if(!vr[j].hdr){ g_requestFocus=j; break; } }
            if(!ok){ ImGui::PopStyleColor(); ImGui::SameLine(0,4); ImGui::TextColored(ImVec4(0.95f,0.35f,0.25f,1.f),"\xe2\x9a\xa0");
                if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s",T("tags/brackets differ from English!","tag/วงเล็บไม่ตรงกับ English!")); }
            if(warm) ImGui::PopStyleColor();
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar(2);
    scrollTo=-1;
}
// ============================ Tabs ============================
void DrawStoryTab(){
    ImVec4 ac(sao::g_accent.x,sao::g_accent.y,sao::g_accent.z,1);
    ImGui::AlignTextToFramePadding(); ImGui::TextColored(ac,"%s",T("Chapter","บท")); ImGui::SameLine();
    ImGui::SetNextItemWidth(110);
    if(ImGui::Combo("##ch",&g_ch,CHAPS,IM_ARRAYSIZE(CHAPS))) LoadSheet(g_ch,g_reg);
    ImGui::SameLine(); ImGui::SetNextItemWidth(150);
    const char* R[]={T("Polite","สุภาพ"),T("Rough","หยาบ")}; if(ImGui::Combo("##reg",&g_reg,R,2)) LoadSheet(g_ch,g_reg);
    ImGui::SameLine();
    if(ImGui::Button(g_sheet.dirty?T("\xe2\x97\x8f Save","\xe2\x97\x8f บันทึก"):T("Save","บันทึก"))) SaveSheet();
    ImGui::SameLine(); ImGui::Checkbox(T("All files","รวมทุกไฟล์"),&g_allFiles);
    ImGui::SameLine(); float pc=g_sheet.total?100.f*g_sheet.done/g_sheet.total:0; ImGui::TextColored(ac,"%d/%d (%.1f%%)",g_sheet.done,g_sheet.total,pc);
    ImGui::Separator();
    static float g_leftW=0.24f;
    float h=ImGui::GetContentRegionAvail().y;
    float fullW=ImGui::GetContentRegionAvail().x;
    BeginPanel("##fl", ImVec2(fullW*g_leftW,h));
    { ImDrawList* wdl=ImGui::GetWindowDrawList(); ImDrawListSplitter sp; sp.Split(wdl,2);
      for(int fi=0;fi<(int)g_sheet.files.size();++fi){ auto&fe=g_sheet.files[fi]; int dn=0; for(auto&l:fe.lines)if(!l.th.empty())dn++;
        char lb[300]; snprintf(lb,300,"%s [%d/%d]",fe.rel.c_str(),dn,(int)fe.lines.size());
        bool sel=(fi==g_selFile);
        sp.SetCurrentChannel(wdl,1);
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.10f,0.28f,0.33f,0.45f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0,0,0,0));
        if(sel) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1,1,1,1));
        if(ImGui::Selectable(lb, sel)){ g_selFile=fi; g_jumpFile=fi; }
        if(sel) ImGui::PopStyleColor();
        ImGui::PopStyleColor(3);
        if(sel){ sp.SetCurrentChannel(wdl,0); sao::SkillBar(wdl, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), sao::Pulse(1.2f,0.72f,1.f)); }
      }
      sp.Merge(wdl);
    }
    EndPanel();
    ImGui::SameLine(0,0);
    ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(0.2f,0.6f,0.7f,0.45f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,ImVec4(0.2f,0.83f,0.92f,0.6f));
    ImGui::Button("##split",ImVec2(8,h));
    if(ImGui::IsItemHovered()||ImGui::IsItemActive()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    if(ImGui::IsItemActive()){ g_leftW += ImGui::GetIO().MouseDelta.x/fullW; if(g_leftW<0.12f)g_leftW=0.12f; if(g_leftW>0.55f)g_leftW=0.55f; }
    ImGui::PopStyleColor(3);
    ImGui::SameLine(0,0);
    BeginPanel("##ed", ImVec2(0,h));
    // build flat ref list (all-files or one) as (fileIndex,lineIndex)
    static std::vector<std::pair<int,int>> refs; refs.clear();
    auto add=[&](int fi){ auto&fe=g_sheet.files[fi]; for(int li=0;li<(int)fe.lines.size();++li) refs.push_back({fi,li}); };
    if(g_allFiles){ for(int fi=0;fi<(int)g_sheet.files.size();++fi) add(fi); } else if(g_selFile>=0) add(g_selFile);
    if(ImGui::SmallButton(T("Copy EN","คัดลอก EN"))){ std::string s; for(auto&r:refs)s+=g_sheet.files[r.first].lines[r.second].en+"\n"; ImGui::SetClipboardText(s.c_str()); }
    ImGui::SameLine(); if(ImGui::SmallButton(T("Copy Thai","คัดลอก ไทย"))){ std::string s; for(auto&r:refs)s+=g_sheet.files[r.first].lines[r.second].th+"\n"; ImGui::SetClipboardText(s.c_str()); }
    StoryEditTable(refs);
    EndPanel();
}
void DrawUITab(){
    if(!g_uiLoaded) LoadUI();
    if(ImGui::Button(g_uiDirty?T("\xe2\x97\x8f Save","\xe2\x97\x8f บันทึก"):T("Save","บันทึก"))) SaveUI();
    ImGui::SameLine(); ImGui::TextDisabled("%s",T("Menu / settings / prompts (text.th.translation)","เมนู/ตั้งค่า/คำเตือน (text.th.translation)"));
    ImGui::Separator(); float h=ImGui::GetContentRegionAvail().y; BeginPanel("##uip",ImVec2(0,h));
    EditTable("ui",(int)g_ui.size(),
        [&](int k){ return g_ui[k].en; }, [&](int k){ return g_ui[k].th; },
        [&](int k,const char* v){ g_ui[k].th=v; g_uiDirty=true; },
        [&](int k){ return g_ui[k].key; }, true, 230.f);
    EndPanel();
}
void DrawDBTab(){
    static bool first=true; if(first){ LoadDB(0); first=false; }
    ImGui::PushItemWidth(180); if(ImGui::Combo("##db",&g_dbIdx,DBS,IM_ARRAYSIZE(DBS))) LoadDB(g_dbIdx); ImGui::PopItemWidth();
    ImGui::SameLine(); if(ImGui::Button(g_dbDirty?T("\xe2\x97\x8f Save","\xe2\x97\x8f บันทึก"):T("Save","บันทึก"))) SaveDB();
    ImGui::SameLine(); ImGui::TextDisabled("%s",T("Databases (email / FB / news / chat...)","ฐานข้อมูล (อีเมล/FB/ข่าว/แชต...)"));
    ImGui::Separator(); float h=ImGui::GetContentRegionAvail().y; BeginPanel("##dbp",ImVec2(0,h));
    EditTable("db",(int)g_dbKeys.size(),
        // g_dbFlatEn/Th are FLATTENED objects: keys are literal pointer-strings like "/0/text".
        // Access with the literal string key, NOT json_pointer (which would navigate nested and crash).
        [&](int k)->std::string{ const std::string& key=g_dbKeys[k]; auto it=g_dbFlatEn.find(key); return (it!=g_dbFlatEn.end()&&it->is_string())? it->get<std::string>():std::string(); },
        [&](int k)->std::string{ const std::string& key=g_dbKeys[k]; auto it=g_dbFlatTh.find(key); return (it!=g_dbFlatTh.end()&&it->is_string())? it->get<std::string>():std::string(); },
        [&](int k,const char* v){ g_dbFlatTh[g_dbKeys[k]]=std::string(v); g_dbDirty=true; },
        [&](int k){ return g_dbKeys[k]; }, false, 185.f, true);
    EndPanel();
}
void DrawBuildTab(){
    ImVec4 ac(sao::g_accent.x,sao::g_accent.y,sao::g_accent.z,1);
    ImGui::TextWrapped("%s",T("Inject + pack the pck in-app (runs the proven pipeline) — close Steam before installing","สั่ง inject + แพ็ก pck ในตัว (เรียก pipeline ที่พิสูจน์แล้ว) — ปิด Steam ก่อนติดตั้ง"));
    ImGui::Separator();
    // --- game folder picker ---
    ImGui::TextColored(ac,"%s",T("Until Then game folder","โฟลเดอร์เกม Until Then"));
    static char gp[512]={0}; static bool gpInit=false;
    if(!gpInit){ strncpy(gp,g_gamePath.c_str(),511); gpInit=true; }
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x-180);
    if(ImGui::InputText("##gamepath",gp,sizeof(gp))) g_gamePath=gp;
    ImGui::SameLine();
    if(ImGui::Button(T("Browse...","เลือกโฟลเดอร์..."))){ std::string f=BrowseFolder(T("Pick the Until Then folder (with UntilThen.pck)","เลือกโฟลเดอร์ Until Then (มี UntilThen.pck)")); if(!f.empty()){ g_gamePath=f; strncpy(gp,f.c_str(),511); SaveCfg(); } }
    { bool ok=std::ifstream(g_gamePath+"\\UntilThen.pck").good()||std::ifstream(g_gamePath+"\\UntilThen.pck.bak").good();
      ImGui::TextColored(ok?ImVec4(0.4f,0.85f,0.5f,1):ImVec4(0.95f,0.5f,0.4f,1),"%s", ok?T("\xe2\x9c\x93 Found UntilThen.pck","\xe2\x9c\x93 เจอ UntilThen.pck"):T("\xe2\x9c\x97 UntilThen.pck not found here","\xe2\x9c\x97 ไม่เจอ UntilThen.pck ในโฟลเดอร์นี้")); }
    ImGui::Separator();
    if(KitBuildMode()){
        ImGui::TextColored(ac,"%s",T("\xe2\x98\x85 Build a playable mod from your translation","\xe2\x98\x85 สร้างมอดเล่นได้จากคำแปลของคุณ"));
        std::string game=g_gamePath, userPck=game+"\\UntilThen.pck";
        std::string payload=g_dataRoot+"/ThaiMod/payload", outPck=g_dataRoot+"/UntilThen.thmod.pck";
        bool b0=g_busy; if(b0) ImGui::BeginDisabled();
        if(ImGui::Button(T("\xe2\x98\x85 Build mod (inject + pack)","\xe2\x98\x85 สร้างมอด (inject + pack)"))){
            std::vector<std::string> c;
            c.push_back("if not exist \""+bs(payload)+"\" mkdir \""+bs(payload)+"\"");
            c.push_back("xcopy \""+bs(EXEDIR)+"\\scaffold\\*\" \""+bs(payload)+"\\\" /E /I /Y /Q");   // Constants.gd + Game.gd + fonts
            for(auto ch:CHAPS) c.push_back("set PYTHONUTF8=1 && "+PyExe()+" \""+PipeDir()+"/inject_inkb.py\" --sheet \""+g_dataRoot+"/translation_sheets/sheet_"+ch+"_clean.json\" --out \""+payload+"/assets/story/locales/th\"");
            c.push_back("set PYTHONUTF8=1 && "+PyExe()+" \""+PipeDir()+"/build_translation.py\" --json \""+g_dataRoot+"/tools/transproj/combined_th.json\" --locale th --out \""+payload+"/assets/locales/text.th.translation\"");   // UI/menu (no Godot needed)
            c.push_back("set PYTHONUTF8=1 && "+PyExe()+" \""+PipeDir()+"/validate_inkb.py\"");
            c.push_back("del /q \""+bs(outPck)+"\" 2>nul");
            c.push_back(PckTool()+" -pc \""+userPck+"\" \""+payload+"\" \""+outPck+"\" 2.4.1.4");
            runAsync(c,"Build mod");
        }
        ImGui::SameLine();
        if(ImGui::Button(T("\xe2\x98\x85 Install to game","\xe2\x98\x85 ติดตั้งลงเกม"))){
            runAsync({ "if not exist \""+game+"\\UntilThen.pck.bak\" copy /Y \""+userPck+"\" \""+game+"\\UntilThen.pck.bak\"",
                       "copy /Y \""+bs(outPck)+"\" \""+userPck+"\"",
                       "echo Done! Open the game then Settings -> Language -> Thai" }, "Install mod"); }
        if(b0) ImGui::EndDisabled();
        ImGui::TextDisabled("%s",T("Injects your story into the th locale + packs it into your UntilThen.pck (close Steam first). Menu/UI stays English.","ฉีด story ลง locale th + แพ็กเข้า UntilThen.pck ของคุณ (ปิด Steam ก่อน) — เมนู/UI ยังเป็นอังกฤษ"));
        ImGui::Separator();
    }
    bool busy=g_busy;
    if(busy) ImGui::BeginDisabled();
    if(ImGui::Button(T("Inject + Validate (this chapter)","Inject + Validate (บทนี้)"))){
        runAsync({ injectCmd(CHAPS[g_ch],false), injectCmd(CHAPS[g_ch],true),
                   "set PYTHONUTF8=1 && "+PY+" \""+ROOT+"/tools/validate_inkb.py\"" }, std::string("Inject ")+CHAPS[g_ch]); }
    ImGui::SameLine();
    if(ImGui::Button(T("Inject all + Validate","Inject ทุกบท + Validate"))){ std::vector<std::string> c;
        for(auto ch:CHAPS){ c.push_back(injectCmd(ch,false)); c.push_back(injectCmd(ch,true)); }
        c.push_back("set PYTHONUTF8=1 && "+PY+" \""+ROOT+"/tools/validate_inkb.py\""); runAsync(c,"Inject ALL"); }
    ImGui::SameLine();
    if(ImGui::Button(T("Validate only","ตรวจสอบอย่างเดียว"))){
        runAsync({ "set PYTHONUTF8=1 && "+PY+" \""+ROOT+"/tools/validate_inkb.py\"" }, "Validate .inkb"); }
    if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s",T("Check every injected .inkb is safe (idempotent round-trip), no inject","ตรวจว่า .inkb ที่ inject แล้วปลอดภัยทุกไฟล์ (ไม่ inject ใหม่)"));
    ImGui::SameLine();
    if(ImGui::Button(T("Deep test","Deep test (เต็มรูปแบบ)"))){
        runAsync({ "set PYTHONUTF8=1 && "+PY+" \""+ROOT+"/tools/deep_test.py\"" }, "Deep test"); }
    if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s",T("Full integrity gate: rebuild + tag parity + DB/UI/font checks","ตรวจครบ: rebuild + เทียบแท็ก + เช็ค DB/UI/ฟอนต์"));
    ImGui::Dummy(ImVec2(0,4));
    if(ImGui::Button(T("\xe2\x96\xa0 Pack to pck","\xe2\x96\xa0 Pack เป็น pck"))){
        std::string tool="\""+ROOT+"/godot-pck-explorer-dotnet-ui-console-win-linux-mac/GodotPCKExplorer.Console.exe\"";
        std::string bak="\""+g_gamePath+"\\UntilThen.pck.bak\"";
        std::string pay="\""+ROOT+"/ThaiMod/payload\""; std::string out="\""+ROOT+"/build/UntilThen.thmod.pck\"";
        runAsync({ "del /q \""+ROOT+"\\build\\UntilThen.thmod.pck\" 2>nul", tool+" -pc "+bak+" "+pay+" "+out+" 2.4.1.4" }, "Pack pck"); }
    ImGui::Dummy(ImVec2(0,4));
    if(ImGui::Button(T("Build UI (text.th/fil)","Build UI (text.th/fil)"))){
        std::string godot="\""+ROOT+"/tools/godot/Godot_v4.1.4-stable_win64_console.exe\"";
        std::string proj="\""+ROOT+"/tools/transproj\"";
        std::string loc=ROOT+"\\ThaiMod\\payload\\assets\\locales";
        runAsync({ godot+" --headless --path "+proj+" --script res://build_th.gd",
                   godot+" --headless --path "+proj+" --script res://build_fil.gd",
                   "copy /Y \""+ROOT+"\\tools\\transproj\\text.th.translation\" \""+loc+"\\text.th.translation\"",
                   "copy /Y \""+ROOT+"\\tools\\transproj\\text.fil.translation\" \""+loc+"\\text.fil.translation\"" }, "Build UI translation"); }
    if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s",T("Save the UI tab first, then press this to build text.th/fil.translation into the payload","แก้แท็บ UI แล้วกด \"บันทึก\" ก่อน\nแล้วกดนี่เพื่อ build text.th/fil.translation เข้า payload"));
    ImGui::SameLine(); ImGui::TextDisabled("%s",T("(after editing the UI / Menu tab)","(หลังแก้แท็บ UI/เมนู)"));
    ImGui::Dummy(ImVec2(0,8)); ImGui::Separator(); ImGui::Dummy(ImVec2(0,4));
    ImGui::TextColored(ImVec4(0.95f,0.75f,0.25f,1.f),"%s",T("\xe2\x9a\xa0 Fully close Steam before installing","\xe2\x9a\xa0 ปิด Steam ให้สนิทก่อนติดตั้ง"));
    if(ImGui::Button(T("\xe2\x98\x85 Install to game (pack + overwrite UntilThen.pck)","\xe2\x98\x85 ติดตั้งลงเกมเลย (pack + ทับ UntilThen.pck)"))){
        std::string tool="\""+ROOT+"/godot-pck-explorer-dotnet-ui-console-win-linux-mac/GodotPCKExplorer.Console.exe\"";
        std::string gameDir=g_gamePath;
        std::string gamePck=gameDir+"\\UntilThen.pck", gameBak=gameDir+"\\UntilThen.pck.bak";
        std::string bak="\""+gameDir+"\\UntilThen.pck.bak\"", pay="\""+ROOT+"/ThaiMod/payload\"", out="\""+ROOT+"/build/UntilThen.thmod.pck\"";
        runAsync({
            "if not exist \""+gameBak+"\" copy /Y \""+gamePck+"\" \""+gameBak+"\"",
            "del /q \""+ROOT+"\\build\\UntilThen.thmod.pck\" 2>nul",
            tool+" -pc "+bak+" "+pay+" "+out+" 2.4.1.4",
            "copy /Y \""+ROOT+"\\build\\UntilThen.thmod.pck\" \""+gamePck+"\"",
            "echo ติดตั้งเสร็จ! เปิดเกม -> Settings -> Language -> ภาษาไทย"
        }, "ติดตั้งลงเกม"); }
    if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s",T("Backup .bak (first time) -> pack -> overwrite the game's UntilThen.pck\nif Steam is open the copy fails (see log)","สำรอง .bak (ครั้งแรก) -> pack -> ทับ UntilThen.pck ของเกม\nถ้า Steam เปิดอยู่จะ copy ไม่ได้ (ดู log)"));
    if(busy){ ImGui::EndDisabled(); ImGui::SameLine(); ImGui::TextColored(ImVec4(sao::g_accent.x,sao::g_accent.y,sao::g_accent.z,1),"  %s",T("\xe2\x97\x8f working...","\xe2\x97\x8f กำลังทำงาน...")); }
    ImGui::Separator();
    ImGui::TextDisabled("Log:");
    BeginPanel("##log", ImVec2(0, ImGui::GetContentRegionAvail().y));
    { std::lock_guard<std::mutex> lk(g_logMx); ImGui::TextUnformatted(g_log.c_str()); }
    if(g_busy) ImGui::SetScrollHereY(1.f);
    EndPanel();
}

void DrawSettingsTab(){
    ImGui::Dummy(ImVec2(0,6));
    ImGui::TextColored(ImVec4(sao::g_accent.x,sao::g_accent.y,sao::g_accent.z,1),"%s",T("Settings","ตั้งค่า")); ImGui::Separator(); ImGui::Dummy(ImVec2(0,8));

    // --- program language ---
    ImGui::TextColored(ImVec4(sao::g_accent.x,sao::g_accent.y,sao::g_accent.z,1),"%s",T("Program language","ภาษาของโปรแกรม"));
    const char* LANGS[]={"English","ไทย (Thai)"};
    ImGui::PushItemWidth(260); if(ImGui::Combo("##lang",&g_lang,LANGS,2)){ SaveCfg(); } ImGui::PopItemWidth();
    ImGui::SameLine(); ImGui::TextDisabled("%s",T("(switches this app's UI text)","(สลับข้อความหน้าจอของโปรแกรมนี้)"));
    ImGui::Dummy(ImVec2(0,14)); ImGui::Separator(); ImGui::Dummy(ImVec2(0,8));

    // --- game data folder (program ships WITHOUT game data; the user points it at their own copy) ---
    ImGui::TextColored(ImVec4(sao::g_accent.x,sao::g_accent.y,sao::g_accent.z,1),"%s",T("Game data folder","โฟลเดอร์ข้อมูลเกม"));
    static char dr[600]={0}; static bool drInit=false; if(!drInit){ strncpy(dr,g_dataRoot.c_str(),599); drInit=true; }
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x-210);
    if(ImGui::InputText("##dataroot",dr,sizeof(dr))) g_dataRoot=dr;
    ImGui::SameLine();
    if(ImGui::Button(T("Choose folder...","เลือกโฟลเดอร์..."))){ std::string f=BrowseFolder(T("Pick the folder that contains 'translation_sheets'","เลือกโฟลเดอร์ที่มี translation_sheets")); if(!f.empty()){ g_dataRoot=f; strncpy(dr,f.c_str(),599); SaveCfg(); LoadSheet(g_ch,g_reg); g_uiLoaded=false; LoadDB(g_dbIdx); g_status=T("Data folder set","ตั้งโฟลเดอร์ข้อมูลแล้ว"); } }
    if(ImGui::Button(T("Auto-detect game","ค้นหาเกมอัตโนมัติ"))){ std::string g=AutoFindGame();
        if(g.empty()) g_status=T("Couldn't find the game automatically — pick the folder manually","หาเกมอัตโนมัติไม่เจอ — เลือกโฟลเดอร์เองได้");
        else if(HasData(g)){ g_dataRoot=g; strncpy(dr,g.c_str(),599); SaveCfg(); LoadSheet(g_ch,g_reg); g_uiLoaded=false; LoadDB(g_dbIdx); g_status=T("Found game + data!","เจอเกม + ข้อมูลแล้ว!"); }
        else { g_gamePath=g; SaveCfg(); g_status=T("Found the game, but its data is still PACKED inside UntilThen.pck — it must be unpacked first (see README)","เจอเกมแล้ว แต่ข้อมูลยังถูกอัดอยู่ใน UntilThen.pck ต้องแกะออกมาก่อน (ดู README)"); } }
    if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s",T("Scans your Steam libraries for Until Then","สแกนหาเกม Until Then ใน Steam library ของคุณ"));
    if(KitMode()){
        bool busy=g_busy; if(busy) ImGui::BeginDisabled();
        if(ImGui::Button(T("\xe2\x96\xb6 Load from my game (.pck)","\xe2\x96\xb6 ดึงข้อความจากเกมของฉัน (.pck)"))){
            std::string g=AutoFindGame(); if(g.empty()) g=g_gamePath;
            std::string pp=g+"\\UntilThen.pck";
            if(GetFileAttributesA(pp.c_str())==INVALID_FILE_ATTRIBUTES) pp=g+"\\UntilThen.pck.bak";
            if(GetFileAttributesA(pp.c_str())==INVALID_FILE_ATTRIBUTES) g_status=T("UntilThen.pck not found — set the game folder first","ไม่เจอ UntilThen.pck — ตั้งโฟลเดอร์เกมก่อน");
            else LoadFromGame(pp);
        }
        if(busy){ ImGui::EndDisabled(); ImGui::SameLine(); ImGui::TextColored(ImVec4(sao::g_accent.x,sao::g_accent.y,sao::g_accent.z,1),"  %s",T("\xe2\x97\x8f working...","\xe2\x97\x8f กำลังดึง...")); }
        if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s",T("Unpacks only story + databases from YOUR UntilThen.pck and builds the editable sheets (no 3 GB extraction)","แกะเฉพาะ story + databases จาก UntilThen.pck ของคุณ แล้วสร้างชีตให้แก้ (ไม่ต้องแตกทั้ง 3GB)"));
    }
    { bool ok=std::ifstream(g_dataRoot+"/translation_sheets/sheet_1_clean.json").good();
      ImGui::TextColored(ok?ImVec4(0.4f,0.85f,0.5f,1):ImVec4(0.95f,0.6f,0.3f,1),"%s", ok?T("\xe2\x9c\x93 game data found","\xe2\x9c\x93 เจอข้อมูลเกมแล้ว"):T("\xe2\x9a\xa0 no data here — the edit tabs will be empty until you point this at your data","\xe2\x9a\xa0 ยังไม่เจอข้อมูล — แท็บแก้คำแปลจะว่างจนกว่าจะชี้โฟลเดอร์ถูก")); }
    ImGui::TextDisabled("%s",T("This program ships WITHOUT game data. Point it at your own extracted Until Then files.","โปรแกรมนี้ไม่ได้แถมข้อมูลเกมมา ต้องชี้ไปที่ไฟล์เกม Until Then ที่คุณแยกออกมาเอง"));
    ImGui::Dummy(ImVec2(0,14)); ImGui::Separator(); ImGui::Dummy(ImVec2(0,8));

    // --- theme ---
    ImGui::TextColored(ImVec4(sao::g_accent.x,sao::g_accent.y,sao::g_accent.z,1),"%s",T("Theme","ธีมสี"));
    auto tname=[&](int i)->const char*{ return i==0? T("Default","ค่าตั้งต้น") : THEMES[i].name; };
    ImGui::PushItemWidth(260);
    if(ImGui::BeginCombo("##theme", g_themeIdx<0? T("Custom","กำหนดเอง"):tname(g_themeIdx))){
        for(int i=0;i<IM_ARRAYSIZE(THEMES);i++){ bool sel=(g_themeIdx==i);
            ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(THEMES[i].r,THEMES[i].g,THEMES[i].b,1));
            char lbl[64]; snprintf(lbl,64,"%s###th%d",tname(i),i);
            if(ImGui::Selectable(lbl,sel)){ g_themeIdx=i; g_accentCol[0]=THEMES[i].r; g_accentCol[1]=THEMES[i].g; g_accentCol[2]=THEMES[i].b; ApplyTheme(); SaveCfg(); }
            ImGui::PopStyleColor(); }
        ImGui::EndCombo(); }
    ImGui::PopItemWidth();
    ImGui::SameLine(); ImGui::SetNextItemWidth(220);
    if(ImGui::ColorEdit3(T("Custom colour","สีกำหนดเอง"),g_accentCol,ImGuiColorEditFlags_NoInputs)){ g_themeIdx=-1; ApplyTheme(); }
    if(ImGui::IsItemDeactivatedAfterEdit()) SaveCfg();
    if(ImGui::Checkbox(T("Light background","พื้นหลังสว่าง"),&g_lightMode)){ ApplyTheme(); SaveCfg(); }
    ImGui::Dummy(ImVec2(0,14)); ImGui::Separator(); ImGui::Dummy(ImVec2(0,8));

    // --- font ---
    ImGui::TextColored(ImVec4(sao::g_accent.x,sao::g_accent.y,sao::g_accent.z,1),"%s",T("Font","ฟอนต์"));
    ImGui::TextDisabled("%s", g_fontPath.empty()? T("Bundled: Sarabun (Thai + Latin)","ในตัว: Sarabun (ไทย + ละติน)") : g_fontPath.c_str());
    if(ImGui::Button(T("Load font (.ttf/.otf)...","เพิ่มฟอนต์ (.ttf/.otf)..."))){
        std::string f=BrowseFile(T("Choose a font to support other languages","เลือกฟอนต์เพื่อรองรับภาษาอื่น"));
        if(!f.empty()){ g_fontPath=f; g_fontReload=true; SaveCfg(); g_status=T("Font loaded","โหลดฟอนต์แล้ว"); } }
    ImGui::SameLine();
    if(ImGui::Button(T("Reset font","คืนค่าฟอนต์เดิม"))){ g_fontPath.clear(); g_fontReload=true; SaveCfg(); }
    ImGui::TextDisabled("%s",T("Load a Latin/CJK/Cyrillic font to translate into languages Sarabun doesn't cover.","โหลดฟอนต์ละติน/จีน-ญี่ปุ่น/ซีริลลิก เพื่อแปลภาษาที่ Sarabun ไม่รองรับ"));
    ImGui::Dummy(ImVec2(0,12));
    ImGui::PushItemWidth(420);
    ImGui::Text("%s",T("Font size","ขนาดฟอนต์")); ImGui::SliderFloat("##fs",&g_fontSize,10.f,40.f,"%.0f px");
    ImGui::Dummy(ImVec2(0,8));
    ImGui::Text("%s",T("UI scale (fields / buttons / spacing)","ขนาด UI (ช่อง / ปุ่ม / ระยะห่าง)")); ImGui::SliderFloat("##us",&g_uiScale,0.6f,1.8f,"%.2f x");
    ImGui::PopItemWidth();
    ImGui::Dummy(ImVec2(0,10)); ImGui::Separator(); ImGui::Dummy(ImVec2(0,6));
    if(ImGui::Button(T("Apply & Save","ใช้งาน + บันทึก"))){ g_needFont=true; ApplyUIScale(); SaveCfg(); g_status=T("Settings applied & saved","บันทึก + ใช้งานการตั้งค่าแล้ว"); }
    ImGui::SameLine();
    if(ImGui::Button(T("Reset defaults","รีเซ็ตค่าเริ่มต้น"))){ g_fontSize=22.f; g_uiScale=1.f; g_themeIdx=0; g_accentCol[0]=0.18f;g_accentCol[1]=0.83f;g_accentCol[2]=0.92f; g_lightMode=false; g_needFont=true; ApplyTheme(); SaveCfg(); g_status=T("Reset","รีเซ็ตแล้ว"); }
    ImGui::Dummy(ImVec2(0,16));
    ImGui::TextDisabled("UntilThen Translator  |  Dear ImGui + DirectX11");
}

// ============================ Global search (across every tab) ============================
struct GHit{ int type; int ch,reg,fi,li; int dbIdx; std::string dbKey,uiKey,label,en,th; };
static std::vector<GHit> g_index; static bool g_indexBuilt=false; static int g_indexGen=0; static char g_gsearch[128]="";
static Sheet LoadSheetData(int ch,int reg){ Sheet s; s.path=SheetPath(ch,reg);
    std::ifstream f(s.path); if(!f) return s; json j; try{ f>>j; }catch(...){ return s; }
    for(auto it=j.begin();it!=j.end();++it){ FileE fe; fe.rel=it.key();
        const json& a=it.value().is_object()&&it.value().contains("lines")? it.value()["lines"]:it.value();
        for(auto& ln:a){ Line L; L.i=ln.value("i",0); L.speaker=ln.value("speaker",""); L.full=ln.value("full","");
            L.en=ln.value("en",""); L.th=ln.value("th",""); fe.lines.push_back(std::move(L)); } s.files.push_back(std::move(fe)); }
    return s; }
void BuildGlobalIndex(){ g_index.clear();
    for(int ci=0;ci<IM_ARRAYSIZE(CHAPS);ci++) for(int r=0;r<2;r++){ Sheet s=LoadSheetData(ci,r);
        for(int fi=0;fi<(int)s.files.size();fi++) for(int li=0;li<(int)s.files[fi].lines.size();li++){ auto& L=s.files[fi].lines[li];
            if(L.en.empty()&&L.th.empty()) continue;
            GHit h{}; h.type=0; h.ch=ci; h.reg=r; h.fi=fi; h.li=li; h.dbIdx=-1;
            h.label=std::string("Story ")+CHAPS[ci]+(r?" [หยาบ] ":" [สุภาพ] ")+s.files[fi].rel+" #"+std::to_string(L.i);
            h.en=L.en; h.th=L.th; g_index.push_back(std::move(h)); } }
    if(!g_uiLoaded) LoadUI();
    for(auto& kv:g_ui){ GHit h{}; h.type=1; h.dbIdx=-1; h.uiKey=kv.key; h.label="UI   "+kv.key; h.en=kv.en; h.th=kv.th; g_index.push_back(std::move(h)); }
    for(int di=0;di<IM_ARRAYSIZE(DBS);di++){ std::string nm=DBS[di]; json th,en;
        { std::ifstream f(g_dataRoot+"/ThaiMod/payload/assets/databases/"+nm+"_th.json"); if(f) try{ json j; f>>j; th=j.flatten(); }catch(...){} }
        { std::ifstream f(g_dataRoot+"/UntilThenExtrallPCK/assets/databases/"+nm+".json"); if(f) try{ json j; f>>j; en=j.flatten(); }catch(...){} }
        for(auto it=th.begin();it!=th.end();++it){ if(!it.value().is_string()) continue;
            GHit h{}; h.type=2; h.dbIdx=di; h.dbKey=it.key(); h.label=std::string("DB   ")+nm+"  "+it.key();
            h.th=it.value().get<std::string>(); auto e=en.find(it.key()); h.en=(e!=en.end()&&e->is_string())?e->get<std::string>():"";
            g_index.push_back(std::move(h)); } }
    g_indexBuilt=true; g_indexGen++; g_status=std::string(T("Search index: ","ดัชนีค้นหารวม: "))+std::to_string(g_index.size())+T(" entries"," รายการ"); }
void JumpTo(const GHit& h){ g_search[0]=0;
    if(h.type==0){ g_ch=h.ch; g_reg=h.reg; LoadSheet(g_ch,g_reg); g_allFiles=false; g_selFile=h.fi; g_requestFocus=h.li; g_gotoTab=0; }
    else if(h.type==1){ if(!g_uiLoaded) LoadUI(); g_gotoTab=1; }
    else if(h.type==2){ g_dbIdx=h.dbIdx; LoadDB(g_dbIdx); g_gotoTab=2; } }
void DrawSearchTab(){
    ImGui::Dummy(ImVec2(0,4));
    if(ImGui::Button(g_indexBuilt?T("Rebuild index","รีเฟรชดัชนี"):T("Build search index","สร้างดัชนีค้นหา"))) BuildGlobalIndex();
    ImGui::SameLine();
    if(!g_indexBuilt){ ImGui::TextDisabled("%s",T("Press to load every Story chapter + UI + all Databases for one combined search","กดเพื่อโหลด Story ทุกบท + UI + Databases ทั้งหมด มาค้นหาพร้อมกัน")); return; }
    ImGui::TextDisabled(T("(%d entries)","(%d รายการ)"), (int)g_index.size());
    ImGui::Dummy(ImVec2(0,4)); ImGui::SetNextItemWidth(-1.f);
    ImGui::InputTextWithHint("##gs",T("Type a query (EN or Thai) to search every menu...","พิมพ์คำค้น (EN หรือ ไทย) ค้นจากทุกเมนู..."),g_gsearch,sizeof(g_gsearch));
    ImGui::Separator();
    if(strlen(g_gsearch)==0){ ImGui::TextDisabled("%s",T("Searches English and Thai across Story / UI / Databases at once","ค้นทั้ง English และ ไทย จาก Story / UI / Databases ทั้งหมดในครั้งเดียว")); return; }
    // cache: only re-scan the (large) index when the query or the index actually changes, never every frame
    static std::string lastRaw="\x01"; static int seenGen=-1; static std::vector<int> cview;
    if(std::string(g_gsearch)!=lastRaw || seenGen!=g_indexGen){ lastRaw=g_gsearch; seenGen=g_indexGen;
        std::string q=lastRaw; std::transform(q.begin(),q.end(),q.begin(),::tolower);
        cview.clear();
        for(int i=0;i<(int)g_index.size();i++){ std::string e=g_index[i].en; std::transform(e.begin(),e.end(),e.begin(),::tolower);
            if(e.find(q)!=std::string::npos || g_index[i].th.find(g_gsearch)!=std::string::npos) cview.push_back(i); } }
    std::vector<int>& view=cview;
    int shown=(int)view.size()>400?400:(int)view.size();
    ImGui::TextDisabled(T("%d hits%s — click  Go \xe2\x86\x92  to jump","เจอ %d รายการ%s — คลิก  ไป \xe2\x86\x92  เพื่อกระโดดไปแก้"), (int)view.size(), (int)view.size()>400?T(" (showing first 400)"," (แสดง 400 แรก)"):"");
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(12,10));
    ImGuiTableFlags tf=ImGuiTableFlags_Resizable|ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_ScrollY|ImGuiTableFlags_SizingStretchProp;
    if(ImGui::BeginTable("gst",4,tf)){
        ImGui::TableSetupColumn(T("Source","แหล่ง"),ImGuiTableColumnFlags_WidthFixed,250.f);
        ImGui::TableSetupColumn("English",ImGuiTableColumnFlags_WidthStretch,1.f);
        ImGui::TableSetupColumn(T("Thai","ไทย"),ImGuiTableColumnFlags_WidthStretch,1.1f);
        ImGui::TableSetupColumn("",ImGuiTableColumnFlags_WidthFixed,54.f);
        ImGui::TableSetupScrollFreeze(0,1); ImGui::TableHeadersRow();
        for(int vi=0;vi<shown;++vi){ const GHit& h=g_index[view[vi]];
            ImGui::TableNextRow(); ImGui::PushID(view[vi]);
            ImGui::TableSetColumnIndex(0); ImGui::TextColored(ImVec4(0.45f,0.80f,0.88f,1),"%s",h.label.c_str());
            ImGui::TableSetColumnIndex(1); ImGui::TextWrapped("%s",h.en.c_str());
            ImGui::TableSetColumnIndex(2); ImGui::TextWrapped("%s",h.th.c_str());
            ImGui::TableSetColumnIndex(3); if(ImGui::SmallButton(T("Go \xe2\x86\x92","ไป \xe2\x86\x92"))) JumpTo(h);
            ImGui::PopID(); }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}
// ---- find & replace in the Thai column ----
int ReplaceCurrentSheet(const std::string& f,const std::string& r){ if(f.empty())return 0; int c=0;
    for(auto&fe:g_sheet.files)for(auto&l:fe.lines){ size_t p=0; while((p=l.th.find(f,p))!=std::string::npos){ l.th.replace(p,f.size(),r); p+=r.size(); c++; } }
    if(c){ g_sheet.dirty=true; g_sheet.recount(); } return c; }
int ReplaceAllStoryDisk(const std::string& f,const std::string& r){ if(f.empty())return 0;
    if(g_sheet.dirty) SaveSheet();
    int total=0;
    for(int ci=0;ci<IM_ARRAYSIZE(CHAPS);ci++)for(int rg=0;rg<2;rg++){ std::string path=SheetPath(ci,rg);
        std::ifstream in(path); if(!in)continue; json j; try{ in>>j; }catch(...){ continue; } in.close(); bool ch=false;
        for(auto it=j.begin();it!=j.end();++it){ json& arr = it.value().is_object()&&it.value().contains("lines")? it.value()["lines"]:it.value();
            if(!arr.is_array())continue;
            for(auto& ln:arr){ if(!ln.contains("th")||!ln["th"].is_string())continue; std::string t=ln["th"].get<std::string>();
                size_t p=0;int c=0; while((p=t.find(f,p))!=std::string::npos){ t.replace(p,f.size(),r); p+=r.size(); c++; }
                if(c){ ln["th"]=t; total+=c; ch=true; } } }
        if(ch){ std::ofstream o(path); o<<j.dump(1); } }
    LoadSheet(g_ch,g_reg); g_indexBuilt=false; return total; }
void RenderReplacePopup(){
    ImGui::SetNextWindowSize(ImVec2(560,0));
    if(ImGui::BeginPopupModal(T("Find & Replace","ค้นหา & แทนที่"),nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
        ImGui::TextDisabled("%s",T("Replace text in the Thai translation only — never touches English","แทนที่คำในคำแปลภาษาไทย (เฉพาะช่อง ไทย) — ไม่ยุ่งกับ English"));
        ImGui::Dummy(ImVec2(0,4));
        ImGui::Text("%s",T("Find","ค้นหา")); ImGui::SetNextItemWidth(-1); ImGui::InputText("##frf",g_frFind,sizeof(g_frFind));
        ImGui::Text("%s",T("Replace with","แทนที่ด้วย")); ImGui::SetNextItemWidth(-1); ImGui::InputText("##frr",g_frRepl,sizeof(g_frRepl));
        ImGui::Dummy(ImVec2(0,6));
        ImGui::RadioButton(T("Current chapter only (in memory, save it yourself after)","เฉพาะบทที่เปิดอยู่ (ในหน่วยความจำ, กดบันทึกเองทีหลัง)"),&g_frScope,0);
        ImGui::RadioButton(T("All story chapters, polite + rough (writes files now)","ทุกบท story ทั้งสุภาพ+หยาบ (เขียนลงไฟล์ทันที)"),&g_frScope,1);
        ImGui::Dummy(ImVec2(0,8));
        bool can=strlen(g_frFind)>0;
        if(!can) ImGui::BeginDisabled();
        if(ImGui::Button(T("Replace all","แทนที่ทั้งหมด"),ImVec2(150,0))){
            int c = g_frScope? ReplaceAllStoryDisk(g_frFind,g_frRepl) : ReplaceCurrentSheet(g_frFind,g_frRepl);
            g_status=std::string(T("Replaced","แทนที่"))+" \""+std::string(g_frFind)+"\" \xe2\x86\x92 \""+std::string(g_frRepl)+"\" : "+std::to_string(c);
            ImGui::CloseCurrentPopup(); }
        if(!can) ImGui::EndDisabled();
        ImGui::SameLine(); if(ImGui::Button(T("Close","ปิด"),ImVec2(90,0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}
void DrawUI(){
    ImGuiViewport* vp=ImGui::GetMainViewport(); ImGui::SetNextWindowPos(vp->WorkPos); ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::Begin("##root",nullptr, ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoBringToFrontOnFocus|ImGuiWindowFlags_NoNavFocus|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
    ImDrawList* dl=ImGui::GetWindowDrawList(); ImVec2 wp=ImGui::GetWindowPos(), ws=ImGui::GetWindowSize();
    // header
    ImVec2 hA=ImVec2(wp.x+8,wp.y+8), hB=ImVec2(wp.x+ws.x-8,wp.y+54);
    dl->AddRectFilled(hA,hB, sao::Panel(0.88f),3.f); dl->AddRect(hA,hB, sao::CyanDim(0.55f),3.f,0,1.f);
    sao::Sheen(dl,hA,hB, sao::Cyan()); sao::CornerBrackets(dl,hA,hB, sao::Cyan(sao::Pulse(0.8f,0.72f,1.f)),20.f,2.f);
    sao::AccentBar(dl, ImVec2(hA.x,hB.y-3), ImVec2(hB.x,hB.y), sao::Cyan(0.85f));
    ImGui::SetCursorPos(ImVec2(26,18)); ImGui::PushStyleColor(ImGuiCol_Text,sao::Cyan()); ImGui::Text("UNTIL THEN"); ImGui::PopStyleColor();
    ImGui::SameLine(); ImGui::Text("%s",T("// Thai Translator","// ตัวแปลภาษาไทย"));
    ImGui::SameLine(0,10); ImGui::TextColored(ImVec4(0.4f,0.85f,0.95f,1),"v1.0");
    // right side of the header: per-tab search box + Replace + Undo
    const char* RB=T("Replace","แทนที่"); const char* UB=T("Undo","ย้อนกลับ"); float fp=ImGui::GetStyle().FramePadding.x*2;
    float bwR=ImGui::CalcTextSize(RB).x+fp+8, bwU=ImGui::CalcTextSize(UB).x+fp+8, sw=240.f;
    float sx=ws.x-18-bwU-8-bwR-8-sw;
    ImGui::SetCursorPos(ImVec2(sx,11)); ImGui::SetNextItemWidth(sw);
    ImGui::InputTextWithHint("##s",T("Search (EN/Thai) in this tab","ค้นหา (EN/ไทย) ในแท็บนี้"),g_search,sizeof(g_search));
    ImGui::SameLine(0,8); if(ImGui::Button(RB)) g_openReplace=true;
    if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s",T("Find & replace text in the Thai column (one chapter or all)","ค้นหา & แทนที่คำในคำแปลไทย (ทีละบท หรือทั้งเล่ม)"));
    ImGui::SameLine(0,8); bool noU=g_undo.empty(); if(noU)ImGui::BeginDisabled();
    if(ImGui::Button(UB)){ g_undo.back().restore(); g_undo.pop_back(); g_status=T("Undone","ย้อนกลับแล้ว"); }
    if(ImGui::IsItemHovered()&&!noU) ImGui::SetTooltip("%s",T("Undo last edit (Ctrl+Z when not typing)","ย้อนการแก้ครั้งล่าสุด (Ctrl+Z ตอนไม่ได้พิมพ์อยู่)"));
    if(noU)ImGui::EndDisabled();
    ImGui::SetCursorPosY(60);
    if(g_openReplace){ ImGui::OpenPopup(T("Find & Replace","ค้นหา & แทนที่")); g_openReplace=false; }
    RenderReplacePopup();
    if(ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z,false) && !ImGui::GetIO().WantTextInput && !g_undo.empty()){
        g_undo.back().restore(); g_undo.pop_back(); g_status="ย้อนกลับแล้ว (เหลือประวัติ "+std::to_string(g_undo.size())+")"; }
    // content (search + tabs + panels) lives in a padded child that RESERVES a footer -> status bar always
    // visible, nothing hugs the window edges, and search/tabs/panels all line up. Scales to any window size.
    float footerH = ImGui::GetTextLineHeightWithSpacing() + 10.f;
    float contentH = ImGui::GetContentRegionAvail().y - footerH;
    if(contentH < 80.f) contentH = 80.f;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(26,12));
    ImGui::BeginChild("##content", ImVec2(0, contentH), ImGuiChildFlags_AlwaysUseWindowPadding);   // borderless child IGNORES WindowPadding unless this flag is set
    if(ImGui::BeginTabBar("tabs")){
        auto gf=[&](int i)->ImGuiTabItemFlags{ return g_gotoTab==i? ImGuiTabItemFlags_SetSelected : 0; };
        // stable "###id" so switching language (which changes the visible label) doesn't reshuffle/garble the tab bar
        static std::string tl; auto TT=[&](const char* en,const char* th,const char* id){ tl=std::string(T(en,th))+"###"+id; return tl.c_str(); };
        if(ImGui::BeginTabItem(TT("Story","เนื้อเรื่อง","t0"),nullptr,gf(0))){ if(g_gotoTab==0)g_gotoTab=-1; DrawStoryTab(); ImGui::EndTabItem(); }
        if(ImGui::BeginTabItem(TT("UI / Menu","UI / เมนู","t1"),nullptr,gf(1))){ if(g_gotoTab==1)g_gotoTab=-1; DrawUITab(); ImGui::EndTabItem(); }
        if(ImGui::BeginTabItem(TT("Databases","ฐานข้อมูล","t2"),nullptr,gf(2))){ if(g_gotoTab==2)g_gotoTab=-1; DrawDBTab(); ImGui::EndTabItem(); }
        if(ImGui::BeginTabItem(TT("Search all","ค้นหารวม","t3"))){ DrawSearchTab(); ImGui::EndTabItem(); }
        if(ImGui::BeginTabItem(TT("Build / pck","Build / pck","t4"))){ DrawBuildTab(); ImGui::EndTabItem(); }
        if(ImGui::BeginTabItem(TT("Settings","ตั้งค่า","t5"))){ DrawSettingsTab(); ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    sao::CornerBrackets(dl, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), sao::Cyan(0.35f), 15.f, 1.5f);
    // status footer (in the reserved space, always on-screen) with a softly pulsing SAO indicator dot
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.2f,0.9f,1.f, sao::Pulse(2.2f,0.35f,1.f)), "\xe2\x97\x8f"); ImGui::SameLine(0,6);
    ImGui::TextColored(ImVec4(0.6f,0.8f,0.85f,1),"%s",g_status.c_str());
    ImGui::End();
}

// ============================ main ============================
int main(int,char**){
    ImGui_ImplWin32_EnableDpiAwareness();   // render at native pixels (กัน Windows ขยายทั้งจอ → เล็ก/คมขึ้น)
    HICON appIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(101));
    WNDCLASSEXW wc{ sizeof(wc),CS_CLASSDC,WndProc,0,0,GetModuleHandle(nullptr),appIcon,LoadCursorW(nullptr,IDC_ARROW),nullptr,nullptr,L"UTTrans",appIcon };
    RegisterClassExW(&wc);
    HWND h=CreateWindowW(wc.lpszClassName,L"UntilThen Thai Translator",WS_OVERLAPPEDWINDOW,60,30,1560,940,nullptr,nullptr,wc.hInstance,nullptr);
    if(!MkDev(h)){ RmDev(); UnregisterClassW(wc.lpszClassName,wc.hInstance); return 1; }
    ShowWindow(h,SW_SHOWMAXIMIZED); UpdateWindow(h);
    IMGUI_CHECKVERSION(); ImGui::CreateContext(); ImGuiIO& io=ImGui::GetIO(); io.IniFilename=nullptr;
    LoadCfg();
    // first-run convenience: if we have no editable data yet, try to locate the game automatically
    if(!HasData(g_dataRoot)){ std::string g=AutoFindGame(); if(!g.empty()){ if(HasData(g)) g_dataRoot=g; else g_gamePath=g; } }
    BuildFonts();
    sao::g_accent=ImVec4(g_accentCol[0],g_accentCol[1],g_accentCol[2],1.f); sao::g_light=g_lightMode;
    sao::ApplyStyle(); g_baseStyle=ImGui::GetStyle(); ApplyUIScale();
    ImGui_ImplWin32_Init(h); ImGui_ImplDX11_Init(g_dev,g_ctx);
    LoadSheet(g_ch,g_reg);
    bool done=false;
    while(!done){ MSG m; while(PeekMessage(&m,nullptr,0,0,PM_REMOVE)){ TranslateMessage(&m); DispatchMessage(&m); if(m.message==WM_QUIT)done=true; } if(done)break;
        if(g_needFont) RebuildFont();
        if(g_fontReload){ ImGui_ImplDX11_InvalidateDeviceObjects(); BuildFonts(); ImGui_ImplDX11_CreateDeviceObjects(); ImGui::GetStyle().FontSizeBase=g_fontSize; g_fontReload=false; }
        if(g_reloadData){ g_dataRoot=EXEDIR; g_ch=0; LoadSheet(g_ch,g_reg); g_uiLoaded=false; LoadDB(g_dbIdx); g_indexBuilt=false; SaveCfg(); g_reloadData=false; g_status="พร้อมแปลแล้ว — โหลดข้อมูลจากเกมเสร็จ"; }
        ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame(); DrawUI(); ImGui::Render();
        const float c[4]={0.02f,0.03f,0.045f,1.f}; g_ctx->OMSetRenderTargets(1,&g_rtv,nullptr); g_ctx->ClearRenderTargetView(g_rtv,c);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData()); g_sc->Present(1,0); }
    ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext(); RmDev(); DestroyWindow(h); UnregisterClassW(wc.lpszClassName,wc.hInstance);
    return 0;
}
