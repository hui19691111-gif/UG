param(
    [string]$OutputPath = (Join-Path $PSScriptRoot '..\TiaoZenBanLeiCiCun.dlx')
)

$ErrorActionPreference = 'Stop'

function Escape-Xml([string]$Text) {
    return [System.Security.SecurityElement]::Escape($Text)
}

function Common-Properties([string]$Id, [string]$Class, [bool]$Show = $true) {
    $visible = if ($Show) { 'True' } else { 'False' }
    return @"
<Property ClassID="UGS::UICOMP" group="General::" hierarchy="UGS::$Class" id="API Name" mask="16400" name="API Name" sname="BlockID" source="3" type="string" value="$Id"/>
<Property ClassID="UGS::UICOMP" group="General::" hierarchy="UGS::$Class" id="Visibility" mask="0" name="Visibility" sname="Show" source="1" type="logical" value="$visible"/>
<Property ClassID="UGS::UICOMP" group="General::" hierarchy="UGS::$Class" id="Sensitivity" mask="0" name="Sensitivity" sname="Enable" source="1" type="logical" value="True"/>
<Property ClassID="UGS::UICOMP" group="General::" hierarchy="UGS::$Class" id="Group" mask="16384" name="Group" sname="Group" source="1" type="logical" value="False"/>
<Property ClassID="UGS::UICOMP" group="General::" hierarchy="UGS::$Class" id="Expanded" mask="4" name="Expanded" sname="Expanded" source="1" type="logical" value="True"/>
"@
}

function New-Double([string]$Id, [string]$Title, [double]$Value = 0.0, [bool]$Show = $true) {
    $class = 'UICOMP_double'
    $common = Common-Properties $Id $class $Show
    $safeTitle = Escape-Xml $Title
    return @"
<Property class="$class" hierarchy="" id="$Id" mask="256" name="$Id" presentation="Double" type="uicomp">
  <item Expanded="1" class="$class" icon="styler_double.bmp" id="$Id" name="$Id" presentation="Double" type="uicomp">
    <PropertyList id="id" mode="0">
      $common
      <Property ClassID="UGS::UICOMP" group="General::" hierarchy="UGS::$class" id="Title" mask="256" name="Title" sname="Label" source="1" type="utfstring" value="$safeTitle"/>
      <Property ClassID="UGS::$class" group="Block Specific::" hierarchy="UGS::$class" id="Value" mask="0" name="Value" sname="Value" source="4" type="double" value="$Value"/>
      <Property ClassID="UGS::$class" group="Block Specific::" hierarchy="UGS::$class" id="MinimumValue" mask="0" name="MinimumValue" sname="MinimumValue" source="1" type="double" value="-1000000"/>
      <Property ClassID="UGS::$class" group="Block Specific::" hierarchy="UGS::$class" id="MaximumValue" mask="0" name="MaximumValue" sname="MaximumValue" source="1" type="double" value="1000000"/>
      <Property ClassID="UGS::$class" group="Block Specific::" hierarchy="UGS::$class" id="Increment" mask="0" name="Increment" sname="Increment" source="1" type="double" value="1"/>
      <Property ClassID="UGS::$class" group="Block Specific::" hierarchy="UGS::$class" id="VisibleDecimals" mask="0" name="VisibleDecimals" sname="VisibleDecimals" source="1" type="integer" value="3"/>
    </PropertyList>
  </item>
</Property>
"@
}

function New-Toggle([string]$Id, [string]$Title, [bool]$Value = $false, [bool]$Show = $true) {
    $class = 'UICOMP_toggle'
    $common = Common-Properties $Id $class $Show
    $safeTitle = Escape-Xml $Title
    $logical = if ($Value) { 'True' } else { 'False' }
    return @"
<Property class="$class" hierarchy="" id="$Id" mask="256" name="$Id" presentation="Toggle" type="uicomp">
  <item Expanded="1" class="$class" icon="styler_toggle.bmp" id="$Id" name="$Id" presentation="Toggle" type="uicomp">
    <PropertyList id="id" mode="0">
      $common
      <Property ClassID="UGS::UICOMP" group="General::" hierarchy="UGS::$class" id="Title" mask="256" name="Title" sname="Label" source="1" type="utfstring" value="$safeTitle"/>
      <Property ClassID="UGS::$class" group="Block Specific::" hierarchy="UGS::$class" id="Value" mask="256" name="Value" sname="Value" source="4" type="logical" value="$logical"/>
    </PropertyList>
  </item>
</Property>
"@
}

