param(
    [string]$TargetCharacter = "L I L B I S C U I T",
    [string]$TargetExePath = "C:\Program Files (x86)\Guild Wars - Copy - Copy 2\Gw.exe",
    [string]$GwcaPath = "C:\Users\Robert\Documents\GWA Censured X BotsHub\toolbox\GWToolboxpp-master\Dependencies\GWCA\bin\gwca.dll",
    [string]$AccountsFile = "C:\Users\Robert\Documents\GWA Censured X BotsHub\GWA Censured\Accounts.json",
    [switch]$LaunchIfMissing,
    [switch]$RunScannerInit,
    [switch]$RunGwInitialize
)

$ErrorActionPreference = "Stop"

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

public static class Win32 {
    public const uint PROCESS_CREATE_THREAD = 0x0002;
    public const uint PROCESS_QUERY_INFORMATION = 0x0400;
    public const uint PROCESS_VM_OPERATION = 0x0008;
    public const uint PROCESS_VM_WRITE = 0x0020;
    public const uint PROCESS_VM_READ = 0x0010;
    public const uint MEM_COMMIT = 0x1000;
    public const uint MEM_RESERVE = 0x2000;
    public const uint MEM_RELEASE = 0x8000;
    public const uint PAGE_READWRITE = 0x04;
    public const uint TH32CS_SNAPMODULE = 0x00000008;
    public const uint TH32CS_SNAPMODULE32 = 0x00000010;
    public const uint WAIT_OBJECT_0 = 0x00000000;

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct MODULEENTRY32 {
        public uint dwSize;
        public uint th32ModuleID;
        public uint th32ProcessID;
        public uint GlblcntUsage;
        public uint ProccntUsage;
        public IntPtr modBaseAddr;
        public uint modBaseSize;
        public IntPtr hModule;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 256)]
        public string szModule;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 260)]
        public string szExePath;
    }

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern IntPtr OpenProcess(uint access, bool inheritHandle, uint processId);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool CloseHandle(IntPtr handle);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern IntPtr VirtualAllocEx(IntPtr hProcess, IntPtr lpAddress, UIntPtr dwSize, uint flAllocationType, uint flProtect);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool VirtualFreeEx(IntPtr hProcess, IntPtr lpAddress, UIntPtr dwSize, uint dwFreeType);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool WriteProcessMemory(IntPtr hProcess, IntPtr lpBaseAddress, byte[] lpBuffer, UIntPtr nSize, out UIntPtr lpNumberOfBytesWritten);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool ReadProcessMemory(IntPtr hProcess, IntPtr lpBaseAddress, byte[] lpBuffer, UIntPtr size, out UIntPtr lpNumberOfBytesRead);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern IntPtr CreateRemoteThread(IntPtr hProcess, IntPtr lpThreadAttributes, UIntPtr dwStackSize, IntPtr lpStartAddress, IntPtr lpParameter, uint dwCreationFlags, out uint lpThreadId);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern uint WaitForSingleObject(IntPtr hHandle, uint dwMilliseconds);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern IntPtr GetModuleHandleW(string lpModuleName);

    [DllImport("kernel32.dll", CharSet = CharSet.Ansi, SetLastError = true)]
    public static extern IntPtr GetProcAddress(IntPtr hModule, string procName);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern IntPtr CreateToolhelp32Snapshot(uint dwFlags, uint th32ProcessID);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern bool Module32FirstW(IntPtr hSnapshot, ref MODULEENTRY32 lpme);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern bool Module32NextW(IntPtr hSnapshot, ref MODULEENTRY32 lpme);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool GetExitCodeThread(IntPtr hThread, out uint lpExitCode);
}
"@

function Get-TargetProcess {
    Get-CimInstance Win32_Process -Filter "Name = 'Gw.exe'" |
        Where-Object { $_.ExecutablePath -eq $TargetExePath } |
        Sort-Object ProcessId -Descending |
        Select-Object -First 1
}

