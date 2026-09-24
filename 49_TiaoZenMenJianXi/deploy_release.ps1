param([string]$InstallRoot = 'D:\UG智辉钣金插件')
$ErrorActionPreference = 'Stop'
$name = 'TiaoZenMenJianXi'
$title = '查看调整门间隙'
$featureCode = 'ZHIHUI.TIAOZENMENJIANXI'
$release = Join-Path $PSScriptRoot 'bin\Release'
$application = Join-Path $InstallRoot 'application'
$startup = Join-Path $InstallRoot 'startup'
$manifest = Join-Path $InstallRoot 'manifest'
$packagePath = Join-Path $manifest 'zhihui-package.json'
$hashPath = Join-Path $manifest 'file-hashes.json'
$registration = @('UGZH_design.men', 'UGZH_design.tbr', 'UGZH_design.rtb')
$artifacts = @("$name.dll", "$name.dlx", "$name.bmp")
$gbk = [Text.Encoding]::GetEncoding(936)
$utf8 = [Text.UTF8Encoding]::new($false)

function Get-PeChecksum([string]$Path) {
    $bytes = [IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 256 -or $bytes[0] -ne 0x4D -or $bytes[1] -ne 0x5A) { throw "Invalid PE: $Path" }
    $offset = [BitConverter]::ToInt32($bytes, 0x3C)
    if ($offset -lt 0 -or $offset + 92 -gt $bytes.Length -or
        $bytes[$offset] -ne 0x50 -or $bytes[$offset+1] -ne 0x45) { throw "Invalid PE header: $Path" }
    return [BitConverter]::ToUInt32($bytes, $offset + 24 + 64)
}
function Get-PeDllCharacteristics([string]$Path) {
    $bytes = [IO.File]::ReadAllBytes($Path)
    $offset = [BitConverter]::ToInt32($bytes, 0x3C)
    return [BitConverter]::ToUInt16($bytes, $offset + 24 + 70)
}

foreach ($path in @($packagePath, $hashPath,
    (Join-Path $application 'ZhaoFuNxLicenseGate.dll')) +
    @($registration | ForEach-Object { Join-Path $startup $_ }) +
    @($artifacts | ForEach-Object { Join-Path $release $_ })) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing: $path" }
}
$sourceDll = Join-Path $release "$name.dll"
$checksum = Get-PeChecksum $sourceDll
if ($checksum -eq 0) { throw 'Release DLL has a zero PE checksum.' }
$characteristics = Get-PeDllCharacteristics $sourceDll
if (($characteristics -band 0x40) -eq 0 -or
    ($characteristics -band 0x100) -eq 0) { throw 'Release DLL lacks ASLR or DEP/NX.' }
$package = [IO.File]::ReadAllText($packagePath, [Text.Encoding]::UTF8) | ConvertFrom-Json
if ($package.requiresZhaoFuGate -ne $true -or
    $package.multiEntryValidation -ne $true -or
    $package.tamperValidation -ne 'pe-checksum') {
    throw 'Installed protection policy is incomplete.'
}

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$backup = Join-Path (Join-Path $InstallRoot 'backup') "${name}_deploy_$stamp"
foreach ($folder in @('application','startup','manifest')) {
    New-Item -ItemType Directory -Path (Join-Path $backup $folder) -Force | Out-Null
}
foreach ($file in $registration) {
    Copy-Item -LiteralPath (Join-Path $startup $file) -Destination (Join-Path $backup 'startup') -Force
}
foreach ($file in @('zhihui-package.json','file-hashes.json')) {
    Copy-Item -LiteralPath (Join-Path $manifest $file) -Destination (Join-Path $backup 'manifest') -Force
}
foreach ($file in $artifacts) {
    $old = Join-Path $application $file
    if (Test-Path -LiteralPath $old -PathType Leaf) {
        Copy-Item -LiteralPath $old -Destination (Join-Path $backup 'application') -Force
    }
}

