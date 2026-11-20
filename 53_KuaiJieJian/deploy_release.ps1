param([string]$InstallRoot = '')
$ErrorActionPreference = 'Stop'
function U([int[]]$codes) { return -join ($codes | ForEach-Object { [char]$_ }) }
$defaultRoot = 'D:\UG' + (U @(0x667A,0x8F89,0x94A3,0x91D1,0x63D2,0x4EF6))
if ([string]::IsNullOrWhiteSpace($InstallRoot)) { $InstallRoot = $defaultRoot }

$name = 'KuaiJieJian'
$loaderName = 'KuaiJieJianLoader'
$title = U @(0x5FEB,0x6377,0x952E)
$featureCode = 'ZHIHUI.KUAIJIEJIAN'

$release = Join-Path $PSScriptRoot 'bin\Release'
$application = Join-Path $InstallRoot 'application'
$startup = Join-Path $InstallRoot 'startup'
$manifest = Join-Path $InstallRoot 'manifest'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$packagePath = Join-Path $manifest 'zhihui-package.json'
$hashPath = Join-Path $manifest 'file-hashes.json'
$repoPackagePath = Join-Path $repoRoot 'deployment_reference\zhihui-package.json'
$registration = @('UGZH_design.men', 'UGZH_design.tbr', 'UGZH_design.rtb')
$appArtifacts = @("$name.dll", "$name.bmp")
$startupArtifacts = @("$loaderName.dll")
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
        '(?m)(^[ \t]*BUTTON[ \t]+JieSanZuJian[ \t]*\r?\n' +
        '^[ \t]*LABEL[^\r\n]*\r?\n' +
        '^[ \t]*BITMAP[^\r\n]*\r?\n' +
        '^[ \t]*ACTIONS[ \t]+JieSanZuJian[ \t]*\r?$)'
    } else {
        '(?m)(^[ \t]*BUTTON[ \t]+JieSanZuJian[ \t]*\r?\n' +
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
    @($appArtifacts | ForEach-Object { Join-Path $release $_ }) +
    @($startupArtifacts | ForEach-Object { Join-Path $release $_ })
foreach ($path in $required) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing: $path" }
}

foreach ($dllName in @("$name.dll", "$loaderName.dll")) {
    $sourceDll = Join-Path $release $dllName
    $checksum = Get-PeChecksum $sourceDll
    if ($checksum -eq 0) { throw "Release DLL has a zero PE checksum: $dllName" }
    $characteristics = Get-PeDllCharacteristics $sourceDll
    if (($characteristics -band 0x40) -eq 0 -or ($characteristics -band 0x100) -eq 0) {
        throw "Release DLL lacks ASLR or DEP/NX: $dllName"
    }
}

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
foreach ($file in $appArtifacts) {
    $old = Join-Path $application $file
    if (Test-Path -LiteralPath $old -PathType Leaf) {
        Copy-Item -LiteralPath $old -Destination (Join-Path $backup 'application') -Force
    }
}
foreach ($file in $startupArtifacts) {
    $old = Join-Path $startup $file
    if (Test-Path -LiteralPath $old -PathType Leaf) {
        Copy-Item -LiteralPath $old -Destination (Join-Path $backup 'startup') -Force
    }
}

foreach ($file in $appArtifacts) {
    Copy-Item -LiteralPath (Join-Path $release $file) -Destination (Join-Path $application $file) -Force
}
foreach ($file in $startupArtifacts) {
    $srcPath = Join-Path $release $file
    $dstPath = Join-Path $startup $file
    $needCopy = $true
    if (Test-Path -LiteralPath $dstPath -PathType Leaf) {
        if ((Get-FileHash -LiteralPath $srcPath -Algorithm SHA256).Hash -eq
            (Get-FileHash -LiteralPath $dstPath -Algorithm SHA256).Hash) {
            $needCopy = $false
        }
    }
    if ($needCopy) {
        Copy-Item -LiteralPath $srcPath -Destination $dstPath -Force
    }
    $refStartup = Join-Path $repoRoot "deployment_reference\startup\$file"
    Copy-Item -LiteralPath $srcPath -Destination $refStartup -Force
}
foreach ($file in $registration) {
    Add-MenuButton (Join-Path $startup $file)
    Add-MenuButton (Join-Path $repoRoot "deployment_reference\startup\$file")
}

$dllHash = (Get-FileHash -LiteralPath (Join-Path $application "$name.dll") -Algorithm SHA256).Hash
$loaderHash = (Get-FileHash -LiteralPath (Join-Path $startup "$loaderName.dll") -Algorithm SHA256).Hash
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
    iconFiles = "application/$name.bmp"
}

function Set-PackageCommand([string]$Path) {
    $document = [IO.File]::ReadAllText($Path, [Text.Encoding]::UTF8) | ConvertFrom-Json
    $commands = [Collections.ArrayList]::new()
    $inserted = $false
    foreach ($item in @($document.commands)) {
        if ($item.launcherName -eq $name) { continue }
        [void]$commands.Add($item)
        if (-not $inserted -and $item.launcherName -eq 'JieSanZuJian') {
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

$paths = @($appArtifacts | ForEach-Object { "application/$_" }) +
    @($startupArtifacts | ForEach-Object { "startup/$_" }) +
    @($registration | ForEach-Object { "startup/$_" })
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

foreach ($file in $appArtifacts) {
    if ((Get-FileHash (Join-Path $release $file) -Algorithm SHA256).Hash -ne
        (Get-FileHash (Join-Path $application $file) -Algorithm SHA256).Hash) {
        throw "Hash mismatch: $file"
    }
}
foreach ($file in $startupArtifacts) {
    if ((Get-FileHash (Join-Path $release $file) -Algorithm SHA256).Hash -ne
        (Get-FileHash (Join-Path $startup $file) -Algorithm SHA256).Hash) {
        throw "Hash mismatch: $file"
    }
}
Write-Output "Backup=$backup"
Write-Output "DllSha256=$dllHash"
Write-Output "LoaderSha256=$loaderHash"
Write-Output "Deployed=$name.dll,$name.bmp,$loaderName.dll and manifests"
