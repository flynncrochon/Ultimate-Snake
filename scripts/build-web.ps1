[CmdletBinding()]
param(
    [ValidateSet('Release', 'Debug')][string]$Configuration = 'Release',
    [string]$RaylibSource = ''
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw 'CMake 3.20+ is required. Install CMake and add it to PATH.'
}
if (-not (Get-Command emcmake -ErrorAction SilentlyContinue)) {
    throw 'Emscripten is required. Install/activate emsdk, then run emsdk_env.ps1 in this PowerShell before building. See docs/WEB.md.'
}
if (-not (Get-Command ninja -ErrorAction SilentlyContinue) -and ${env:ProgramFiles(x86)}) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $visualStudio = & $vswhere -latest -products '*' -property installationPath
        if ($visualStudio) {
            $bundledNinja = Join-Path $visualStudio 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja'
            if (Test-Path -LiteralPath (Join-Path $bundledNinja 'ninja.exe')) { $env:PATH = "$bundledNinja;$env:PATH" }
        }
    }
}
if (-not (Get-Command ninja -ErrorAction SilentlyContinue)) { throw 'Ninja is required. Install Ninja and add it to PATH.' }

$buildDirectory = Join-Path $projectRoot 'build-web'
$configureArguments = @('cmake', '-S', $projectRoot, '-B', $buildDirectory, '-G', 'Ninja',
    "-DCMAKE_BUILD_TYPE=$Configuration", '-DBUILD_TESTING=OFF', '-DSNAKE_BUILD_GAME=ON')
if ($RaylibSource) { $configureArguments += "-DFETCHCONTENT_SOURCE_DIR_RAYLIB=$((Resolve-Path -LiteralPath $RaylibSource).Path)" }
& emcmake @configureArguments
if ($LASTEXITCODE -ne 0) { throw 'Web CMake configuration failed.' }
& cmake --build $buildDirectory --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) { throw 'Web compilation failed.' }
& cmake --install $buildDirectory --config $Configuration --prefix "$buildDirectory/site" --component game
if ($LASTEXITCODE -ne 0) { throw 'Web packaging failed.' }
Write-Host "Ready to host: $buildDirectory/site (serve over HTTP; see docs/WEB.md)"