function Launch-TargetProcess {
    if (-not (Test-Path $AccountsFile)) {
        throw "Accounts.json not found at $AccountsFile"
    }
    $accounts = Get-Content $AccountsFile -Raw | ConvertFrom-Json
    $account = $accounts | Where-Object { $_.character -eq $TargetCharacter } | Select-Object -First 1
    if (-not $account) {
        throw "Target character not found in Accounts.json"
    }

    $gwPath = [string]$account.gwpath
    $args = @()
    if ($account.email -and $account.password) {
        $args += "-email"
        $args += [string]$account.email
        $args += "-password"
        $args += [string]$account.password
        if ($account.character) {
            $args += "-character"
            $args += [string]$account.character
        }
    }
    if ($account.extraargs) {
        $args += ([string]$account.extraargs -split "\s+" | Where-Object { $_ })
    }

    $gwDir = Split-Path $gwPath -Parent
    New-Item -Path "HKCU:\Software\ArenaNet\Guild Wars" -Force | Out-Null
    Set-ItemProperty -Path "HKCU:\Software\ArenaNet\Guild Wars" -Name Path -Value $gwPath
    Set-ItemProperty -Path "HKCU:\Software\ArenaNet\Guild Wars" -Name Src -Value $gwDir

    $proc = Start-Process -FilePath $gwPath -ArgumentList $args -PassThru
    Start-Sleep -Seconds 20
    return Get-CimInstance Win32_Process -Filter ("ProcessId = {0}" -f $proc.Id)
}

function Get-Modules([uint32]$ProcessId) {
    $snapshot = [Win32]::CreateToolhelp32Snapshot([Win32]::TH32CS_SNAPMODULE -bor [Win32]::TH32CS_SNAPMODULE32, $ProcessId)
    if ($snapshot -eq [IntPtr]::Zero -or $snapshot.ToInt64() -eq -1) {
        throw "CreateToolhelp32Snapshot failed for PID $ProcessId"
    }
    try {
        $me = New-Object Win32+MODULEENTRY32
        $me.dwSize = [Runtime.InteropServices.Marshal]::SizeOf([type] [Win32+MODULEENTRY32])
        $ok = [Win32]::Module32FirstW($snapshot, [ref]$me)
        $result = @()
        while ($ok) {
            $result += [pscustomobject]@{
                ModuleName = $me.szModule
                ModulePath = $me.szExePath
                BaseAddress = [uint64]$me.modBaseAddr.ToInt64()
                Size = $me.modBaseSize
            }
            $ok = [Win32]::Module32NextW($snapshot, [ref]$me)
        }
        return $result
    } finally {
        [void][Win32]::CloseHandle($snapshot)
    }
}

function Read-Bytes([IntPtr]$ProcessHandle, [uint64]$Address, [int]$Length) {
    $buf = New-Object byte[] $Length
    $read = [UIntPtr]::Zero
    if (-not [Win32]::ReadProcessMemory($ProcessHandle, [IntPtr]([Int64]$Address), $buf, [UIntPtr]::op_Explicit($Length), [ref]$read)) {
        throw ("ReadProcessMemory failed at 0x{0:X}" -f $Address)
    }
    return $buf
}

function Read-U32([IntPtr]$ProcessHandle, [uint64]$Address) {
    $bytes = Read-Bytes $ProcessHandle $Address 4
    return [BitConverter]::ToUInt32($bytes, 0)
}

