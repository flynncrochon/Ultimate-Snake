[CmdletBinding()]
param([string]$Python = '')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Installed Visual Studio discovery tool is unavailable.' }
$visualStudio = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $visualStudio) { throw 'Installed MSVC C++ tools were not found.' }
$developerCommand = Join-Path $visualStudio 'Common7/Tools/VsDevCmd.bat'
$env:PATH = "$(Split-Path -Parent $vswhere);$env:PATH"
$environmentLines = & $env:ComSpec /d /c "call `"$developerCommand`" -no_logo -arch=x64 -host_arch=x64 >nul && set"
if ($LASTEXITCODE -ne 0) { throw 'Could not initialize the installed MSVC environment.' }
foreach ($line in $environmentLines) {
    if ($line -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process') }
}
$bundledNinja = Join-Path $visualStudio 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja'
$env:PATH = "$bundledNinja;$env:PATH"
$compiler = Get-Command cl.exe -ErrorAction Stop
$asanRuntime = Join-Path (Split-Path -Parent $compiler.Source) 'clang_rt.asan_dynamic-x86_64.dll'
if (-not (Test-Path -LiteralPath $asanRuntime)) { throw 'The installed MSVC compiler does not include its x64 AddressSanitizer runtime.' }
Write-Host "Compiler: $($compiler.Source)"
Write-Host "AddressSanitizer runtime: $asanRuntime"
$buildDirectory = Join-Path $projectRoot 'build-asan'
$configureArguments = @('-S', $projectRoot, '-B', $buildDirectory, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release', '-DSNAKE_BUILD_GAME=OFF', '-DBUILD_TESTING=ON', '-DSNAKE_RUN_STRESS_TESTS=OFF', '-DCMAKE_CXX_FLAGS=/fsanitize=address /Zi /EHsc', '-DCMAKE_EXE_LINKER_FLAGS=/DEBUG /INCREMENTAL:NO')
if ($Python) { $configureArguments += "-DSNAKE_PYTHON_EXECUTABLE=$Python" }
& cmake @configureArguments
if ($LASTEXITCODE -ne 0) { throw 'AddressSanitizer CMake configuration failed.' }
& cmake --build $buildDirectory --parallel 3
if ($LASTEXITCODE -ne 0) { throw 'AddressSanitizer compilation failed.' }
$env:ASAN_OPTIONS = 'halt_on_error=1'
& ctest --test-dir $buildDirectory --output-on-failure --timeout 60
if ($LASTEXITCODE -ne 0) { throw 'AddressSanitizer regression run failed.' }
Write-Host 'AddressSanitizer checks passed for every configured regression suite.'
