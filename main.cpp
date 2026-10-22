#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <initguid.h>
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <psapi.h>
#include <shlguid.h>
#include <exdisp.h>
#include <shldisp.h>
#include <propidl.h>
#include <gdiplus.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <vector>
#include <string>
#include <sstream>

struct WheelItem {
    std::wstring label;
    std::wstring target; // Action name atau URL
};

// Global Wheel Pages (Dinamis dari config)
std::vector<WheelItem> g_page0Items; // Media Tools
std::vector<WheelItem> g_page1Items; // Web Launcher
HWND g_hWndSettings = NULL;

// Helper path konfigurasi di %APPDATA%\RadialTool\config.ini
std::wstring GetConfigFilePath() {
    WCHAR appData[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appData))) {
        std::wstring dir = std::wstring(appData) + L"\\RadialTool";
        CreateDirectoryW(dir.c_str(), NULL);
        return dir + L"\\config.ini";
    }
    return L"config.ini";
}

void LoadDefaultWheelItems() {
    g_page0Items.clear();
    g_page0Items.push_back({ L"Format Image", L"action:format" });
    g_page0Items.push_back({ L"Image to PDF", L"action:pdf" });
    g_page0Items.push_back({ L"Screenshot", L"action:screenshot" });
    g_page0Items.push_back({ L"Compress", L"action:compress" });
    g_page0Items.push_back({ L"PDF Split", L"action:split" });

    g_page1Items.clear();
    g_page1Items.push_back({ L"YouTube", L"https://www.youtube.com" });
    g_page1Items.push_back({ L"Netflix", L"https://www.netflix.com" });
    g_page1Items.push_back({ L"GitHub", L"https://github.com" });
    g_page1Items.push_back({ L"ChatGPT", L"https://chatgpt.com" });
    g_page1Items.push_back({ L"Google", L"https://www.google.com" });
}

// Forward declaration logging
void WriteLog(const wchar_t* format, ...);

void SaveWheelConfig() {
    std::wstring path = GetConfigFilePath();
    FILE* fp = _wfopen(path.c_str(), L"w, ccs=UTF-8");
    if (!fp) return;

    fwprintf(fp, L"[PAGE0]\n");
    fwprintf(fp, L"Count=%d\n", (int)g_page0Items.size());
    for (size_t i = 0; i < g_page0Items.size(); i++) {
        fwprintf(fp, L"Label_%d=%ls\n", (int)i, g_page0Items[i].label.c_str());
        fwprintf(fp, L"Target_%d=%ls\n", (int)i, g_page0Items[i].target.c_str());
    }

    fwprintf(fp, L"\n[PAGE1]\n");
    fwprintf(fp, L"Count=%d\n", (int)g_page1Items.size());
    for (size_t i = 0; i < g_page1Items.size(); i++) {
        fwprintf(fp, L"Label_%d=%ls\n", (int)i, g_page1Items[i].label.c_str());
        fwprintf(fp, L"Target_%d=%ls\n", (int)i, g_page1Items[i].target.c_str());
    }
    fclose(fp);
    WriteLog(L"SaveWheelConfig: saved to %ls", path.c_str());
}

void LoadWheelConfig() {
    std::wstring path = GetConfigFilePath();
    DWORD attrib = GetFileAttributesW(path.c_str());
    if (attrib == INVALID_FILE_ATTRIBUTES) {
        LoadDefaultWheelItems();
        SaveWheelConfig();
        return;
    }

    WCHAR valBuf[512];
    int count0 = GetPrivateProfileIntW(L"PAGE0", L"Count", 0, path.c_str());
    if (count0 <= 0 || count0 > 5) {
        LoadDefaultWheelItems();
        SaveWheelConfig();
        return;
    }

    g_page0Items.clear();
    for (int i = 0; i < count0; i++) {
        WCHAR keyLabel[32], keyTarget[32];
        swprintf(keyLabel, 32, L"Label_%d", i);
        swprintf(keyTarget, 32, L"Target_%d", i);

        GetPrivateProfileStringW(L"PAGE0", keyLabel, L"", valBuf, 512, path.c_str());
        std::wstring lbl = valBuf;
        GetPrivateProfileStringW(L"PAGE0", keyTarget, L"", valBuf, 512, path.c_str());
        std::wstring tgt = valBuf;
        g_page0Items.push_back({ lbl, tgt });
    }

    int count1 = GetPrivateProfileIntW(L"PAGE1", L"Count", 0, path.c_str());
    g_page1Items.clear();
    if (count1 > 0 && count1 <= 5) {
        for (int i = 0; i < count1; i++) {
            WCHAR keyLabel[32], keyTarget[32];
            swprintf(keyLabel, 32, L"Label_%d", i);
            swprintf(keyTarget, 32, L"Target_%d", i);

            GetPrivateProfileStringW(L"PAGE1", keyLabel, L"", valBuf, 512, path.c_str());
            std::wstring lbl = valBuf;
            GetPrivateProfileStringW(L"PAGE1", keyTarget, L"", valBuf, 512, path.c_str());
            std::wstring tgt = valBuf;
            g_page1Items.push_back({ lbl, tgt });
        }
    } else {
        g_page1Items.push_back({ L"YouTube", L"https://www.youtube.com" });
        g_page1Items.push_back({ L"Netflix", L"https://www.netflix.com" });
        g_page1Items.push_back({ L"GitHub", L"https://github.com" });
        g_page1Items.push_back({ L"ChatGPT", L"https://chatgpt.com" });
        g_page1Items.push_back({ L"Google", L"https://www.google.com" });
    }
}

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

using namespace Gdiplus;

#define WM_TRAYICON (WM_USER + 1)
#define WM_APP_OPENWHEEL (WM_APP + 1)
#define WM_APP_SCROLLWHEEL (WM_APP + 2)
#define WM_APP_EXECUTEWHEEL (WM_APP + 3)
#define ID_TRAY_EXIT 1001
#define ID_TRAY_ABOUT 1002
#define ID_TRAY_SETTINGS 1003

const int WHEEL_SIZE = 340;
const float R_OUTER = 150.0f;
const float R_INNER = 45.0f;
const float PI = 3.1415926535f;

enum MenuState {
    STATE_MAIN = 0,
    STATE_FORMAT = 1,
    STATE_SCREENSHOT = 2
};

MenuState g_state = STATE_MAIN;
int g_mainPage = 0; // 0 = Media & Files, 1 = Web Launcher
const int TOTAL_PAGES = 2;
int g_hovered = -1;
POINT g_spawnPos;
HWND g_hWndWheel = NULL;
HWND g_hWndSnip = NULL;
HHOOK g_hMouseHook = NULL;
ULONG_PTR g_gdiplusToken;
NOTIFYICONDATA g_nid;
HWND g_hWndTray = NULL;
std::vector<std::wstring> g_smartFiles;

// Page 0: Media & Files (Maks 5 item)
const wchar_t* PAGE0_ITEMS[] = { L"Format Image", L"Image to PDF", L"Screenshot", L"Compress", L"PDF Split" };
const int PAGE0_COUNT = 5;

// Page 1: Web Quick Launcher (Maks 5 item)
const wchar_t* PAGE1_ITEMS[] = { L"YouTube", L"Netflix", L"GitHub", L"ChatGPT", L"Google" };
const int PAGE1_COUNT = 5;
const wchar_t* PAGE1_URLS[] = {
    L"https://www.youtube.com",
    L"https://www.netflix.com",
    L"https://github.com",
    L"https://chatgpt.com",
    L"https://www.google.com"
};

const wchar_t* FORMAT_ITEMS[] = { L"PNG", L"JPG", L"WebP", L"Kembali" };
const int FORMAT_COUNT = 4;

const wchar_t* SNIP_ITEMS[] = { L"Area Crop", L"Full Screen", L"Kembali" };
const int SNIP_COUNT = 3;

// Logging helper sederhana ke %TEMP%\RadialTool.log dengan format %ls untuk wchar_t*
void WriteLog(const wchar_t* format, ...) {
    WCHAR tempPath[MAX_PATH];
    DWORD dwRet = GetTempPathW(MAX_PATH, tempPath);
    if (dwRet == 0 || dwRet > MAX_PATH) return;

    std::wstring logFile(tempPath);
    logFile += L"RadialTool.log";

    FILE* fp = _wfopen(logFile.c_str(), L"a, ccs=UTF-8");
    if (!fp) return;

    SYSTEMTIME st;
    GetLocalTime(&st);
    fwprintf(fp, L"[%04d-%02d-%02d %02d:%02d:%02d.%03d] ",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

    va_list args;
    va_start(args, format);
    vfwprintf(fp, format, args);
    va_end(args);

    fwprintf(fp, L"\n");
    fclose(fp);
}

// Helper GDI+ CLSID Encoder
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT num = 0, size = 0;
    GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;
    ImageCodecInfo* pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
    if (pImageCodecInfo == NULL) return -1;
    GetImageEncoders(num, size, pImageCodecInfo);
    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j;
        }
    }
    free(pImageCodecInfo);
    return -1;
}

// Cek ekstensi file gambar (diperluas dengan jfif, gif, heic, avif)
bool IsImageExtension(const std::wstring& path) {
    size_t dot = path.rfind(L'.');
    if (dot == std::wstring::npos) return false;
    std::wstring ext = path.substr(dot + 1);
    for (auto& c : ext) c = towlower(c);
    return (ext == L"png" || ext == L"jpg" || ext == L"jpeg" || ext == L"webp" ||
            ext == L"bmp" || ext == L"ico" || ext == L"tif" || ext == L"tiff" ||
            ext == L"jfif" || ext == L"gif" || ext == L"heic" || ext == L"avif");
}

// Ekstrak file dari IDataObject (CF_HDROP) sebagai fallback
void ExtractFromDataObject(IDataObject* pDO, std::vector<std::wstring>& outFiles) {
    if (!pDO) return;
    FORMATETC fmt = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM stg;
    HRESULT hr = pDO->GetData(&fmt, &stg);
    WriteLog(L"  ExtractFromDataObject: pDO->GetData hr=0x%08X", (unsigned int)hr);
    if (SUCCEEDED(hr)) {
        HDROP hDrop = (HDROP)GlobalLock(stg.hGlobal);
        if (hDrop) {
            UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, NULL, 0);
            WriteLog(L"  ExtractFromDataObject: CF_HDROP raw item count=%u", count);
            for (UINT i = 0; i < count; i++) {
                WCHAR szFile[MAX_PATH];
                if (DragQueryFileW(hDrop, i, szFile, MAX_PATH)) {
                    std::wstring p(szFile);
                    if (IsImageExtension(p)) {
                        outFiles.push_back(p);
                        WriteLog(L"    Accepted image: %ls", szFile);
                    } else {
                        WriteLog(L"    Rejected non-image: %ls", szFile);
                    }
                }
            }
            GlobalUnlock(stg.hGlobal);
        }
        ReleaseStgMedium(&stg);
    }
}

