using System;
using System.ComponentModel;
using System.Windows;
using System.Windows.Controls;

namespace ZhsmToolboxSettingsUI
{
    public partial class MainWindow : Window
    {
        private readonly string _responsePath;
        private readonly ToolboxSettingsRequest _initialRequest;
        private bool _responseWritten;

        public MainWindow(ToolboxSettingsRequest request, string responsePath)
        {
            _initialRequest = request ?? new ToolboxSettingsRequest();
            InitializeComponent();
            _responsePath = responsePath ?? string.Empty;
            Title = string.IsNullOrWhiteSpace(_initialRequest.WindowTitle) ? "昭福工具设置" : _initialRequest.WindowTitle;
            LoadFromRequest(_initialRequest);
        }

        private void LoadFromRequest(ToolboxSettingsRequest request)
        {
            ExportStepCheckBox.IsChecked = request.ExportStep;
            ExportIgesCheckBox.IsChecked = request.ExportIges;
            StepPreserveAssemblyCheckBox.IsChecked = request.StepPreserveAssembly;
            IgesFlattenAssemblyCheckBox.IsChecked = request.IgesFlattenAssembly;
            MultibodyMinBodyVolumeTextBox.Text = request.MultibodyMinBodyVolume ?? string.Empty;
            MultibodyMinBodyMaxDimensionTextBox.Text = request.MultibodyMinBodyMaxDimension ?? string.Empty;
            MultibodyAutoOpenCheckBox.IsChecked = request.MultibodyAutoOpen;
            MultibodyIncludeHiddenCheckBox.IsChecked = request.MultibodyIncludeHidden;
            AssemblyToMultibodyAutoOpenCheckBox.IsChecked = request.AssemblyToMultibodyAutoOpen;
            AssemblyToMultibodyRecursiveCheckBox.IsChecked = request.AssemblyToMultibodyRecursive;
            AssemblyToMultibodyInheritDisplayCheckBox.IsChecked = request.AssemblyToMultibodyInheritDisplay;
            OneClickColorShowPaletteEveryClickCheckBox.IsChecked = request.OneClickColorShowPaletteEveryClick;
            SelectComboItem(
                OneClickColorPaletteKindComboBox,
                string.IsNullOrWhiteSpace(request.OneClickColorPaletteKind)
                    ? "Light"
                    : request.OneClickColorPaletteKind);
            OneClickColorColorSheetMetalBodiesCheckBox.IsChecked = request.OneClickColorColorSheetMetalBodies;
            OneClickColorColorSameEntityGroupsCheckBox.IsChecked = request.OneClickColorColorSameEntityGroups;
            OneClickColorColorMirroredSameEntityGroupsCheckBox.IsChecked = request.OneClickColorColorMirroredSameEntityGroups;
            OneClickColorColorRemainingEntitiesCheckBox.IsChecked = request.OneClickColorColorRemainingEntities;
            OneClickColorOnlyTopLevelOccurrenceCheckBox.IsChecked = request.OneClickColorOnlyTopLevelOccurrence;
            OneClickColorClearBodyColorCheckBox.IsChecked = request.OneClickColorClearBodyColor;
            OneClickColorClearFaceColorCheckBox.IsChecked = request.OneClickColorClearFaceColor;
            OneClickColorClearFeatureColorCheckBox.IsChecked = request.OneClickColorClearFeatureColor;
        }

