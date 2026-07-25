param(
    [Parameter(Mandatory = $false)]
    [string]$InstallRoot = "D:\UG智辉钣金插件"
)

$ErrorActionPreference = "Stop"
$workspace = Split-Path -Parent $MyInvocation.MyCommand.Path
$release = Join-Path $workspace "bin\Release"
$application = Join-Path $InstallRoot "application"
$startup = Join-Path $InstallRoot "startup"
$manifest = Join-Path $InstallRoot "manifest"
$logs = Join-Path $InstallRoot "logs"
$config = Join-Path $InstallRoot "config"
$backupRoot = Join-Path $InstallRoot "backup"
$menuPath = Join-Path $startup "UGZH_design.men"
$toolbarPath = Join-Path $startup "UGZH_design.tbr"
$ribbonPath = Join-Path $startup "UGZH_design.rtb"
$packagePath = Join-Path $manifest "zhihui-package.json"
$hashPath = Join-Path $manifest "file-hashes.json"
$runtimeLogPath = Join-Path $logs "TiaoZenBanLeiCiCun.log"
$gatePath = Join-Path $application "ZhaoFuNxLicenseGate.dll"
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$gbk = [System.Text.Encoding]::GetEncoding(936)
$launcherName = "TiaoZenBanLeiCiCun"
$displayName = "板件调尺"
$featureCode = "ZHIHUI.TIAOZENBANLEICICUN"
$artifactNames = @(
    "$launcherName.dll",
    "$launcherName.dlx",
    "$launcherName.bmp",
    "${launcherName}Guide.bmp")

function Get-PeChecksum([string]$Path)
{
    $bytes = [System.IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 256 -or $bytes[0] -ne 0x4D -or $bytes[1] -ne 0x5A)
    {
        throw "Not a valid PE file: $Path"
    }
    $peOffset = [BitConverter]::ToInt32($bytes, 0x3C)
    if ($peOffset -lt 0 -or $peOffset + 92 -gt $bytes.Length)
    {
        throw "Invalid PE header: $Path"
    }
    if ($bytes[$peOffset] -ne 0x50 -or $bytes[$peOffset + 1] -ne 0x45)
    {
        throw "Missing PE signature: $Path"
    }
    $optionalHeaderOffset = $peOffset + 24
    return [BitConverter]::ToUInt32($bytes, $optionalHeaderOffset + 64)
}

New-Item -ItemType Directory -Path $logs -Force | Out-Null
New-Item -ItemType Directory -Path $config -Force | Out-Null

foreach ($required in @(
    $gatePath,
    (Join-Path $release "$launcherName.dll"),
    (Join-Path $release "$launcherName.dlx"),
    (Join-Path $release "$launcherName.bmp"),
    (Join-Path $release "${launcherName}Guide.bmp"),
    $menuPath,
    $toolbarPath,
    $ribbonPath,
    $packagePath,
    $hashPath))
{
    if (-not (Test-Path -LiteralPath $required -PathType Leaf))
    {
        throw "Required file is missing: $required"
    }
}

$sourceDll = Join-Path $release "$launcherName.dll"
$sourceChecksum = Get-PeChecksum $sourceDll
if ($sourceChecksum -eq 0)
{
    throw "Refusing to deploy an unprotected DLL with a zero PE checksum."
}
$sourceHashes = @{}
foreach ($name in $artifactNames)
{
    $sourceHashes[$name] =
        (Get-FileHash -LiteralPath (Join-Path $release $name) -Algorithm SHA256).Hash
}

$package = [System.IO.File]::ReadAllText(
    $packagePath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
if ($package.requiresZhaoFuGate -ne $true -or
    $package.multiEntryValidation -ne $true -or
    $package.tamperValidation -ne "pe-checksum")
{
    throw "Refusing to deploy because the installed protection policy is not intact."
}

$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$backup = Join-Path $backupRoot "${launcherName}_deploy_$timestamp"
New-Item -ItemType Directory -Path (Join-Path $backup "application") -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $backup "startup") -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $backup "manifest") -Force | Out-Null

