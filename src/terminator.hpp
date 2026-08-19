#pragma once

#include <windows.h>
#include <tlhelp32.h>
#include <vector>
#include <string>

class Terminator {
public:
    std::vector<DWORD> GetProcessPids(const std::vector<std::string>& names);
    void ProcessKill(const std::vector<std::string>& processNames);
};
