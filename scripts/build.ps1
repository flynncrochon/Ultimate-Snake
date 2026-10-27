[CmdletBinding()]
param(
    [ValidateSet('Release', 'Debug')][string]$Configuration = 'Release',
    [switch]$CoreOnly,
    [switch]$SkipTests,
    [string]$RaylibSource = '',
    [string]$Python = ''
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw 'CMake 3.20+ is required. Install CMake and add it to PATH.'
}

# Ninja avoids coupling the project to a particular Visual Studio generator.
# Import the developer environment when launched from an ordinary PowerShell.
if (-not (Get-Command cl -ErrorAction SilentlyContinue) -and -not (Get-Command g++ -ErrorAction SilentlyContinue) -and -not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) {
        throw 'Install Visual Studio Build Tools with Desktop development with C++, or use a developer shell with a C++17 compiler.'
    }
    $visualStudio = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $visualStudio) { throw 'Visual Studio C++ tools were not found. Install the Desktop development with C++ workload.' }
    $developerCommand = Join-Path $visualStudio 'Common7/Tools/VsDevCmd.bat'
    $env:PATH = "$(Split-Path -Parent $vswhere);$env:PATH"
    $environmentLines = & $env:ComSpec /d /c "call `"$developerCommand`" -no_logo -arch=x64 -host_arch=x64 >nul && set"
    if ($LASTEXITCODE -ne 0) { throw 'Could not initialize the Visual Studio developer environment.' }
    foreach ($line in $environmentLines) {
        if ($line -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process') }
    }
    $bundledNinja = Join-Path $visualStudio 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja'
    if (Test-Path -LiteralPath (Join-Path $bundledNinja 'ninja.exe')) { $env:PATH = "$bundledNinja;$env:PATH" }
}

if (-not (Get-Command ninja -ErrorAction SilentlyContinue)) {
    throw 'Ninja is required. Install Ninja or run from a Visual Studio developer PowerShell with the bundled Ninja on PATH.'
}

$buildDirectory = Join-Path $projectRoot $(if ($CoreOnly) { 'build-tests' } else { 'build' })
$configureArguments = @('-S', $projectRoot, '-B', $buildDirectory, '-G', 'Ninja', "-DCMAKE_BUILD_TYPE=$Configuration", '-DBUILD_TESTING=ON')
if ($CoreOnly) { $configureArguments += '-DSNAKE_BUILD_GAME=OFF' }
else { $configureArguments += '-DSNAKE_BUILD_GAME=ON' }
if ($RaylibSource) { $configureArguments += "-DFETCHCONTENT_SOURCE_DIR_RAYLIB=$((Resolve-Path -LiteralPath $RaylibSource).Path)" }
if ($Python) { $configureArguments += "-DSNAKE_PYTHON_EXECUTABLE=$Python" }
elseif ($env:SNAKE_PYTHON) { $configureArguments += "-DSNAKE_PYTHON_EXECUTABLE=$env:SNAKE_PYTHON" }

& cmake @configureArguments
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
& cmake --build $buildDirectory --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) { throw 'Compilation failed.' }
if (-not $SkipTests) {
    & ctest --test-dir $buildDirectory -C $Configuration --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Core tests failed.' }
}
if (-not $CoreOnly) { Write-Host "Ready: $(Join-Path $buildDirectory 'bin/snake_survivors.exe')" }
