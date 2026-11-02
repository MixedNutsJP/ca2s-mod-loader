// Capcom Arcade 2nd Stadium — MixedNuts Mod Loader (MixedNuts\MixedNutsLoader.dll)
// Created by MixedNuts - https://github.com/MixedNutsJP/ca2s-mod-loader
// Licensed under the MIT License. See LICENSE for details.
//
// Started by the proxy dxgi.dll on the game's first CreateDXGIFactory* call.
//
//   1. Loads MixedNuts\Mods\<name>\<name>.dll in folder-name order and calls
//      MixedNutsPluginInit (plugins register factory hooks there)
//   2. Before every CreateDXGIFactory* call, runs the registered hooks in load order
//
// A plugin that crashes in init or in a hook is logged and then left out, so it
// cannot take the game down with it.

#include <windows.h>

#include <algorithm>
#include <string>
#include <vector>

#include <mixednuts/log.hpp>
#include <mixednuts/plugin.h>

namespace {

constexpr char kVersion[] = "1.0.0";

using mixednuts::log::Write;

std::string Utf8(const std::wstring& text) {
    if (text.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                                         nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), out.data(), size,
                        nullptr, nullptr);
    return out;
}

struct Plugin {
    std::wstring name;
    std::wstring dir;
    MixedNutsApi api{};
};

struct Hook {
    size_t plugin = 0;
    MixedNutsFactoryHookFn fn = nullptr;
    void* ctx = nullptr;
    bool disabled = false;
};

HMODULE g_self = nullptr;
std::wstring g_gameDir;  // Ends with a backslash.
std::wstring g_rootDir;  // The MixedNuts folder, ends with a backslash.
std::vector<Plugin*> g_plugins;  // Pointers, so each api keeps its address.
std::vector<Hook> g_hooks;
bool g_initializing = false;
SRWLOCK g_hookLock = SRWLOCK_INIT;

size_t PluginIndex(const MixedNutsApi* api) {
    for (size_t i = 0; i < g_plugins.size(); ++i) {
        if (&g_plugins[i]->api == api) {
            return i;
        }
    }
    return SIZE_MAX;
}

int RegisterFactoryHook(const MixedNutsApi* self, MixedNutsFactoryHookFn fn, void* ctx) {
    const size_t index = PluginIndex(self);
    if (!g_initializing || index == SIZE_MAX || fn == nullptr) {
        return 0;
    }
    g_hooks.push_back(Hook{index, fn, ctx, false});
    return 1;
}

std::vector<std::wstring> ListPluginDirs() {
    std::vector<std::wstring> names;
    WIN32_FIND_DATAW data{};
    HANDLE find = FindFirstFileW((g_rootDir + L"Mods\\*").c_str(), &data);
    if (find == INVALID_HANDLE_VALUE) {
        return names;
    }
    do {
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 ||
            wcscmp(data.cFileName, L".") == 0 || wcscmp(data.cFileName, L"..") == 0) {
            continue;
        }
        names.push_back(data.cFileName);
    } while (FindNextFileW(find, &data));
    FindClose(find);
    std::sort(names.begin(), names.end(), [](const std::wstring& a, const std::wstring& b) {
        return _wcsicmp(a.c_str(), b.c_str()) < 0;
    });
    return names;
}

int CallInit(MixedNutsPluginInitFn init, const MixedNutsApi* api, DWORD& code) {
    __try {
        return init(api);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        code = GetExceptionCode();
        return -1;
    }
}

bool CallHook(MixedNutsFactoryHookFn fn, void* ctx, DWORD& code) {
    __try {
        fn(ctx);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        code = GetExceptionCode();
        return false;
    }
}