function New-Label([string]$Id, [string]$Title, [bool]$Show = $true) {
    $class = 'UICOMP_label'
    $common = Common-Properties $Id $class $Show
    $safeTitle = Escape-Xml $Title
    return @"
<Property class="$class" hierarchy="" id="$Id" mask="256" name="$Id" presentation="Label" type="uicomp">
  <item Expanded="1" class="$class" icon="styler_label.bmp" id="$Id" name="$Id" presentation="Label" type="uicomp">
    <PropertyList id="id" mode="0">
      $common
      <Property ClassID="UGS::UICOMP" group="General::" hierarchy="UGS::$class" id="Title" mask="256" name="Title" sname="Label" source="1" type="utfstring" value="$safeTitle"/>
      <Property ClassID="UGS::$class" group="Block Specific::" hierarchy="UGS::$class" id="DisplayBitmapLabel" mask="0" name="DisplayBitmapLabel" sname="DisplayBitmapLabel" source="1" type="logical" value="True"/>
    </PropertyList>
  </item>
</Property>
"@
}

function New-Enum([string]$Id, [string]$Title, [string[]]$Options, [int]$Selected = 0, [bool]$Radio = $false, [int]$Columns = 1, [bool]$Show = $true) {
    $class = 'UICOMP_enum'
    $common = Common-Properties $Id $class $Show
    $safeTitle = Escape-Xml $Title
    $presentation = if ($Radio) { 1 } else { 0 }
    $optionXml = for ($i = 0; $i -lt $Options.Count; $i++) {
        '<Option name="' + (Escape-Xml $Options[$i]) + '" value="' + $i + '"/>'
    }
    return @"
<Property class="$class" hierarchy="" id="$Id" mask="256" name="$Id" presentation="Enumeration" type="uicomp">
  <item Expanded="1" class="$class" icon="styler_enum.bmp" id="$Id" name="$Id" presentation="Enumeration" type="uicomp">
    <PropertyList id="id" mode="0">
      $common
      <Property ClassID="UGS::UICOMP" group="General::" hierarchy="UGS::$class" id="Title" mask="256" name="Title" sname="Label" source="1" type="utfstring" value="$safeTitle"/>
      <Property ClassID="UGS::$class" brief="0" dynamic="0" group="Block Specific::" hierarchy="UGS::$class" id="PresentationStyle" mask="16384" name="PresentationStyle" selected="$presentation" sname="PresentationStyle" source="1" type="enum">
        <Option name="Option Menu" value="0"/><Option name="Radio Box" value="1"/><Option name="Pulldown" value="2"/>
      </Property>
      <Property ClassID="UGS::$class" brief="0" dynamic="0" group="Block Specific::" hierarchy="UGS::$class" id="Titles_1" mask="256" name="Titles_1" selected="$Selected" sname="Value" source="4" type="enum">
        $($optionXml -join '')
      </Property>
      <Property ClassID="UGS::$class" brief="0" dynamic="0" group="Block Specific::" hierarchy="UGS::$class" id="RadioOrientation" mask="16386" name="RadioOrientation" selected="1" sname="Layout" source="1" type="enum">
        <Option name="Vertical" value="0"/><Option name="Horizontal" value="1"/>
      </Property>
      <Property ClassID="UGS::$class" group="Block Specific::" hierarchy="UGS::$class" id="Width" mask="16386" name="Width" sname="NumberOfColumns" source="1" type="integer" value="$Columns"/>
    </PropertyList>
  </item>
</Property>
"@
}