foreach ($file in $artifacts) {
    Copy-Item -LiteralPath (Join-Path $release $file) -Destination (Join-Path $application $file) -Force
}
foreach ($file in $registration) {
    $path = Join-Path $startup $file
    $content = [IO.File]::ReadAllText($path, $gbk)
    if ($content -notmatch "(?m)^\s*BUTTON\s+$name\s*$") {
        $anchor = if ($file -eq 'UGZH_design.men') {
            '(?m)(^[ \t]*BUTTON[ \t]+TiaoZenBanLeiCiCun[ \t]*\r?\n' +
            '^[ \t]*LABEL[^\r\n]*\r?\n' +
            '^[ \t]*BITMAP[^\r\n]*\r?\n' +
            '^[ \t]*ACTIONS[ \t]+TiaoZenBanLeiCiCun[ \t]*\r?$)'
        } else {
            '(?m)(^[ \t]*BUTTON[ \t]+TiaoZenBanLeiCiCun[ \t]*\r?\n' +
            '^[ \t]*LABEL[^\r\n]*\r?$)'
        }
        $block = "`r`n`r`n BUTTON  $name`r`n LABEL   $title"
        if ($file -eq 'UGZH_design.men') {
            $block += "`r`n BITMAP  $name.bmp`r`n ACTIONS $name"
        }
        $updated = [regex]::Replace($content, $anchor, ('$1' + $block), 1)
        if ($updated -eq $content) { throw "Could not find menu anchor in $path" }
        [IO.File]::WriteAllText($path, $updated, $gbk)
    }
}

$dllHash = (Get-FileHash -LiteralPath (Join-Path $application "$name.dll") -Algorithm SHA256).Hash
$command = [pscustomobject][ordered]@{
    launcherName = $name
    nativeDll = "$name.dll"
    featureCode = $featureCode
    displayName = $title
    entryPoint = 'ufusr'
    authorizationGate = 'native-multi-entry'
    menuButton = $name
    actionsName = $name
    exportsVerified = $true
    sha256 = $dllHash
    dlxFiles = "application/$name.dlx"
    iconFiles = "application/$name.bmp"
}
$commands = [Collections.ArrayList]::new()
$inserted = $false
foreach ($item in @($package.commands)) {
    if ($item.launcherName -eq $name) { continue }
    [void]$commands.Add($item)
    if (-not $inserted -and $item.launcherName -eq 'TiaoZenBanLeiCiCun') {
        [void]$commands.Add($command); $inserted = $true
    }
}
if (-not $inserted) { [void]$commands.Add($command) }
$package.commands = @($commands)
$package.packageBuiltAtUtc = [DateTime]::UtcNow.ToString('o')
[IO.File]::WriteAllText($packagePath, ($package | ConvertTo-Json -Depth 30), $utf8)

$paths = @($artifacts | ForEach-Object { "application/$_" }) +
    @($registration | ForEach-Object { "startup/$_" })
$entries = @([IO.File]::ReadAllText($hashPath,[Text.Encoding]::UTF8) | ConvertFrom-Json)
if ($entries.Count -eq 1 -and $entries[0] -is [Array]) { $entries = @($entries[0]) }
$remaining = @($entries | Where-Object { $paths -notcontains $_.path })
$newEntries = foreach ($relative in $paths) {
    $path = Join-Path $InstallRoot ($relative.Replace('/','\'))
    [pscustomobject][ordered]@{
        path = $relative
        sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
        bytes = (Get-Item -LiteralPath $path).Length
    }
}
[IO.File]::WriteAllText($hashPath,
    (@($remaining + $newEntries | Sort-Object path) | ConvertTo-Json -Depth 5),$utf8)

foreach ($file in $artifacts) {
    $source = Join-Path $release $file
    $target = Join-Path $application $file
    if ((Get-FileHash $source -Algorithm SHA256).Hash -ne
        (Get-FileHash $target -Algorithm SHA256).Hash) { throw "Hash mismatch: $file" }
}
if ((Get-PeChecksum (Join-Path $application "$name.dll")) -ne $checksum) {
    throw 'Deployed PE checksum mismatch.'
}
Write-Output "Backup=$backup"
Write-Output "DllSha256=$dllHash"
Write-Output ('PeChecksum=0x{0:X8}' -f $checksum)
Write-Output "Deployed=$name.dll,$name.dlx,$name.bmp and command manifests"
