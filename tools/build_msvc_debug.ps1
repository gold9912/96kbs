param(
    [string]$BuildDir = 'build',
    [switch]$RunApp
)

$ErrorActionPreference = 'Stop'
$repo = Resolve-Path (Join-Path $PSScriptRoot '..')

function Find-VsDevCmd {
    $vswhereCandidates = @(
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe",
        "$env:ProgramFiles\Microsoft Visual Studio\Installer\vswhere.exe"
    )

    foreach ($candidate in $vswhereCandidates) {
        if (Test-Path $candidate) {
            $installPath = & $candidate -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
            if ($LASTEXITCODE -eq 0 -and $installPath) {
                $devCmd = Join-Path $installPath 'Common7\Tools\VsDevCmd.bat'
                if (Test-Path $devCmd) {
                    return $devCmd
                }
            }
        }
    }
    return $null
}

$devCmd = Find-VsDevCmd
if (-not $devCmd) {
    Write-Error 'VsDevCmd.bat was not found. Install Visual Studio 2022 Build Tools with the C++ workload first.'
}

$runLine = "call `"$devCmd`" -arch=amd64 -host_arch=amd64 && cmake -S `"$repo`" -B `"$repo\$BuildDir`" -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build `"$repo\$BuildDir`" && ctest --test-dir `"$repo\$BuildDir`" --output-on-failure"
if ($RunApp) {
    $runLine += " && `"$repo\$BuildDir\rogue96.exe`""
}

cmd.exe /d /c $runLine
exit $LASTEXITCODE

