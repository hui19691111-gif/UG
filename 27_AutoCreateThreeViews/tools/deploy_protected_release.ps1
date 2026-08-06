$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$installRoot = 'D:\UG' + [string]::Concat(
    [char]0x667A,
    [char]0x8F89,
    [char]0x94A3,
    [char]0x91D1,
    [char]0x63D2,
    [char]0x4EF6)
$applicationRoot = Join-Path $installRoot 'application'
$manifestRoot = Join-Path $installRoot 'manifest'
$sourceDll = Join-Path $projectRoot 'bin\Release\AutoCreateThreeViews.dll'
$sourceUnloadHelper = Join-Path $projectRoot 'bin\Release\AutoCreateThreeViewsUnloadHelper.dll'
$sourceUi = Join-Path $projectRoot 'bin\Release\AutoCreateThreeViewsUI\AutoCreateThreeViewsUI.exe'
$targetDll = Join-Path $applicationRoot 'AutoCreateThreeViews.dll'
$targetUnloadHelper = Join-Path $applicationRoot 'AutoCreateThreeViewsUnloadHelper.dll'
$targetUi = Join-Path $applicationRoot 'AutoCreateThreeViewsUI\AutoCreateThreeViewsUI.exe'
$hashManifestPath = Join-Path $manifestRoot 'file-hashes.json'
$packageManifestPath = Join-Path $manifestRoot 'zhihui-package.json'

foreach ($path in @($sourceDll, $sourceUnloadHelper, $sourceUi, $targetDll, $targetUi, $hashManifestPath, $packageManifestPath)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Missing deployment file: $path"
    }
}

function Get-PeChecksum([string] $Path) {
    $bytes = [System.IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 256) { return 0 }
    $peOffset = [BitConverter]::ToInt32($bytes, 0x3c)
    if ($peOffset -lt 0 -or $peOffset + 92 -gt $bytes.Length) { return 0 }
    return [BitConverter]::ToUInt32($bytes, $peOffset + 88)
}

$sourceChecksum = Get-PeChecksum $sourceDll
if ($sourceChecksum -eq 0) {
    throw 'Protected native DLL has no PE checksum.'
}
$replaceDll =
    (Get-FileHash -LiteralPath $sourceDll -Algorithm SHA256).Hash -ne
    (Get-FileHash -LiteralPath $targetDll -Algorithm SHA256).Hash

$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$backupRoot = Join-Path (Join-Path $installRoot 'backup') ("${stamp}_AutoCreateThreeViews_layer_drawing")
New-Item -ItemType Directory -Path $backupRoot -Force | Out-Null
Copy-Item -LiteralPath $targetDll -Destination (Join-Path $backupRoot 'AutoCreateThreeViews.dll') -Force
if (Test-Path -LiteralPath $targetUnloadHelper) {
    Copy-Item -LiteralPath $targetUnloadHelper -Destination (Join-Path $backupRoot 'AutoCreateThreeViewsUnloadHelper.dll') -Force
}
Copy-Item -LiteralPath $targetUi -Destination (Join-Path $backupRoot 'AutoCreateThreeViewsUI.exe') -Force
Copy-Item -LiteralPath $hashManifestPath -Destination (Join-Path $backupRoot 'file-hashes.json') -Force
Copy-Item -LiteralPath $packageManifestPath -Destination (Join-Path $backupRoot 'zhihui-package.json') -Force