function New-Selection([string]$Id, [string]$Label) {
    $class = 'UICOMP_selection'
    $common = Common-Properties $Id $class $true
    $safeLabel = Escape-Xml $Label
    return @"
<Property class="$class" hierarchy="" id="$Id" mask="256" name="$Id" presentation="Select Object" type="uicomp">
  <item Expanded="1" class="$class" icon="selection_cursor.bmp" id="$Id" name="$Id" presentation="Select Object" type="uicomp">
    <PropertyList id="id" mode="0">
      $common
      <Property ClassID="UGS::$class" group="Block Specific::" hierarchy="UGS::$class" id="Label" mask="256" name="Label" sname="LabelString" source="3" type="utfstring" value="$safeLabel"/>
      <Property ClassID="UGS::$class" group="Block Specific::" hierarchy="UGS::$class" id="Cue" mask="0" name="Cue" sname="Cue" source="1" type="utfstring" value="请选择板件的主平面"/>
      <Property ClassID="UGS::$class" brief="0" dynamic="0" group="Block Specific::" hierarchy="UGS::$class" id="StepStatus" mask="16384" name="StepStatus" selected="0" sname="StepStatus" source="1" type="enum">
        <Option name="Required" value="0"/><Option name="Optional" value="1"/><Option name="Satisfied" value="2"/>
      </Property>
      <Property ClassID="UGS::$class" brief="0" dynamic="0" group="Block Specific::" hierarchy="UGS::$class" id="Many" mask="256" name="Many" selected="0" sname="SelectMode" source="3" type="enum">
        <Option name="Single" value="0"/><Option name="Multiple" value="1"/>
      </Property>
      <Property ClassID="UGS::$class" group="Block Specific::" hierarchy="UGS::$class" id="DetailedFilter" mask="69632" name="DetailedFilter" size="0" sname="DetailedFilter" source="2" type="selfilter"/>
      <Property ClassID="UGS::$class" brief="0" dynamic="0" group="Block Specific::" hierarchy="UGS::$class" id="MaximumScope" mask="0" name="MaximumScope" selected="10" sname="MaximumScope" source="1" type="enum">
        <Option name="Entire Assembly" value="3"/><Option name="Within Work Part Only" value="10"/><Option name="Within Work Part and Components" value="11"/>
      </Property>
      <Property ClassID="UGS::$class" group="Block Specific::" hierarchy="UGS::$class" id="PickPoint" mask="163844" name="PickPoint" sname="PickPoint" source="3" type="point" value="0.000000 0.000000 0.000000"/>
    </PropertyList>
  </item>
</Property>
"@
}

function New-LinearDimension([string]$Id, [string]$Title) {
    $safeTitle = Escape-Xml $Title
    return @"
<Property id="$Id" name="$Id" class="UICOMP_linear_dim" mask="256" type="uicomp" hierarchy="UGS::UI::Comp::WizardGroup" presentation="Linear Dimension">
  <item id="$Id" icon="horizontal_dim.bmp" name="$Id" type="uicomp" class="UICOMP_linear_dim" Expanded="1" hierarchy="UGS::UI::Comp::Container" presentation="Linear Dimension">
    <PropertyList id="id" mode="0">
      <Property id="API Name" mask="16400" name="API Name" type="string" group="General::" sname="BlockID" value="$Id" source="3" ClassID="UGS::UICOMP" hierarchy="UGS::UICOMP_linear_dim"/>
      <Property id="Title" mask="0" name="Title" type="utfstring" group="General::" sname="Label" value="$safeTitle" source="1" ClassID="UGS::UICOMP" hierarchy="UGS::UICOMP_linear_dim"/>
      <Property id="Visibility" mask="0" name="Visibility" type="logical" group="General::" sname="Show" value="True" source="1" ClassID="UGS::UICOMP" hierarchy="UGS::UICOMP_linear_dim"/>
      <Property id="Value" mask="2" name="Value" type="double" group="Block Specific::" sname="Value" value="0.0" source="4" ClassID="UGS::UICOMP_expression" hierarchy="UGS::UICOMP_linear_dim"/>
      <Property id="Formula" mask="0" name="Formula" type="utfstring" group="Block Specific::" sname="Formula" value="0.0" source="1" ClassID="UGS::UICOMP_expression" hierarchy="UGS::UICOMP_linear_dim"/>
      <Property id="MinimumValue" mask="0" name="MinimumValue" type="double" group="Block Specific::" sname="MinimumValue" value="-1000000" source="1" ClassID="UGS::UICOMP_expression" hierarchy="UGS::UICOMP_linear_dim"/>
      <Property id="MaximumValue" mask="0" name="MaximumValue" type="double" group="Block Specific::" sname="MaximumValue" value="1000000" source="1" ClassID="UGS::UICOMP_expression" hierarchy="UGS::UICOMP_linear_dim"/>
      <Property id="ShowFocusHandle" mask="0" name="ShowFocusHandle" type="logical" group="Block Specific::" sname="ShowFocusHandle" value="False" source="1" ClassID="UGS::UICOMP_dim" hierarchy="UGS::UICOMP_linear_dim"/>
      <Property id="ShowHandle" mask="0" name="ShowHandle" type="logical" group="Block Specific::" sname="ShowHandle" value="True" source="1" ClassID="UGS::UICOMP_linear_dim" hierarchy="UGS::UICOMP_linear_dim"/>
      <Property id="HandleOrigin" mask="2" name="HandleOrigin" type="point" group="Block Specific::" sname="HandleOrigin" value="0 0 0" source="1" ClassID="UGS::UICOMP_linear_dim" hierarchy="UGS::UICOMP_linear_dim"/>
      <Property id="HandleOrientation" mask="2" name="HandleOrientation" type="vector" group="Block Specific::" sname="HandleOrientation" value="1 0 0" source="1" ClassID="UGS::UICOMP_linear_dim" hierarchy="UGS::UICOMP_linear_dim"/>
      <Property id="AutoReverseDuringDrag" mask="0" name="AutoReverseDuringDrag" type="logical" group="Block Specific::" sname="AutoReverseDuringDrag" value="True" source="1" ClassID="UGS::UICOMP_linear_dim" hierarchy="UGS::UICOMP_linear_dim"/>
    </PropertyList>
  </item>
</Property>
"@
}

