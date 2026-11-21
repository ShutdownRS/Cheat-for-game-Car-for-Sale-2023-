#include <Windows.h>
#include <tlhelp32.h>
#include <cstdio>
#include <cstring>
#include <string>

#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "user32.lib")

static DWORD FindProcess(const wchar_t* name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    DWORD pid = 0;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, name) == 0) { pid = pe.th32ProcessID; break; }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return pid;
}

int wmain(int argc, wchar_t** argv) {
    std::wstring dll;
    if (argc > 1) {
        dll = argv[1];
    } else {
        wchar_t exePath[MAX_PATH]{};
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        std::wstring p(exePath);
        size_t slash = p.find_last_of(L"\\/");
        dll = (slash == std::wstring::npos ? L"" : p.substr(0, slash + 1)) + L"CarDealerCheat.dll";
    }

    wprintf(L"[*] DLL: %s\n", dll.c_str());
    DWORD pid = FindProcess(L"Car For Sale Simulator 2023.exe");
    if (!pid) {
        wprintf(L"[!] Game process not found. Start the game first.\n");
        return 1;
    }
    wprintf(L"[*] PID: %lu\n", pid);

    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProc) {
        wprintf(L"[!] OpenProcess failed: %lu\n", GetLastError());
        return 1;
    }

    char ansiDll[MAX_PATH]{};
    WideCharToMultiByte(CP_ACP, 0, dll.c_str(), -1, ansiDll, MAX_PATH, nullptr, nullptr);
    size_t len = strlen(ansiDll) + 1;

    void* remote = VirtualAllocEx(hProc, nullptr, len, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote) {
        wprintf(L"[!] VirtualAllocEx failed: %lu\n", GetLastError());
        CloseHandle(hProc);
        return 1;
    }
    if (!WriteProcessMemory(hProc, remote, ansiDll, len, nullptr)) {
        wprintf(L"[!] WriteProcessMemory failed: %lu\n", GetLastError());
        VirtualFreeEx(hProc, remote, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return 1;
    }

    FARPROC loadLib = GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryA");
    HANDLE th = CreateRemoteThread(hProc, nullptr, 0,
        (LPTHREAD_START_ROUTINE)loadLib, remote, 0, nullptr);
    if (!th) {
        wprintf(L"[!] CreateRemoteThread failed: %lu\n", GetLastError());
        VirtualFreeEx(hProc, remote, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return 1;
    }

    wprintf(L"[+] Injected. Press INSERT in game to open the menu.\n");
    WaitForSingleObject(th, 5000);
    CloseHandle(th);
    VirtualFreeEx(hProc, remote, 0, MEM_RELEASE);
    CloseHandle(hProc);
    return 0;
}
