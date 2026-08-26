<#
.SYNOPSIS
    Adds a file (or class pair) to an existing engine module.

.DESCRIPTION
    Stamps templates/file/<Template> into modules/<Module>, mirroring <Path> under both
    include/<Module>/ and src/<Module>/, then lists any generated .cpp in the module's
    engine_add_module() call.

.PARAMETER Path
    Sub-path plus base name without extension, e.g. "log/Assert" or "Column".

.EXAMPLE
    powershell -NoProfile -File tools/new_file.ps1 -Module core -Path log/Assert
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidatePattern('^[a-z][a-z0-9_]*$')]
    [string]$Module,

    [Parameter(Mandatory)]
    [ValidatePattern('^[A-Za-z0-9_]+(/[A-Za-z0-9_]+)*$')]
    [string]$Path,

    [ValidateSet('class', 'header')]
    [string]$Template = 'class',

    # Defaults to $env:MITOSIS_AUTHOR, then git user.name, then the OS user name.
    [string]$Author,

    # Defaults to $env:MITOSIS_ORG, then "DigiPen (USA) Corporation".
    [string]$Organization
)

$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'Template.psm1') -Force

$repoRoot   = Split-Path -Parent $PSScriptRoot
$moduleRoot = Join-Path $repoRoot "modules\$Module"
$moduleCMake = Join-Path $moduleRoot 'CMakeLists.txt'

if (-not (Test-Path $moduleCMake)) {
    Write-Error "No such module: modules/$Module (create it with tools/new_module.ps1)"
    exit 1
}

$name   = Split-Path -Leaf $Path
$subDir = Split-Path -Parent ($Path -replace '/', '\')   # "" when Path has no directory part

# Header include path as written in source, e.g. core/log/Assert.h
$includePath = (@($Module, $subDir, "$name.h") | Where-Object { $_ } ) -join '/'
$includePath = $includePath -replace '\\', '/'

$values = Get-CommonValues -Author $Author -Organization $Organization
$values['MODULE']     = $Module
$values['NAME']       = $name
$values['NAME_UPPER'] = $name.ToUpper()
$values['INCLUDE']    = $includePath

# The template tree is flat; place each generated file by its extension.
$templateRoot = Join-Path $repoRoot "templates\file\$Template"
$staging      = Join-Path ([System.IO.Path]::GetTempPath()) ("mitosis_" + [guid]::NewGuid().ToString('N'))
$created      = @()
$addedSources = @()

try {
    $staged = Copy-Template -TemplateRoot $templateRoot -DestRoot $staging -Values $values

    $plan = foreach ($file in $staged) {
        $leaf = Split-Path -Leaf $file
        $root = switch ([System.IO.Path]::GetExtension($leaf)) {
            '.h'   { "include\$Module" }
            '.hpp' { "include\$Module" }
            default { "src\$Module" }
        }
        $dest = Join-Path $moduleRoot ((@($root, $subDir, $leaf) | Where-Object { $_ }) -join '\')
        if (Test-Path $dest) { throw "File already exists: $dest" }
        [pscustomobject]@{ Staged = $file; Dest = $dest }
    }

    foreach ($item in $plan) {
        $destDir = Split-Path -Parent $item.Dest
        if (-not (Test-Path $destDir)) {
            New-Item -ItemType Directory -Path $destDir -Force | Out-Null
        }
        Move-Item -Path $item.Staged -Destination $item.Dest
        $created += $item.Dest

        if ([System.IO.Path]::GetExtension($item.Dest) -eq '.cpp') {
            $relative = $item.Dest.Substring($moduleRoot.Length).TrimStart('\') -replace '\\', '/'
            $addedSources += $relative
        }
    }
} finally {
    if (Test-Path $staging) { Remove-Item -Recurse -Force $staging }
}

foreach ($source in $addedSources) {
    if (Add-ModuleSource -CMakeLists $moduleCMake -Module $Module -Entry $source) {
        Write-Host "listed: $source"
    } else {
        Write-Host "already listed: $source"
    }
}

Write-Host "created:"
foreach ($file in $created) {
    Write-Host "  $($file.Substring($repoRoot.Length).TrimStart('\'))"
}
