$ErrorActionPreference = 'Stop'
[xml]$dialogXml = Get-Content -LiteralPath (Join-Path $PSScriptRoot '..\StandardPartsLibrary.dlx') -Raw
foreach ($blockId in @('selection0', 'selection01', 'selectionTrim')) {
    $status = $dialogXml.SelectSingleNode("//item[@id='$blockId']/PropertyList/Property[@sname='StepStatus']")
    if ($null -eq $status -or $status.selected -ne '1') {
        throw "Block $blockId must be optional; EnableOK controls placement readiness."
    }
}
$enableCallback = $dialogXml.SelectSingleNode("//Property[@id='Enable OK/Apply Button']")
if ($null -eq $enableCallback -or $enableCallback.value -ne 'True') {
    throw 'Enable OK/Apply callback is disabled in the dialog resource.'
}
Write-Output 'PASS dialog: all selection step defaults optional; explicit Enable OK/Apply callback enabled'

[xml]$captureXml = Get-Content -LiteralPath (Join-Path $PSScriptRoot '..\StandardPartsCapture.dlx') -Raw
foreach ($blockId in @('captureBodies', 'capturePoint')) {
    $status = $captureXml.SelectSingleNode("//item[@id='$blockId']/PropertyList/Property[@sname='StepStatus']")
    if ($null -eq $status -or $status.selected -ne '1') {
        throw "Capture block $blockId must be optional; the capture callback validates readiness."
    }
}
$captureBodies = $captureXml.SelectSingleNode("//item[@id='captureBodies']/PropertyList")
if ($captureBodies.SelectSingleNode("Property[@sname='SelectMode']").selected -ne '1' -or
    $captureBodies.SelectSingleNode("Property[@sname='MaximumScope']").selected -ne '10') {
    throw 'Capture must allow multiple bodies within the work part.'
}
foreach ($callback in @('Apply', 'OK', 'Cancel', 'Close', 'Enable OK/Apply Button')) {
    if ($captureXml.SelectSingleNode("//AutoGenData//Property[@id='$callback']").value -ne 'True') {
        throw "Capture callback $callback is disabled."
    }
}
Write-Output 'PASS capture dialog: work-part multiple selection, optional inputs, navigation callbacks enabled'