function New-HiddenWizardGroup([string]$Id, [string[]]$Members) {
    return @"
<item presentation="Group" id="$Id" type="uicomp" ContainerItems="1" name="$Id" hierarchy="" class="UGS::UI::Comp::WizardGroup" Expanded="0">
  <PropertyList>
    <Property source="1" name="Members" id="Members" hierarchy="UGS::UI::Comp::WizardGroup" sname="Members" type="array" mask="0" group="Block Specific::" dynamic="1" class="UGS::UI::Comp::Container">
      <PropertyList hierarchy="UGS::UI::Comp::WizardGroup" mode="1" class="UGS::UI::Comp::Container" Expanded="0" id="ContainerItems">
        $($Members -join "`n")
      </PropertyList>
    </Property>
    <Property source="3" name="API Name" id="API Name" hierarchy="UGS::UI::Comp::WizardGroup" sname="BlockID" type="string" value="$Id" mask="16400" group="General::" ClassID="UGS::UICOMP"/>
    <Property source="1" name="Visibility" id="Visibility" hierarchy="UGS::UI::Comp::WizardGroup" sname="Show" type="logical" value="False" mask="0" group="General::" ClassID="UGS::UICOMP"/>
    <Property source="1" name="Title" id="Title" hierarchy="UGS::UI::Comp::WizardGroup" sname="Label" type="utfstring" value="尺寸拖拽手柄" mask="257" group="General::" ClassID="UGS::UICOMP"/>
  </PropertyList>
</item>
"@
}