// Deteksi seleksi dari IShellBrowser / IShellView via IFolderView2::Items -> fallback CF_HDROP
void ExtractFromShellBrowser(IShellBrowser* pSB, std::vector<std::wstring>& outFiles) {
    if (!pSB) return;
    IShellView* pSV = NULL;
    HRESULT hrSV = pSB->QueryActiveShellView(&pSV);
    WriteLog(L"  pSB->QueryActiveShellView hr=0x%08X, pSV=%p", (unsigned int)hrSV, pSV);
    if (SUCCEEDED(hrSV) && pSV) {
        // Jalur Utama: IFolderView2 -> Items(SVGIO_SELECTION, IID_IShellItemArray)
        IFolderView2* pFV2 = NULL;
        HRESULT hrFV2 = pSV->QueryInterface(IID_IFolderView2, (void**)&pFV2);
        WriteLog(L"  pSV->QueryInterface(IFolderView2) hr=0x%08X, pFV2=%p", (unsigned int)hrFV2, pFV2);
        if (SUCCEEDED(hrFV2) && pFV2) {
            IShellItemArray* pItems = NULL;
            HRESULT hrItems = pFV2->Items(SVGIO_SELECTION, IID_IShellItemArray, (void**)&pItems);
            WriteLog(L"  pFV2->Items(SVGIO_SELECTION) hr=0x%08X, pItems=%p", (unsigned int)hrItems, pItems);
            if (SUCCEEDED(hrItems) && pItems) {
                DWORD numItems = 0;
                pItems->GetCount(&numItems);
                WriteLog(L"  IFolderView2 raw selected count=%lu", (unsigned long)numItems);
                for (DWORD i = 0; i < numItems; i++) {
                    IShellItem* pItem = NULL;
                    if (SUCCEEDED(pItems->GetItemAt(i, &pItem)) && pItem) {
                        LPWSTR pszPath = NULL;
                        if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszPath)) && pszPath) {
                            std::wstring p(pszPath);
                            if (IsImageExtension(p)) {
                                outFiles.push_back(p);
                                WriteLog(L"    Accepted image: %ls", pszPath);
                            } else {
                                WriteLog(L"    Rejected non-image: %ls", pszPath);
                            }
                            CoTaskMemFree(pszPath);
                        }
                        pItem->Release();
                    }
                }
                pItems->Release();
            }
            pFV2->Release();
        }

        // Fallback: jika jalur IFolderView2 kosong, gunakan jalur CF_HDROP
        if (outFiles.empty()) {
            WriteLog(L"  IFolderView2 yielded 0 files. Falling back to CF_HDROP...");
            IDataObject* pDO = NULL;
            HRESULT hrDO = pSV->GetItemObject(SVGIO_SELECTION, IID_IDataObject, (void**)&pDO);
            WriteLog(L"  pSV->GetItemObject(SVGIO_SELECTION) hr=0x%08X, pDO=%p", (unsigned int)hrDO, pDO);
            if (SUCCEEDED(hrDO) && pDO) {
                ExtractFromDataObject(pDO, outFiles);
                pDO->Release();
            }
        }

        pSV->Release();
    }
}

// Smart Detection Explorer & Desktop dengan dukungan Explorer bertab Windows 11
std::vector<std::wstring> DetectSelectedFiles(HWND hForeground) {
    std::vector<std::wstring> results;

    WCHAR szClass[256] = L"";
    if (hForeground) {
        GetClassNameW(hForeground, szClass, 256);
    }
    WriteLog(L"DetectSelectedFiles: hForeground=%p, Class=%ls", hForeground, szClass);

    IShellWindows* pSW = NULL;
    HRESULT hrSW = CoCreateInstance(CLSID_ShellWindows, NULL, CLSCTX_ALL, IID_IShellWindows, (void**)&pSW);
    WriteLog(L"CoCreateInstance(CLSID_ShellWindows) hr=0x%08X, pSW=%p", (unsigned int)hrSW, pSW);
    if (SUCCEEDED(hrSW) && pSW) {
        long count = 0;
        pSW->get_Count(&count);
        WriteLog(L"IShellWindows count=%ld", count);

        std::vector<std::wstring> activeTabResults;
        std::vector<std::wstring> focusedResults;
        std::vector<std::wstring> fallbackResults;

        for (long i = 0; i < count; i++) {
            VARIANT v;
            VariantInit(&v);
            v.vt = VT_I4;
            v.lVal = i;
            IDispatch* pDisp = NULL;
            HRESULT hrItem = pSW->Item(v, &pDisp);
            WriteLog(L"Item[%ld]: hr=0x%08X, pDisp=%p", i, (unsigned int)hrItem, pDisp);
            if (SUCCEEDED(hrItem) && pDisp) {
                IWebBrowserApp* pWBA = NULL;
                HRESULT hrWBA = pDisp->QueryInterface(IID_IWebBrowserApp, (void**)&pWBA);
                WriteLog(L"Item[%ld]: QueryInterface(IWebBrowserApp) hr=0x%08X, pWBA=%p", i, (unsigned int)hrWBA, pWBA);
                if (SUCCEEDED(hrWBA) && pWBA) {
                    HWND hSWWnd = NULL;
                    pWBA->get_HWND((LONG_PTR*)&hSWWnd);
                    WCHAR swClass[256] = L"";
                    if (hSWWnd) GetClassNameW(hSWWnd, swClass, 256);
                    bool isForeground = (hSWWnd == hForeground) || (hForeground && IsChild(hSWWnd, hForeground));
                    WriteLog(L"Item[%ld]: hSWWnd=%p (%ls), isForeground=%d", i, hSWWnd, swClass, isForeground ? 1 : 0);

                    IServiceProvider* pSP = NULL;
                    HRESULT hrSP = pWBA->QueryInterface(IID_IServiceProvider, (void**)&pSP);
                    WriteLog(L"Item[%ld]: QueryInterface(IServiceProvider) hr=0x%08X, pSP=%p", i, (unsigned int)hrSP, pSP);
                    if (SUCCEEDED(hrSP) && pSP) {
                        IShellBrowser* pSB = NULL;
                        HRESULT hrSB = pSP->QueryService(SID_STopLevelBrowser, IID_IShellBrowser, (void**)&pSB);
                        WriteLog(L"Item[%ld]: QueryService(SID_STopLevelBrowser) hr=0x%08X, pSB=%p", i, (unsigned int)hrSB, pSB);
                        if (SUCCEEDED(hrSB) && pSB) {
                            // Cek apakah tab aktif pada Windows 11 Explorer (ShellTabWindowClass visible)
                            HWND hTabWnd = NULL;
                            IOleWindow* pOW = NULL;
                            if (SUCCEEDED(pSB->QueryInterface(IID_IOleWindow, (void**)&pOW)) && pOW) {
                                pOW->GetWindow(&hTabWnd);
                                pOW->Release();
                            }

                            WCHAR tabClass[256] = L"";
                            if (hTabWnd) GetClassNameW(hTabWnd, tabClass, 256);
                            bool isTabActive = false;

                            if (hTabWnd && hForeground) {
                                bool isDirectChild = (hTabWnd == hForeground) || IsChild(hForeground, hTabWnd);
                                bool isParentChild = (hSWWnd == hForeground) || IsChild(hSWWnd, hForeground);
                                if ((isDirectChild || isParentChild) && IsWindowVisible(hTabWnd)) {
                                    if (_wcsicmp(tabClass, L"ShellTabWindowClass") == 0 ||
                                        _wcsicmp(tabClass, L"CabinetWClass") == 0) {
                                        isTabActive = true;
                                    }
                                }
                            }

                            WriteLog(L"Item[%ld]: hTabWnd=%p (%ls), isTabActive=%d, IsVisible=%d",
                                i, hTabWnd, tabClass, isTabActive ? 1 : 0, hTabWnd ? IsWindowVisible(hTabWnd) : 0);

                            std::vector<std::wstring> curFiles;
                            ExtractFromShellBrowser(pSB, curFiles);

                            if (isTabActive && activeTabResults.empty() && !curFiles.empty()) {
                                activeTabResults = curFiles;
                                WriteLog(L"Item[%ld]: SELECTED as activeTabResults (count=%zu)", i, curFiles.size());
                            } else if (isForeground && focusedResults.empty() && !curFiles.empty()) {
                                focusedResults = curFiles;
                                WriteLog(L"Item[%ld]: SELECTED as focusedResults (count=%zu)", i, curFiles.size());
                            } else if (fallbackResults.empty() && !curFiles.empty()) {
                                fallbackResults = curFiles;
                                WriteLog(L"Item[%ld]: Saved as fallbackResults (count=%zu)", i, curFiles.size());
                            } else {
                                WriteLog(L"Item[%ld]: SKIPPED", i);
                            }

                            pSB->Release();
                        }
                        pSP->Release();
                    }
                    pWBA->Release();
                }
                pDisp->Release();
            }
        }

        // Cek Desktop jika Explorer tidak ada yang fokus
        if (activeTabResults.empty() && focusedResults.empty() && fallbackResults.empty()) {
            VARIANT vEmpty;
            VariantInit(&vEmpty);
            long hwnd = 0;
            IDispatch* pDispDesktop = NULL;
            HRESULT hrFind = pSW->FindWindowSW(&vEmpty, &vEmpty, SWC_DESKTOP, &hwnd, SWFO_NEEDDISPATCH, &pDispDesktop);
            WriteLog(L"FindWindowSW(SWC_DESKTOP) hr=0x%08X, pDispDesktop=%p", (unsigned int)hrFind, pDispDesktop);
            if (SUCCEEDED(hrFind) && pDispDesktop) {
                IServiceProvider* pSP = NULL;
                if (SUCCEEDED(pDispDesktop->QueryInterface(IID_IServiceProvider, (void**)&pSP)) && pSP) {
                    IShellBrowser* pSB = NULL;
                    if (SUCCEEDED(pSP->QueryService(SID_STopLevelBrowser, IID_IShellBrowser, (void**)&pSB)) && pSB) {
                        ExtractFromShellBrowser(pSB, fallbackResults);
                        pSB->Release();
                    }
                    pSP->Release();
                }
                pDispDesktop->Release();
            }
        }

        pSW->Release();

        if (!activeTabResults.empty()) {
            WriteLog(L"DetectSelectedFiles returning activeTabResults (count=%zu)", activeTabResults.size());
            return activeTabResults;
        }
        if (!focusedResults.empty()) {
            WriteLog(L"DetectSelectedFiles returning focusedResults (count=%zu)", focusedResults.size());
            return focusedResults;
        }
        if (!fallbackResults.empty()) {
            WriteLog(L"DetectSelectedFiles returning fallbackResults (count=%zu)", fallbackResults.size());
            return fallbackResults;
        }
    }

    WriteLog(L"DetectSelectedFiles returning empty vector");
    return results;
}

// Fallback File Picker Dialog jika tidak ada file yang dipilih
bool PickImages(WCHAR* outPathList, DWORD maxLen) {
    OPENFILENAMEW ofn = { sizeof(ofn) };
    ofn.lpstrFilter = L"Image Files (*.png;*.jpg;*.jpeg;*.webp;*.bmp;*.jfif;*.gif;*.heic;*.avif)\0*.png;*.jpg;*.jpeg;*.webp;*.bmp;*.jfif;*.gif;*.heic;*.avif\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = outPathList;
    ofn.nMaxFile = maxLen;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT;
    outPathList[0] = L'\0';
    return GetOpenFileNameW(&ofn) == TRUE;
}