        private ToolboxSettingsResponse BuildResponse()
        {
            string projectionStandard = string.IsNullOrWhiteSpace(_initialRequest.AutoDrawingProjectionStandard)
                ? "FirstAngle"
                : _initialRequest.AutoDrawingProjectionStandard;
            string firstAngleTemplatePath = _initialRequest.AutoDrawingFirstAngleTemplatePath ?? string.Empty;
            string thirdAngleTemplatePath = _initialRequest.AutoDrawingThirdAngleTemplatePath ?? string.Empty;
            return new ToolboxSettingsResponse
            {
                ExportStep = ExportStepCheckBox.IsChecked == true,
                ExportIges = ExportIgesCheckBox.IsChecked == true,
                StepPreserveAssembly = StepPreserveAssemblyCheckBox.IsChecked == true,
                IgesFlattenAssembly = IgesFlattenAssemblyCheckBox.IsChecked == true,
                MultibodySignatureTolerance = _initialRequest.MultibodySignatureTolerance ?? string.Empty,
                MultibodyMinBodyVolume = MultibodyMinBodyVolumeTextBox.Text.Trim(),
                MultibodyMinBodyMaxDimension = MultibodyMinBodyMaxDimensionTextBox.Text.Trim(),
                MultibodyPlacementWarnBboxDelta = _initialRequest.MultibodyPlacementWarnBboxDelta ?? string.Empty,
                MultibodyPlacementWarnAxisAngleDeg = _initialRequest.MultibodyPlacementWarnAxisAngleDeg ?? string.Empty,
                MultibodyAutoOpen = MultibodyAutoOpenCheckBox.IsChecked == true,
                MultibodyIncludeHidden = MultibodyIncludeHiddenCheckBox.IsChecked == true,
                MultibodyToAssemblyLayerBlacklist = _initialRequest.MultibodyToAssemblyLayerBlacklist ?? string.Empty,
                MultibodyToAssemblyNamePrefixBlacklist = _initialRequest.MultibodyToAssemblyNamePrefixBlacklist ?? string.Empty,
                AssemblyToMultibodyAutoOpen = AssemblyToMultibodyAutoOpenCheckBox.IsChecked == true,
                AssemblyToMultibodyRecursive = AssemblyToMultibodyRecursiveCheckBox.IsChecked == true,
                AssemblyToMultibodyInheritDisplay = AssemblyToMultibodyInheritDisplayCheckBox.IsChecked == true,
                AssemblyToMultibodyLayerBlacklist = _initialRequest.AssemblyToMultibodyLayerBlacklist ?? string.Empty,
                AssemblyToMultibodyNamePrefixBlacklist = _initialRequest.AssemblyToMultibodyNamePrefixBlacklist ?? string.Empty,
                AutoDrawingProjectionStandard = projectionStandard,
                AutoDrawingFirstAngleTemplatePath = firstAngleTemplatePath,
                AutoDrawingThirdAngleTemplatePath = thirdAngleTemplatePath,
                AutoDrawingTemplatePath = string.IsNullOrWhiteSpace(_initialRequest.AutoDrawingTemplatePath)
                    ? ResolveLegacyAutoDrawingTemplatePath(projectionStandard, firstAngleTemplatePath, thirdAngleTemplatePath)
                    : _initialRequest.AutoDrawingTemplatePath,
                AutoDrawingTopMarginMm = _initialRequest.AutoDrawingTopMarginMm ?? string.Empty,
                AutoDrawingBottomMarginMm = _initialRequest.AutoDrawingBottomMarginMm ?? string.Empty,
                AutoDrawingLeftMarginMm = _initialRequest.AutoDrawingLeftMarginMm ?? string.Empty,
                AutoDrawingRightMarginMm = _initialRequest.AutoDrawingRightMarginMm ?? string.Empty,
                AutoDrawingTitleBlockHeightMm = _initialRequest.AutoDrawingTitleBlockHeightMm ?? string.Empty,
                AutoDrawingTitleBlockWidthMm = _initialRequest.AutoDrawingTitleBlockWidthMm ?? string.Empty,
                AutoDrawingViewToFrameGapMm = _initialRequest.AutoDrawingViewToFrameGapMm ?? string.Empty,
                AutoDrawingViewGapMm = _initialRequest.AutoDrawingViewGapMm ?? string.Empty,
                AutoDrawingRotateViewEnabled = _initialRequest.AutoDrawingRotateViewEnabled,
                AutoDrawingRotateViewMode = string.IsNullOrWhiteSpace(_initialRequest.AutoDrawingRotateViewMode)
                    ? "ViewLongEdgeHorizontal"
                    : _initialRequest.AutoDrawingRotateViewMode,
                OneClickColorShowPaletteEveryClick = OneClickColorShowPaletteEveryClickCheckBox.IsChecked == true,
                OneClickColorPaletteKind = GetSelectedTag(OneClickColorPaletteKindComboBox, "Light"),
                OneClickColorColorSheetMetalBodies = OneClickColorColorSheetMetalBodiesCheckBox.IsChecked == true,
                OneClickColorColorSameEntityGroups = OneClickColorColorSameEntityGroupsCheckBox.IsChecked == true,
                OneClickColorColorMirroredSameEntityGroups = OneClickColorColorMirroredSameEntityGroupsCheckBox.IsChecked == true,
                OneClickColorColorRemainingEntities = OneClickColorColorRemainingEntitiesCheckBox.IsChecked == true,
                OneClickColorOnlyTopLevelOccurrence = OneClickColorOnlyTopLevelOccurrenceCheckBox.IsChecked == true,
                OneClickColorClearBodyColor = OneClickColorClearBodyColorCheckBox.IsChecked == true,
                OneClickColorClearFaceColor = OneClickColorClearFaceColorCheckBox.IsChecked == true,
                OneClickColorClearFeatureColor = OneClickColorClearFeatureColorCheckBox.IsChecked == true
            };
        }

