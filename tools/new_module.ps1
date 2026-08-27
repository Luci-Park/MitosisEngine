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

# Register the module with the root build, next to its siblings.
$rootLists = Join-Path $repoRoot 'CMakeLists.txt'
$entry     = "add_subdirectory(modules/$Name)"

# '#?' keeps a commented-out module inside the run instead of splitting it in two.
if (Add-SortedEntry -File $rootLists -Entry $entry `
        -BlockStart '^\s*#?\s*add_subdirectory\(\s*modules/') {
    Write-Host "registered: $entry"
} else {
    Write-Host "already registered: $entry"
}

# Link the new module into the executable.
$exe = [regex]::Match((Get-Content -Raw -Path $rootLists), '(?m)^\s*add_executable\(\s*([A-Za-z0-9_]+)')
if (-not $exe.Success) {
    throw "No add_executable(...) found in $rootLists"
}
$exeName = $exe.Groups[1].Value

if (Add-SortedEntry -File $rootLists -Entry "mts::$Name" `
        -BlockStart "^\s*target_link_libraries\(\s*$([regex]::Escape($exeName))\s+PRIVATE\b" `
        -BlockEnd '^\s*\)\s*$') {
    Write-Host "linked: $exeName <- mts::$Name"
} else {
    Write-Host "already linked: $exeName <- mts::$Name"
}

Write-Host "created module '$Name':"
foreach ($path in $created) {
    Write-Host "  $($path.Substring($repoRoot.Length).TrimStart('\'))"
}
Write-Host "note: the module has no sources yet - CMake will not configure until you add one."
