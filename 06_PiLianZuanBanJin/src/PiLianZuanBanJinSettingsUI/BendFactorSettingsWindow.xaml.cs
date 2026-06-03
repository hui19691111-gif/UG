using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using Zhaofu.Nx.SheetMetal.BendFactorBatch.Shared;

namespace ZhsmToolboxSettingsUI;

public partial class BendFactorSettingsWindow : Window
{
    private const string WorkspaceDeployRoot = @"I:\ZFBJ\deploy\zhaofu-suite";
    public const string DefaultConfigRelativePath = @"config\bend_factor_batch_config.json";
    private readonly string _configRelativePath;
    private readonly string _configPath;
    private BendFactorBatchConfig _loadedConfig = new BendFactorBatchConfig();

    public BendFactorSettingsWindow()
        : this(DefaultConfigRelativePath)
    {
    }

    public BendFactorSettingsWindow(string configRelativePath)
    {
        _configRelativePath = string.IsNullOrWhiteSpace(configRelativePath) ? DefaultConfigRelativePath : configRelativePath;
        ConditionRows = new ObservableCollection<ConditionTableRowViewModel>();
        CoefficientRows = new ObservableCollection<CoefficientTableRowModel>();
        InitializeComponent();
        DataContext = this;
        _configPath = ResolveConfigPath();
        ConfigPathTextBlock.Text = _configPath;
        LoadCurrentConfig();
    }

    public ObservableCollection<ConditionTableRowViewModel> ConditionRows { get; }

    public ObservableCollection<CoefficientTableRowModel> CoefficientRows { get; }

    private void LoadCurrentConfig()
    {
        try
        {
            _loadedConfig = BendFactorBatchExchange.LoadConfig(_configPath) ?? new BendFactorBatchConfig();
            LoadConfigIntoUi(_loadedConfig);
            StatusTextBlock.Text = "已加载当前配置。";
        }
        catch (Exception ex)
        {
            _loadedConfig = new BendFactorBatchConfig();
            LoadConfigIntoUi(_loadedConfig);
            StatusTextBlock.Text = "加载配置失败，已使用默认模板。";
            MessageBox.Show(this, ex.ToString(), "加载折弯系数设置失败", MessageBoxButton.OK, MessageBoxImage.Warning);
        }
    }

    private void LoadConfigIntoUi(BendFactorBatchConfig config)
    {
        config ??= new BendFactorBatchConfig();
        AutoConvertToSheetmetalCheckBox.IsChecked = config.AutoConvertToSheetmetal;
        SelectComboItem(BaseFaceDirectionPreferenceComboBox, string.IsNullOrWhiteSpace(config.BaseFaceDirectionPreference) ? "PreferUpBends" : config.BaseFaceDirectionPreference);
        FixedFaceColorTextBox.Text = string.IsNullOrWhiteSpace(config.FixedFaceColor) ? "#FFD400" : config.FixedFaceColor;
        MaxDeductionDeviationTextBox.Text = config.MaxDeductionDeviationMm.ToString("0.###", CultureInfo.InvariantCulture);
        CoefficientTablePathTextBox.Text = config.ImportExportCoefficientTablePath ?? string.Empty;
        LoadLargeArcSettings(config.LargeArcRuleSettings ?? new LargeArcRuleSettingsRecord());
        SkipSmallBodyCheckBox.IsChecked = config.SkipSmallBodyByWidthHeight;
        MinBodyLengthTextBox.Text = config.MinBodyLengthForProcessing.ToString("0.###", CultureInfo.InvariantCulture);
        MinBodyWidthTextBox.Text = config.MinBodyWidthForProcessing.ToString("0.###", CultureInfo.InvariantCulture);
        MaxBodyLengthTextBox.Text = ResolveMaxBodyLengthThreshold(config).ToString("0.###", CultureInfo.InvariantCulture);
        LoadConditionRows(config);
        LoadCoefficientRows(config);
    }

    private void LoadLargeArcSettings(LargeArcRuleSettingsRecord settings)
    {
        UseAbsoluteRadiusThresholdCheckBox.IsChecked = settings.UseAbsoluteRadiusThreshold;
        AbsoluteRadiusThresholdTextBox.Text = settings.AbsoluteRadiusThreshold.ToString("0.###", CultureInfo.InvariantCulture);
        UseRadiusThicknessRatioCheckBox.IsChecked = settings.UseRadiusThicknessRatio;
        RadiusThicknessRatioThresholdTextBox.Text = settings.RadiusThicknessRatioThreshold.ToString("0.###", CultureInfo.InvariantCulture);
        RightAngleRadiusThresholdTextBox.Text = settings.RightAngleRadiusThreshold.ToString("0.###", CultureInfo.InvariantCulture);
        RightAngleFallbackKFactorTextBox.Text = settings.RightAngleFallbackKFactor.ToString("0.###", CultureInfo.InvariantCulture);
        AcuteAngleRadiusThresholdTextBox.Text = settings.AcuteAngleRadiusThreshold.ToString("0.###", CultureInfo.InvariantCulture);
        AcuteAngleFallbackKFactorTextBox.Text = settings.AcuteAngleFallbackKFactor.ToString("0.###", CultureInfo.InvariantCulture);
    }