try {
    if ($replaceDll) {
        Copy-Item -LiteralPath $sourceDll -Destination $targetDll -Force
    }
    try {
        Copy-Item -LiteralPath $sourceUnloadHelper -Destination $targetUnloadHelper -Force
    }
    catch [System.IO.IOException] {
        # The tiny helper intentionally remains loaded so it can unload the
        # main DLL after future runs.  Keep its loaded image in the same
        # directory (its sibling lookup depends on that directory), then put
        # the newly built helper at the canonical path for the next NX process.
        $loadedHelperBackup = Join-Path $applicationRoot ("AutoCreateThreeViewsUnloadHelper.loaded_${stamp}.dll")
        Move-Item -LiteralPath $targetUnloadHelper -Destination $loadedHelperBackup
        Copy-Item -LiteralPath $sourceUnloadHelper -Destination $targetUnloadHelper
    }
    Copy-Item -LiteralPath $sourceUi -Destination $targetUi -Force

    $dllHash = (Get-FileHash -LiteralPath $targetDll -Algorithm SHA256).Hash
    $unloadHelperHash = (Get-FileHash -LiteralPath $targetUnloadHelper -Algorithm SHA256).Hash
    $uiHash = (Get-FileHash -LiteralPath $targetUi -Algorithm SHA256).Hash
    $dllBytes = (Get-Item -LiteralPath $targetDll).Length
    $unloadHelperBytes = (Get-Item -LiteralPath $targetUnloadHelper).Length
    $uiBytes = (Get-Item -LiteralPath $targetUi).Length

    $hashItems = Get-Content -LiteralPath $hashManifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
    $updates = @{
        'application/AutoCreateThreeViews.dll' = @{ Hash = $dllHash; Bytes = $dllBytes }
        'application/AutoCreateThreeViewsUnloadHelper.dll' = @{ Hash = $unloadHelperHash; Bytes = $unloadHelperBytes }
        'application/AutoCreateThreeViewsUI/AutoCreateThreeViewsUI.exe' = @{ Hash = $uiHash; Bytes = $uiBytes }
    }
    foreach ($relativePath in $updates.Keys) {
        $item = $hashItems | Where-Object { $_.path -eq $relativePath } | Select-Object -First 1
        if ($null -eq $item) {
            $hashItems += [PSCustomObject]@{
                path = $relativePath
                sha256 = $updates[$relativePath].Hash
                bytes = $updates[$relativePath].Bytes
            }
        }
        else {
            $item.sha256 = $updates[$relativePath].Hash
            $item.bytes = $updates[$relativePath].Bytes
        }
    }

    $package = Get-Content -LiteralPath $packageManifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
    $command = $package.commands | Where-Object { $_.launcherName -eq 'AutoCreateThreeViews' } | Select-Object -First 1
    if ($null -eq $command) { throw 'Package command entry not found: AutoCreateThreeViews' }
    $command.sha256 = $dllHash
    $package.packageBuiltAtUtc = [DateTime]::UtcNow.ToString('o')

    $utf8NoBom = [System.Text.UTF8Encoding]::new($false)
    [System.IO.File]::WriteAllText(
        $hashManifestPath,
        (($hashItems | ConvertTo-Json -Depth 10) + [Environment]::NewLine),
        $utf8NoBom)
    [System.IO.File]::WriteAllText(
        $packageManifestPath,
        (($package | ConvertTo-Json -Depth 20) + [Environment]::NewLine),
        $utf8NoBom)

    $verifyItems = Get-Content -LiteralPath $hashManifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
    foreach ($relativePath in $updates.Keys) {
        $entry = $verifyItems | Where-Object { $_.path -eq $relativePath } | Select-Object -First 1
        $fullPath = Join-Path $installRoot ($relativePath -replace '/', '\')
        $actualHash = (Get-FileHash -LiteralPath $fullPath -Algorithm SHA256).Hash
        if ($null -eq $entry -or $entry.sha256 -ne $actualHash) {
            throw "Installed hash verification failed: $relativePath"
        }
    }
    if ((Get-PeChecksum $targetDll) -eq 0) {
        throw 'Installed native DLL lost its PE checksum.'
    }

    [PSCustomObject]@{
        Backup = $backupRoot
        DllSha256 = $dllHash
        UnloadHelperSha256 = $unloadHelperHash
        UiSha256 = $uiHash
        PeChecksum = ('0x{0:X8}' -f (Get-PeChecksum $targetDll))
    } | Format-List
}
catch {
    if ($replaceDll) {
        try { Copy-Item -LiteralPath (Join-Path $backupRoot 'AutoCreateThreeViews.dll') -Destination $targetDll -Force } catch {}
    }
    if (Test-Path -LiteralPath (Join-Path $backupRoot 'AutoCreateThreeViewsUnloadHelper.dll')) {
        try { Copy-Item -LiteralPath (Join-Path $backupRoot 'AutoCreateThreeViewsUnloadHelper.dll') -Destination $targetUnloadHelper -Force } catch {}
    }
    try { Copy-Item -LiteralPath (Join-Path $backupRoot 'AutoCreateThreeViewsUI.exe') -Destination $targetUi -Force } catch {}
    try { Copy-Item -LiteralPath (Join-Path $backupRoot 'file-hashes.json') -Destination $hashManifestPath -Force } catch {}
    try { Copy-Item -LiteralPath (Join-Path $backupRoot 'zhihui-package.json') -Destination $packageManifestPath -Force } catch {}
    throw
}