function Read-HookEntries([IntPtr]$ProcessHandle, [uint64]$BaseAddress, [uint64]$TablePtr, [uint32]$Count) {
    $entries = @()
    if (-not $TablePtr -or -not $Count) {
        return $entries
    }
    for ($i = 0; $i -lt $Count; $i++) {
        $entryAddr = $TablePtr + [uint64]($i * 0x2C)
        $raw = Read-Bytes -ProcessHandle $ProcessHandle -Address $entryAddr -Length 0x2C
        $target = [BitConverter]::ToUInt32($raw, 0)
        $detour = [BitConverter]::ToUInt32($raw, 4)
        $replay = [BitConverter]::ToUInt32($raw, 8)
        $saved0 = [BitConverter]::ToUInt32($raw, 12)
        $saved1 = [BitConverter]::ToUInt32($raw, 16)
        $flags = $raw[20]
        $relocCount = $raw[24] -band 0x0F
        $origOffsets = @()
        $stubOffsets = @()
        for ($j = 0; $j -lt 8; $j++) {
            $origOffsets += $raw[28 + $j]
            $stubOffsets += $raw[36 + $j]
        }
        $entries += [pscustomobject]@{
            Index = $i
            EntryAddress = $entryAddr
            Target = [uint64]$target
            Detour = [uint64]$detour
            Replay = [uint64]$replay
            Saved0 = [uint64]$saved0
            Saved1 = [uint64]$saved1
            Flags = $flags
            RelocCount = $relocCount
            OrigOffsets = ($origOffsets | ForEach-Object { $_.ToString("X2") }) -join " "
            StubOffsets = ($stubOffsets | ForEach-Object { $_.ToString("X2") }) -join " "
            TargetRva = if ($target -ge $BaseAddress) { [uint64]($target - $BaseAddress) } else { 0 }
            DetourRva = if ($detour -ge $BaseAddress) { [uint64]($detour - $BaseAddress) } else { 0 }
        }
    }
    return $entries
}

function Read-MemoryPatches([IntPtr]$ProcessHandle, [uint64]$BaseAddress, [uint64]$VecStart, [uint64]$VecEnd) {
    $result = @()
    if (-not $VecStart -or -not $VecEnd -or $VecEnd -lt $VecStart) {
        return $result
    }
    $count = [int](($VecEnd - $VecStart) / 4)
    for ($i = 0; $i -lt $count; $i++) {
        $objPtr = Read-U32 -ProcessHandle $ProcessHandle -Address ($VecStart + [uint64]($i * 4))
        if (-not $objPtr) { continue }
        $raw = Read-Bytes -ProcessHandle $ProcessHandle -Address $objPtr -Length 0x14
        $target = [BitConverter]::ToUInt32($raw, 0)
        $patchedBuf = [BitConverter]::ToUInt32($raw, 4)
        $originalBuf = [BitConverter]::ToUInt32($raw, 8)
        $size = [BitConverter]::ToUInt32($raw, 12)
        $enabled = [BitConverter]::ToUInt32($raw, 16)
        $patchedBytes = if ($patchedBuf -and $size) { Read-Bytes -ProcessHandle $ProcessHandle -Address $patchedBuf -Length $size } else { @() }
        $originalBytes = if ($originalBuf -and $size) { Read-Bytes -ProcessHandle $ProcessHandle -Address $originalBuf -Length $size } else { @() }
        $liveBytes = if ($target -and $size) { Read-Bytes -ProcessHandle $ProcessHandle -Address $target -Length $size } else { @() }
        $result += [pscustomobject]@{
            Index = $i
            Object = [uint64]$objPtr
            Target = [uint64]$target
            PatchedBuf = [uint64]$patchedBuf
            OriginalBuf = [uint64]$originalBuf
            Size = $size
            Enabled = $enabled
            TargetRva = if ($target -ge $BaseAddress) { [uint64]($target - $BaseAddress) } else { 0 }
            PatchedBytes = ($patchedBytes | ForEach-Object { $_.ToString("X2") }) -join " "
            OriginalBytes = ($originalBytes | ForEach-Object { $_.ToString("X2") }) -join " "
            LiveBytes = ($liveBytes | ForEach-Object { $_.ToString("X2") }) -join " "
        }
    }
    return $result
}

function Format-HexDump([byte[]]$Bytes, [uint64]$BaseAddress) {
    $lines = @()
    for ($i = 0; $i -lt $Bytes.Length; $i += 16) {
        $slice = $Bytes[$i..([Math]::Min($i + 15, $Bytes.Length - 1))]
        $hex = ($slice | ForEach-Object { $_.ToString("X2") }) -join " "
        $lines += ("{0:X8}: {1}" -f ($BaseAddress + [uint64]$i), $hex)
    }
    return $lines
}

