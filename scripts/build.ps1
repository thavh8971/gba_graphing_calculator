[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [ValidateSet('all', 'gba', 'host-test', 'verify', 'rom-info', 'clean')]
    [string] $Target = 'all',

    [string] $DevkitPro,
    [string] $Make,
    [switch] $Rebuild,

    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]] $MakeArgument
)

$ErrorActionPreference = 'Stop'

function ConvertTo-MakePath {
    param([Parameter(Mandatory = $true)][string] $Path)

    return ([System.IO.Path]::GetFullPath($Path) -replace '\\', '/')
}

function Find-DevkitPro {
    param([string] $RequestedPath)

    $candidates = [System.Collections.Generic.List[string]]::new()
    if ($RequestedPath) {
        $candidates.Add($RequestedPath)
    }
    if ($env:DEVKITPRO -and -not $env:DEVKITPRO.StartsWith('/')) {
        $candidates.Add($env:DEVKITPRO)
    }
    $candidates.Add('C:\devkitPro')
    $candidates.Add('C:\msys64\opt\devkitpro')

    foreach ($candidate in $candidates) {
        $root = [System.IO.Path]::GetFullPath($candidate)
        $compiler = Join-Path $root 'devkitARM\bin\arm-none-eabi-gcc.exe'
        $gbaHeader = Join-Path $root 'libgba\include\gba.h'
        $gbaLibrary = Join-Path $root 'libgba\lib\libgba.a'
        $gbaFix = Join-Path $root 'tools\bin\gbafix.exe'
        if ((Test-Path -LiteralPath $compiler -PathType Leaf) -and
            (Test-Path -LiteralPath $gbaHeader -PathType Leaf) -and
            (Test-Path -LiteralPath $gbaLibrary -PathType Leaf) -and
            (Test-Path -LiteralPath $gbaFix -PathType Leaf)) {
            return $root
        }
    }

    return $null
}

function Find-Make {
    param([string] $RequestedPath)

    if ($RequestedPath) {
        return (Get-Command $RequestedPath -ErrorAction Stop).Source
    }

    $command = Get-Command make -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    $msysMake = 'C:\msys64\usr\bin\make.exe'
    if (Test-Path -LiteralPath $msysMake -PathType Leaf) {
        return $msysMake
    }

    throw 'GNU Make was not found. Install it or pass -Make C:\path\to\make.exe.'
}

$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$makeExe = Find-Make -RequestedPath $Make
$needsGbaTools = $Target -in @('all', 'gba', 'verify', 'rom-info')
$devkitRoot = Find-DevkitPro -RequestedPath $DevkitPro

if ($needsGbaTools -and -not $devkitRoot) {
    throw ('devkitARM, libgba, or gbafix was not found. Install the GBA group ' +
           'with devkitPro, or pass -DevkitPro C:\path\to\devkitpro.')
}

$makeArguments = [System.Collections.Generic.List[string]]::new()
$makeArguments.Add('--no-print-directory')
if ($Rebuild) {
    $makeArguments.Add('-B')
}

if ($devkitRoot) {
    $devkitArm = Join-Path $devkitRoot 'devkitARM'
    $libGba = Join-Path $devkitRoot 'libgba'
    $gbaFix = Join-Path $devkitRoot 'tools\bin\gbafix.exe'

    $env:DEVKITPRO = ConvertTo-MakePath $devkitRoot
    $env:DEVKITARM = ConvertTo-MakePath $devkitArm
    $env:LIBGBA = ConvertTo-MakePath $libGba
    $env:OS = 'Windows_NT'

    $pathEntries = @(
        (Join-Path $devkitArm 'bin'),
        (Join-Path $devkitRoot 'tools\bin')
    )
    $msysBin = 'C:\msys64\usr\bin'
    if (Test-Path -LiteralPath $msysBin -PathType Container) {
        $pathEntries += $msysBin
        $bash = Join-Path $msysBin 'bash.exe'
        if (Test-Path -LiteralPath $bash -PathType Leaf) {
            $env:MAKESHELL = $bash
            $makeArguments.Add('SHELL=' + (ConvertTo-MakePath $bash))
        }
    }
    $env:PATH = (($pathEntries + $env:PATH) -join [System.IO.Path]::PathSeparator)

    # Command-line definitions override stale MSYS-style values inherited by
    # PowerShell (for example /opt/devkitpro).
    $makeArguments.Add('DEVKITPRO=' + (ConvertTo-MakePath $devkitRoot))
    $makeArguments.Add('DEVKITARM=' + (ConvertTo-MakePath $devkitArm))
    $makeArguments.Add('LIBGBA=' + (ConvertTo-MakePath $libGba))
    $makeArguments.Add('GBAFIX=' + (ConvertTo-MakePath $gbaFix))
}
else {
    # Host-only targets can still use the POSIX recipes with an MSYS2 shell
    # when devkitPro itself is not installed.
    $msysBin = 'C:\msys64\usr\bin'
    if (Test-Path -LiteralPath $msysBin -PathType Container) {
        $env:PATH = $msysBin + [System.IO.Path]::PathSeparator + $env:PATH
        $bash = Join-Path $msysBin 'bash.exe'
        if (Test-Path -LiteralPath $bash -PathType Leaf) {
            $env:MAKESHELL = $bash
            $makeArguments.Add('SHELL=' + (ConvertTo-MakePath $bash))
        }
    }
}

$makeArguments.Add($Target)
foreach ($argument in $MakeArgument) {
    $makeArguments.Add($argument)
}

Push-Location $projectRoot
try {
    & $makeExe @makeArguments
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}
finally {
    Pop-Location
}