        private static string ResolveLegacyAutoDrawingTemplatePath(
            string projectionStandard,
            string firstAngleTemplatePath,
            string thirdAngleTemplatePath)
        {
            return string.Equals(projectionStandard, "ThirdAngle", StringComparison.OrdinalIgnoreCase)
                ? thirdAngleTemplatePath
                : firstAngleTemplatePath;
        }

        private static void SelectComboItem(ComboBox comboBox, string tag)
        {
            foreach (ComboBoxItem item in comboBox.Items)
            {
                if (string.Equals(item.Tag as string, tag, StringComparison.OrdinalIgnoreCase))
                {
                    comboBox.SelectedItem = item;
                    return;
                }
            }

            if (comboBox.Items.Count > 0)
            {
                comboBox.SelectedIndex = 0;
            }
        }

        private static string GetSelectedTag(ComboBox comboBox, string fallback)
        {
            return (comboBox.SelectedItem as ComboBoxItem)?.Tag as string ?? fallback;
        }

        private void OpenBendFactorSettingsButton_Click(object sender, RoutedEventArgs e)
        {
            BendFactorSettingsWindow window = new BendFactorSettingsWindow
            {
                Owner = this
            };
            window.ShowDialog();
        }

        private void SaveButton_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                ToolboxSettingsResponse response = BuildResponse();
                response.Accepted = true;
                ToolboxSettingsExchange.SavePersistentSettings(response);
                WriteResponseOnce(response);
                Close();
            }
            catch (Exception ex)
            {
                MessageBox.Show(this, ex.ToString(), "保存设置失败", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        private void CancelButton_Click(object sender, RoutedEventArgs e)
        {
            TryWriteCancelResponse();
            Close();
        }

        protected override void OnClosing(CancelEventArgs e)
        {
            if (!_responseWritten)
            {
                TryWriteCancelResponse();
            }

            base.OnClosing(e);
        }

        private void TryWriteCancelResponse()
        {
            try
            {
                WriteResponseOnce(new ToolboxSettingsResponse { Accepted = false });
            }
            catch
            {
            }
        }

        private void WriteResponseOnce(ToolboxSettingsResponse response)
        {
            if (_responseWritten)
            {
                return;
            }

            if (!string.IsNullOrWhiteSpace(_responsePath))
            {
                ToolboxSettingsExchange.SaveResponse(response, _responsePath);
            }

            _responseWritten = true;
        }
    }
}
