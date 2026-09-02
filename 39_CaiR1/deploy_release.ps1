param(
    [Parameter(Mandatory = $true)]
    [string]$InstallRoot
)

$ErrorActionPreference = "Stop"
$workspace = Split-Path -Parent $MyInvocation.MyCommand.Path
$release = Join-Path $workspace "bin\Release"
$application = Join-Path $InstallRoot "application"
$startup = Join-Path $InstallRoot "startup"
$manifest = Join-Path $InstallRoot "manifest"
$backupRoot = Join-Path $InstallRoot "backup"
$menuPath = Join-Path $startup "UGZH_design.men"
$toolbarPath = Join-Path $startup "UGZH_design.tbr"
$ribbonPath = Join-Path $startup "UGZH_design.rtb"
$packagePath = Join-Path $manifest "zhihui-package.json"
$hashPath = Join-Path $manifest "file-hashes.json"
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$gbk = [System.Text.Encoding]::GetEncoding(936)
$displayName = -join @(
    [char]0x5206, [char]0x5272, [char]0x5706,
    [char]0x89D2, [char]0x0031)
$gatePath = Join-Path $application "ZhaoFuNxLicenseGate.dll"
$customFeatureConfigPath =
    Join-Path $application "CustomFeatureConfiguration.xml"

if (-not (Test-Path -LiteralPath $gatePath -PathType Leaf))
{
    throw "Unexpected install root (license gate not found): $InstallRoot"
}
foreach ($required in @(
    (Join-Path $release "CaiR1.dll"),
    (Join-Path $release "CaiR1CoreEdit.dll"),
    (Join-Path $release "CaiR1.dlx"),
    (Join-Path $release "CaiR1.bmp"),
    $customFeatureConfigPath,
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

$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$backup = Join-Path $backupRoot "CaiR1_deploy_$timestamp"
New-Item -ItemType Directory -Path $backup -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $backup "application") -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $backup "startup") -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $backup "manifest") -Force | Out-Null

Copy-Item -LiteralPath $menuPath `
    -Destination (Join-Path $backup "startup\UGZH_design.men")
Copy-Item -LiteralPath $toolbarPath `
    -Destination (Join-Path $backup "startup\UGZH_design.tbr")
Copy-Item -LiteralPath $ribbonPath `
    -Destination (Join-Path $backup "startup\UGZH_design.rtb")
Copy-Item -LiteralPath $packagePath `
    -Destination (Join-Path $backup "manifest\zhihui-package.json")
Copy-Item -LiteralPath $hashPath `
    -Destination (Join-Path $backup "manifest\file-hashes.json")
foreach ($name in @(
    "CaiR1.dll",
    "CaiR1CoreEdit.dll",
    "CaiR1.dlx",
    "CaiR1.bmp",
    "CustomFeatureConfiguration.xml"))
{
    $oldPath = Join-Path $application $name
    if (Test-Path -LiteralPath $oldPath -PathType Leaf)
    {
        Copy-Item -LiteralPath $oldPath `
            -Destination (Join-Path $backup "application\$name")
    }
}