function Invoke-RemoteCall([IntPtr]$ProcessHandle, [uint64]$StartAddress, [uint64]$Parameter, [int]$TimeoutMs = 15000) {
    $threadId = 0
    $thread = [Win32]::CreateRemoteThread($ProcessHandle, [IntPtr]::Zero, [UIntPtr]::Zero, [IntPtr]([Int64]$StartAddress), [IntPtr]([Int64]$Parameter), 0, [ref]$threadId)
    if ($thread -eq [IntPtr]::Zero) {
        throw ("CreateRemoteThread failed for start=0x{0:X}" -f $StartAddress)
    }
    try {
        [void][Win32]::WaitForSingleObject($thread, [uint32]$TimeoutMs)
        $exitCode = 0
        [void][Win32]::GetExitCodeThread($thread, [ref]$exitCode)
        return $exitCode
    } finally {
        [void][Win32]::CloseHandle($thread)
    }
}

$target = Get-TargetProcess
if (-not $target) {
    if ($LaunchIfMissing) {
        $target = Launch-TargetProcess
        Start-Sleep -Seconds 5
        $target = Get-TargetProcess
    }
    if (-not $target) {
        throw "No Gw.exe process found for target path."
    }
}

$access = [Win32]::PROCESS_CREATE_THREAD -bor [Win32]::PROCESS_QUERY_INFORMATION -bor [Win32]::PROCESS_VM_OPERATION -bor [Win32]::PROCESS_VM_WRITE -bor [Win32]::PROCESS_VM_READ
$hProcess = [Win32]::OpenProcess($access, $false, [uint32]$target.ProcessId)
if ($hProcess -eq [IntPtr]::Zero) {
    throw "OpenProcess failed for PID $($target.ProcessId)"
}

