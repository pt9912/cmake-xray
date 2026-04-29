# AP M5-1.7 Tranche D.1: separater Windows-PowerShell-Smoke.
#
# Plan-M5-1-7.md "Windows-Bewertung" verlangt eine konvertierungsfreie
# Pflichtmodus-Variante; der existierende run_e2e_smoke.sh laeuft auf
# Windows ueber MSYS-Bash mit MSYS_NO_PATHCONV=1 (Modus `disabled`).
# Dieser PowerShell-Pfad ist die zweite konvertierungsfreie Variante
# (`native_powershell`) und treibt die echte cmake-xray-Binary mit
# nativen Windows-Pfaden, ohne dass MSYS irgendwo dazwischensteht.
#
# Usage:
#   pwsh -File scripts/platform-smoke.ps1 -Binary <path-to-cmake-xray.exe>
#
# Exit codes:
#   0  All checks passed.
#   1  At least one check failed.
#   2  Binary path missing or not executable.

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Binary
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path $Binary)) {
    Write-Error "binary path '$Binary' does not exist"
    exit 2
}

$BinaryFullPath = (Resolve-Path $Binary).Path
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Set-Location $RepoRoot

$script:failures = 0

function Assert-ExitCode {
    param(
        [string]$Description,
        [int]$Expected,
        [scriptblock]$Action
    )
    $output = & $Action 2>&1 | Out-String
    if ($LASTEXITCODE -eq $Expected) {
        Write-Host "PASS: $Description"
    }
    else {
        Write-Host "FAIL: $Description -- expected exit $Expected, got $LASTEXITCODE"
        if ($output) { Write-Host $output }
        $script:failures += 1
    }
}

function Assert-FileWritten {
    param(
        [string]$Description,
        [string]$Path
    )
    if (Test-Path $Path) {
        Write-Host "PASS: $Description"
    }
    else {
        Write-Host "FAIL: $Description -- file '$Path' was not created"
        $script:failures += 1
    }
}

# Print host info for the audit trail; the workflow step also captures
# this through its own "Show toolchain versions" step, but having it
# here keeps the smoke self-contained when run locally.
Write-Host "=== platform-smoke.ps1 (Windows native PowerShell) ==="
Write-Host "binary: $BinaryFullPath"
Write-Host "host:   $env:OS / $((Get-CimInstance -ClassName Win32_OperatingSystem -ErrorAction SilentlyContinue).Caption)"
Write-Host "pwsh:   $($PSVersionTable.PSVersion)"
Write-Host ""

# --- Pflichtkommandos: --help ---
Assert-ExitCode "powershell --help exits 0" 0 {
    & $BinaryFullPath --help | Out-Null
}
Assert-ExitCode "powershell analyze --help exits 0" 0 {
    & $BinaryFullPath analyze --help | Out-Null
}
Assert-ExitCode "powershell impact --help exits 0" 0 {
    & $BinaryFullPath impact --help | Out-Null
}

# --- Pflichtkommandos: analyze x console/json/dot/html ohne --output ---
$ccPath = "tests/e2e/testdata/m2/basic_project/compile_commands.json"
foreach ($format in @('console', 'json', 'dot', 'html')) {
    Assert-ExitCode "powershell analyze --format $format exits 0" 0 {
        & $BinaryFullPath analyze --compile-commands $ccPath --format $format | Out-Null
    }
}

# --- Pflichtkommandos: impact x console/json/dot/html ohne --output ---
$changedFile = "include/common/shared.h"
foreach ($format in @('console', 'json', 'dot', 'html')) {
    Assert-ExitCode "powershell impact --format $format exits 0" 0 {
        & $BinaryFullPath impact --compile-commands $ccPath --changed-file $changedFile --format $format | Out-Null
    }
}

# --- Pflichtkommandos: --output json/dot/html (analyze + impact) ---
$outDir = Join-Path $env:TEMP "cmake-xray-ps-smoke-$PID"
New-Item -ItemType Directory -Path $outDir -Force | Out-Null

foreach ($format in @('json', 'dot', 'html')) {
    $analyzeTarget = Join-Path $outDir "analyze.$format"
    Assert-ExitCode "powershell analyze --format $format --output exits 0" 0 {
        & $BinaryFullPath analyze --compile-commands $ccPath --format $format --output $analyzeTarget | Out-Null
    }
    Assert-FileWritten "powershell analyze --output $format wrote file" $analyzeTarget

    $impactTarget = Join-Path $outDir "impact.$format"
    Assert-ExitCode "powershell impact --format $format --output exits 0" 0 {
        & $BinaryFullPath impact --compile-commands $ccPath --changed-file $changedFile --format $format --output $impactTarget | Out-Null
    }
    Assert-FileWritten "powershell impact --output $format wrote file" $impactTarget
}

Remove-Item -Recurse -Force $outDir -ErrorAction SilentlyContinue

Write-Host ""
if ($script:failures -gt 0) {
    Write-Host "$script:failures PowerShell smoke check(s) FAILED"
    exit 1
}
Write-Host "All PowerShell smoke checks passed (msys_path_mode=native_powershell)"
exit 0