foreach ($path in @($menuPath, $toolbarPath, $ribbonPath))
{
    Copy-Item -LiteralPath $path -Destination (Join-Path $backup "startup") -Force
}
foreach ($path in @($packagePath, $hashPath))
{
    Copy-Item -LiteralPath $path -Destination (Join-Path $backup "manifest") -Force
}
foreach ($name in $artifactNames)
{
    $oldPath = Join-Path $application $name
    if (Test-Path -LiteralPath $oldPath -PathType Leaf)
    {
        Copy-Item -LiteralPath $oldPath `
            -Destination (Join-Path $backup "application\$name") -Force
    }
}

foreach ($name in $artifactNames)
{
    Copy-Item -LiteralPath (Join-Path $release $name) `
        -Destination (Join-Path $application $name) -Force
}

$menuText = [System.IO.File]::ReadAllText(
    $menuPath, [System.Text.Encoding]::UTF8)
if ($menuText -notmatch "(?m)^\s*BUTTON\s+$launcherName\s*$")
{
    $menuBlock =
        "`r`n`r`n BUTTON  $launcherName`r`n" +
        " LABEL   $displayName`r`n" +
        " BITMAP  $launcherName.bmp`r`n" +
        " ACTIONS $launcherName"
    $pattern =
        "(?m)(^[ \t]*BUTTON[ \t]+CaiR1[ \t]*\r?\n" +
        "^[ \t]*LABEL[^\r\n]*\r?\n" +
        "^[ \t]*BITMAP[^\r\n]*\r?\n" +
        "^[ \t]*ACTIONS[ \t]+CaiR1[ \t]*\r?$)"
    $menuText = [regex]::Replace(
        $menuText, $pattern, ('$1' + $menuBlock), 1)
    if ($menuText -notmatch "(?m)^\s*BUTTON\s+$launcherName\s*$")
    {
        throw "Could not insert $launcherName after CaiR1 in the active menu."
    }
    [System.IO.File]::WriteAllText($menuPath, $menuText, $utf8NoBom)
}

foreach ($registrationPath in @($toolbarPath, $ribbonPath))
{
    $registrationText =
        [System.IO.File]::ReadAllText($registrationPath, $gbk)
    if ($registrationText -notmatch "(?m)^\s*BUTTON\s+$launcherName\s*$")
    {
        $registrationBlock =
            "`r`n`r`n BUTTON  $launcherName`r`n" +
            " LABEL   $displayName"
        $pattern =
            "(?m)(^[ \t]*BUTTON[ \t]+CaiR1[ \t]*\r?\n" +
            "^[ \t]*LABEL[^\r\n]*\r?$)"
        $registrationText = [regex]::Replace(
            $registrationText, $pattern,
            ('$1' + $registrationBlock), 1)
        if ($registrationText -notmatch
            "(?m)^\s*BUTTON\s+$launcherName\s*$")
        {
            throw "Could not insert $launcherName in $registrationPath."
        }
        [System.IO.File]::WriteAllText(
            $registrationPath, $registrationText, $gbk)
    }
}

$targetDll = Join-Path $application "$launcherName.dll"
$targetDllHash =
    (Get-FileHash -LiteralPath $targetDll -Algorithm SHA256).Hash
$command = [pscustomobject][ordered]@{
    launcherName = $launcherName
    nativeDll = "$launcherName.dll"
    featureCode = $featureCode
    displayName = $displayName
    entryPoint = "ufusr"
    authorizationGate = "native-multi-entry"
    menuButton = $launcherName
    actionsName = $launcherName
    exportsVerified = $true
    sha256 = $targetDllHash
    dlxFiles = "application/$launcherName.dlx"
    iconFiles = @(
        "application/$launcherName.bmp",
        "application/${launcherName}Guide.bmp")
}

$commands = @($package.commands)
$existingIndex = -1
for ($index = 0; $index -lt $commands.Count; ++$index)
{
    if ($commands[$index].launcherName -eq $launcherName)
    {
        $existingIndex = $index
        break
    }
}
if ($existingIndex -ge 0)
{
    $commands[$existingIndex] = $command
}
else
{
    $updated = New-Object System.Collections.ArrayList
    foreach ($item in $commands)
    {
        [void]$updated.Add($item)
        if ($item.launcherName -eq "CaiR1")
        {
            [void]$updated.Add($command)
        }
    }
    if (-not ($updated | Where-Object {
        $_.launcherName -eq $launcherName }))
    {
        [void]$updated.Add($command)
    }
    $commands = @($updated)
}
$package.commands = $commands
$package.packageBuiltAtUtc = [DateTime]::UtcNow.ToString("o")
[System.IO.File]::WriteAllText(
    $packagePath,
    ($package | ConvertTo-Json -Depth 30),
    $utf8NoBom)

$hashEntries = @(
    [System.IO.File]::ReadAllText(
        $hashPath, [System.Text.Encoding]::UTF8) |
        ConvertFrom-Json)
if ($hashEntries.Count -eq 1 -and $hashEntries[0] -is [System.Array])
{
    $hashEntries = @($hashEntries[0])
}
$managedRelativePaths = @(
    "application/$launcherName.bmp",
    "application/$launcherName.dll",
    "application/$launcherName.dlx",
    "application/${launcherName}Guide.bmp",
    "startup/UGZH_design.men",
    "startup/UGZH_design.tbr",
    "startup/UGZH_design.rtb")
$remaining = @(
    $hashEntries |
        Where-Object { $managedRelativePaths -notcontains $_.path })
$newEntries = foreach ($relative in $managedRelativePaths)
{
    $absolute = Join-Path $InstallRoot ($relative -replace "/", "\")
    [pscustomobject][ordered]@{
        path = $relative
        sha256 =
            (Get-FileHash -LiteralPath $absolute -Algorithm SHA256).Hash
        bytes = (Get-Item -LiteralPath $absolute).Length
    }
}
$allEntries = @($remaining + $newEntries | Sort-Object path)
[System.IO.File]::WriteAllText(
    $hashPath,
    ($allEntries | ConvertTo-Json -Depth 5),
    $utf8NoBom)

foreach ($name in $artifactNames)
{
    $targetHash = (Get-FileHash -LiteralPath `
        (Join-Path $application $name) -Algorithm SHA256).Hash
    if ($targetHash -ne $sourceHashes[$name])
    {
        throw "Post-deployment hash mismatch: $name"
    }
}
$targetChecksum = Get-PeChecksum $targetDll
if ($targetChecksum -ne $sourceChecksum -or $targetChecksum -eq 0)
{
    throw "Post-deployment PE checksum validation failed."
}

[System.IO.File]::AppendAllText(
    $runtimeLogPath,
    ((Get-Date -Format "yyyy-MM-dd HH:mm:ss.fff") +
        " [DEPLOY] Installed detailed logging build; DllSha256=" +
        $targetDllHash + "`r`n"),
    $utf8NoBom)

Write-Output "Backup=$backup"
Write-Output "DllSha256=$targetDllHash"
Write-Output ("PeChecksum=0x{0:X8}" -f $targetChecksum)
Write-Output "Protection=ZhaoFuGate+native-multi-entry+pe-checksum+sha256"
Write-Output "RuntimeLog=$runtimeLogPath"
Write-Output "Deployed=$launcherName.dll,$launcherName.dlx,$launcherName.bmp,${launcherName}Guide.bmp,UGZH_design.men,UGZH_design.tbr,UGZH_design.rtb,zhihui-package.json,file-hashes.json"
