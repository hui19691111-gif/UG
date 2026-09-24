param([string]$OutputPath = (Join-Path $PSScriptRoot '..\TiaoZenMenJianXi.dlx'))
$ErrorActionPreference = 'Stop'

# Reuse the tested NX 2412 Block Styler XML constructors from the adjacent
# panel-sizing feature. Keep this dialog's layout and IDs local to this script.
$base = [System.IO.File]::ReadAllText(
    (Join-Path $PSScriptRoot '..\..\40_TiaoZenBanLeiCiCun\tools\generate_dlx.ps1'))
$start = $base.IndexOf('function Escape-Xml')
$end = $base.IndexOf('$selectionGroup =')
if ($start -lt 0 -or $end -le $start) { throw 'DLX helper source was not found.' }
Invoke-Expression $base.Substring($start, $end - $start)

$selection = New-Selection 'door_face' '门板平面'
$selection = $selection.Replace('selected="10" sname="MaximumScope"',
                                'selected="3" sname="MaximumScope"')
$selectionGroup = New-Group 'selection_group' '选择门板' @(
    $selection,
    (New-Label 'status' '请选择门板平面。可在多实体零件或装配中测量邻件。')
)
$gapGroup = New-Group 'gap_group' '四边目标间隙（mm）' @(
    (New-Double 'left_gap' '左' 0),
    (New-Label 'left_status' '左：--'),
    (New-Double 'right_gap' '右' 0),
    (New-Label 'right_status' '右：--'),
    (New-Double 'bottom_gap' '下' 0),
    (New-Label 'bottom_status' '下：--'),
    (New-Double 'top_gap' '上' 0),
    (New-Label 'top_status' '上：--')
) -Show:$false
$hintGroup = New-Group 'hint_group' '操作' @(
    (New-Label 'hint' '选面后直接在模型四边中点的输入框修改目标间隙；单击“应用”修改门板。'),
    (New-Label 'corner_hint' '带 R 角的门板请先去除角部圆角，再调整间隙。')
)
$handles = New-HiddenWizardGroup 'dimension_group' @(
    (New-LinearDimension 'left_handle' '左侧间隙'),
    (New-LinearDimension 'right_handle' '右侧间隙'),
    (New-LinearDimension 'bottom_handle' '下侧间隙'),
    (New-LinearDimension 'top_handle' '上侧间隙')
)
$dialog = @"
<?xml version="1.0" encoding="UTF-8"?>
<Dialog NX="2412" id="Dialog" icon="TiaoZenMenJianXi.bmp" name="Dialog" type="uicomp" title="查看调整门间隙" creator="Zhihui" version="1.0.0" Expanded="1" languageInfo="Language and Codeset: simplified chinese 936" ContainerItems="4">
$selectionGroup
$gapGroup
$hintGroup
$handles
<PropertyList id="id" mode="0">
  <Property ClassID="UGS::UICOMP" group="General::" hierarchy="UGS::Styler::DialogItem" id="Visibility" mask="0" name="Visibility" sname="Show" source="1" type="logical" value="True"/>
  <Property ClassID="UGS::UICOMP" group="General::" hierarchy="UGS::Styler::DialogItem" id="Sensitivity" mask="0" name="Sensitivity" sname="Enable" source="1" type="logical" value="True"/>
  <Property ClassID="UGS::UICOMP" group="General::" hierarchy="UGS::Styler::DialogItem" id="Expanded" mask="4" name="Expanded" sname="Expanded" source="1" type="logical" value="True"/>
  <Property ClassID="UGS::UICOMP" group="General::Other::" hierarchy="UGS::Styler::DialogItem" id="HideApply" mask="4096" name="HideApply" sname="HideApply" source="3" type="logical" value="False"/>
  <Property ClassID="UGS::UICOMP" group="General::" hierarchy="UGS::Styler::DialogItem" id="Title" mask="256" name="Title" sname="Label" source="1" type="utfstring" value="查看调整门间隙"/>
</PropertyList>
</Dialog>
"@
[System.IO.File]::WriteAllText([System.IO.Path]::GetFullPath($OutputPath),
    $dialog, [System.Text.UTF8Encoding]::new($false))