void LoadPlugins() {
    for (const std::wstring& name : ListPluginDirs()) {
        const std::wstring dir = g_rootDir + L"Mods\\" + name + L"\\";
        const std::wstring dll = dir + name + L".dll";
        if (GetFileAttributesW(dll.c_str()) == INVALID_FILE_ATTRIBUTES) {
            continue;  // Folders without a DLL of the same name are ignored.
        }

        SetDllDirectoryW(dir.c_str());  // Let the plugin find DLLs in its own folder.
        HMODULE module = LoadLibraryW(dll.c_str());
        SetDllDirectoryW(nullptr);
        if (module == nullptr) {
            Write("[NG] %s: cannot load (error %lu)", Utf8(name).c_str(), GetLastError());
            continue;
        }
        auto init = reinterpret_cast<MixedNutsPluginInitFn>(
            reinterpret_cast<void*>(GetProcAddress(module, MIXEDNUTS_PLUGIN_INIT)));
        if (init == nullptr) {
            Write("[NG] %s: not a MixedNuts plugin (no %s)", Utf8(name).c_str(),
                  MIXEDNUTS_PLUGIN_INIT);
            continue;
        }

        auto* plugin = new Plugin{name, dir, {}};
        plugin->api.size = sizeof(MixedNutsApi);
        plugin->api.version = MIXEDNUTS_API_VERSION;
        plugin->api.gameDir = g_gameDir.c_str();
        plugin->api.pluginDir = plugin->dir.c_str();
        plugin->api.RegisterFactoryHook = &RegisterFactoryHook;
        g_plugins.push_back(plugin);

        const size_t before = g_hooks.size();
        DWORD code = 0;
        g_initializing = true;
        const int result = CallInit(init, &plugin->api, code);
        g_initializing = false;
        if (result == 1) {
            Write("[OK] %s: loaded (%zu factory hooks)", Utf8(name).c_str(),
                  g_hooks.size() - before);
        } else {
            if (result == -1) {
                Write("[NG] %s: crashed during init (exception 0x%08lX)", Utf8(name).c_str(), code);
            } else {
                Write("[NG] %s: init failed (see its own log)", Utf8(name).c_str());
            }
            g_hooks.resize(before);  // Do not use what a failed plugin registered.
        }
    }
}

// A dxgi.dll from version 1.0.0 of this mod (the single-DLL version) wrote this log.
void ReportLeftovers() {
    const std::wstring oldLog = g_gameDir + L"ca2s-adapter-fix.log";
    if (GetFileAttributesW(oldLog.c_str()) != INVALID_FILE_ATTRIBUTES) {
        Write("[--] ca2s-adapter-fix.log in the game folder is from the old single-DLL version"
              " and can be deleted.");
    }
}

void Start(const wchar_t* proxyPath) {
    wchar_t dir[MAX_PATH];
    if (!mixednuts::log::ModuleDir(g_self, dir)) {
        return;
    }
    g_rootDir = dir;
    g_gameDir = g_rootDir.substr(0, g_rootDir.size() - 1);
    g_gameDir = g_gameDir.substr(0, g_gameDir.find_last_of(L'\\') + 1);

    const std::wstring ini = g_rootDir + L"loader.ini";
    const bool logEnabled = GetPrivateProfileIntW(L"General", L"Log", 1, ini.c_str()) != 0;
    mixednuts::log::Open(g_rootDir.c_str(), L"loader.log", logEnabled);
    Write("=== MixedNuts Mod Loader %s for Capcom Arcade 2nd Stadium / Created by MixedNuts ===",
          kVersion);
    Write("Proxy: %s", Utf8(proxyPath != nullptr ? proxyPath : L"").c_str());

    ReportLeftovers();
    LoadPlugins();
    Write("%zu plugins, %zu factory hooks", g_plugins.size(), g_hooks.size());
}

}  // namespace

extern "C" __declspec(dllexport) void WINAPI MixedNutsLoaderStart(const wchar_t* proxyPath) {
    static bool started = false;
    if (started) {
        return;
    }
    started = true;
    Start(proxyPath);
}

extern "C" __declspec(dllexport) void WINAPI MixedNutsLoaderBeforeFactory() {
    AcquireSRWLockExclusive(&g_hookLock);
    for (Hook& hook : g_hooks) {
        if (hook.disabled) {
            continue;
        }
        DWORD code = 0;
        if (!CallHook(hook.fn, hook.ctx, code)) {
            hook.disabled = true;
            Write("[NG] %s: crashed in its factory hook (exception 0x%08lX); the hook is disabled",
                  Utf8(g_plugins[hook.plugin]->name).c_str(), code);
        }
    }
    ReleaseSRWLockExclusive(&g_hookLock);
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        g_self = instance;
    }
    return TRUE;
}
