// Capcom Arcade 2nd Stadium — MixedNuts Mod Loader (proxy dxgi.dll)
// Created by MixedNuts - https://github.com/MixedNutsJP/ca2s-mod-loader
// Licensed under the MIT License. See LICENSE for details.
//
// Placed in the game folder, this dxgi.dll is loaded instead of the system one
// (the exe imports dxgi.dll statically). It forwards every export to
// System32\dxgi.dll and starts MixedNuts\MixedNutsLoader.dll. All features live in
// the loader and its plugins, so this file should rarely need to change.
//
// The loader is started on the first CreateDXGIFactory* call, not in DllMain:
// the exe is packed and only unpacked after DllMain, and loading DLLs outside the
// loader lock is safer. Every later factory call runs the plugins' hooks first.
// Without the MixedNuts folder this DLL only forwards.

#include <windows.h>

#include <cwchar>

namespace {

constexpr wchar_t kGameExeName[] = L"CapcomArcade2ndStadium.exe";

struct ExportEntry {
    const char* name;
    bool createsFactory;
};

// Index is ordinal - 1. Keep in the same order as dxgi.def and stubs.asm.
constexpr ExportEntry kExports[] = {
    {"ApplyCompatResolutionQuirking", false},
    {"CompatString", false},
    {"CompatValue", false},
    {"DXGIDumpJournal", false},
    {"PIXBeginCapture", false},
    {"PIXEndCapture", false},
    {"PIXGetCaptureState", false},
    {"SetAppCompatStringPointer", false},
    {"UpdateHMDEmulationStatus", false},
    {"CreateDXGIFactory", true},
    {"CreateDXGIFactory1", true},
    {"CreateDXGIFactory2", true},
    {"DXGID3D10CreateDevice", false},
    {"DXGID3D10CreateLayeredDevice", false},
    {"DXGID3D10GetLayeredDeviceSize", false},
    {"DXGID3D10RegisterLayers", false},
    {"DXGIDeclareAdapterRemovalSupport", false},
    {"DXGIDisableVBlankVirtualization", false},
    {"DXGIGetDebugInterface1", false},
    {"DXGIReportAdapterConfiguration", false},
};
constexpr unsigned kExportCount = sizeof(kExports) / sizeof(kExports[0]);

using LoaderStartFn = void(WINAPI*)(const wchar_t* proxyPath);
using LoaderBeforeFactoryFn = void(WINAPI*)();

HMODULE g_self = nullptr;
bool g_isGame = false;  // CrashReport.exe in the same folder also loads this DLL.

SRWLOCK g_resolveLock = SRWLOCK_INIT;
HMODULE g_realDxgi = nullptr;
void* volatile g_realExports[kExportCount] = {};

SRWLOCK g_loaderLock = SRWLOCK_INIT;
bool g_loaderTried = false;
LoaderBeforeFactoryFn volatile g_beforeFactory = nullptr;

bool IsGameProcess() {
    wchar_t path[MAX_PATH];
    const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return false;
    }
    const wchar_t* slash = wcsrchr(path, L'\\');
    return _wcsicmp(slash != nullptr ? slash + 1 : path, kGameExeName) == 0;
}

void StartLoader() {
    wchar_t proxyPath[MAX_PATH];
    const DWORD length = GetModuleFileNameW(g_self, proxyPath, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return;
    }
    wchar_t loaderPath[MAX_PATH];
    wcscpy_s(loaderPath, proxyPath);
    wchar_t* slash = wcsrchr(loaderPath, L'\\');
    if (slash == nullptr) {
        return;
    }
    slash[1] = L'\0';
    if (wcscat_s(loaderPath, L"MixedNuts\\MixedNutsLoader.dll") != 0) {
        return;
    }

    HMODULE loader = LoadLibraryW(loaderPath);
    if (loader == nullptr) {
        return;  // Only the proxy is installed: forward and do nothing else.
    }
    auto start = reinterpret_cast<LoaderStartFn>(
        reinterpret_cast<void*>(GetProcAddress(loader, "MixedNutsLoaderStart")));
    auto beforeFactory = reinterpret_cast<LoaderBeforeFactoryFn>(
        reinterpret_cast<void*>(GetProcAddress(loader, "MixedNutsLoaderBeforeFactory")));
    if (start == nullptr || beforeFactory == nullptr) {
        return;
    }
    start(proxyPath);
    g_beforeFactory = beforeFactory;
}

void BeforeFactory() {
    if (!g_isGame) {
        return;
    }
    AcquireSRWLockExclusive(&g_loaderLock);
    if (!g_loaderTried) {
        g_loaderTried = true;
        StartLoader();
    }
    const LoaderBeforeFactoryFn hook = g_beforeFactory;
    ReleaseSRWLockExclusive(&g_loaderLock);
    if (hook != nullptr) {
        hook();
    }
}

bool LoadRealDxgi() {
    wchar_t path[MAX_PATH];
    const UINT length = GetSystemDirectoryW(path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH - 10) {
        return false;
    }
    wcscat_s(path, L"\\dxgi.dll");  // A bare name would load this DLL again.
    g_realDxgi = LoadLibraryW(path);
    return g_realDxgi != nullptr;
}

void* ResolveReal(unsigned index) {
    void* cached = InterlockedCompareExchangePointer(
        const_cast<void**>(&g_realExports[index]), nullptr, nullptr);
    if (cached != nullptr) {
        return cached;
    }

    void* resolved = nullptr;
    AcquireSRWLockExclusive(&g_resolveLock);
    if (g_realDxgi != nullptr || LoadRealDxgi()) {
        resolved = reinterpret_cast<void*>(GetProcAddress(g_realDxgi, kExports[index].name));
        if (resolved != nullptr) {
            InterlockedExchangePointer(const_cast<void**>(&g_realExports[index]), resolved);
        }
    }
    ReleaseSRWLockExclusive(&g_resolveLock);
    return resolved;
}

HRESULT WINAPI ReportUnavailable() {
    return E_FAIL;
}

}  // namespace

// Called by the stubs in stubs.asm with the export's index. Returns where to jump.
extern "C" void* proxy_resolve(unsigned index) {
    if (index >= kExportCount) {
        return reinterpret_cast<void*>(&ReportUnavailable);
    }
    if (kExports[index].createsFactory) {
        BeforeFactory();
    }
    void* real = ResolveReal(index);
    return real != nullptr ? real : reinterpret_cast<void*>(&ReportUnavailable);
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        g_self = instance;
        g_isGame = IsGameProcess();
    }
    return TRUE;
}
