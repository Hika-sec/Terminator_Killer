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
    constexpr DWORD IOCTL_KILL_PROCESS = 0xB822200C;
    constexpr const char* DRIVER_PATH = "\\\\.\\STProcessMonitorDriver";

    std::string ToLower(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
            [](unsigned char c) { return std::tolower(c); });
        return s;
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

    HANDLE hDevice = CreateFileA(DRIVER_PATH, GENERIC_READ | GENERIC_WRITE, 0, nullptr,
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

                ULONG64 pid64 = pid;
                BOOL success = DeviceIoControl(hDevice, IOCTL_KILL_PROCESS,
                                               &pid64, sizeof(pid64),
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

    std::vector<std::string> processNames;
    for (int i = 2; i < argc; ++i) {
        processNames.emplace_back(argv[i]);
    }

    Terminator terminator;
    terminator.ProcessKill(processNames);
    return 0;
}