// Helper generate unique output path di folder yang sama menggunakan std::wstring
void MakeUniquePath(const WCHAR* inputPath, const WCHAR* suffix, const WCHAR* targetExt, WCHAR* outPath, DWORD maxLen) {
    WCHAR drive[_MAX_DRIVE] = { 0 };
    WCHAR dir[_MAX_DIR] = { 0 };
    WCHAR fname[_MAX_FNAME] = { 0 };
    WCHAR ext[_MAX_EXT] = { 0 };
    _wsplitpath(inputPath, drive, dir, fname, ext);

    std::wstring sDrive(drive);
    std::wstring sDir(dir);
    std::wstring sFname(fname);
    std::wstring sSuffix(suffix ? suffix : L"");
    std::wstring sExt;

    if (targetExt && targetExt[0] != L'\0') {
        sExt = targetExt;
    } else {
        sExt = (ext[0] == L'.') ? (ext + 1) : ext;
    }

    std::wstring candidate = sDrive + sDir + sFname + sSuffix + L"." + sExt;
    if (GetFileAttributesW(candidate.c_str()) == INVALID_FILE_ATTRIBUTES) {
        wcsncpy_s(outPath, maxLen, candidate.c_str(), _TRUNCATE);
        return;
    }

    for (int i = 1; i < 10000; i++) {
        std::wstring numbered = sDrive + sDir + sFname + sSuffix + L"-" + std::to_wstring(i) + L"." + sExt;
        if (GetFileAttributesW(numbered.c_str()) == INVALID_FILE_ATTRIBUTES) {
            wcsncpy_s(outPath, maxLen, numbered.c_str(), _TRUNCATE);
            return;
        }
    }

    wcsncpy_s(outPath, maxLen, candidate.c_str(), _TRUNCATE);
}

// Kumpulkan target file dari smart detection Explorer atau dialog picker
std::vector<std::wstring> GatherTargets() {
    std::vector<std::wstring> targets = g_smartFiles;

    if (targets.empty()) {
        WCHAR fileBuf[8192];
        if (!PickImages(fileBuf, 8192)) return targets;

        WCHAR* p = fileBuf;
        std::wstring baseDir(p);
        p += wcslen(p) + 1;

        if (*p == L'\0') {
            targets.push_back(baseDir);
        } else {
            while (*p) {
                std::wstring fullPath = baseDir + L"\\" + p;
                targets.push_back(fullPath);
                p += wcslen(p) + 1;
            }
        }
    }

    return targets;
}

// Tampilkan notifikasi balon di system tray
void ShowTrayBalloon(const wchar_t* title, const wchar_t* msg) {
    if (!g_hWndTray) return;
    NOTIFYICONDATAW nid = { 0 };
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = g_hWndTray;
    nid.uID = 1;
    nid.uFlags = NIF_INFO;
    nid.dwInfoFlags = NIIF_INFO;
    wcsncpy_s(nid.szInfoTitle, 64, title, _TRUNCATE);
    wcsncpy_s(nid.szInfo, 256, msg, _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

// Eksekusi ganti format gambar (dengan composite putih jika target JPG dan ada transparansi)
void ExecuteConvert(const wchar_t* targetExt) {
    std::vector<std::wstring> targets = GatherTargets();
    if (targets.empty()) return;

    CLSID encoderClsid;
    bool isJpg = (wcscmp(targetExt, L"jpg") == 0 || wcscmp(targetExt, L"jpeg") == 0);

    if (wcscmp(targetExt, L"png") == 0) GetEncoderClsid(L"image/png", &encoderClsid);
    else if (isJpg) GetEncoderClsid(L"image/jpeg", &encoderClsid);
    else if (wcscmp(targetExt, L"webp") == 0) GetEncoderClsid(L"image/webp", &encoderClsid);
    else GetEncoderClsid(L"image/jpeg", &encoderClsid);

    int successCount = 0;
    std::wstring lastFolder;

    for (const auto& filePath : targets) {
        WriteLog(L"ExecuteConvert: Opening %ls", filePath.c_str());
        Bitmap bmp(filePath.c_str());
        Status initStatus = bmp.GetLastStatus();
        if (initStatus != Ok) {
            WriteLog(L"  Bitmap open failed for %ls, status=%d", filePath.c_str(), (int)initStatus);
            continue;
        }

        WCHAR outPath[MAX_PATH];
        MakeUniquePath(filePath.c_str(), L"", targetExt, outPath, MAX_PATH);
        WriteLog(L"  Saving convert to: %ls", outPath);

        Status saveStatus = GenericError;
        if (isJpg && IsAlphaPixelFormat(bmp.GetPixelFormat())) {
            // Gambar ke latar putih agar area transparan PNG tidak hitam di JPEG
            UINT w = bmp.GetWidth();
            UINT h = bmp.GetHeight();
            Bitmap bmpFlat(w, h, PixelFormat24bppRGB);
            Graphics g(&bmpFlat);
            g.Clear(Color(255, 255, 255));
            g.DrawImage(&bmp, 0, 0, w, h);
            saveStatus = bmpFlat.Save(outPath, &encoderClsid, NULL);
        } else {
            saveStatus = bmp.Save(outPath, &encoderClsid, NULL);
        }

        if (saveStatus == Ok) {
            successCount++;
            WriteLog(L"  Convert success -> %ls", outPath);
            SHChangeNotify(SHCNE_CREATE, SHCNF_PATHW, outPath, NULL);

            WCHAR drive[_MAX_DRIVE] = { 0 }, d[_MAX_DIR] = { 0 };
            _wsplitpath(filePath.c_str(), drive, d, NULL, NULL);
            lastFolder = std::wstring(drive) + d;
        } else {
            WriteLog(L"  Convert FAILED for %ls, status=%d", outPath, (int)saveStatus);
        }
    }

    if (!lastFolder.empty()) {
        SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_PATHW, lastFolder.c_str(), NULL);
    }
}

// Eksekusi kompresi gambar (dengan composite putih jika ada transparansi)
void ExecuteCompress() {
    std::vector<std::wstring> targets = GatherTargets();
    if (targets.empty()) return;

    CLSID encoderClsid;
    GetEncoderClsid(L"image/jpeg", &encoderClsid);

    EncoderParameters encoderParameters;
    ULONG quality = 60;
    encoderParameters.Count = 1;
    encoderParameters.Parameter[0].Guid = EncoderQuality;
    encoderParameters.Parameter[0].Type = EncoderParameterValueTypeLong;
    encoderParameters.Parameter[0].NumberOfValues = 1;
    encoderParameters.Parameter[0].Value = &quality;

    int successCount = 0;
    std::wstring lastFolder;

    for (const auto& filePath : targets) {
        WriteLog(L"ExecuteCompress: Opening %ls", filePath.c_str());
        Bitmap bmp(filePath.c_str());
        Status initStatus = bmp.GetLastStatus();
        if (initStatus != Ok) {
            WriteLog(L"  Bitmap open failed for %ls, status=%d", filePath.c_str(), (int)initStatus);
            continue;
        }

        WCHAR outPath[MAX_PATH];
        MakeUniquePath(filePath.c_str(), L"-compressed", L"jpg", outPath, MAX_PATH);
        WriteLog(L"  Saving compress to: %ls", outPath);

        Status saveStatus = GenericError;
        if (IsAlphaPixelFormat(bmp.GetPixelFormat())) {
            // Gambar ke latar putih agar transparan tidak hitam di JPEG
            UINT w = bmp.GetWidth();
            UINT h = bmp.GetHeight();
            Bitmap bmpFlat(w, h, PixelFormat24bppRGB);
            Graphics g(&bmpFlat);
            g.Clear(Color(255, 255, 255));
            g.DrawImage(&bmp, 0, 0, w, h);
            saveStatus = bmpFlat.Save(outPath, &encoderClsid, &encoderParameters);
        } else {
            saveStatus = bmp.Save(outPath, &encoderClsid, &encoderParameters);
        }

        if (saveStatus == Ok) {
            successCount++;
            WriteLog(L"  Compress success -> %ls", outPath);
            SHChangeNotify(SHCNE_CREATE, SHCNF_PATHW, outPath, NULL);

            WCHAR drive[_MAX_DRIVE] = { 0 }, d[_MAX_DIR] = { 0 };
            _wsplitpath(filePath.c_str(), drive, d, NULL, NULL);
            lastFolder = std::wstring(drive) + d;
        } else {
            WriteLog(L"  Compress FAILED for %ls, status=%d", outPath, (int)saveStatus);
        }
    }

    if (!lastFolder.empty()) {
        SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_PATHW, lastFolder.c_str(), NULL);
    }
}

struct PdfPageData {
    int widthPt;
    int heightPt;
    int imgObjId;
    int pageObjId;
    int contentObjId;
};