function New-Group([string]$Id, [string]$Title, [string[]]$Members, [bool]$Show = $true, [int]$Columns = 1) {
    $visible = if ($Show) { 'True' } else { 'False' }
    $safeTitle = Escape-Xml $Title
    return @"
<item Expanded="1" class="UGS::UICOMP_group" icon="styler_block.bmp" id="$Id" name="$Id" presentation="Group" type="uicomp">
  <PropertyList id="id" mode="0">
    <Property ClassID="UGS::UI::Comp::Container" dynamic="1" group="Block Specific::" hierarchy="UGS::UICOMP_group" id="Members" mask="16384" name="Members" sname="Members" source="1" type="array">
      <PropertyList ClassID="UGS::UI::Comp::Container" Expanded="1" hierarchy="UGS::UICOMP_group" icon="tiles_palette.bmp" id="ContainerItems" mode="1">
        $($Members -join "`n")
      </PropertyList>
    </Property>
    <Property ClassID="UGS::UICOMP" group="General::" hierarchy="UGS::UICOMP_group" id="API Name" mask="16400" name="API Name" sname="BlockID" source="3" type="string" value="$Id"/>
    <Property ClassID="UGS::UICOMP" group="General::" hierarchy="UGS::UICOMP_group" id="Visibility" mask="0" name="Visibility" sname="Show" source="1" type="logical" value="$visible"/>
    <Property ClassID="UGS::UICOMP" group="General::" hierarchy="UGS::UICOMP_group" id="Sensitivity" mask="0" name="Sensitivity" sname="Enable" source="1" type="logical" value="True"/>
    <Property ClassID="UGS::UICOMP" group="General::" hierarchy="UGS::UICOMP_group" id="Expanded" mask="0" name="Expanded" sname="Expanded" source="2" type="logical" value="True"/>
    <Property ClassID="UGS::UICOMP" group="General::" hierarchy="UGS::UICOMP_group" id="Title" mask="257" name="Title" sname="Label" source="1" type="utfstring" value="$safeTitle"/>
    <Property ClassID="UGS::UICOMP" group="General::" hierarchy="UGS::UICOMP_group" id="Group" mask="4100" name="Group" sname="Group" source="1" type="logical" value="True"/>
    <Property ClassID="UGS::UICOMP_group" group="Block Specific::" hierarchy="UGS::UICOMP_group" id="Column" mask="16384" name="Column" sname="Column" source="1" type="integer" value="$Columns"/>
  </PropertyList>
</item>
"@
}

function New-DrawingArea(
    [string]$Id,
    [int]$Width,
    [int]$Height,
    [string]$Image = 'TiaoZenBanLeiCiCunGuide.bmp') {
    $safeImage = Escape-Xml $Image
    return @"
<item Expanded="1" class="UGS::UI::Comp::DrawingArea" hierarchy="UGS::UI::Comp::Container" icon="styler_drawingarea.bmp" id="$Id" name="$Id" presentation="Drawing Area" type="uicomp">
  <PropertyList id="id" mode="0">
    <Property ClassID="UGS::UICOMP" group="General::" hierarchy="UGS::UI::Comp::DrawingArea" id="API Name" mask="16400" name="API Name" sname="BlockID" source="3" type="string" value="$Id"/>
    <Property ClassID="UGS::UICOMP" group="General::" hierarchy="UGS::UI::Comp::DrawingArea" id="Title" mask="4" name="Title" sname="Label" source="1" type="utfstring" value="No Title"/>
    <Property ClassID="UGS::UICOMP" group="General::" hierarchy="UGS::UI::Comp::DrawingArea" id="Visibility" mask="0" name="Visibility" sname="Show" source="1" type="logical" value="True"/>
    <Property ClassID="UGS::UICOMP" group="General::" hierarchy="UGS::UI::Comp::DrawingArea" id="Sensitivity" mask="0" name="Sensitivity" sname="Enable" source="1" type="logical" value="True"/>
    <Property ClassID="UGS::UICOMP" group="General::" hierarchy="UGS::UI::Comp::DrawingArea" id="Group" mask="16384" name="Group" sname="Group" source="1" type="logical" value="False"/>
    <Property ClassID="UGS::UICOMP" group="General::" hierarchy="UGS::UI::Comp::DrawingArea" id="Expanded" mask="4" name="Expanded" sname="Expanded" source="1" type="logical" value="True"/>
    <Property ClassID="UGS::UI::Comp::DrawingArea" group="Block Specific::" hierarchy="UGS::UI::Comp::DrawingArea" id="Height" mask="16384" name="Height" sname="Height" source="1" type="integer" value="$Height"/>
    <Property ClassID="UGS::UI::Comp::DrawingArea" group="Block Specific::" hierarchy="UGS::UI::Comp::DrawingArea" id="Width" mask="16384" name="Width" sname="Width" source="1" type="integer" value="$Width"/>
    <Property ClassID="UGS::UI::Comp::DrawingArea" group="Block Specific::" hierarchy="UGS::UI::Comp::DrawingArea" id="Image" mask="0" name="Image" sname="Image" source="1" type="file" value="$safeImage"/>
  </PropertyList>
</item>
"@
}

