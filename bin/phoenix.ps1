# Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
# Phoenix command line launcher for Windows PowerShell (see bin/phoenix).
$ErrorActionPreference = 'Stop'
$env:PHOENIX_ROOT = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$toolSrc = Join-Path $env:PHOENIX_ROOT 'packages/phoenix_tools/src'
$toolOut = Join-Path $env:PHOENIX_ROOT 'bin/cache/phoenix_tools'
$stamp = Join-Path $toolOut '.stamp'
$dll = Join-Path $toolOut 'Phoenix.Tools.dll'

$needsBuild = -not (Test-Path $stamp) -or -not (Test-Path $dll)
if (-not $needsBuild) {
    $stampTime = (Get-Item $stamp).LastWriteTimeUtc
    $needsBuild = [bool](Get-ChildItem $toolSrc -Recurse -File |
        Where-Object { $_.FullName -notmatch '[\\/](obj|bin)[\\/]' -and $_.LastWriteTimeUtc -gt $stampTime } |
        Select-Object -First 1)
}
if ($needsBuild) {
    Write-Host 'Building phoenix tool...'
    New-Item -ItemType Directory -Force -Path $toolOut | Out-Null
    dotnet publish (Join-Path $toolSrc 'Phoenix.Tools.csproj') -c Release -o $toolOut --nologo -v quiet -p:UseAppHost=false
    if ($LASTEXITCODE -ne 0) { throw 'Failed to build the phoenix tool.' }
    New-Item -ItemType File -Force -Path $stamp | Out-Null
}
& dotnet $dll @args
exit $LASTEXITCODE