foreach ($name in @("CaiR1.dll", "CaiR1CoreEdit.dll", "CaiR1.dlx", "CaiR1.bmp"))
{
    $sourcePath = Join-Path $release $name
    $destinationPath = Join-Path $application $name
    $alreadyCurrent =
        (Test-Path -LiteralPath $destinationPath -PathType Leaf) -and
        ((Get-FileHash -LiteralPath $sourcePath -Algorithm SHA256).Hash -eq
         (Get-FileHash -LiteralPath $destinationPath -Algorithm SHA256).Hash)
    if (-not $alreadyCurrent)
    {
        Copy-Item -LiteralPath $sourcePath `
            -Destination $destinationPath -Force
    }
}

[xml]$customFeatureConfig =
    [System.IO.File]::ReadAllText(
        $customFeatureConfigPath, [System.Text.Encoding]::UTF8)
$rootElement = $customFeatureConfig.DocumentElement
if ($null -eq $rootElement -or
    $rootElement.LocalName -ne "CustomFeatureLib")
{
    throw "Invalid CustomFeatureConfiguration.xml."
}
$caiR1Node = @(
    $rootElement.CustomFeature |
        Where-Object {
            $_.FeatureClass -eq "NXOpen::CustomFeature::CaiR1"
        }) | Select-Object -First 1
if ($null -eq $caiR1Node)
{
    $caiR1Node =
        $customFeatureConfig.CreateElement("CustomFeature")
    [void]$rootElement.AppendChild($caiR1Node)
}
$caiR1Node.SetAttribute(
    "FeatureClass", "NXOpen::CustomFeature::CaiR1")
$caiR1Node.SetAttribute("FeatureName", $displayName)
$caiR1Node.SetAttribute("FeatureIcon", "CaiR1")
$caiR1Node.SetAttribute("FeatureLibrary", "CaiR1CoreEdit")
$caiR1Node.SetAttribute("FeatureUILibrary", "CaiR1")
$caiR1Node.SetAttribute("IsWithoutBody", "true")
$xmlSettings = New-Object System.Xml.XmlWriterSettings
$xmlSettings.Encoding = $utf8NoBom
$xmlSettings.Indent = $true
$xmlWriter = [System.Xml.XmlWriter]::Create(
    $customFeatureConfigPath, $xmlSettings)
$customFeatureConfig.Save($xmlWriter)
$xmlWriter.Close()

$menuText = [System.IO.File]::ReadAllText(
    $menuPath, [System.Text.Encoding]::UTF8)
if ($menuText -notmatch "(?m)^\s*BUTTON\s+CaiR1\s*$")
{
    $menuBlock =
        " ACTIONS CaiRBan`r`n`r`n" +
        " BUTTON  CaiR1`r`n" +
        " LABEL   $displayName`r`n" +
        " BITMAP  CaiR1.bmp`r`n" +
        " ACTIONS CaiR1"
    $menuText = $menuText.Replace(" ACTIONS CaiRBan", $menuBlock)
    if ($menuText -notmatch "(?m)^\s*BUTTON\s+CaiR1\s*$")
    {
        throw "Could not insert CaiR1 after CaiRBan in the active menu."
    }
    [System.IO.File]::WriteAllText(
        $menuPath, $menuText, $utf8NoBom)
}

foreach ($registrationPath in @($toolbarPath, $ribbonPath))
{
    $registrationText = [System.IO.File]::ReadAllText(
        $registrationPath, $gbk)
    if ($registrationText -notmatch "(?m)^\s*BUTTON\s+CaiR1\s*$")
    {
        $registrationBlock =
            "`r`n`r`n BUTTON  CaiR1`r`n" +
            " LABEL   $displayName"
        $pattern =
            "(?m)(^[ \t]*BUTTON[ \t]+CaiRBan[ \t]*\r?\n" +
            "^[ \t]*LABEL[^\r\n]*\r?$)"
        $registrationText = [regex]::Replace(
            $registrationText,
            $pattern,
            ('$1' + $registrationBlock),
            1)
        if ($registrationText -notmatch
            "(?m)^\s*BUTTON\s+CaiR1\s*$")
        {
            throw "Could not insert CaiR1 in $registrationPath."
        }
        [System.IO.File]::WriteAllText(
            $registrationPath, $registrationText, $gbk)
    }
}

$dllPath = Join-Path $application "CaiR1.dll"
$dllHash = (Get-FileHash -LiteralPath $dllPath -Algorithm SHA256).Hash
$packageText = [System.IO.File]::ReadAllText(
    $packagePath, [System.Text.Encoding]::UTF8)
$package = $packageText | ConvertFrom-Json
if ($package.tamperValidation -ne "pe-checksum")
{
    throw "Refusing to weaken or replace tamperValidation."
}

$command = [pscustomobject][ordered]@{
    launcherName = "CaiR1"
    nativeDll = "CaiR1.dll"
    featureCode = "ZHIHUI.CAIR1"
    displayName = $displayName
    entryPoint = "ufusr"
    authorizationGate = "native-multi-entry"
    menuButton = "CaiR1"
    actionsName = "CaiR1"
    exportsVerified = $true
    sha256 = $dllHash
    dlxFiles = "application/CaiR1.dlx"
    iconFiles = "application/CaiR1.bmp"
    runtimeFiles = @(
        "application/CaiR1CoreEdit.dll",
        "application/CustomFeatureConfiguration.xml")
}

$commands = @($package.commands)
$existingIndex = -1
for ($index = 0; $index -lt $commands.Count; ++$index)
{
    if ($commands[$index].launcherName -eq "CaiR1")
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
        if ($item.launcherName -eq "CaiRBan")
        {
            [void]$updated.Add($command)
        }
    }
    if (-not ($updated | Where-Object { $_.launcherName -eq "CaiR1" }))
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

$hashText = [System.IO.File]::ReadAllText(
    $hashPath, [System.Text.Encoding]::UTF8)
$hashEntries = @($hashText | ConvertFrom-Json)
if ($hashEntries.Count -eq 1 -and
    $hashEntries[0] -is [System.Array])
{
    $hashEntries = @($hashEntries[0])
}
$managedRelativePaths = @(
    "application/CaiR1.bmp",
    "application/CaiR1CoreEdit.dll",
    "application/CaiR1.dll",
    "application/CaiR1.dlx",
    "application/CustomFeatureConfiguration.xml",
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
        sha256 = (Get-FileHash -LiteralPath $absolute -Algorithm SHA256).Hash
        bytes = (Get-Item -LiteralPath $absolute).Length
    }
}
$allEntries = @($remaining + $newEntries | Sort-Object path)
[System.IO.File]::WriteAllText(
    $hashPath,
    ($allEntries | ConvertTo-Json -Depth 5),
    $utf8NoBom)

Write-Output "Backup=$backup"
Write-Output "DllSha256=$dllHash"
Write-Output "Deployed=CaiR1.dll,CaiR1CoreEdit.dll,CaiR1.dlx,CaiR1.bmp,CustomFeatureConfiguration.xml,UGZH_design.men,UGZH_design.tbr,UGZH_design.rtb"