function New-TabPage([string]$Id, [string]$Title, [string[]]$Members) {
    $safeTitle = Escape-Xml $Title
    return @"
<Property class="UGS::UICOMP_group" hierarchy="UGS::UICOMP_tabs" id="$Id" mask="256" name="$Id" presentation="Tab Page" type="uicomp">
  <item ContainerItems="1" Expanded="1" IsASubContainer="1" class="UGS::UICOMP_group" hierarchy="" icon="styler_browser_tab_control.bmp" id="$Id" name="$Id" presentation="Tab Page" subContainerBrowserName="Tab Page Contents" type="uicomp">
    <PropertyList id="id" mode="0">
      <Property ClassID="UGS::UICOMP" group="General::" hierarchy="UGS::UICOMP_group" id="API Name" mask="16400" name="API Name" sname="BlockID" source="3" type="string" value="$Id"/>
      <Property ClassID="UGS::UICOMP" group="General::" hierarchy="UGS::UICOMP_group" id="Title" mask="257" name="Title" sname="Label" source="1" type="utfstring" value="$safeTitle"/>
      <Property ClassID="UGS::UICOMP_group" group="Block Specific::" hierarchy="UGS::UICOMP_group" id="Column" mask="16384" name="Column" sname="Column" source="1" type="integer" value="1"/>
      <Property class="UGS::UI::Comp::Container" dynamic="1" group="Block Specific::" hierarchy="UGS::UICOMP_group" id="Members" mask="16384" name="Members" sname="Members" source="1" type="array">
        <PropertyList ClassID="UGS::UI::Comp::Container" Expanded="1" hierarchy="UGS::UICOMP_group" id="ContainerItems" mode="1">
          $($Members -join "`n")
        </PropertyList>
      </Property>
    </PropertyList>
  </item>
</Property>
"@
}

function New-TabControl([string]$Id, [string[]]$Pages) {
    return @"
<item ContainerItems="1" DoNotGroupThisObject="1" Expanded="1" IsAContainer="1" class="UGS::UICOMP_tabs" containerBrowserName="$Id" hierarchy="" icon="styler_browser_tab_control.bmp" id="$Id" name="$Id" presentation="Tab Control" type="uicomp">
  <PropertyList>
    <Property class="UGS::UI::Comp::Container" dynamic="1" group="Block Specific::" hierarchy="UGS::UICOMP_tabs" id="Members" mask="0" name="Members" sname="Members" source="1" type="array">
      <PropertyList ClassID="UGS::UI::Comp::Container" Expanded="1" hierarchy="UGS::UICOMP_tabs" icon="tiles_palette.bmp" id="ContainerItems" mode="1">
        $($Pages -join "`n")
      </PropertyList>
    </Property>
    <Property ClassID="UGS::UICOMP" group="General::" hierarchy="UGS::UICOMP_tabs" id="API Name" mask="16400" name="API Name" sname="BlockID" source="3" type="string" value="$Id"/>
    <Property ClassID="UGS::UICOMP" group="General::" hierarchy="UGS::UICOMP_tabs" id="Title" mask="1" name="Title" sname="Label" source="1" type="utfstring" value="No Title"/>
    <Property ClassID="UGS::UICOMP_tabs" group="Block Specific::" hierarchy="UGS::UICOMP_tabs" id="ActivePage" mask="0" name="ActivePage" sname="ActivePage" source="2" type="integer" value="0"/>
    <Property ClassID="UGS::UICOMP_tabs" group="Block Specific::" hierarchy="UGS::UICOMP_tabs" id="TabsPerRow" mask="0" name="TabsPerRow" sname="TabsPerRow" source="2" type="integer" value="3"/>
  </PropertyList>
</item>
"@
}

$selectionGroup = New-Group 'selection_group' '选择与方向' @(
    (New-Selection 'plane_select' '板件平面'),
    (New-Toggle 'swap_direction' '交换长度、宽度方向' $false),
    (New-Label 'current_size' '当前尺寸：请先选择板件平面')
)

$independentGroup = New-Group 'independent_group' '拖拽调整' @(
    (New-Label 'drag_hint' '选择板件平面后，在模型中拖拽四侧箭头调整尺寸'),
    (New-Double 'left_offset' '左' 0 $false),
    (New-Double 'right_offset' '右' 0 $false),
    (New-Double 'bottom_offset' '下' 0 $false),
    (New-Double 'top_offset' '上' 0 $false)
)

