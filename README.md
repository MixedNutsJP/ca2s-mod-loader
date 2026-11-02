# MixedNuts Mod Loader for Capcom Arcade 2nd Stadium

**Capcom Arcade 2nd Stadium**(Steam AppID 1755910)用の、MixedNuts の Mod に共通のローダーです。

The common mod loader for MixedNuts mods for Capcom Arcade 2nd Stadium (Steam, AppID 1755910).

次の Mod は、このローダーの上で動きます。
The following mods run on this loader:

- [ca2s-adapter-fix](https://github.com/MixedNutsJP/ca2s-adapter-fix) — DXGI アダプタが4個以上ある PC での起動時クラッシュの修正 / fixes the startup crash on PCs with four or more DXGI adapters

## 導入 / Installation

`dxgi.dll` と `MixedNuts` フォルダを、ゲームのルート(`CapcomArcade2ndStadium.exe` と同じ場所)にコピーします。Mod はその後、`MixedNuts\Mods\` の中に入れます。

Copy `dxgi.dll` and the `MixedNuts` folder into the game's root directory (the folder containing `CapcomArcade2ndStadium.exe`). Mods then go inside `MixedNuts\Mods\`.

```
Capcom Arcade 2nd Stadium/
  CapcomArcade2ndStadium.exe
  dxgi.dll                          <- ローダー / the loader
  MixedNuts/MixedNutsLoader.dll     <- ローダー本体 / the loader itself
  MixedNuts/loader.ini
  MixedNuts/loader.log              <- 起動時に生成 / generated at launch
  MixedNuts/Mods/<name>/<name>.dll  <- Mod
```

削除は `dxgi.dll` と `MixedNuts` フォルダを消すだけです。ゲームのファイルは一切変更しません。

To uninstall, delete `dxgi.dll` and the `MixedNuts` folder. No game files are modified.

## 設定 / Configuration

`MixedNuts\loader.ini`

| 項目 / Key | 意味 / Meaning |
|---|---|
| `Log` | `1` = `MixedNuts\loader.log` を出力 write a log / `0` = 出力しない no log |

## 仕組み / How it works

1. ゲームの exe は `dxgi.dll` を静的にインポートしているので、ゲームフォルダの `dxgi.dll`(プロキシ)が読み込まれます。本来の関数は System32 の実体へ転送します
   The exe imports `dxgi.dll` statically, so the proxy `dxgi.dll` in the game folder is loaded. The real functions are forwarded to System32.
2. ゲームが最初に `CreateDXGIFactory` / `CreateDXGIFactory1` / `CreateDXGIFactory2` を呼んだとき、プロキシは `MixedNuts\MixedNutsLoader.dll` を読み込みます。exe は起動時に展開されるパック形式で、この時点では展開済みです
   On the game's first `CreateDXGIFactory*` call, the proxy loads `MixedNuts\MixedNutsLoader.dll`. The exe is packed and unpacks itself at startup; by then it has.
3. ローダーは `MixedNuts\Mods\<名前>\<名前>.dll` をフォルダ名の順に読み込み、`MixedNutsPluginInit` を呼びます
   The loader loads `MixedNuts\Mods\<name>\<name>.dll` in folder-name order and calls `MixedNutsPluginInit`.
4. 以後、`CreateDXGIFactory*` が呼ばれるたびに、本物を呼ぶ前に、Mod が登録したフックを読み込み順に実行します
   From then on, every `CreateDXGIFactory*` call first runs the hooks the mods registered, in load order.

ローダーは `DllMain` の中ではなく通常の関数呼び出しの中で動くので、ローダーロックは掛かっていません。同じフォルダの `CrashReport.exe` も `dxgi.dll` を読み込みますが、そちらではローダーを起動せず、転送だけを行います。

The loader runs inside a normal call rather than `DllMain`, so the loader lock is not held. `CrashReport.exe` in the same folder also loads `dxgi.dll`; there the proxy only forwards.

## Mod の作り方 / Writing a mod

Mod は `MixedNutsPluginInit` をエクスポートする DLL です。API は [`api/mixednuts/plugin.h`](api/mixednuts/plugin.h) にあります(C の呼び出し規約)。

A mod is a DLL that exports `MixedNutsPluginInit`. The API is in [`api/mixednuts/plugin.h`](api/mixednuts/plugin.h) (plain C ABI).

```cpp
#include <mixednuts/plugin.h>

void OnFactory(void* ctx)
{
    /* patch the game's code here; return quickly once done */
}

MIXEDNUTS_PLUGIN_EXPORT int WINAPI MixedNutsPluginInit(const MixedNutsApi* api)
{
    return api->RegisterFactoryHook(api, &OnFactory, nullptr);
}
```

- フックは読み込み順に1つずつ、Factory を作るスレッドで呼ばれます。処理が済んだら、すぐに戻るようにしてください
  Hooks run one at a time, in load order, on the thread that creates the factory. Return immediately once there is nothing left to do.
- init やフックの中で例外が起きた Mod は、ログに記録して以後は呼びません
  A mod that throws in init or in a hook is logged and not called again.

`common/mixednuts/log.hpp` に、Mod ごとのログ(時刻付き)のヘッダがあります。Mod のリポジトリにはこのリポジトリを submodule で取り込み、`common` と `api` を include パスに加えます。

`common/mixednuts/log.hpp` provides a per-mod, timestamped log. Mods add this repository as a submodule and put `common` and `api` on the include path.

```
git submodule add https://github.com/MixedNutsJP/ca2s-mod-loader.git mod-loader
```

## ビルド / Build

Visual Studio 2022 の C++ ワークロードと CMake が必要です。
Requires the Visual Studio 2022 C++ workload and CMake.

```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
cmake --install build --config Release --prefix dist
```

`dist\` に、ゲームのルートへそのままコピーできる一式が出力されます。
`dist\` receives the files to copy into the game's root.

## 免責事項 / Disclaimer

**このソフトウェアは無保証で提供されます。使用によって生じたいかなる損害についても、作者は一切の責任を負いません。** セーブデータの破損・消失、ゲームの動作不良、その他の不具合を含みます。自己責任でご使用ください。

**This software is provided as-is, without any warranty. The author accepts no liability for any damage arising from its use,** including but not limited to corruption or loss of save data, game malfunction, or any other problem. Use it at your own risk.

このソフトウェアは非公式のものです。株式会社カプコンとは関係ありません。

This is unofficial software and is not affiliated with or endorsed by Capcom Co., Ltd.

## 不具合の報告 / Reporting issues

不具合を見つけた場合は、GitHub の Issue でご報告ください。その際、**必ず `MixedNuts\loader.log` と、該当する Mod のログを添付してください。**

If you run into a problem, please open a GitHub Issue. **Be sure to attach `MixedNuts\loader.log` and the log of the mod concerned.**

---

## クレジット / Credits

**Created by MixedNuts**

## ライセンス / License

MIT License — 詳細は [LICENSE](LICENSE) を参照してください。
See [LICENSE](LICENSE) for details.

再配布・改変は自由ですが、**著作権表示とライセンス文を必ず残してください。**
MIT ライセンスの条件です。

You are free to redistribute and modify this, but **the copyright notice and the
license text must be retained** — that is a condition of the MIT License.