// Eksekusi gabung banyak gambar ke satu file PDF 1.4 manual
void ExecutePdf() {
    std::vector<std::wstring> targets = GatherTargets();
    if (targets.empty()) {
        WriteLog(L"ExecutePdf: targets empty, aborting.");
        return;
    }

    WriteLog(L"ExecutePdf: Starting PDF export for %zu targets", targets.size());

    CLSID jpegClsid;
    GetEncoderClsid(L"image/jpeg", &jpegClsid);

    EncoderParameters encParams;
    ULONG quality = 85;
    encParams.Count = 1;
    encParams.Parameter[0].Guid = EncoderQuality;
    encParams.Parameter[0].Type = EncoderParameterValueTypeLong;
    encParams.Parameter[0].NumberOfValues = 1;
    encParams.Parameter[0].Value = &quality;

    struct ProcessedImage {
        UINT pixelWidth;
        UINT pixelHeight;
        double mediaWidth;
        double mediaHeight;
        std::vector<unsigned char> jpegData;
    };

    std::vector<ProcessedImage> validImages;

    for (const auto& path : targets) {
        WriteLog(L"ExecutePdf: Reading %ls", path.c_str());
        Bitmap bmp(path.c_str());
        Status st = bmp.GetLastStatus();
        if (st != Ok) {
            WriteLog(L"  ExecutePdf: Failed to open %ls, status=%d", path.c_str(), (int)st);
            continue;
        }

        // Baca EXIF orientation tag (0x0112)
        UINT propSize = bmp.GetPropertyItemSize(PropertyTagOrientation);
        if (propSize > 0) {
            PropertyItem* pProp = (PropertyItem*)malloc(propSize);
            if (pProp) {
                if (bmp.GetPropertyItem(PropertyTagOrientation, propSize, pProp) == Ok && pProp->value) {
                    short orientation = *(short*)pProp->value;
                    switch (orientation) {
                    case 2: bmp.RotateFlip(RotateNoneFlipX); break;
                    case 3: bmp.RotateFlip(Rotate180FlipNone); break;
                    case 4: bmp.RotateFlip(Rotate180FlipX); break;
                    case 5: bmp.RotateFlip(Rotate90FlipX); break;
                    case 6: bmp.RotateFlip(Rotate90FlipNone); break;
                    case 7: bmp.RotateFlip(Rotate270FlipX); break;
                    case 8: bmp.RotateFlip(Rotate270FlipNone); break;
                    default: break;
                    }
                }
                free(pProp);
            }
        }

        UINT w = bmp.GetWidth();
        UINT h = bmp.GetHeight();
        if (w == 0 || h == 0) {
            WriteLog(L"  ExecutePdf: Invalid dimensions %ux%u for %ls", w, h, path.c_str());
            continue;
        }

        // Gambar ulang ke bitmap 24bpp latar belakang putih
        Bitmap flatBmp(w, h, PixelFormat24bppRGB);
        Graphics g(&flatBmp);
        g.Clear(Color(255, 255, 255));
        g.DrawImage(&bmp, 0, 0, w, h);

        // Simpan ke IStream dalam format JPEG kualitas 85
        IStream* pStream = NULL;
        HRESULT hrStream = CreateStreamOnHGlobal(NULL, TRUE, &pStream);
        if (FAILED(hrStream) || !pStream) {
            WriteLog(L"  ExecutePdf: CreateStreamOnHGlobal failed, hr=0x%08X", (unsigned int)hrStream);
            continue;
        }

        Status saveStatus = flatBmp.Save(pStream, &jpegClsid, &encParams);
        if (saveStatus != Ok) {
            WriteLog(L"  ExecutePdf: flatBmp.Save to stream failed, status=%d", (int)saveStatus);
            pStream->Release();
            continue;
        }

        HGLOBAL hMem = NULL;
        HRESULT hrMem = GetHGlobalFromStream(pStream, &hMem);
        if (FAILED(hrMem) || !hMem) {
            WriteLog(L"  ExecutePdf: GetHGlobalFromStream failed, hr=0x%08X", (unsigned int)hrMem);
            pStream->Release();
            continue;
        }

        SIZE_T byteCount = GlobalSize(hMem);
        void* pBytes = GlobalLock(hMem);
        if (!pBytes || byteCount == 0) {
            WriteLog(L"  ExecutePdf: GlobalLock failed or byteCount=0");
            if (pBytes) GlobalUnlock(hMem);
            pStream->Release();
            continue;
        }

        ProcessedImage img;
        img.pixelWidth = w;
        img.pixelHeight = h;
        img.mediaWidth = w * 0.72;
        if (img.mediaWidth < 1.0) img.mediaWidth = 1.0;
        img.mediaHeight = h * 0.72;
        if (img.mediaHeight < 1.0) img.mediaHeight = 1.0;

        img.jpegData.resize(byteCount);
        memcpy(img.jpegData.data(), pBytes, byteCount);

        GlobalUnlock(hMem);
        pStream->Release();

        validImages.push_back(img);
        WriteLog(L"  ExecutePdf: Image accepted (%ux%u, %zu bytes JPEG)", w, h, byteCount);
    }

    if (validImages.empty()) {
        WriteLog(L"ExecutePdf: No valid images processed. PDF creation cancelled.");
        return;
    }

    WCHAR outPdfPath[MAX_PATH];
    MakeUniquePath(targets[0].c_str(), L"-merged", L"pdf", outPdfPath, MAX_PATH);
    WriteLog(L"ExecutePdf: Output file -> %ls", outPdfPath);

    // Buka file tujuan dengan CreateFileW agar aman Unicode
    HANDLE hFile = CreateFileW(outPdfPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        DWORD dwErr = GetLastError();
        WriteLog(L"ExecutePdf: CreateFileW FAILED for %ls, err=%lu", outPdfPath, dwErr);
        return;
    }

    auto WriteRaw = [hFile](const void* pData, DWORD len) -> bool {
        DWORD written = 0;
        return WriteFile(hFile, pData, len, &written, NULL) && (written == len);
    };

    auto WriteStr = [&WriteRaw](const std::string& str) -> bool {
        return WriteRaw(str.data(), (DWORD)str.size());
    };

    size_t currentOffset = 0;
    auto AppendRaw = [&WriteRaw, &currentOffset](const void* pData, DWORD len) -> bool {
        if (!WriteRaw(pData, len)) return false;
        currentOffset += len;
        return true;
    };

    auto AppendStr = [&AppendRaw](const std::string& str) -> bool {
        return AppendRaw(str.data(), (DWORD)str.size());
    };

    // Tulis PDF 1.4 manual
    AppendStr("%PDF-1.4\n%\xE2\xE3\xCF\xD3\n");

    std::vector<size_t> xrefOffsets;
    xrefOffsets.push_back(0); // Objek 0 dummy

    int numPages = (int)validImages.size();
    int catalogId = 1;
    int pagesId = 2;
    int nextId = 3;

    struct PageEntry {
        int pageId;
        int contentId;
        int imageId;
    };
    std::vector<PageEntry> pageEntries;
    for (int i = 0; i < numPages; i++) {
        PageEntry pe;
        pe.pageId = nextId++;
        pe.contentId = nextId++;
        pe.imageId = nextId++;
        pageEntries.push_back(pe);
    }
    int totalObjects = nextId - 1;
    xrefOffsets.resize(totalObjects + 1, 0);

    // Obj 1: Catalog
    xrefOffsets[catalogId] = currentOffset;
    {
        std::ostringstream ss;
        ss << catalogId << " 0 obj\n"
           << "<< /Type /Catalog /Pages " << pagesId << " 0 R >>\n"
           << "endobj\n";
        AppendStr(ss.str());
    }

    // Obj 2: Pages
    xrefOffsets[pagesId] = currentOffset;
    {
        std::ostringstream ss;
        ss << pagesId << " 0 obj\n"
           << "<< /Type /Pages /Count " << numPages << " /Kids [";
        for (int i = 0; i < numPages; i++) {
            ss << " " << pageEntries[i].pageId << " 0 R";
        }
        ss << " ] >>\nendobj\n";
        AppendStr(ss.str());
    }

    // Per Page Objects
    for (int i = 0; i < numPages; i++) {
        const auto& img = validImages[i];
        const auto& pe = pageEntries[i];

        // Page Object
        xrefOffsets[pe.pageId] = currentOffset;
        {
            std::ostringstream ss;
            ss << pe.pageId << " 0 obj\n"
               << "<< /Type /Page /Parent " << pagesId << " 0 R\n"
               << "   /MediaBox [0 0 " << img.mediaWidth << " " << img.mediaHeight << "]\n"
               << "   /Contents " << pe.contentId << " 0 R\n"
               << "   /Resources << /XObject << /Im0 " << pe.imageId << " 0 R >> >>\n"
               << ">>\nendobj\n";
            AppendStr(ss.str());
        }

        // Contents Object
        xrefOffsets[pe.contentId] = currentOffset;
        {
            std::ostringstream contentStream;
            contentStream << "q\n"
                          << img.mediaWidth << " 0 0 " << img.mediaHeight << " 0 0 cm\n"
                          << "/Im0 Do\n"
                          << "Q\n";
            std::string contentStr = contentStream.str();

            std::ostringstream ss;
            ss << pe.contentId << " 0 obj\n"
               << "<< /Length " << contentStr.size() << " >>\n"
               << "stream\n"
               << contentStr
               << "endstream\nendobj\n";
            AppendStr(ss.str());
        }

        // Image XObject
        xrefOffsets[pe.imageId] = currentOffset;
        {
            std::ostringstream ss;
            ss << pe.imageId << " 0 obj\n"
               << "<< /Type /XObject /Subtype /Image\n"
               << "   /Width " << img.pixelWidth << "\n"
               << "   /Height " << img.pixelHeight << "\n"
               << "   /ColorSpace /DeviceRGB\n"
               << "   /BitsPerComponent 8\n"
               << "   /Filter /DCTDecode\n"
               << "   /Length " << img.jpegData.size() << "\n"
               << ">>\nstream\n";
            AppendStr(ss.str());
            AppendRaw(img.jpegData.data(), (DWORD)img.jpegData.size());
            AppendStr("\nendstream\nendobj\n");
        }
    }

    // xref table
    size_t startXref = currentOffset;
    {
        std::ostringstream ss;
        ss << "xref\n0 " << (totalObjects + 1) << "\n";
        char entry[32];
        snprintf(entry, sizeof(entry), "%010zu %05d f \n", (size_t)0, 65535);
        ss << entry;
        for (int i = 1; i <= totalObjects; i++) {
            snprintf(entry, sizeof(entry), "%010zu %05d n \n", xrefOffsets[i], 0);
            ss << entry;
        }
        ss << "trailer\n"
           << "<< /Size " << (totalObjects + 1) << "\n"
           << "   /Root " << catalogId << " 0 R\n"
           << ">>\n"
           << "startxref\n"
           << startXref << "\n"
           << "%%EOF\n";
        AppendStr(ss.str());
    }

    CloseHandle(hFile);

    WriteLog(L"ExecutePdf: Finished successfully. Total %zu bytes written.", currentOffset);

    // Shell change notifications
    SHChangeNotify(SHCNE_CREATE, SHCNF_PATHW, outPdfPath, NULL);
    WCHAR drive[_MAX_DRIVE] = { 0 }, d[_MAX_DIR] = { 0 }, fname[_MAX_FNAME] = { 0 }, ext[_MAX_EXT] = { 0 };
    _wsplitpath(outPdfPath, drive, d, fname, ext);
    std::wstring folder = std::wstring(drive) + d;
    SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_PATHW, folder.c_str(), NULL);

    // Tampilkan balon tray
    std::wstring outFileName = std::wstring(fname) + ext;
    WCHAR balloonMsg[256];
    swprintf(balloonMsg, 256, L"PDF dibuat: %ls", outFileName.c_str());
    ShowTrayBalloon(L"Image to PDF Selesai", balloonMsg);
}

// Fallback File Picker Dialog jika tidak ada PDF yang dipilih di Explorer
bool PickPdfFile(WCHAR* outPathList, DWORD maxLen) {
    OPENFILENAMEW ofn = { sizeof(ofn) };
    ofn.lpstrFilter = L"PDF Documents (*.pdf)\0*.pdf\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = outPathList;
    ofn.nMaxFile = maxLen;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT;
    ofn.lpstrTitle = L"Pilih File PDF untuk di-Split / Unmerge";
    outPathList[0] = L'\0';
    return GetOpenFileNameW(&ofn) == TRUE;
}

// Kumpulkan target file PDF dari Explorer atau dialog picker
std::vector<std::wstring> GatherPdfTargets() {
    std::vector<std::wstring> pdfTargets;
    for (const auto& path : g_smartFiles) {
        if (path.length() >= 4) {
            std::wstring ext = path.substr(path.length() - 4);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
            if (ext == L".pdf") {
                pdfTargets.push_back(path);
            }
        }
    }

    if (pdfTargets.empty()) {
        WCHAR fileBuf[8192];
        if (!PickPdfFile(fileBuf, 8192)) return pdfTargets;

        WCHAR* p = fileBuf;
        std::wstring baseDir(p);
        p += wcslen(p) + 1;

        if (*p == L'\0') {
            pdfTargets.push_back(baseDir);
        } else {
            while (*p) {
                std::wstring fullPath = baseDir + L"\\" + p;
                pdfTargets.push_back(fullPath);
                p += wcslen(p) + 1;
            }
        }
    }
    return pdfTargets;
}

// Helper aman pencarian byte sequence (pengganti memmem yang portabel)
const unsigned char* FindBytes(const unsigned char* hay, size_t hayLen, const void* needle, size_t needleLen) {
    if (!hay || !needle || needleLen == 0 || hayLen < needleLen) return NULL;
    const unsigned char* n = (const unsigned char*)needle;
    for (size_t i = 0; i <= hayLen - needleLen; i++) {
        if (hay[i] == n[0] && memcmp(hay + i, n, needleLen) == 0) {
            return hay + i;
        }
    }
    return NULL;
}

