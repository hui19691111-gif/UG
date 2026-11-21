param([string]$InstallRoot = '')
$ErrorActionPreference = 'Stop'
function U([int[]]$codes) { return -join ($codes | ForEach-Object { [char]$_ }) }
$defaultRoot = 'D:\UG' + (U @(0x667A,0x8F89,0x94A3,0x91D1,0x63D2,0x4EF6))
if ([string]::IsNullOrWhiteSpace($InstallRoot)) { $InstallRoot = $defaultRoot }

$name = 'GonCaBiaoZu'
$title = U @(0x516C,0x5DEE,0x6807,0x6CE8)
$featureCode = 'ZHIHUI.GONCABIAOZU'

$release = Join-Path $PSScriptRoot 'bin\Release'
$application = Join-Path $InstallRoot 'application'
$startup = Join-Path $InstallRoot 'startup'
$manifest = Join-Path $InstallRoot 'manifest'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$packagePath = Join-Path $manifest 'zhihui-package.json'
$hashPath = Join-Path $manifest 'file-hashes.json'
$repoPackagePath = Join-Path $repoRoot 'deployment_reference\zhihui-package.json'
$registration = @('UGZH_design.men', 'UGZH_design.tbr', 'UGZH_design.rtb')
$extraIcons = @(Get-ChildItem -LiteralPath $release -Filter "${name}_*.bmp" -File -ErrorAction SilentlyContinue | Sort-Object Name | ForEach-Object { $_.Name })
$artifacts = @("$name.dll", "$name.dlx", "$name.bmp") + $extraIcons
$uiSourceDir = Join-Path $release "${name}UI"
$uiTargetDir = Join-Path $application "${name}UI"
$uiFiles = @()
if (Test-Path -LiteralPath $uiSourceDir -PathType Container) {
    $uiFiles = @(Get-ChildItem -LiteralPath $uiSourceDir -File | Where-Object { $_.Extension -notin @('.pdb', '.ilk', '.exp', '.iobj', '.ipdb') } | Sort-Object Name | ForEach-Object { $_.Name })
}
$helpFiles = @(
    'application\ZhihuiHelp\ZhihuiAll.map',
    "application\ZhihuiHelp\$name.html"
)
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

function Add-MenuButton([string]$Path) {
    $content = [IO.File]::ReadAllText($Path, $gbk)
    if ($content -match "(?m)^\s*BUTTON\s+$name\s*$") { return }
    $isMenu = $Path -like '*.men'
    $anchor = if ($isMenu) {
        '(?m)(^[ \t]*BUTTON[ \t]+ZiDonBiaoZhu[ \t]*\r?\n' +
        '^[ \t]*LABEL[^\r\n]*\r?\n' +
        '^[ \t]*BITMAP[^\r\n]*\r?\n' +
        '^[ \t]*ACTIONS[ \t]+ZiDonBiaoZhu[ \t]*\r?$)'
    } else {
        '(?m)(^[ \t]*BUTTON[ \t]+ZiDonBiaoZhu[ \t]*\r?\n' +
        '^[ \t]*LABEL[^\r\n]*\r?$)'
    }
    $block = "`r`n`r`n BUTTON  $name`r`n LABEL   $title"
    if ($isMenu) { $block += "`r`n BITMAP  $name.bmp`r`n ACTIONS $name" }
    $updated = [regex]::Replace($content, $anchor, ('$1' + $block), 1)
    if ($updated -eq $content) { throw "Could not find menu anchor in $Path" }
    [IO.File]::WriteAllText($Path, $updated, $gbk)
}

$required = @($packagePath, $hashPath,
    (Join-Path $application 'ZhaoFuNxLicenseGate.dll')) +
    @($registration | ForEach-Object { Join-Path $startup $_ }) +
    @($artifacts | ForEach-Object { Join-Path $release $_ })
foreach ($path in $required) {
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
foreach ($folder in @('application','startup','manifest','application\ZhihuiHelp')) {
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
foreach ($relative in $helpFiles) {
    $old = Join-Path $InstallRoot $relative
    if (Test-Path -LiteralPath $old -PathType Leaf) {
        Copy-Item -LiteralPath $old -Destination (Join-Path $backup $relative) -Force
    }
}

foreach ($file in $artifacts) {
    Copy-Item -LiteralPath (Join-Path $release $file) -Destination (Join-Path $application $file) -Force
}
if ($uiFiles.Count -gt 0) {
    if (Test-Path -LiteralPath $uiTargetDir -PathType Container) {
        $uiBackup = Join-Path $backup "application\${name}UI"
        New-Item -ItemType Directory -Path $uiBackup -Force | Out-Null
        Copy-Item -LiteralPath (Join-Path $uiTargetDir '*') -Destination $uiBackup -Recurse -Force -ErrorAction SilentlyContinue
    }
    New-Item -ItemType Directory -Path $uiTargetDir -Force | Out-Null
    foreach ($uif in $uiFiles) {
        Copy-Item -LiteralPath (Join-Path $uiSourceDir $uif) -Destination (Join-Path $uiTargetDir $uif) -Force
    }
}
foreach ($relative in $helpFiles) {
    $source = Join-Path $repoRoot ("deployment_reference\" + $relative)
    $target = Join-Path $InstallRoot $relative
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw "Missing help source: $source" }
    Copy-Item -LiteralPath $source -Destination $target -Force
}
foreach ($file in $registration) {
    Add-MenuButton (Join-Path $startup $file)
    Add-MenuButton (Join-Path $repoRoot "deployment_reference\startup\$file")
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

function Set-PackageCommand([string]$Path) {
    $document = [IO.File]::ReadAllText($Path, [Text.Encoding]::UTF8) | ConvertFrom-Json
    $commands = [Collections.ArrayList]::new()
    $inserted = $false
    foreach ($item in @($document.commands)) {
        if ($item.launcherName -eq $name) { continue }
        [void]$commands.Add($item)
        if (-not $inserted -and $item.launcherName -eq 'ZiDonBiaoZhu') {
            [void]$commands.Add($command); $inserted = $true
        }
    }
    if (-not $inserted) { [void]$commands.Add($command) }
    $document.commands = @($commands)
    $document.packageBuiltAtUtc = [DateTime]::UtcNow.ToString('o')
    [IO.File]::WriteAllText($Path, ($document | ConvertTo-Json -Depth 30), $utf8)
}
Set-PackageCommand $packagePath
if (Test-Path -LiteralPath $repoPackagePath) { Set-PackageCommand $repoPackagePath }

$paths = @($artifacts | ForEach-Object { "application/$_" }) +
    @($uiFiles | ForEach-Object { "application/${name}UI/$_" }) +
    @($registration | ForEach-Object { "startup/$_" }) +
    @($helpFiles | ForEach-Object { $_.Replace('\','/') })
$entries = @([IO.File]::ReadAllText($hashPath, [Text.Encoding]::UTF8) | ConvertFrom-Json)
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
    (@($remaining + $newEntries | Sort-Object path) | ConvertTo-Json -Depth 5), $utf8)

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