$uniformGroup = New-Group 'uniform_group' '统一调整（指定成品尺寸）' @(
    (New-Double 'target_length' '目标长度' 0),
    (New-Double 'target_width' '目标宽度' 0)
)

$roundGroup = New-Group 'round_group' '取整设置' @(
    (New-Toggle 'round_length' '长度取整' $true),
    (New-Double 'length_step' '长度步长' 10),
    (New-Toggle 'round_width' '宽度取整' $true),
    (New-Double 'width_step' '宽度步长' 10),
    (New-Enum 'round_policy' '取整方向' @('就近', '向上', '向下') 0 $false 1)
)

$adjustTabs = New-TabControl 'adjust_tabs' @(
    (New-TabPage 'tab_independent' '每侧独立' @($independentGroup)),
    (New-TabPage 'tab_uniform' '统一调整' @($uniformGroup)),
    (New-TabPage 'tab_round' '取整' @($roundGroup))
)

$modeGroup = New-Group 'mode_group' '调整方式' @(
    $adjustTabs
)

$anchorGroup = New-Group 'anchor_group' '固定基准' @(
    (New-Enum 'anchor' '保持不动的位置' @('左上', '中上', '右上', '左中', '中心', '右中', '左下', '中下', '右下') 4 $true 3),
    (New-Label 'anchor_hint' '统一调整和取整时，尺寸变化向基准点的相反方向延伸')
)

$resultGroup = New-Group 'result_group' '结果预览' @(
    (New-Label 'result_length' '调整后长度：--'),
    (New-Label 'result_width' '调整后宽度：--'),
    (New-Toggle 'live_preview' '实时预览' $true)
)

$dimensionGroup = New-HiddenWizardGroup 'dimension_group' @(
    (New-LinearDimension 'left_handle' '左侧调整'),
    (New-LinearDimension 'right_handle' '右侧调整'),
    (New-LinearDimension 'bottom_handle' '下侧调整'),
    (New-LinearDimension 'top_handle' '上侧调整')
)

$dialog = @"
<?xml version="1.0" encoding="UTF-8"?>
<Dialog NX="2412" id="Dialog" icon="TiaoZenBanLeiCiCun.bmp" name="Dialog" type="uicomp" title="板件调尺" creator="Zhihui" version="1.0.0" Expanded="1" languageInfo="Language and Codeset: simplified chinese 936" ContainerItems="5">
$selectionGroup
$modeGroup
$anchorGroup
$resultGroup
$dimensionGroup
<PropertyList id="id" mode="0">
  <Property ClassID="UGS::UICOMP" group="General::" hierarchy="UGS::Styler::DialogItem" id="Visibility" mask="0" name="Visibility" sname="Show" source="1" type="logical" value="True"/>
  <Property ClassID="UGS::UICOMP" group="General::" hierarchy="UGS::Styler::DialogItem" id="Sensitivity" mask="0" name="Sensitivity" sname="Enable" source="1" type="logical" value="True"/>
  <Property ClassID="UGS::UICOMP" group="General::" hierarchy="UGS::Styler::DialogItem" id="Expanded" mask="4" name="Expanded" sname="Expanded" source="1" type="logical" value="True"/>
  <Property ClassID="UGS::UICOMP" group="General::Other::" hierarchy="UGS::Styler::DialogItem" id="HideApply" mask="4096" name="HideApply" sname="HideApply" source="3" type="logical" value="False"/>
  <Property ClassID="UGS::UICOMP" group="General::" hierarchy="UGS::Styler::DialogItem" id="Title" mask="256" name="Title" sname="Label" source="1" type="utfstring" value="板件调尺"/>
</PropertyList>
</Dialog>
"@

$resolved = [System.IO.Path]::GetFullPath($OutputPath)
$directory = Split-Path -Parent $resolved
if (-not (Test-Path $directory)) {
    New-Item -ItemType Directory -Path $directory | Out-Null
}
[System.IO.File]::WriteAllText($resolved, $dialog, [System.Text.UTF8Encoding]::new($false))
Write-Host "Generated $resolved"
