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
    in both the relative path and the contents, and stripping a trailing .tmpl.
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
        if ($relative.EndsWith('.tmpl')) {
            $relative = $relative.Substring(0, $relative.Length - '.tmpl'.Length)
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

# An entry compares equal whether or not it is commented out, so regenerating a
# module never resurrects a line the author deliberately disabled.
function Get-EntryKey {
    param([Parameter(Mandatory)][AllowEmptyString()][string]$Line)
    return ($Line -replace '^\s*#\s*', '').Trim()
}

<#
    Inserts $Entry into a sorted block of lines and rewrites $File.
    No-op (returns $false) when the entry is already there, commented out or not.

    Two block shapes, chosen by whether -BlockEnd is given:

      * multi-line call - the block runs from the line matching $BlockStart to the
        line matching $BlockEnd. A call written on one line, e.g.
        "target_link_libraries(app PRIVATE foo)", is expanded into the multi-line
        form first. $BlockStart must match the whole head of the call including any
        PRIVATE/PUBLIC/INTERFACE keyword, because whatever follows the match on that
        line is treated as entries.

      * run of sibling lines - the block is the maximal contiguous run of lines
        matching $BlockStart (e.g. every add_subdirectory(modules/...) line).

    When the block is absent and -CreateAfter is given, a new one is written just
    below the first line matching it, using -CreateHeader / -CreateFooter.
#>
function Add-SortedEntry {
    param(
        [Parameter(Mandatory)][string]$File,
        [Parameter(Mandatory)][string]$Entry,
        [Parameter(Mandatory)][string]$BlockStart,
        [string]$BlockEnd,
        [string]$CreateAfter,
        [string]$CreateHeader,
        [string]$CreateFooter,
        [string]$Indent = '    '
    )

    $lines = @(Get-Content -Path $File)
    $key   = Get-EntryKey $Entry

    $start = -1
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i] -match $BlockStart) { $start = $i; break }
    }

    if ($start -lt 0) {
        if (-not $CreateAfter) {
            throw "No block matching /$BlockStart/ in $File"
        }
        $anchor = -1
        for ($i = 0; $i -lt $lines.Count; $i++) {
            if ($lines[$i] -match $CreateAfter) { $anchor = $i; break }
        }
        if ($anchor -lt 0) {
            throw "No block matching /$BlockStart/ and no anchor /$CreateAfter/ in $File"
        }
        $block = @('', $CreateHeader, "$Indent$Entry", $CreateFooter)
        $head  = $lines[0..$anchor]
        $tail  = if ($anchor + 1 -lt $lines.Count) { $lines[($anchor + 1)..($lines.Count - 1)] } else { @() }
        Set-Content -Path $File -Value (@($head) + $block + @($tail)) -Encoding utf8
        return $true
    }

    $head      = $lines[0..$start]
    $entries   = @()
    $footer    = @()
    $collapsed = $false

    if ($BlockEnd) {
        $startLine = $lines[$start]
        $matched   = [regex]::Match($startLine, $BlockStart).Value
        $rest      = $startLine.Substring($matched.Length)

        if ($rest -match '\)') {
            # Collapsed onto one line - split it so the entries get their own lines.
            $head    = if ($start -gt 0) { $lines[0..($start - 1)] } else { @() }
            $head   += $matched
            $entries = @($rest -replace '\).*$', '' -split '\s+' | Where-Object { $_ })
            $footer    = @(')')
            $tailAt    = $start + 1
            $collapsed = $true
        } else {
            $end = -1
            for ($i = $start + 1; $i -lt $lines.Count; $i++) {
                if ($lines[$i] -match $BlockEnd) { $end = $i; break }
            }
            if ($end -lt 0) {
                throw "Could not find the closing line of the block starting at $($File):$($start + 1)"
            }
            if ($end -gt $start + 1) {
                $entries = @($lines[($start + 1)..($end - 1)] | Where-Object { $_.Trim() -ne '' })
            }
            $footer = @($lines[$end])
            $tailAt = $end + 1
        }
    } else {
        # Maximal contiguous run of matching lines.
        $end = $start
        while ($end + 1 -lt $lines.Count -and $lines[$end + 1] -match $BlockStart) { $end++ }
        $head    = if ($start -gt 0) { $lines[0..($start - 1)] } else { @() }
        $entries = @($lines[$start..$end])
        $tailAt  = $end + 1
        $Indent  = ''
    }

    if ($entries | Where-Object { (Get-EntryKey $_) -eq $key }) {
        return $false
    }

    if ($BlockEnd) {
        # Keep the indent the block already uses, then apply it to every entry so a
        # collapsed call that was just split lines up with the rest.
        if (-not $collapsed -and $entries.Count -gt 0) {
            $existingIndent = [regex]::Match($entries[0], '^\s*').Value
            if ($existingIndent) { $Indent = $existingIndent }
        }
        $entries = @($entries | ForEach-Object { "$Indent$($_.Trim())" })
    }

    $entries = @($entries + "$Indent$Entry" | Sort-Object -Property { Get-EntryKey $_ })
    $tail    = if ($tailAt -lt $lines.Count) { $lines[$tailAt..($lines.Count - 1)] } else { @() }

    Set-Content -Path $File -Value (@($head) + $entries + $footer + @($tail)) -Encoding utf8
    return $true
}

<#
    Lists a source file in modules/<Module>'s target_sources(engine_<Module> PRIVATE ...)
    call, creating that call below engine_add_module(<Module>) when it does not exist yet.
    Returns $true when the file was modified.
#>
function Add-ModuleSource {
    param(
        [Parameter(Mandatory)][string]$CMakeLists,
        [Parameter(Mandatory)][string]$Module,
        [Parameter(Mandatory)][string]$Entry
    )

    $target = "engine_$Module"
    return Add-SortedEntry -File $CMakeLists -Entry $Entry `
        -BlockStart "^\s*target_sources\(\s*$([regex]::Escape($target))\s+PRIVATE\b" `
        -BlockEnd '^\s*\)\s*$' `
        -CreateAfter "^\s*engine_add_module\(\s*$([regex]::Escape($Module))\s*\)" `
        -CreateHeader "target_sources($target PRIVATE" `
        -CreateFooter ')'
}

Export-ModuleMember -Function Expand-Placeholders, Copy-Template, Add-SortedEntry, Add-ModuleSource, Get-CommonValues