try {
    $modules = Get-Modules -ProcessId $target.ProcessId
    $gwModule = $modules | Where-Object { $_.ModuleName -ieq "Gw.exe" } | Select-Object -First 1
    if (-not $gwModule) {
        throw "Could not locate Gw.exe module in target process."
    }

    $gwcaModule = $modules | Where-Object { $_.ModuleName -ieq "gwca.dll" } | Select-Object -First 1
    $gwcaBase = if ($gwcaModule) { $gwcaModule.BaseAddress } else { 0 }

    if (-not $gwcaBase) {
        if (-not (Test-Path $GwcaPath)) {
            throw "GWCA DLL not found at $GwcaPath"
        }
        $kernel32 = [Win32]::GetModuleHandleW("kernel32.dll")
        $loadLibraryW = [Win32]::GetProcAddress($kernel32, "LoadLibraryW")
        if ($loadLibraryW -eq [IntPtr]::Zero) {
            throw "GetProcAddress(LoadLibraryW) failed"
        }
        $pathBytes = [Text.Encoding]::Unicode.GetBytes($GwcaPath + [char]0)
        $remoteBuf = [Win32]::VirtualAllocEx($hProcess, [IntPtr]::Zero, [UIntPtr]::op_Explicit($pathBytes.Length), [Win32]::MEM_COMMIT -bor [Win32]::MEM_RESERVE, [Win32]::PAGE_READWRITE)
        if ($remoteBuf -eq [IntPtr]::Zero) {
            throw "VirtualAllocEx failed"
        }
        try {
            $written = [UIntPtr]::Zero
            if (-not [Win32]::WriteProcessMemory($hProcess, $remoteBuf, $pathBytes, [UIntPtr]::op_Explicit($pathBytes.Length), [ref]$written)) {
                throw "WriteProcessMemory for DLL path failed"
            }
            $exitCode = Invoke-RemoteCall -ProcessHandle $hProcess -StartAddress ([uint64]$loadLibraryW.ToInt64()) -Parameter ([uint64]$remoteBuf.ToInt64())
            if (-not $exitCode) {
                throw "LoadLibraryW returned null"
            }
            $gwcaBase = [uint64]$exitCode
            Start-Sleep -Milliseconds 500
            $modules = Get-Modules -ProcessId $target.ProcessId
        } finally {
            [void][Win32]::VirtualFreeEx($hProcess, $remoteBuf, [UIntPtr]::Zero, [Win32]::MEM_RELEASE)
        }
    }

    if ($RunScannerInit) {
        $scannerInit = $gwcaBase + 0x21850
        [void](Invoke-RemoteCall -ProcessHandle $hProcess -StartAddress $scannerInit -Parameter $gwModule.BaseAddress)
        Start-Sleep -Milliseconds 250
    }

    if ($RunGwInitialize) {
        $gwInit = $gwcaBase + 0x18E90
        [void](Invoke-RemoteCall -ProcessHandle $hProcess -StartAddress $gwInit -Parameter 0 -TimeoutMs 20000)
        Start-Sleep -Milliseconds 500
    }

    $report = [System.Collections.Generic.List[string]]::new()
    $report.Add(("PID: {0}" -f $target.ProcessId))
    $report.Add(("TargetExePath: {0}" -f $TargetExePath))
    $report.Add(("GwBase: 0x{0:X8}" -f $gwModule.BaseAddress))
    $report.Add(("GwcaBase: 0x{0:X8}" -f $gwcaBase))
    $report.Add(("ScannerInitRun: {0}" -f [bool]$RunScannerInit))
    $report.Add(("GWInitializeRun: {0}" -f [bool]$RunGwInitialize))
    $report.Add("")

    $sendFrame = $gwModule.BaseAddress + 0x2286D0
    $getChild = $gwModule.BaseAddress + 0x20E2B0
    $rootFrame = $gwModule.BaseAddress + 0x22DC20
    $report.Add(("Game SendFrameUIMsg: 0x{0:X8}" -f $sendFrame))
    $report.Add(("Game GetChildFrame: 0x{0:X8}" -f $getChild))
    $report.Add(("Game RootFrame: 0x{0:X8}" -f $rootFrame))
    $report.Add("")

    $gwcaOffsets = [ordered]@{
        "SendFrameUIMsg_Orig" = 0x8A39C
        "SendFrameUIMsg_Hook" = 0x8A3A0
        "GetChildFrame" = 0x8A37C
        "RootFrame" = 0x8A410
        "SetWindowVisible" = 0x8A3D0
        "FrameHashTable" = 0x8A3B0
        "HookPageList" = 0x8B0B4
        "HookTablePtr" = 0x8B0C0
        "HookTableCap" = 0x8B0C4
        "HookTableCount" = 0x8B0C8
        "MemPatchVecStart" = 0x8A1F8
        "MemPatchVecEnd" = 0x8A1FC
        "MemPatchVecCap" = 0x8A200
        "MemPatchEnabled" = 0x88144
    }

    foreach ($entry in $gwcaOffsets.GetEnumerator()) {
        $val = Read-U32 -ProcessHandle $hProcess -Address ($gwcaBase + $entry.Value)
        $report.Add(("{0}: 0x{1:X8}" -f $entry.Key, $val))
    }
    $report.Add("")

    $sendFrameHook = Read-U32 -ProcessHandle $hProcess -Address ($gwcaBase + $gwcaOffsets["SendFrameUIMsg_Hook"])

    $sendFrameBytes = Read-Bytes -ProcessHandle $hProcess -Address $sendFrame -Length 128
    $report.Add("=== Game SendFrameUIMsg Bytes ===")
    $report.AddRange([string[]](Format-HexDump -Bytes $sendFrameBytes -BaseAddress $sendFrame))
    $report.Add("")

    if ($sendFrameHook) {
        $hookBytes = Read-Bytes -ProcessHandle $hProcess -Address $sendFrameHook -Length 96
        $report.Add("=== GWCA Replay Stub Bytes ===")
        $report.AddRange([string[]](Format-HexDump -Bytes $hookBytes -BaseAddress $sendFrameHook))
        $report.Add("")
    }

    $gwcaDataBytes = Read-Bytes -ProcessHandle $hProcess -Address ($gwcaBase + 0x8A370) -Length 0xB0
    $report.Add("=== GWCA Data Section 0x8A370.. ===")
    $report.AddRange([string[]](Format-HexDump -Bytes $gwcaDataBytes -BaseAddress ($gwcaBase + 0x8A370)))
    $report.Add("")

    $hookTablePtr = Read-U32 -ProcessHandle $hProcess -Address ($gwcaBase + $gwcaOffsets["HookTablePtr"])
    $hookTableCount = Read-U32 -ProcessHandle $hProcess -Address ($gwcaBase + $gwcaOffsets["HookTableCount"])
    $hookEntries = Read-HookEntries -ProcessHandle $hProcess -BaseAddress $gwcaBase -TablePtr $hookTablePtr -Count $hookTableCount
    if ($hookEntries.Count -gt 0) {
        $report.Add("=== GWCA Hook Entries ===")
        foreach ($entry in $hookEntries) {
            $report.Add((
                "idx={0} entry=0x{1:X8} target=0x{2:X8} detour=0x{3:X8} replay=0x{4:X8} flags=0x{5:X2} reloc={6} target_rva=0x{7:X} detour_rva=0x{8:X}" -f
                $entry.Index, $entry.EntryAddress, $entry.Target, $entry.Detour, $entry.Replay, $entry.Flags, $entry.RelocCount, $entry.TargetRva, $entry.DetourRva
            ))
            $report.Add(("  saved0=0x{0:X8} saved1=0x{1:X8}" -f $entry.Saved0, $entry.Saved1))
            $report.Add(("  orig_offsets={0}" -f $entry.OrigOffsets))
            $report.Add(("  stub_offsets={0}" -f $entry.StubOffsets))
        }
    }
    $report.Add("")

    $memPatchVecStart = Read-U32 -ProcessHandle $hProcess -Address ($gwcaBase + $gwcaOffsets["MemPatchVecStart"])
    $memPatchVecEnd = Read-U32 -ProcessHandle $hProcess -Address ($gwcaBase + $gwcaOffsets["MemPatchVecEnd"])
    $memPatchVecCap = Read-U32 -ProcessHandle $hProcess -Address ($gwcaBase + $gwcaOffsets["MemPatchVecCap"])
    $memPatchEnabled = Read-U32 -ProcessHandle $hProcess -Address ($gwcaBase + $gwcaOffsets["MemPatchEnabled"])
    $report.Add(("MemPatchVecStartPtr: 0x{0:X8}" -f $memPatchVecStart))
    $report.Add(("MemPatchVecEndPtr: 0x{0:X8}" -f $memPatchVecEnd))
    $report.Add(("MemPatchVecCapPtr: 0x{0:X8}" -f $memPatchVecCap))
    $report.Add(("MemPatchEnabled: 0x{0:X8}" -f $memPatchEnabled))
    $patches = Read-MemoryPatches -ProcessHandle $hProcess -BaseAddress $gwcaBase -VecStart $memPatchVecStart -VecEnd $memPatchVecEnd
    if ($patches.Count -gt 0) {
        $report.Add("=== GWCA Memory Patches ===")
        foreach ($patch in $patches) {
            $report.Add((
                "idx={0} obj=0x{1:X8} target=0x{2:X8} patched=0x{3:X8} original=0x{4:X8} size=0x{5:X} enabled=0x{6:X8} target_rva=0x{7:X}" -f
                $patch.Index, $patch.Object, $patch.Target, $patch.PatchedBuf, $patch.OriginalBuf, $patch.Size, $patch.Enabled, $patch.TargetRva
            ))
            $report.Add(("  patched_bytes={0}" -f $patch.PatchedBytes))
            $report.Add(("  original_bytes={0}" -f $patch.OriginalBytes))
            $report.Add(("  live_bytes={0}" -f $patch.LiveBytes))
        }
    }

    $reportPath = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\GWA Censured\tests\live_gwca_runtime_report.txt"))
    [IO.File]::WriteAllLines($reportPath, $report)
    $report -join [Environment]::NewLine
} finally {
    if ($hProcess -ne [IntPtr]::Zero) {
        [void][Win32]::CloseHandle($hProcess)
    }
}