// Generate single-page PDF 1.4 murni untuk setiap halaman yang diekstrak
bool WriteSinglePagePdf(const std::wstring& outPath, const unsigned char* pJpeg, size_t jpegSize, UINT w, UINT h) {
    HANDLE hFile = CreateFileW(outPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    auto WriteRaw = [hFile](const void* pData, DWORD len) -> bool {
        DWORD written = 0;
        return WriteFile(hFile, pData, len, &written, NULL) && (written == len);
    };

    size_t currentOffset = 0;
    auto AppendRaw = [&WriteRaw, &currentOffset](const void* pData, DWORD len) -> bool {
        if (!WriteRaw(pData, len)) return false;
        currentOffset += len;
        return true;
    };
    auto AppendStr = [&AppendRaw](const std::string& str) -> bool {
        return AppendRaw(str.data(), (DWORD)str.size());
    };

    AppendStr("%PDF-1.4\n%\xE2\xE3\xCF\xD3\n");
    std::vector<size_t> xrefOffsets(6, 0);

    double mediaW = w * 0.72; if (mediaW < 1.0) mediaW = 1.0;
    double mediaH = h * 0.72; if (mediaH < 1.0) mediaH = 1.0;

    // Obj 1: Catalog
    xrefOffsets[1] = currentOffset;
    AppendStr("1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n");

    // Obj 2: Pages
    xrefOffsets[2] = currentOffset;
    AppendStr("2 0 obj\n<< /Type /Pages /Kids [ 3 0 R ] /Count 1 >>\nendobj\n");

    // Obj 3: Page
    xrefOffsets[3] = currentOffset;
    {
        std::ostringstream ss;
        ss.precision(2);
        ss << std::fixed;
        ss << "3 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [ 0 0 "
           << mediaW << " " << mediaH
           << " ] /Resources << /XObject << /Im0 5 0 R >> >> /Contents 4 0 R >>\nendobj\n";
        AppendStr(ss.str());
    }

    // Obj 4: Contents
    xrefOffsets[4] = currentOffset;
    {
        std::ostringstream streamContent;
        streamContent.precision(2);
        streamContent << std::fixed;
        streamContent << "q " << mediaW << " 0 0 " << mediaH << " 0 0 cm /Im0 Do Q\n";
        std::string sStream = streamContent.str();

        std::ostringstream ss;
        ss << "4 0 obj\n<< /Length " << sStream.size() << " >>\nstream\n"
           << sStream << "endstream\nendobj\n";
        AppendStr(ss.str());
    }

    // Obj 5: Image
    xrefOffsets[5] = currentOffset;
    {
        std::ostringstream ss;
        ss << "5 0 obj\n<< /Type /XObject /Subtype /Image /Width " << w
           << " /Height " << h
           << " /ColorSpace /DeviceRGB /BitsPerComponent 8 /Filter /DCTDecode /Length "
           << jpegSize << " >>\nstream\n";
        AppendStr(ss.str());
        AppendRaw(pJpeg, (DWORD)jpegSize);
        AppendStr("\nendstream\nendobj\n");
    }

    // XRef
    size_t xrefStart = currentOffset;
    {
        std::ostringstream ss;
        ss << "xref\n0 6\n0000000000 65535 f \n";
        for (int i = 1; i <= 5; i++) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%010zu 00000 n \n", xrefOffsets[i]);
            ss << buf;
        }
        ss << "trailer\n<< /Size 6 /Root 1 0 R >>\nstartxref\n"
           << xrefStart << "\n%%EOF\n";
        AppendStr(ss.str());
    }

    CloseHandle(hFile);
    return true;
}

// Engine PDF Split & Unmerge Halaman Gambar
void ExecutePdfSplit() {
    std::vector<std::wstring> pdfTargets = GatherPdfTargets();
    if (pdfTargets.empty()) {
        WriteLog(L"ExecutePdfSplit: targets empty, aborting.");
        return;
    }

    int totalPagesExtracted = 0;

    for (const auto& pdfPath : pdfTargets) {
        WriteLog(L"ExecutePdfSplit: Processing %ls", pdfPath.c_str());

        HANDLE hFile = CreateFileW(pdfPath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            WriteLog(L"  ExecutePdfSplit: Failed to open file, err=%lu", GetLastError());
            continue;
        }

        LARGE_INTEGER liSize;
        if (!GetFileSizeEx(hFile, &liSize) || liSize.QuadPart <= 0 || liSize.QuadPart > 500 * 1024 * 1024) {
            WriteLog(L"  ExecutePdfSplit: Invalid file size");
            CloseHandle(hFile);
            continue;
        }

        DWORD fileSize = (DWORD)liSize.QuadPart;
        std::vector<unsigned char> buf(fileSize);
        DWORD bytesRead = 0;
        if (!ReadFile(hFile, buf.data(), fileSize, &bytesRead, NULL) || bytesRead != fileSize) {
            WriteLog(L"  ExecutePdfSplit: ReadFile failed or incomplete");
            CloseHandle(hFile);
            continue;
        }
        CloseHandle(hFile);

        WCHAR drive[_MAX_DRIVE] = { 0 }, dir[_MAX_DIR] = { 0 }, fname[_MAX_FNAME] = { 0 }, ext[_MAX_EXT] = { 0 };
        _wsplitpath(pdfPath.c_str(), drive, dir, fname, ext);
        std::wstring baseFolder = std::wstring(drive) + dir;

        struct ExtractedImage {
            std::vector<unsigned char> data;
            UINT width;
            UINT height;
        };
        std::vector<ExtractedImage> images;

        size_t pos = 0;
        const char* streamMarker = "stream";
        size_t streamMarkerLen = 6;

        while (pos + 3 < fileSize) {
            const unsigned char* pStream = FindBytes(buf.data() + pos, fileSize - pos, streamMarker, streamMarkerLen);
            if (!pStream) break;

            size_t streamStart = (pStream - buf.data()) + streamMarkerLen;
            while (streamStart < fileSize && (buf[streamStart] == '\r' || buf[streamStart] == '\n')) {
                streamStart++;
            }

            // Periksa signature JPEG SOI (0xFF 0xD8 0xFF)
            if (streamStart + 3 < fileSize &&
                buf[streamStart] == 0xFF &&
                buf[streamStart + 1] == 0xD8 &&
                buf[streamStart + 2] == 0xFF) {

                const char* endMarker = "endstream";
                const unsigned char* pEnd = FindBytes(buf.data() + streamStart, fileSize - streamStart, endMarker, 9);
                if (pEnd) {
                    size_t rawEnd = pEnd - buf.data();
                    while (rawEnd > streamStart && (buf[rawEnd - 1] == '\r' || buf[rawEnd - 1] == '\n')) {
                        rawEnd--;
                    }

                    // Cari JPEG EOI marker 0xFF 0xD9
                    if (rawEnd >= streamStart + 2 && buf[rawEnd - 2] == 0xFF && buf[rawEnd - 1] == 0xD9) {
                        size_t imgLen = rawEnd - streamStart;
                        if (imgLen > 100) {
                            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, imgLen);
                            if (hMem) {
                                void* pLocked = GlobalLock(hMem);
                                memcpy(pLocked, buf.data() + streamStart, imgLen);
                                GlobalUnlock(hMem);

                                IStream* pStreamObj = NULL;
                                if (SUCCEEDED(CreateStreamOnHGlobal(hMem, TRUE, &pStreamObj)) && pStreamObj) {
                                    Bitmap testBmp(pStreamObj);
                                    if (testBmp.GetLastStatus() == Ok && testBmp.GetWidth() > 0 && testBmp.GetHeight() > 0) {
                                        ExtractedImage img;
                                        img.width = testBmp.GetWidth();
                                        img.height = testBmp.GetHeight();
                                        img.data.assign(buf.data() + streamStart, buf.data() + rawEnd);
                                        images.push_back(img);
                                        WriteLog(L"  ExecutePdfSplit: Valid JPEG extracted #%zu (%ux%u, %zu bytes)", images.size(), img.width, img.height, imgLen);
                                    }
                                    pStreamObj->Release();
                                }
                            }
                        }
                    }
                    pos = pEnd - buf.data() + 9;
                    continue;
                }
            }
            pos = streamStart;
        }

        if (images.empty()) {
            WriteLog(L"ExecutePdfSplit: No valid JPEG image pages found in %ls", pdfPath.c_str());
            ShowTrayBalloon(L"PDF Split", L"Tidak ditemukan halaman gambar yang dapat di-unmerge pada file ini.");
            continue;
        }

        // Simpan setiap halaman yang diekstrak sebagai .jpg dan .pdf di folder yang sama
        for (size_t i = 0; i < images.size(); i++) {
            WCHAR pageSuffix[64];
            swprintf(pageSuffix, 64, L"-page-%02d", (int)(i + 1));

            WCHAR outJpgPath[MAX_PATH];
            MakeUniquePath(pdfPath.c_str(), pageSuffix, L"jpg", outJpgPath, MAX_PATH);

            WCHAR outSinglePdfPath[MAX_PATH];
            MakeUniquePath(pdfPath.c_str(), pageSuffix, L"pdf", outSinglePdfPath, MAX_PATH);

            // 1. Tulis file JPG
            HANDLE hJpg = CreateFileW(outJpgPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hJpg != INVALID_HANDLE_VALUE) {
                DWORD written = 0;
                WriteFile(hJpg, images[i].data.data(), (DWORD)images[i].data.size(), &written, NULL);
                CloseHandle(hJpg);
                SHChangeNotify(SHCNE_CREATE, SHCNF_PATHW, outJpgPath, NULL);
                WriteLog(L"  ExecutePdfSplit: Created %ls", outJpgPath);
            }

            // 2. Tulis file single-page PDF
            if (WriteSinglePagePdf(outSinglePdfPath, images[i].data.data(), images[i].data.size(), images[i].width, images[i].height)) {
                SHChangeNotify(SHCNE_CREATE, SHCNF_PATHW, outSinglePdfPath, NULL);
                WriteLog(L"  ExecutePdfSplit: Created %ls", outSinglePdfPath);
            }

            totalPagesExtracted++;
        }

        SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_PATHW, baseFolder.c_str(), NULL);
    }

    if (totalPagesExtracted > 0) {
        WCHAR balloonMsg[256];
        swprintf(balloonMsg, 256, L"Berhasil memecah %d halaman menjadi file JPG & PDF terpisah!", totalPagesExtracted);
        ShowTrayBalloon(L"PDF Split Selesai", balloonMsg);
    }
}

// Copy text string to Windows Clipboard
void CopyToClipboard(const wchar_t* text) {
    if (!text || !OpenClipboard(NULL)) return;
    EmptyClipboard();
    size_t len = (wcslen(text) + 1) * sizeof(wchar_t);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
    if (hMem) {
        memcpy(GlobalLock(hMem), text, len);
        GlobalUnlock(hMem);
        SetClipboardData(CF_UNICODETEXT, hMem);
    }
    CloseClipboard();
}

// Quick Eyedropper Color Picker di posisi kursor spawn
void ExecutePickColor() {
    HDC hdc = GetDC(NULL);
    if (!hdc) return;
    COLORREF color = GetPixel(hdc, g_spawnPos.x, g_spawnPos.y);
    ReleaseDC(NULL, hdc);

    int r = GetRValue(color);
    int g = GetGValue(color);
    int b = GetBValue(color);

    WCHAR hexBuf[64];
    swprintf(hexBuf, 64, L"#%02X%02X%02X", r, g, b);
    CopyToClipboard(hexBuf);

    WCHAR notifyMsg[128];
    swprintf(notifyMsg, 128, L"%ls (RGB: %d, %d, %d) tersalin ke clipboard!", hexBuf, r, g, b);
    ShowTrayBalloon(L"Color Picker", notifyMsg);
    WriteLog(L"ExecutePickColor: %ls copied at (%ld, %ld)", hexBuf, g_spawnPos.x, g_spawnPos.y);
}

// Screenshot satu layar penuh
void CaptureFullScreen() {
    int w = GetSystemMetrics(SM_CXSCREEN);
    int h = GetSystemMetrics(SM_CYSCREEN);
    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hbm = CreateCompatibleBitmap(hdcScreen, w, h);
    SelectObject(hdcMem, hbm);
    BitBlt(hdcMem, 0, 0, w, h, hdcScreen, 0, 0, SRCCOPY);

    Bitmap bmp(hbm, NULL);
    CLSID clsid;
    GetEncoderClsid(L"image/png", &clsid);

    OPENFILENAMEW ofn = { sizeof(ofn) };
    WCHAR outPath[MAX_PATH] = L"screenshot.png";
    ofn.lpstrFilter = L"PNG Image (*.png)\0*.png\0";
    ofn.lpstrFile = outPath;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT;
    if (GetSaveFileNameW(&ofn)) {
        bmp.Save(outPath, &clsid, NULL);
    }

    DeleteObject(hbm);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);
}

