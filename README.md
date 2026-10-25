# Terminator - Red Team Process Killer

A kernel-mode process termination tool leveraging a vulnerable driver (BYOVD) to kill protected processes from user mode.

## Compilation

Requires MinGW-w64 (GCC) on Windows. Compile with:

```powershell
g++ --% .\Terminator_libwasys.cpp -o terminator_libwasys.exe -Oz -ffunction-sections -fdata-sections -static -static-libgcc -static-libstdc++ -Wl,--gc-sections -W
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

Output: standalone `terminator_libwasys.exe` (~20-30 KB stripped).

## Technical Focus

This project demonstrates exploitation of a vulnerable kernel driver (`libwasys.sys`) that exposes an unvalidated `IOCTL_KILL` handler, allowing arbitrary process termination regardless of privileges or protection mechanisms (PPL, EPROCESS flags, etc.).

### Architecture Breakdown

| Component | Purpose |
|-----------|---------|
| `Terminator_libwasys.cpp` | User-mode client that enumerates processes and sends kill IOCTLs |
| `terminator.hpp` | Class interface for process enumeration and termination |
| `libwasys.sys` | Vulnerable kernel driver (included as `ilbwasys.sys`) |

### Code Analysis

**`Terminator::GetProcessPids()`**
- Creates a snapshot via `CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS)`
- Iterates all processes with `Process32First`/`Process32Next`
- Case-insensitive matches against target process names
- Returns vector of matching PIDs

**`Terminator::ProcessKill()`**
- Opens handle to `\\.\WasysControl`
- Enters infinite monitoring loop (300ms interval)
- For each discovered PID, sends `IOCTL_KILL (0x22E00C)` with 4-byte PID buffer
- Logs only on kill attempts

## Usage Guide

### Prerequisites

Load the vulnerable driver (requires Administrator):

```cmd
sc.exe create WasysControl type=kernel binPath=C:\Path\To\Driver\libwasys.sys
sc.exe start WasysControl
```

### Exploiting an Existing Service

If a vulnerable version of `libwasys.sys` is already installed and running as a service, Terminator can connect directly without loading a new driver.

**Check if the service exists and is running:**
```cmd
sc.exe query WasysControl
```

### Run Terminator

```cmd
terminator_libwasys.exe -proc <process_name1> [process_name2] ...
```

Example:
```cmd
terminator_libwasys.exe -proc notepad.exe calc.exe
```

### Output

```
[+] Connected to driver. Monitoring for processes...
[*] Sending IOCTL 0x22E00C to terminate PID 4188...
[+] Success! IOCTL sent for PID 4188.
```

## Vulnerability Analysis

**Vulnerability**: The driver `libwasys.sys` registers an IOCTL handler (`0x22E00C`) that:
1. Accepts a user-supplied PID as input buffer
2. Performs **no validation** on the PID value
3. Calls termination functionality directly
4. Runs in kernel context (`SYSTEM` token), bypassing all user-mode ACLs

**Impact**: Any user with driver access can terminate:
- Protected Processes (PPL)
- Antivirus/EDR agents
- Critical system processes
- Processes with elevated privileges

## Disclaimer

> **This tool is for authorized security research and red team exercises only.**
>
> Unauthorized use against systems you do not own or have explicit permission to test is illegal. The author assumes no liability for misuse. Ensure you have written authorization before deploying in any environment.
>
> The vulnerable driver (`libwasys.sys`) is intentionally flawed for demonstration purposes and should **never** be deployed in production.
