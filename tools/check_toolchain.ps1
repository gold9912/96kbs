param(
    [switch]$InstallHint
)

$ErrorActionPreference = 'Stop'

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

    $roots = @(
        "$env:ProgramFiles\Microsoft Visual Studio",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio"
    )
    foreach ($root in $roots) {
        if (Test-Path $root) {
            $found = Get-ChildItem $root -Recurse -Filter VsDevCmd.bat -ErrorAction SilentlyContinue | Select-Object -First 1
            if ($found) {
                return $found.FullName
            }
        }
    }

    return $null
}

function Find-CommandPath($name) {
    $cmd = Get-Command $name -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }
    return $null
}

function Display-OrMissing($value, $missing) {
    if ($value) {
        return $value
    }
    return $missing
}

$vsDevCmd = Find-VsDevCmd
$cl = Find-CommandPath 'cl.exe'
$dxc = Find-CommandPath 'dxc.exe'
$cmake = Find-CommandPath 'cmake.exe'
$ninja = Find-CommandPath 'ninja.exe'

Write-Host "MSVC cl.exe:     $(Display-OrMissing $cl '<not in PATH>')"
Write-Host "DXC dxc.exe:     $(Display-OrMissing $dxc '<not in PATH>')"
Write-Host "CMake:           $(Display-OrMissing $cmake '<not in PATH>')"
Write-Host "Ninja:           $(Display-OrMissing $ninja '<not in PATH>')"
Write-Host "VsDevCmd.bat:    $(Display-OrMissing $vsDevCmd '<not found>')"

$ok = $true
if (-not $vsDevCmd -and -not $cl) { $ok = $false }
if (-not $dxc) { $ok = $false }
if (-not $cmake) { $ok = $false }
if (-not $ninja) { $ok = $false }

if (-not $ok) {
    Write-Host ''
    Write-Host 'Required MSVC/DXC toolchain is incomplete.'
    if ($InstallHint) {
        Write-Host ''
        Write-Host 'Suggested installs:'
        Write-Host '  winget install --id Microsoft.VisualStudio.2022.BuildTools --exact --override "--quiet --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"'
        Write-Host '  winget install --id Microsoft.DirectX.ShaderCompiler --exact'
    }
    exit 1
}

exit 0