// Snipping Tool Overlay Window Proc
POINT g_snipStart, g_snipCur;
bool g_isSnipping = false;

LRESULT CALLBACK SnipWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_LBUTTONDOWN:
        g_isSnipping = true;
        g_snipStart.x = GET_X_LPARAM(lParam);
        g_snipStart.y = GET_Y_LPARAM(lParam);
        g_snipCur = g_snipStart;
        SetCapture(hWnd);
        return 0;
    case WM_MOUSEMOVE:
        if (g_isSnipping) {
            g_snipCur.x = GET_X_LPARAM(lParam);
            g_snipCur.y = GET_Y_LPARAM(lParam);
            InvalidateRect(hWnd, NULL, FALSE);
        }
        return 0;
    case WM_LBUTTONUP:
        if (g_isSnipping) {
            g_isSnipping = false;
            ReleaseCapture();
            ShowWindow(hWnd, SW_HIDE);

            int x = std::min(g_snipStart.x, g_snipCur.x);
            int y = std::min(g_snipStart.y, g_snipCur.y);
            int w = abs(g_snipCur.x - g_snipStart.x);
            int h = abs(g_snipCur.y - g_snipStart.y);

            if (w > 5 && h > 5) {
                HDC hdcScreen = GetDC(NULL);
                HDC hdcMem = CreateCompatibleDC(hdcScreen);
                HBITMAP hbm = CreateCompatibleBitmap(hdcScreen, w, h);
                SelectObject(hdcMem, hbm);
                BitBlt(hdcMem, 0, 0, w, h, hdcScreen, x, y, SRCCOPY);

                Bitmap bmp(hbm, NULL);
                CLSID clsid;
                GetEncoderClsid(L"image/png", &clsid);

                OPENFILENAMEW ofn = { sizeof(ofn) };
                WCHAR outPath[MAX_PATH] = L"crop_screenshot.png";
                ofn.lpstrFilter = L"PNG Image (*.png)\0*.png\0";
                ofn.lpstrFile = outPath;
                ofn.nMaxFile = MAX_PATH;
                ofn.Flags = OFN_OVERWRITEPROMPT;
                if (GetSaveFileNameW(&ofn)) {
                    bmp.Save(outPath, &clsid, NULL);
                }

                DeleteObject(hbm);
                DeleteDC(hdcMem);
                ReleaseDC(NULL, hdcScreen);
            }
            DestroyWindow(hWnd);
            g_hWndSnip = NULL;
        }
        return 0;
    case WM_RBUTTONDOWN:
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE || msg == WM_RBUTTONDOWN) {
            ReleaseCapture();
            DestroyWindow(hWnd);
            g_hWndSnip = NULL;
        }
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        int sw = GetSystemMetrics(SM_CXSCREEN);
        int sh = GetSystemMetrics(SM_CYSCREEN);

        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBM = CreateCompatibleBitmap(hdc, sw, sh);
        SelectObject(memDC, memBM);

        Graphics g(memDC);
        SolidBrush dimBrush(Color(120, 0, 0, 0));
        g.FillRectangle(&dimBrush, 0, 0, sw, sh);

        if (g_isSnipping) {
            int x = std::min(g_snipStart.x, g_snipCur.x);
            int y = std::min(g_snipStart.y, g_snipCur.y);
            int w = abs(g_snipCur.x - g_snipStart.x);
            int h = abs(g_snipCur.y - g_snipStart.y);

            g.SetCompositingMode(CompositingModeSourceCopy);
            SolidBrush clearBrush(Color(0, 0, 0, 0));
            g.FillRectangle(&clearBrush, x, y, w, h);

            g.SetCompositingMode(CompositingModeSourceOver);
            Pen borderPen(Color(255, 0, 180, 255), 2.0f);
            g.DrawRectangle(&borderPen, x, y, w, h);
        }

        BitBlt(hdc, 0, 0, sw, sh, memDC, 0, 0, SRCCOPY);
        DeleteObject(memBM);
        DeleteDC(memDC);
        EndPaint(hWnd, &ps);
        return 0;
    }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

void StartSnipping() {
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = SnipWndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"RadialSnipWnd";
    wc.hCursor = LoadCursor(NULL, IDC_CROSS);
    RegisterClassExW(&wc);

    g_hWndSnip = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"RadialSnipWnd", L"",
        WS_POPUP,
        0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
        NULL, NULL, GetModuleHandle(NULL), NULL
    );

    ShowWindow(g_hWndSnip, SW_SHOW);
    UpdateWindow(g_hWndSnip);
}

// Render Radial Wheel dengan visual modern premium ala Rovyl
void RenderWheel(HWND hWnd) {
    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    
    BITMAPINFO bmi = { 0 };
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = WHEEL_SIZE;
    bmi.bmiHeader.biHeight = -WHEEL_SIZE;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = NULL;
    HBITMAP hbm = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
    SelectObject(hdcMem, hbm);

    Graphics g(hdcMem);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

    // Ambil daftar items dinamis berdasarkan state dan page
    std::vector<std::wstring> currentLabels;
    if (g_state == STATE_MAIN) {
        if (g_mainPage == 0) {
            for (auto& it : g_page0Items) currentLabels.push_back(it.label);
        } else if (g_mainPage == 1) {
            for (auto& it : g_page1Items) currentLabels.push_back(it.label);
        }
    } else if (g_state == STATE_FORMAT) {
        for (int i = 0; i < FORMAT_COUNT; i++) currentLabels.push_back(FORMAT_ITEMS[i]);
    } else if (g_state == STATE_SCREENSHOT) {
        for (int i = 0; i < SNIP_COUNT; i++) currentLabels.push_back(SNIP_ITEMS[i]);
    }

    int count = (int)currentLabels.size();
    if (count <= 0) count = 1;

    float span = 360.0f / count;
    float cx = WHEEL_SIZE / 2.0f;
    float cy = WHEEL_SIZE / 2.0f;

    // 1. Outer Ambient Shadow Ring (Halus & Elegan)
    Pen shadowPen(Color(35, 0, 0, 0), 12.0f);
    g.DrawEllipse(&shadowPen, cx - (R_OUTER + 4), cy - (R_OUTER + 4), (R_OUTER + 4) * 2, (R_OUTER + 4) * 2);

    // 2. Render Tiap Sektor Pie Menu dengan Gap Tipis (Radial Separator Ala Rovyl)
    float gapAngle = 2.2f; // Gap pemisah antar sektor (derajat)
    for (int i = 0; i < count; i++) {
        float startAngle = i * span + (gapAngle / 2.0f);
        float sweepAngle = span - gapAngle;
        bool isHovered = (i == g_hovered);

        // Warna tema: Deep Obsidian vs Electric Cyan/Indigo Glow
        Color sliceColor;
        Color strokeColor;
        if (isHovered) {
            sliceColor = Color(245, 14, 165, 233); // Vibrant Cyan Glow
            strokeColor = Color(255, 255, 255, 255);
        } else {
            sliceColor = Color(225, 22, 27, 34);  // Deep Slate Glass (Dark 90% opacity)
            strokeColor = Color(40, 255, 255, 255);
        }

        SolidBrush brush(sliceColor);
        Pen borderPen(strokeColor, isHovered ? 2.0f : 1.0f);

        GraphicsPath path;
        path.AddPie(cx - R_OUTER, cy - R_OUTER, R_OUTER * 2, R_OUTER * 2, startAngle, sweepAngle);
        g.FillPath(&brush, &path);
        g.DrawPath(&borderPen, &path);
    }

    // 3. Ring Pembatas Luar & Dalam (Accent Glow)
    Pen outerRingPen(Color(50, 255, 255, 255), 1.5f);
    g.DrawEllipse(&outerRingPen, cx - R_OUTER, cy - R_OUTER, R_OUTER * 2, R_OUTER * 2);

    Pen innerGlowPen(Color(45, 255, 255, 255), 2.0f);
    g.DrawEllipse(&innerGlowPen, cx - (R_INNER + 2), cy - (R_INNER + 2), (R_INNER + 2) * 2, (R_INNER + 2) * 2);

    // 4. Center Hub (Tombol Tengah Cancel / Indicator)
    Color centerColor = (g_hovered == -2) ? Color(245, 225, 29, 72) : Color(250, 13, 17, 23);
    SolidBrush centerBrush(centerColor);
    g.FillEllipse(&centerBrush, cx - R_INNER, cy - R_INNER, R_INNER * 2, R_INNER * 2);

    Pen centerBorderPen(g_hovered == -2 ? Color(255, 255, 255, 255) : Color(60, 255, 255, 255), 1.5f);
    g.DrawEllipse(&centerBorderPen, cx - R_INNER, cy - R_INNER, R_INNER * 2, R_INNER * 2);

    // 5. Tipografi & Icons Glyph
    FontFamily fontFamily(L"Segoe UI");
    Gdiplus::Font font(&fontFamily, 9.5f, FontStyleBold, UnitPoint);
    Gdiplus::Font iconFont(&fontFamily, 14.0f, FontStyleRegular, UnitPoint);
    Gdiplus::Font closeFont(&fontFamily, 13.0f, FontStyleBold, UnitPoint);
    Gdiplus::Font smallFont(&fontFamily, 8.0f, FontStyleBold, UnitPoint);

    StringFormat strFormat;
    strFormat.SetAlignment(StringAlignmentCenter);
    strFormat.SetLineAlignment(StringAlignmentCenter);
    SolidBrush textBrush(Color(255, 255, 255, 255));
    SolidBrush dimTextBrush(Color(200, 255, 255, 255));

    // Glyph Icon per item untuk mempercantik visual
    auto GetItemGlyph = [](const wchar_t* item) -> const wchar_t* {
        if (wcscmp(item, L"Format Image") == 0) return L"\x27F3"; // ⟳
        if (wcscmp(item, L"Image to PDF") == 0) return L"\x25A4"; // ▤
        if (wcscmp(item, L"Screenshot") == 0) return L"\x25A3"; // ▣
        if (wcscmp(item, L"Compress") == 0) return L"\x25BC"; // ▼
        if (wcscmp(item, L"PDF Split") == 0) return L"\x2702"; // ✂
        if (wcscmp(item, L"YouTube") == 0) return L"\x25B6"; // ▶
        if (wcscmp(item, L"Netflix") == 0) return L"\x25C9"; // ◉
        if (wcscmp(item, L"GitHub") == 0) return L"\x2699"; // ⚙
        if (wcscmp(item, L"ChatGPT") == 0) return L"\x2728"; // ✨
        if (wcscmp(item, L"Google") == 0) return L"\x2315"; // ⌕
        if (wcscmp(item, L"Kembali") == 0) return L"\x21A9"; // ↩
        return L"\x25CB";
    };

    for (int i = 0; i < count; i++) {
        float midAngle = (i * span + span / 2.0f) * (PI / 180.0f);
        float midR = (R_OUTER + R_INNER) / 2.0f;
        float tx = cx + midR * cosf(midAngle);
        float ty = cy + midR * sinf(midAngle);

        const wchar_t* glyph = GetItemGlyph(currentLabels[i].c_str());
        RectF iconRect(tx - 35.0f, ty - 22.0f, 70.0f, 20.0f);
        RectF textRect(tx - 48.0f, ty + 1.0f, 96.0f, 22.0f);

        g.DrawString(glyph, -1, &iconFont, iconRect, &strFormat, (i == g_hovered) ? &textBrush : &dimTextBrush);
        g.DrawString(currentLabels[i].c_str(), -1, &font, textRect, &strFormat, &textBrush);
    }

    SolidBrush brandBrush(Color(255, 56, 189, 248)); // Sky Blue Accent
    SolidBrush closeBrush(Color(255, 255, 255, 255));
    SolidBrush pageDotBrush(Color(255, 56, 189, 248));
    SolidBrush pageDimBrush(Color(80, 255, 255, 255));

    if (g_hovered == -2) {
        RectF centerRect(cx - 35, cy - 14, 70, 28);
        g.DrawString(L"\x2715 BATAL", -1, &font, centerRect, &strFormat, &closeBrush);
    } else if (!g_smartFiles.empty() && g_state == STATE_MAIN) {
        WCHAR statusBuf[32];
        swprintf(statusBuf, 32, L"%d FILE", (int)g_smartFiles.size());
        RectF centerRect(cx - 35, cy - 16, 70, 16);
        g.DrawString(statusBuf, -1, &font, centerRect, &strFormat, &brandBrush);
        RectF subRect(cx - 35, cy + 1, 70, 12);
        g.DrawString(L"\x2715 Tutup", -1, &smallFont, subRect, &strFormat, &brandBrush);
    } else {
        const wchar_t* PAGE_TITLES[] = { L"MEDIA", L"WEB" };
        const wchar_t* pTitle = (g_state == STATE_MAIN) ? PAGE_TITLES[g_mainPage] : (g_state == STATE_FORMAT ? L"FORMAT" : L"SNIP");

        RectF titleRect(cx - 35, cy - 14, 70, 14);
        g.DrawString(pTitle, -1, &smallFont, titleRect, &strFormat, &closeBrush);

        if (g_state == STATE_MAIN) {
            float dotY = cy + 4.0f;
            float dotStartX = cx - 6.0f;
            for (int p = 0; p < TOTAL_PAGES; p++) {
                g.FillEllipse(p == g_mainPage ? &pageDotBrush : &pageDimBrush, dotStartX + p * 12.0f, dotY, 4.0f, 4.0f);
            }
        } else {
            RectF subRect(cx - 35, cy + 2, 70, 12);
            g.DrawString(L"\x2715 Tutup", -1, &smallFont, subRect, &strFormat, &brandBrush);
        }
    }

    POINT ptSrc = { 0, 0 };
    SIZE size = { WHEEL_SIZE, WHEEL_SIZE };
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    POINT ptDst = { g_spawnPos.x - WHEEL_SIZE / 2, g_spawnPos.y - WHEEL_SIZE / 2 };

    UpdateLayeredWindow(hWnd, hdcScreen, &ptDst, &size, hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);

    DeleteObject(hbm);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);
}

