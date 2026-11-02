/* Capcom Arcade 2nd Stadium — MixedNuts Mod Loader plugin API
 * Created by MixedNuts - https://github.com/MixedNutsJP/ca2s-mod-loader
 * Licensed under the MIT License. See LICENSE for details.
 *
 * The loader loads MixedNuts\Mods\<name>\<name>.dll in folder-name order and
 * calls its exported MixedNutsPluginInit.
 *
 * When: the root dxgi.dll starts the loader on the game's first call to
 * CreateDXGIFactory / CreateDXGIFactory1 / CreateDXGIFactory2, before that call
 * is forwarded. By then the exe has been unpacked, so plugins can read and patch
 * the game's code. This is not inside DllMain, so the loader lock is not held.
 *
 * Factory hooks: a hook registered with RegisterFactoryHook runs before every
 * CreateDXGIFactory* call, including the first one (right after init). Hooks run
 * one at a time, in load order, on the thread that creates the factory. Keep them
 * short; return immediately once there is nothing left to do.
 */

#ifndef MIXEDNUTS_PLUGIN_H
#define MIXEDNUTS_PLUGIN_H

#include <windows.h>
#include <stdint.h>

#define MIXEDNUTS_API_VERSION 1

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*MixedNutsFactoryHookFn)(void* ctx);

typedef struct MixedNutsApi {
    uint32_t       size;       /* sizeof(MixedNutsApi) */
    uint32_t       version;    /* MIXEDNUTS_API_VERSION */
    const wchar_t* gameDir;    /* The game folder, ending with \ */
    const wchar_t* pluginDir;  /* This plugin's folder, ending with \ */
    /* Run hook(ctx) before every CreateDXGIFactory* call. Only callable inside
     * MixedNutsPluginInit. Returns 1 on success. */
    int (*RegisterFactoryHook)(const struct MixedNutsApi* self, MixedNutsFactoryHookFn hook,
                               void* ctx);
} MixedNutsApi;

/* Exported by each plugin. Returns 1 on success. api stays valid while loaded. */
typedef int(WINAPI* MixedNutsPluginInitFn)(const MixedNutsApi* api);
#define MIXEDNUTS_PLUGIN_INIT "MixedNutsPluginInit"

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#define MIXEDNUTS_PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
#define MIXEDNUTS_PLUGIN_EXPORT __declspec(dllexport)
#endif

#endif /* MIXEDNUTS_PLUGIN_H */