    private void LoadConditionRows(BendFactorBatchConfig config)
    {
        ConditionRows.Clear();
        IEnumerable<ConditionTableRowRecord> rows = config.ConditionRows != null && config.ConditionRows.Count > 0
            ? config.ConditionRows
            : BendFactorRuleTables.CreateDefaultConditionRows().Select(item => item.ToRecord());

        foreach (ConditionTableRowRecord row in rows)
        {
            ConditionRows.Add(ConditionTableRowViewModel.FromRecord(row));
        }
    }

    private void LoadCoefficientRows(BendFactorBatchConfig config)
    {
        CoefficientRows.Clear();
        IReadOnlyList<CoefficientTableRowModel> rows = ResolveCoefficientRows(config);
        foreach (CoefficientTableRowModel row in rows)
        {
            CoefficientRows.Add(row);
        }
    }

    private static IReadOnlyList<CoefficientTableRowModel> ResolveCoefficientRows(BendFactorBatchConfig config)
    {
        if (config.CoefficientRows != null && config.CoefficientRows.Count > 0)
        {
            return config.CoefficientRows.Select(CoefficientTableRowModel.FromRecord).ToList();
        }

        if (!string.IsNullOrWhiteSpace(config.ImportExportCoefficientTablePath) &&
            File.Exists(config.ImportExportCoefficientTablePath))
        {
            try
            {
                return CoefficientTableInterop.LoadRows(config.ImportExportCoefficientTablePath);
            }
            catch
            {
            }
        }

        return BendFactorRuleTables.CreateDefaultCoefficientRows();
    }

    private BendFactorBatchConfig BuildConfigFromUi()
    {
        BendFactorBatchConfig config = CloneRuntimeFields(_loadedConfig);
        config.AutoConvertToSheetmetal = AutoConvertToSheetmetalCheckBox.IsChecked == true;
        config.BaseFaceDirectionPreference = GetSelectedTag(BaseFaceDirectionPreferenceComboBox, "PreferUpBends");
        config.FixedFaceColor = string.IsNullOrWhiteSpace(FixedFaceColorTextBox.Text) ? "#FFD400" : FixedFaceColorTextBox.Text.Trim();
        config.MaxDeductionDeviationMm = ParseDoubleOrDefault(MaxDeductionDeviationTextBox.Text, 0.02);
        config.ImportExportCoefficientTablePath = CoefficientTablePathTextBox.Text.Trim();
        config.LargeArcRuleSettings = BuildLargeArcSettings();
        config.SkipSmallBodyByWidthHeight = SkipSmallBodyCheckBox.IsChecked == true;
        config.MinBodyLengthForProcessing = ParseDoubleOrDefault(MinBodyLengthTextBox.Text, 0.0);
        config.MinBodyWidthForProcessing = ParseDoubleOrDefault(MinBodyWidthTextBox.Text, 0.0);
        config.MaxBodyLengthForSmallBodyFiltering = ParseDoubleOrDefault(MaxBodyLengthTextBox.Text, 40.0);
        config.ConditionRows = ConditionRows.Select(item => item.ToRecord()).ToList();
        config.CoefficientRows = CoefficientRows.Where(item => item.Thickness > 0.0).Select(item => item.ToRecord()).ToList();
        config.Rules = BendFactorRuleTables.BuildRules(ConditionRows, CoefficientRows);
        return config;
    }

    private LargeArcRuleSettingsRecord BuildLargeArcSettings()
    {
        return new LargeArcRuleSettingsRecord
        {
            UseAbsoluteRadiusThreshold = UseAbsoluteRadiusThresholdCheckBox.IsChecked == true,
            AbsoluteRadiusThreshold = ParseDoubleOrDefault(AbsoluteRadiusThresholdTextBox.Text, 7.0),
            UseRadiusThicknessRatio = UseRadiusThicknessRatioCheckBox.IsChecked == true,
            RadiusThicknessRatioThreshold = ParseDoubleOrDefault(RadiusThicknessRatioThresholdTextBox.Text, 5.0),
            RightAngleRadiusThreshold = ParseDoubleOrDefault(RightAngleRadiusThresholdTextBox.Text, 5.0),
            RightAngleFallbackKFactor = ParseDoubleOrDefault(RightAngleFallbackKFactorTextBox.Text, 0.5),
            AcuteAngleRadiusThreshold = ParseDoubleOrDefault(AcuteAngleRadiusThresholdTextBox.Text, 5.0),
            AcuteAngleFallbackKFactor = ParseDoubleOrDefault(AcuteAngleFallbackKFactorTextBox.Text, 0.5)
        };
    }