void UpdateHover(int mouseX, int mouseY) {
    float dx = (float)(mouseX - WHEEL_SIZE / 2);
    float dy = (float)(mouseY - WHEEL_SIZE / 2);
    float dist = sqrtf(dx * dx + dy * dy);

    int prev = g_hovered;

    // Radius dalam (< R_INNER) = Tombol Close / Batal
    if (dist < R_INNER) {
        g_hovered = -2; // -2 = Center close button
    } else {
        // Gestural Flick / Polar Angle: dihitung dari arah dorongan mouse
        float angle = atan2f(dy, dx) * (180.0f / PI);
        if (angle < 0) angle += 360.0f;

        int count = 5;
        if (g_state == STATE_MAIN) {
            count = (g_mainPage == 0) ? (int)g_page0Items.size() : (int)g_page1Items.size();
        } else if (g_state == STATE_FORMAT) {
            count = FORMAT_COUNT;
        } else if (g_state == STATE_SCREENSHOT) {
            count = SNIP_COUNT;
        }
        if (count <= 0) count = 1;

        float span = 360.0f / count;
        g_hovered = (int)(angle / span);
    }

    if (prev != g_hovered) {
        RenderWheel(g_hWndWheel);
    }
}

// Helper untuk menutup wheel dan mengosongkan RAM / working set ke OS
void DismissWheel(HWND hWnd) {
    if (IsWindowVisible(hWnd)) {
        ShowWindow(hWnd, SW_HIDE);
    }
    // Kosongkan cache file seleksi yang sudah selesai
    g_smartFiles.clear();
    g_smartFiles.shrink_to_fit();
    g_hovered = -1;

    // Bersihkan RAM ke level minimum saat aplikasi kembali idle
    SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
}

// ==============================================================================
// GUI WINDOW PENGATURAN (SETTINGS DASHBOARD)
// ==============================================================================
#define IDC_TAB_PAGES     3001
#define IDC_LIST_ITEMS    3002
#define IDC_EDIT_LABEL    3003
#define IDC_EDIT_TARGET   3004
#define IDC_BTN_ADD       3005
#define IDC_BTN_DELETE    3006
#define IDC_BTN_SAVE      3007
#define IDC_BTN_DEFAULT   3008

int g_selectedTab = 1; // Default ke Web Launcher (0: Media, 1: Web)
HWND g_hTabCtrl = NULL;
HWND g_hListCtrl = NULL;
HWND g_hEditLabel = NULL;
HWND g_hEditTarget = NULL;

void RefreshSettingsList() {
    if (!g_hListCtrl) return;
    SendMessageW(g_hListCtrl, LB_RESETCONTENT, 0, 0);

    const std::vector<WheelItem>& items = (g_selectedTab == 0) ? g_page0Items : g_page1Items;
    for (size_t i = 0; i < items.size(); i++) {
        WCHAR rowText[512];
        swprintf(rowText, 512, L"[%d] %ls  ->  %ls", (int)(i + 1), items[i].label.c_str(), items[i].target.c_str());
        SendMessageW(g_hListCtrl, LB_ADDSTRING, 0, (LPARAM)rowText);
    }
}

LRESULT CALLBACK SettingsWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_TAB_CLASSES };
        InitCommonControlsEx(&icex);

        // Tab Control
        g_hTabCtrl = CreateWindowExW(0, WC_TABCONTROLW, L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
            20, 15, 520, 30, hWnd, (HMENU)IDC_TAB_PAGES, GetModuleHandle(NULL), NULL);

        TCITEMW tie = { 0 };
        tie.mask = TCIF_TEXT;
        tie.pszText = (LPWSTR)L"Media Tools (Page 0)";
        TabCtrl_InsertItem(g_hTabCtrl, 0, &tie);
        tie.pszText = (LPWSTR)L"Web Launcher (Page 1)";
        TabCtrl_InsertItem(g_hTabCtrl, 1, &tie);
        TabCtrl_SetCurSel(g_hTabCtrl, g_selectedTab);

        // Listbox Items
        CreateWindowExW(0, L"STATIC", L"Daftar Sektor Wheel (Maksimal 5 Sektor per Layer):",
            WS_CHILD | WS_VISIBLE, 20, 55, 520, 18, hWnd, NULL, GetModuleHandle(NULL), NULL);

        g_hListCtrl = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY,
            20, 75, 520, 140, hWnd, (HMENU)IDC_LIST_ITEMS, GetModuleHandle(NULL), NULL);

        // Inputs
        CreateWindowExW(0, L"STATIC", L"Nama / Label:", WS_CHILD | WS_VISIBLE, 20, 230, 100, 18, hWnd, NULL, GetModuleHandle(NULL), NULL);
        g_hEditLabel = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 120, 227, 420, 24, hWnd, (HMENU)IDC_EDIT_LABEL, GetModuleHandle(NULL), NULL);

        CreateWindowExW(0, L"STATIC", L"URL / Target:", WS_CHILD | WS_VISIBLE, 20, 265, 100, 18, hWnd, NULL, GetModuleHandle(NULL), NULL);
        g_hEditTarget = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 120, 262, 420, 24, hWnd, (HMENU)IDC_EDIT_TARGET, GetModuleHandle(NULL), NULL);

        // Buttons
        CreateWindowExW(0, L"BUTTON", L"+ Tambah / Update", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            120, 300, 140, 30, hWnd, (HMENU)IDC_BTN_ADD, GetModuleHandle(NULL), NULL);

        CreateWindowExW(0, L"BUTTON", L"- Hapus Sektor", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            270, 300, 120, 30, hWnd, (HMENU)IDC_BTN_DELETE, GetModuleHandle(NULL), NULL);

        CreateWindowExW(0, L"BUTTON", L"Reset Default", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            400, 300, 140, 30, hWnd, (HMENU)IDC_BTN_DEFAULT, GetModuleHandle(NULL), NULL);

        CreateWindowExW(0, L"BUTTON", L"Simpan & Terapkan", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            380, 350, 160, 34, hWnd, (HMENU)IDC_BTN_SAVE, GetModuleHandle(NULL), NULL);

        RefreshSettingsList();
        return 0;
    }
    case WM_NOTIFY: {
        NMHDR* pnm = (NMHDR*)lParam;
        if (pnm->idFrom == IDC_TAB_PAGES && pnm->code == TCN_SELCHANGE) {
            g_selectedTab = TabCtrl_GetCurSel(g_hTabCtrl);
            RefreshSettingsList();
            SetWindowTextW(g_hEditLabel, L"");
            SetWindowTextW(g_hEditTarget, L"");
        }
        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == IDC_LIST_ITEMS && HIWORD(wParam) == LBN_SELCHANGE) {
            int sel = (int)SendMessageW(g_hListCtrl, LB_GETCURSEL, 0, 0);
            if (sel != LB_ERR) {
                const std::vector<WheelItem>& items = (g_selectedTab == 0) ? g_page0Items : g_page1Items;
                if (sel < (int)items.size()) {
                    SetWindowTextW(g_hEditLabel, items[sel].label.c_str());
                    SetWindowTextW(g_hEditTarget, items[sel].target.c_str());
                }
            }
        } else if (id == IDC_BTN_ADD) {
            WCHAR lbl[128], tgt[512];
            GetWindowTextW(g_hEditLabel, lbl, 128);
            GetWindowTextW(g_hEditTarget, tgt, 512);
            if (wcslen(lbl) == 0) {
                MessageBoxW(hWnd, L"Label / Nama tidak boleh kosong!", L"Perhatian", MB_OK | MB_ICONWARNING);
                return 0;
            }

            std::vector<WheelItem>& items = (g_selectedTab == 0) ? g_page0Items : g_page1Items;
            int sel = (int)SendMessageW(g_hListCtrl, LB_GETCURSEL, 0, 0);
            if (sel != LB_ERR && sel < (int)items.size()) {
                // Update item yang dipilih
                items[sel].label = lbl;
                items[sel].target = tgt;
            } else {
                // Tambah item baru jika belum 5
                if (items.size() >= 5) {
                    MessageBoxW(hWnd, L"Maksimal 5 sektor per layer wheel agar navigasi tetap akurat!", L"Batas Maksimum", MB_OK | MB_ICONWARNING);
                    return 0;
                }
                items.push_back({ lbl, tgt });
            }
            RefreshSettingsList();
        } else if (id == IDC_BTN_DELETE) {
            std::vector<WheelItem>& items = (g_selectedTab == 0) ? g_page0Items : g_page1Items;
            int sel = (int)SendMessageW(g_hListCtrl, LB_GETCURSEL, 0, 0);
            if (sel != LB_ERR && sel < (int)items.size()) {
                if (items.size() <= 2) {
                    MessageBoxW(hWnd, L"Minimal harus ada 2 sektor per layer wheel!", L"Perhatian", MB_OK | MB_ICONWARNING);
                    return 0;
                }
                items.erase(items.begin() + sel);
                RefreshSettingsList();
                SetWindowTextW(g_hEditLabel, L"");
                SetWindowTextW(g_hEditTarget, L"");
            }
        } else if (id == IDC_BTN_DEFAULT) {
            if (MessageBoxW(hWnd, L"Kembalikan sektor ke susunan bawaan default?", L"Konfirmasi", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                LoadDefaultWheelItems();
                RefreshSettingsList();
            }
        } else if (id == IDC_BTN_SAVE) {
            SaveWheelConfig();
            ShowTrayBalloon(L"Pengaturan Tersimpan", L"Susunan wheel berhasil diperbarui!");
            DestroyWindow(hWnd);
        }
        return 0;
    }
    case WM_CLOSE:
        DestroyWindow(hWnd);
        return 0;
    case WM_DESTROY:
        g_hWndSettings = NULL;
        SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

