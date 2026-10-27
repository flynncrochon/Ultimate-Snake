[CmdletBinding(PositionalBinding = $false)]
param(
    [Parameter(Position = 0)][ValidateSet('build','web','unit','gametest','bench')][string]$Action = 'gametest',
    [string]$Python = '',
    [ValidateSet('Release','Debug')][string]$Configuration = 'Release',
    [string]$RaylibSource = '',
    [switch]$CoreOnly,
    [switch]$SkipTests,
    [Alias('R')][string]$TestRegex = '',
    [Parameter(Position = 1, ValueFromRemainingArguments = $true)][string[]]$Arguments = @()
)
$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot

function Find-TestPython {
    if ($Python) { return $Python }
    if ($env:SNAKE_PYTHON) { return $env:SNAKE_PYTHON }
    foreach ($name in @('python','python3')) {
        $command = Get-Command $name -ErrorAction SilentlyContinue
        if ($command -and $command.Source -notlike '*WindowsApps*') { return $command.Source }
    }
    # Blender ships a complete CPython runtime; use it when it is already installed.
    $blenderRoot = Join-Path $env:ProgramFiles 'Blender Foundation'
    if (Test-Path -LiteralPath $blenderRoot) {
        $bundled = Get-ChildItem -LiteralPath $blenderRoot -Recurse -File -Filter python.exe -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -like '*\python\bin\python.exe' } |
            Sort-Object FullName -Descending | Select-Object -First 1
        if ($bundled) { return $bundled.FullName }
    }
    throw 'Python 3.10+ is required. Pass -Python PATH or set SNAKE_PYTHON.'
}

Push-Location $projectRoot
try {
    switch ($Action) {
        'web' {
            & "$projectRoot/scripts/build-web.ps1" -Configuration $Configuration -RaylibSource $RaylibSource @Arguments
        }
        'build' {
            $buildOptions = @{ Configuration = $Configuration; RaylibSource = $RaylibSource }
            if ($Python) { $buildOptions.Python = $Python }
            if ($CoreOnly) { $buildOptions.CoreOnly = $true }
            if ($SkipTests) { $buildOptions.SkipTests = $true }
            & "$projectRoot/scripts/build.ps1" @buildOptions @Arguments
        }
        'unit' {
            $testArguments = @('--test-dir', "$projectRoot/build", '--output-on-failure')
            if ($TestRegex) { $testArguments += @('-R', $TestRegex) }
            & ctest @testArguments @Arguments
            if ($LASTEXITCODE -ne 0) { throw 'Native regression checks failed.' }
        }
        'gametest' {
            $pythonPath = Find-TestPython
            $explicitExecutable = @($Arguments | Where-Object { $_ -eq '--exe' -or $_ -like '--exe=*' }).Count -gt 0
            $nativeRequested = $Arguments -contains '--native'
            $hasHost = if ($nativeRequested) {
                Test-Path -LiteralPath "$projectRoot/build/bin/snake_survivors.exe"
            } else {
                (Test-Path -LiteralPath "$projectRoot/build/bin/snake_game_test.exe") -or
                    (Test-Path -LiteralPath "$projectRoot/build-tests/bin/snake_game_test.exe")
            }
            if (-not $explicitExecutable -and -not $hasHost) {
                & "$projectRoot/scripts/build.ps1"
            }
            & $pythonPath -m game_tests.run @Arguments
            if ($LASTEXITCODE -ne 0) { throw 'Python game tests failed. See the reported result.json.' }
        }
        'bench' {
            if (-not (Test-Path -LiteralPath "$projectRoot/build/bin/snake_render_benchmark.exe")) {
                & "$projectRoot/scripts/build.ps1"
            }
            & "$projectRoot/build/bin/snake_render_benchmark.exe" @Arguments
            if ($LASTEXITCODE -ne 0) { throw 'Native render benchmark failed.' }
        }
    }
} finally { Pop-Location }