    private static BendFactorBatchConfig CloneRuntimeFields(BendFactorBatchConfig source)
    {
        source ??= new BendFactorBatchConfig();
        return new BendFactorBatchConfig
        {
            Mode = string.IsNullOrWhiteSpace(source.Mode) ? "current" : source.Mode,
            FolderPath = source.FolderPath ?? string.Empty,
            Recursive = source.Recursive,
            IncludeAssemblyChildrenWhenCurrent = source.IncludeAssemblyChildrenWhenCurrent,
            MarkerLineFaceUp = source.MarkerLineFaceUp,
            FilterBodiesByLayerRange = source.FilterBodiesByLayerRange,
            StartBodyLayer = source.StartBodyLayer <= 0 ? 1 : source.StartBodyLayer,
            EndBodyLayer = source.EndBodyLayer <= 0 ? 99 : source.EndBodyLayer,
            ProcessLargestBodyOnlyPerLayer = source.ProcessLargestBodyOnlyPerLayer
        };
    }

    private bool ValidateConfig(BendFactorBatchConfig config)
    {
        if (config.CoefficientRows == null || config.CoefficientRows.Count == 0)
        {
            StatusTextBlock.Text = "系数表为空，请先导入或恢复模板。";
            return false;
        }

        if (config.Rules == null || config.Rules.Count == 0)
        {
            StatusTextBlock.Text = "当前条件表和系数表没有生成可用规则。";
            return false;
        }

        if (config.MaxDeductionDeviationMm < 0.0)
        {
            StatusTextBlock.Text = "扣除偏差上限不能小于 0。";
            return false;
        }

        if (config.SkipSmallBodyByWidthHeight &&
            (config.MinBodyLengthForProcessing <= 0.0 || config.MinBodyWidthForProcessing <= 0.0 || config.MaxBodyLengthForSmallBodyFiltering <= 0.0))
        {
            StatusTextBlock.Text = "启用最小实体过滤时，长度、宽度和最大方向阈值都必须大于 0。";
            return false;
        }

        if (!TryParseHexColor(config.FixedFaceColor))
        {
                StatusTextBlock.Text = "展开基面颜色无效，请使用 #RRGGBB。";
            return false;
        }

        LargeArcRuleSettingsRecord settings = config.LargeArcRuleSettings ?? new LargeArcRuleSettingsRecord();
        if (settings.UseAbsoluteRadiusThreshold && settings.AbsoluteRadiusThreshold <= 0.0)
        {
            StatusTextBlock.Text = "大圆弧绝对内R阈值必须大于 0。";
            return false;
        }

        if (settings.UseRadiusThicknessRatio && settings.RadiusThicknessRatioThreshold <= 0.0)
        {
            StatusTextBlock.Text = "大圆弧 R/T 阈值必须大于 0。";
            return false;
        }

        return true;
    }

    private void ImportCoefficientTableButton_Click(object sender, RoutedEventArgs e)
    {
        string selectedPath = CoefficientTableInterop.PickImportPath(CoefficientTablePathTextBox.Text.Trim());
        if (string.IsNullOrWhiteSpace(selectedPath))
        {
            return;
        }

        try
        {
            IReadOnlyList<CoefficientTableRowModel> rows = CoefficientTableInterop.LoadRows(selectedPath);
            CoefficientRows.Clear();
            foreach (CoefficientTableRowModel row in rows)
            {
                CoefficientRows.Add(row);
            }

            CoefficientTablePathTextBox.Text = selectedPath;
            StatusTextBlock.Text = $"已导入系数表，共 {rows.Count} 行。";
        }
        catch (Exception ex)
        {
            StatusTextBlock.Text = "导入系数表失败: " + ex.Message;
            MessageBox.Show(this, ex.ToString(), "导入系数表失败", MessageBoxButton.OK, MessageBoxImage.Error);
        }
    }