void OpenSettingsDialog() {
    if (g_hWndSettings && IsWindow(g_hWndSettings)) {
        SetForegroundWindow(g_hWndSettings);
        return;
    }

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = SettingsWndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"RadialSettingsWndClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    RegisterClassExW(&wc);

    int scrW = GetSystemMetrics(SM_CXSCREEN);
    int scrH = GetSystemMetrics(SM_CYSCREEN);
    int w = 575;
    int h = 440;
    int x = (scrW - w) / 2;
    int y = (scrH - h) / 2;

    g_hWndSettings = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        L"RadialSettingsWndClass",
        L"Radial Tool Pro - Pengaturan Wheel",
        WS_POPUPWINDOW | WS_CAPTION | WS_VISIBLE,
        x, y, w, h,
        NULL, NULL, GetModuleHandle(NULL), NULL
    );

    SetForegroundWindow(g_hWndSettings);
}

// Eksekusi pilihan sektor aktif (dipanggil saat mouse click atau trigger button release)
void ConfirmSelection(HWND hWnd) {
    if (g_hovered == -2 || g_hovered == -1) {
        // Lingkaran tengah atau tidak ada yang dipilih -> Tutup / Batal
        DismissWheel(hWnd);
        return;
    }

    int selected = g_hovered;
    if (g_state == STATE_MAIN) {
        if (g_mainPage == 0) {
            // Page 0: Media & Files
            if (selected >= 0 && selected < (int)g_page0Items.size()) {
                std::wstring target = g_page0Items[selected].target;
                if (target == L"action:format") {
                    g_state = STATE_FORMAT;
                    g_hovered = -1;
                    RenderWheel(hWnd);
                } else if (target == L"action:pdf") {
                    DismissWheel(hWnd);
                    ExecutePdf();
                } else if (target == L"action:screenshot") {
                    g_state = STATE_SCREENSHOT;
                    g_hovered = -1;
                    RenderWheel(hWnd);
                } else if (target == L"action:compress") {
                    DismissWheel(hWnd);
                    ExecuteCompress();
                } else if (target == L"action:split") {
                    DismissWheel(hWnd);
                    ExecutePdfSplit();
                } else if (target.rfind(L"http", 0) == 0) {
                    DismissWheel(hWnd);
                    ShellExecuteW(NULL, L"open", target.c_str(), NULL, NULL, SW_SHOWNORMAL);
                }
            }
        } else if (g_mainPage == 1) {
            // Page 1: Web Quick Launcher
            if (selected >= 0 && selected < (int)g_page1Items.size()) {
                DismissWheel(hWnd);
                std::wstring targetUrl = g_page1Items[selected].target;
                ShellExecuteW(NULL, L"open", targetUrl.c_str(), NULL, NULL, SW_SHOWNORMAL);
                WriteLog(L"Web Launcher: Opening %ls", targetUrl.c_str());
            }
        }
    } else if (g_state == STATE_FORMAT) {
        if (selected == 3) { // Kembali
            g_state = STATE_MAIN;
            g_hovered = -1;
            RenderWheel(hWnd);
        } else {
            DismissWheel(hWnd);
            if (selected == 0) ExecuteConvert(L"png");
            else if (selected == 1) ExecuteConvert(L"jpg");
            else if (selected == 2) ExecuteConvert(L"webp");
        }
    } else if (g_state == STATE_SCREENSHOT) {
        if (selected == 2) { // Kembali
            g_state = STATE_MAIN;
            g_hovered = -1;
            RenderWheel(hWnd);
        } else {
            DismissWheel(hWnd);
            if (selected == 0) StartSnipping();
            else if (selected == 1) CaptureFullScreen();
        }
    }
}

// Window Procedure untuk Radial Wheel
LRESULT CALLBACK WheelWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_APP_OPENWHEEL: {
        HWND hTargetForeground = (HWND)lParam;
        WriteLog(L"=== WM_APP_OPENWHEEL received ===");
        WriteLog(L"Step 1: DetectSelectedFiles with saved foreground=%p", hTargetForeground);
        g_smartFiles = DetectSelectedFiles(hTargetForeground);

        WriteLog(L"Step 2: Reset UI state, SetWindowPos, RenderWheel, SetForegroundWindow");
        g_state = STATE_MAIN;
        g_mainPage = 0; // Reset ke Page 0 (Media)
        g_hovered = -1;
        SetWindowPos(hWnd, HWND_TOPMOST, g_spawnPos.x - WHEEL_SIZE / 2, g_spawnPos.y - WHEEL_SIZE / 2, WHEEL_SIZE, WHEEL_SIZE, SWP_SHOWWINDOW);
        RenderWheel(hWnd);
        SetForegroundWindow(hWnd);
        return 0;
    }
    case WM_APP_SCROLLWHEEL:
    case WM_MOUSEWHEEL: {
        short delta = (short)HIWORD(wParam);
        if (g_state == STATE_MAIN) {
            if (delta > 0) {
                // Scroll Up -> Previous page
                g_mainPage = (g_mainPage - 1 + TOTAL_PAGES) % TOTAL_PAGES;
            } else if (delta < 0) {
                // Scroll Down -> Next page
                g_mainPage = (g_mainPage + 1) % TOTAL_PAGES;
            }
            g_hovered = -1;
            RenderWheel(hWnd);
        }
        return 0;
    }
    case WM_APP_EXECUTEWHEEL: {
        // Trigger button release (Hold-Aim-Release ala Rovyl)
        if (IsWindowVisible(hWnd)) {
            ConfirmSelection(hWnd);
        }
        return 0;
    }
    case WM_MOUSEMOVE:
        UpdateHover(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_LBUTTONDOWN: {
        ConfirmSelection(hWnd);
        return 0;
    }
    case WM_RBUTTONDOWN:
    case WM_KILLFOCUS:
        DismissWheel(hWnd);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// Global Low-Level Mouse Hook (Deteksi M4/M5 / XBUTTON1/XBUTTON2)
LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        MSLLHOOKSTRUCT* pMouse = (MSLLHOOKSTRUCT*)lParam;
        WORD btn = HIWORD(pMouse->mouseData);
        if (btn == XBUTTON1 || btn == XBUTTON2) {
            if (wParam == WM_XBUTTONDOWN) {
                HWND hForeground = GetForegroundWindow();
                GetCursorPos(&g_spawnPos);
                PostMessageW(g_hWndWheel, WM_APP_OPENWHEEL, 0, (LPARAM)hForeground);
                return 1;
            } else if (wParam == WM_XBUTTONUP) {
                // Hold, Aim, Release: Lepas trigger button langsung eksekusi sektor aktif
                if (IsWindowVisible(g_hWndWheel)) {
                    PostMessageW(g_hWndWheel, WM_APP_EXECUTEWHEEL, 0, 0);
                }
                return 1;
            }
        } else if (wParam == WM_MOUSEWHEEL && IsWindowVisible(g_hWndWheel)) {
            // Forward mouse wheel scroll ke radial window untuk paging antar-halaman
            PostMessageW(g_hWndWheel, WM_APP_SCROLLWHEEL, pMouse->mouseData, 0);
            return 1;
        }
    }
    return CallNextHookEx(g_hMouseHook, nCode, wParam, lParam);
}

// Tray Icon Window Proc
LRESULT CALLBACK TrayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_TRAYICON) {
        if (lParam == WM_RBUTTONUP) {
            POINT pt;
            GetCursorPos(&pt);
            HMENU hMenu = CreatePopupMenu();
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_SETTINGS, L"Pengaturan Wheel (Settings)");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_ABOUT, L"Radial Tool v1.9 (PDF Split Engine)");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"Exit");

            SetForegroundWindow(hWnd);
            TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, NULL);
            DestroyMenu(hMenu);
        }
    } else if (msg == WM_COMMAND) {
        if (LOWORD(wParam) == ID_TRAY_EXIT) {
            PostQuitMessage(0);
        } else if (LOWORD(wParam) == ID_TRAY_SETTINGS) {
            OpenSettingsDialog();
        } else if (LOWORD(wParam) == ID_TRAY_ABOUT) {
            WCHAR aboutMsg[512];
            swprintf(aboutMsg, 512,
                L"Radial Tool v1.9 (PDF Split Engine)\n"
                L"Build: %hs %hs\n\n"
                L"- PDF Split / Unmerge: Ekstrak halaman PDF jadi JPG & single PDF\n"
                L"- Settings GUI: Kustomisasi sektor wheel via Desktop GUI\n"
                L"- Hold-Aim-Release: M4/M5 gesture flick selection\n"
                L"- Scroll mouse: Ganti layer (MEDIA / WEB)\n"
                L"- Auto-Trim RAM: ~2-5 MB idle, 0%% CPU",
                __DATE__, __TIME__);
            MessageBoxW(hWnd, aboutMsg, L"Tentang Radial Tool v1.9", MB_OK | MB_ICONINFORMATION);
        }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    CoInitialize(NULL);

    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, NULL);

    LoadWheelConfig();

    WNDCLASSEXW tc = { sizeof(tc) };
    tc.lpfnWndProc = TrayWndProc;
    tc.hInstance = hInstance;
    tc.lpszClassName = L"RadialTrayClass";
    RegisterClassExW(&tc);
    HWND hWndTray = CreateWindowExW(0, L"RadialTrayClass", L"", 0, 0, 0, 0, 0, NULL, NULL, hInstance, NULL);
    g_hWndTray = hWndTray;

    memset(&g_nid, 0, sizeof(g_nid));
    g_nid.cbSize = sizeof(NOTIFYICONDATA);
    g_nid.hWnd = hWndTray;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcscpy(g_nid.szTip, L"Radial Tool v1.9 (Hold-Aim-Release)");
    Shell_NotifyIconW(NIM_ADD, &g_nid);

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = WheelWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"RadialWheelClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassExW(&wc);

    g_hWndWheel = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"RadialWheelClass", L"",
        WS_POPUP,
        0, 0, WHEEL_SIZE, WHEEL_SIZE,
        NULL, NULL, hInstance, NULL
    );

    g_hMouseHook = SetWindowsHookExW(WH_MOUSE_LL, LowLevelMouseProc, hInstance, 0);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_hMouseHook) UnhookWindowsHookEx(g_hMouseHook);
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
    GdiplusShutdown(g_gdiplusToken);
    CoUninitialize();

    return 0;
}
