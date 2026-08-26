<#
.SYNOPSIS
    Scaffolds an empty engine module.

.DESCRIPTION
    Creates modules/<Name> from templates/module (an empty engine_add_module call plus
    the include/ and src/ trees) and registers it in the root CMakeLists.txt.
    Add files to it with tools/new_file.ps1.

.EXAMPLE
    powershell -NoProfile -File tools/new_module.ps1 -Name render
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidatePattern('^[a-z][a-z0-9_]*$')]
    [string]$Name,

    # Defaults to $env:MITOSIS_AUTHOR, then git user.name, then the OS user name.
    [string]$Author,

    # Defaults to $env:MITOSIS_ORG, then "DigiPen (USA) Corporation".
    [string]$Organization
)

$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'Template.psm1') -Force

$repoRoot = Split-Path -Parent $PSScriptRoot
$destRoot = Join-Path $repoRoot "modules\$Name"

if (Test-Path $destRoot) {
    Write-Error "Module already exists: $destRoot"
    exit 1
}

$values = Get-CommonValues -Author $Author -Organization $Organization
$values['MODULE']       = $Name
$values['MODULE_UPPER'] = $Name.ToUpper()

$created = Copy-Template `
    -TemplateRoot (Join-Path $repoRoot 'templates\module') `
    -DestRoot $destRoot `
    -Values $values

# The header/source trees start out empty; new_file.ps1 fills them in.
foreach ($dir in @("include\$Name", "src\$Name")) {
    New-Item -ItemType Directory -Path (Join-Path $destRoot $dir) -Force | Out-Null
}

# Register the module with the root build.
$rootLists = Join-Path $repoRoot 'CMakeLists.txt'
$entry     = "add_subdirectory(modules/$Name)"
$rootText  = Get-Content -Raw -Path $rootLists
if ($rootText -notmatch [regex]::Escape($entry)) {
    if (-not $rootText.EndsWith("`n")) { $rootText += "`n" }
    Set-Content -Path $rootLists -Value ($rootText + $entry + "`n") -Encoding utf8 -NoNewline
    Write-Host "registered: $entry"
} else {
    Write-Host "already registered: $entry"
}

Write-Host "created module '$Name':"
foreach ($path in $created) {
    Write-Host "  $($path.Substring($repoRoot.Length).TrimStart('\'))"
}
Write-Host "note: the module has no sources yet - CMake will not configure until you add one."