    private void ExportCoefficientTableButton_Click(object sender, RoutedEventArgs e)
    {
        string selectedPath = CoefficientTableInterop.PickExportPath(CoefficientTablePathTextBox.Text.Trim());
        if (string.IsNullOrWhiteSpace(selectedPath))
        {
            return;
        }

        try
        {
            CoefficientTableInterop.ExportRows(selectedPath, CoefficientRows.ToList());
            CoefficientTablePathTextBox.Text = selectedPath;
            StatusTextBlock.Text = $"已导出系数表，共 {CoefficientRows.Count} 行。";
        }
        catch (Exception ex)
        {
            StatusTextBlock.Text = "导出系数表失败: " + ex.Message;
            MessageBox.Show(this, ex.ToString(), "导出系数表失败", MessageBoxButton.OK, MessageBoxImage.Error);
        }
    }

    private void RestoreTemplateButton_Click(object sender, RoutedEventArgs e)
    {
        BendFactorBatchConfig template = CloneRuntimeFields(_loadedConfig);
        template.AutoConvertToSheetmetal = true;
        template.MaxBodyLengthForSmallBodyFiltering = 40.0;
        template.BaseFaceDirectionPreference = "PreferUpBends";
        template.FixedFaceColor = "#FFD400";
        template.MaxDeductionDeviationMm = 0.02;
        template.LargeArcRuleSettings = new LargeArcRuleSettingsRecord();
        template.ConditionRows = BendFactorRuleTables.CreateDefaultConditionRows().Select(item => item.ToRecord()).ToList();
        template.CoefficientRows = BendFactorRuleTables.CreateDefaultCoefficientRows().Select(item => item.ToRecord()).ToList();
        template.Rules = BendFactorRuleTables.BuildRules(
            template.ConditionRows.Select(ConditionTableRowViewModel.FromRecord),
            template.CoefficientRows.Select(CoefficientTableRowModel.FromRecord));
        LoadConfigIntoUi(template);
        StatusTextBlock.Text = "已恢复折弯系数默认模板，尚未保存。";
    }

    private void SaveButton_Click(object sender, RoutedEventArgs e)
    {
        try
        {
            BendFactorBatchConfig config = BuildConfigFromUi();
            if (!ValidateConfig(config))
            {
                return;
            }

            BendFactorBatchExchange.SaveConfig(config, _configPath);
            _loadedConfig = config;
            StatusTextBlock.Text = $"已保存，生成规则 {config.Rules.Count} 条。";
        }
        catch (Exception ex)
        {
            StatusTextBlock.Text = "保存失败: " + ex.Message;
            MessageBox.Show(this, ex.ToString(), "保存折弯系数设置失败", MessageBoxButton.OK, MessageBoxImage.Error);
        }
    }

    private void CloseButton_Click(object sender, RoutedEventArgs e)
    {
        Close();
    }

    private string ResolveConfigPath()
    {
        string? explicitRoot = Environment.GetEnvironmentVariable("ZHAOFU_DEPLOY_ROOT");
        if (!string.IsNullOrWhiteSpace(explicitRoot))
        {
            return Path.Combine(explicitRoot, _configRelativePath);
        }

        if (Directory.Exists(WorkspaceDeployRoot))
        {
            return Path.Combine(WorkspaceDeployRoot, _configRelativePath);
        }

        DirectoryInfo current = new DirectoryInfo(AppContext.BaseDirectory);
        DirectoryInfo? applicationDirectory = current.Parent;
        if (applicationDirectory != null &&
            string.Equals(applicationDirectory.Name, "application", StringComparison.OrdinalIgnoreCase))
        {
            DirectoryInfo? deployRoot = applicationDirectory.Parent;
            if (deployRoot != null)
            {
                return Path.Combine(deployRoot.FullName, _configRelativePath);
            }
        }

        return Path.Combine(current.FullName, _configRelativePath);
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

    private static double ParseDoubleOrDefault(string text, double fallback)
    {
        return double.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out double value) ? value : fallback;
    }

    private static double ResolveMaxBodyLengthThreshold(BendFactorBatchConfig config)
    {
        if (config == null || config.MaxBodyLengthForSmallBodyFiltering <= 0.0)
        {
            return 40.0;
        }

        return config.MaxBodyLengthForSmallBodyFiltering;
    }

    private static bool TryParseHexColor(string text)
    {
        if (string.IsNullOrWhiteSpace(text))
        {
            return false;
        }

        string value = text.Trim();
        if (value.StartsWith("#", StringComparison.Ordinal))
        {
            value = value[1..];
        }

        return value.Length == 6 &&
               int.TryParse(value[..2], NumberStyles.HexNumber, CultureInfo.InvariantCulture, out _) &&
               int.TryParse(value.Substring(2, 2), NumberStyles.HexNumber, CultureInfo.InvariantCulture, out _) &&
               int.TryParse(value.Substring(4, 2), NumberStyles.HexNumber, CultureInfo.InvariantCulture, out _);
    }
}
