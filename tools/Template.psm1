<#
    Shared helpers for the module/file scaffolding scripts.
#>

function Expand-Placeholders {
    param(
        [Parameter(Mandatory)][AllowEmptyString()][string]$Text,
        [Parameter(Mandatory)][hashtable]$Values
    )
    foreach ($key in $Values.Keys | Sort-Object -Property Length -Descending) {
        $Text = $Text.Replace("@$key@", [string]$Values[$key])
    }
    return $Text
}

<#
    Copies every file under $TemplateRoot into $DestRoot, substituting placeholders
    in both the relative path and the contents, and stripping a trailing .in.
    Returns the list of created paths. Never overwrites an existing file.
#>
function Copy-Template {
    param(
        [Parameter(Mandatory)][string]$TemplateRoot,
        [Parameter(Mandatory)][string]$DestRoot,
        [Parameter(Mandatory)][hashtable]$Values
    )

    if (-not (Test-Path $TemplateRoot)) {
        throw "Template not found: $TemplateRoot"
    }

    $files = @(Get-ChildItem -Path $TemplateRoot -Recurse -File)
    if ($files.Count -eq 0) {
        throw "Template is empty: $TemplateRoot"
    }

    # Resolve every destination first so a collision aborts before anything is written.
    $plan = foreach ($file in $files) {
        $relative = $file.FullName.Substring($TemplateRoot.Length).TrimStart('\')
        $relative = Expand-Placeholders -Text $relative -Values $Values
        if ($relative.EndsWith('.in')) {
            $relative = $relative.Substring(0, $relative.Length - 3)
        }
        $destPath = Join-Path $DestRoot $relative
        if (Test-Path $destPath) {
            throw "File already exists: $destPath"
        }
        [pscustomobject]@{ Source = $file.FullName; Dest = $destPath }
    }

    $created = @()
    foreach ($item in $plan) {
        $destDir = Split-Path -Parent $item.Dest
        if (-not (Test-Path $destDir)) {
            New-Item -ItemType Directory -Path $destDir -Force | Out-Null
        }
        # @FILE@ is per-file, so it is added on top of the caller's values.
        $fileValues = $Values.Clone()
        $fileValues['FILE'] = Split-Path -Leaf $item.Dest
        $content = Expand-Placeholders -Text (Get-Content -Raw -Path $item.Source) -Values $fileValues
        Set-Content -Path $item.Dest -Value $content -Encoding utf8 -NoNewline
        $created += $item.Dest
    }
    return $created
}

<#
    Values shared by every template: who is generating the file, and the copyright line.
    Author resolution: -Author argument, then $env:MITOSIS_AUTHOR, then git user.name,
    then the OS user name - so each teammate gets their own without editing anything.
#>
function Get-CommonValues {
    param(
        [string]$Author,
        [string]$Organization
    )

    if (-not $Author) { $Author = $env:MITOSIS_AUTHOR }
    if (-not $Author) {
        $gitName = (& git config user.name 2>$null)
        if ($LASTEXITCODE -eq 0 -and $gitName) { $Author = $gitName.Trim() }
    }
    if (-not $Author) { $Author = $env:USERNAME }
    if (-not $Author) { $Author = 'unknown' }

    if (-not $Organization) { $Organization = $env:MITOSIS_ORG }
    if (-not $Organization) { $Organization = 'DigiPen (USA) Corporation' }

    return @{
        AUTHOR = $Author
        ORG    = $Organization
        YEAR   = (Get-Date).Year
    }
}

<#
    Inserts a source entry into an existing engine_add_module(<Module> ... ) call,
    keeping the list sorted. No-op when the entry is already listed.
    Returns $true when the file was modified.
#>
function Add-ModuleSource {
    param(
        [Parameter(Mandatory)][string]$CMakeLists,
        [Parameter(Mandatory)][string]$Module,
        [Parameter(Mandatory)][string]$Entry
    )

    $lines = @(Get-Content -Path $CMakeLists)

    $start = -1
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i] -match "^\s*engine_add_module\(\s*$([regex]::Escape($Module))\b") {
            $start = $i
            break
        }
    }
    if ($start -lt 0) {
        throw "engine_add_module($Module ...) not found in $CMakeLists"
    }

    $end = -1
    for ($i = $start + 1; $i -lt $lines.Count; $i++) {
        if ($lines[$i].Trim() -eq ')') {
            $end = $i
            break
        }
    }
    if ($end -lt 0) {
        throw "Could not find the closing ')' of engine_add_module($Module ...) in $CMakeLists"
    }

    $existing = @()
    if ($end -gt $start + 1) {
        $existing = @($lines[($start + 1)..($end - 1)] | Where-Object { $_.Trim() -ne '' })
    }
    if ($existing | Where-Object { $_.Trim() -eq $Entry }) {
        return $false
    }

    $sources = @($existing + "    $Entry" | Sort-Object -Property { $_.Trim() })

    $head = if ($start -ge 0) { $lines[0..$start] } else { @() }
    $tail = $lines[$end..($lines.Count - 1)]
    Set-Content -Path $CMakeLists -Value (@($head) + $sources + @($tail)) -Encoding utf8
    return $true
}

Export-ModuleMember -Function Expand-Placeholders, Copy-Template, Add-ModuleSource, Get-CommonValues
