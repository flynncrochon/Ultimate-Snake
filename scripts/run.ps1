[CmdletBinding()]
param([Parameter(ValueFromRemainingArguments = $true)][string[]]$GameArguments)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$gamePath = Join-Path $projectRoot 'build/bin/snake_survivors.exe'
if (-not (Test-Path -LiteralPath $gamePath)) { & (Join-Path $PSScriptRoot 'build.ps1') }
Push-Location (Split-Path -Parent $gamePath)
try { & $gamePath @GameArguments }
finally { Pop-Location }
