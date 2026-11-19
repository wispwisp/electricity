# SPDX-License-Identifier: GPL-3.0-or-later
# Fails if the executable needs a DLL that is not part of Windows (ADR 0019).
param([Parameter(Mandatory)][string]$Exe)

$output = dumpbin /nologo /dependents $Exe
if ($LASTEXITCODE -ne 0) {
    $output
    "dumpbin failed with exit code $LASTEXITCODE"
    exit 1
}

$dlls = @(foreach ($line in $output) {
    if ($line -match '^\s+(\S+\.dll)\s*$') { $Matches[1] }
})
if ($dlls.Count -eq 0) {
    "no DLL dependencies found"
    exit 1
}

$failed = $false
foreach ($dll in $dlls) {
    # The C/C++ runtime and Qt must be linked statically, even though ucrtbase.dll ships with Windows.
    $mustBeStatic = $dll -match '^(vcruntime|msvcp|ucrtbase|concrt|api-ms-win-crt-|qt6)'
    $isSystem = $dll -match '^(api|ext)-ms-win-' -or (Test-Path -LiteralPath "$env:SystemRoot\System32\$dll")
    if ($mustBeStatic -or -not $isSystem) {
        "not a system DLL: $dll"
        $failed = $true
    }
}
if ($failed) {
    exit 1
}

"System DLLs needed:"
$dlls
