// Capcom Arcade 2nd Stadium — MixedNuts Mod Loader
// Created by MixedNuts - https://github.com/MixedNutsJP/ca2s-mod-loader
// Licensed under the MIT License. See LICENSE for details.
//
// A small timestamped log file, one per module. Each DLL that includes this
// header gets its own state.

#pragma once

#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <cwchar>

namespace mixednuts::log {

struct State {
    wchar_t path[MAX_PATH] = {};
    bool enabled = false;
    SRWLOCK lock = SRWLOCK_INIT;
};

inline State& Get() {
    static State state;
    return state;
}

// Starts a fresh log at dir + name. dir must end with a backslash.
inline void Open(const wchar_t* dir, const wchar_t* name, bool enabled) {
    State& s = Get();
    s.enabled = enabled && swprintf_s(s.path, L"%s%s", dir, name) > 0;
    if (!s.enabled) {
        return;
    }
    HANDLE file = CreateFileW(s.path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        CloseHandle(file);
    }
}

inline void Write(const char* format, ...) {
    State& s = Get();
    if (!s.enabled) {
        return;
    }

    char text[1024];
    SYSTEMTIME now;
    GetLocalTime(&now);
    int length = snprintf(text, sizeof(text), "%02u:%02u:%02u.%03u ", now.wHour, now.wMinute,
                          now.wSecond, now.wMilliseconds);
    va_list args;
    va_start(args, format);
    const int body = vsnprintf(text + length, sizeof(text) - length - 2, format, args);
    va_end(args);
    if (body < 0) {
        return;
    }
    length += body;
    if (length > static_cast<int>(sizeof(text)) - 3) {
        length = static_cast<int>(sizeof(text)) - 3;
    }
    text[length++] = '\r';
    text[length++] = '\n';

    AcquireSRWLockExclusive(&s.lock);
    HANDLE file = CreateFileW(s.path, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(file, text, static_cast<DWORD>(length), &written, nullptr);
        CloseHandle(file);
    }
    ReleaseSRWLockExclusive(&s.lock);
}

// Folder of the given module, ending with a backslash. Empty on failure.
inline bool ModuleDir(HMODULE module, wchar_t (&dir)[MAX_PATH]) {
    const DWORD length = GetModuleFileNameW(module, dir, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        dir[0] = L'\0';
        return false;
    }
    wchar_t* slash = wcsrchr(dir, L'\\');
    if (slash == nullptr) {
        dir[0] = L'\0';
        return false;
    }
    slash[1] = L'\0';
    return true;
}

}  // namespace mixednuts::log
