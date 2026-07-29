using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Threading;
using System.IO;
using System.Text;
using Microsoft.Win32;

namespace AutoCreateThreeViewsUI;

public partial class MainWindow : Window
{
    private readonly object? topViewContent;
    private readonly object? bottomViewContent;
    private readonly object? leftPositionViewContent;
    private readonly object? rightPositionViewContent;
    private AssemblyNode? selectedAssemblyNode;
    private AssemblyNode? shiftAnchorAssemblyNode;
    private readonly Dictionary<string, Dictionary<string, string>> partOptionsByOccurrence = new(StringComparer.OrdinalIgnoreCase);
    private readonly Dictionary<MenuItem, BatchTargetFilterRule> assemblyFilterRulesByMenuItem = new();
    private bool isUpdatingAssemblyChecks;
    private bool isLoadingPartOptions;
    private bool assemblySelectionAvailable = true;
    private static readonly string DefaultTechnicalRequirementsPath = Path.Combine(AppContext.BaseDirectory, "Assets", "technical_requirements.default.cfg");
    private static readonly string UserTechnicalRequirementsPath = Path.Combine(GetConfigDirectory(), "technical_requirements.user.cfg");
    private bool isUpdatingPreview;
    private readonly DispatcherTimer progressTimer = new() { Interval = TimeSpan.FromMilliseconds(500) };
    private Window? progressWindow;
    private TextBlock? progressWindowTextBlock;
    private ProgressBar? progressWindowBar;

    static MainWindow()
    {
        Encoding.RegisterProvider(CodePagesEncodingProvider.Instance);
    }

    public MainWindow()
    {
        InitializeComponent();
        LoadViewButtonImages();
        NormalizeTemplatePaths();

        topViewContent = TopViewButton.Content;
        bottomViewContent = BottomViewButton.Content;
        leftPositionViewContent = LeftPositionViewButton.Content;
        rightPositionViewContent = RightPositionViewButton.Content;

        LoadTechnicalRequirements();
        LoadAssemblyTree();
        InitializeAssemblyFilterMenu();
        LoadSavedOptions();

        progressTimer.Tick += ProgressTimer_Tick;
    }

    private void LoadViewButtonImages()
    {
        LoadImage(TopViewButtonImage, "top.png");
        LoadImage(LeftPositionViewButtonImage, "left.png");
        LoadImage(FrontViewButtonImage, "front.png");
        LoadImage(RightPositionViewButtonImage, "right.png");
        LoadImage(BackViewButtonImage, "back.png");
        LoadImage(BottomViewButtonImage, "bottom.png");
        LoadImage(BackBottomViewButtonImage, "backbottom.png");
        LoadImage(IsoAuxViewImage, "iso.png");
        LoadImage(FlatAuxViewImage, "flat.png");

        LoadImage(PreviewTopViewImage, "top.png");
        LoadImage(PreviewLeftPositionViewImage, "left.png");
        LoadImage(PreviewFrontViewImage, "front.png");
        LoadImage(PreviewRightPositionViewImage, "right.png");
        LoadImage(PreviewBackViewImage, "back.png");
        LoadImage(PreviewBackBottomViewImage, "backbottom.png");
        LoadImage(PreviewBottomViewImage, "bottom.png");
        LoadImage(PreviewIsoViewImage, "iso.png");
        LoadImage(PreviewFlatViewImage, "flat.png");
    }

    private static void LoadImage(Image image, string fileName)
    {
        string path = Path.Combine(AppContext.BaseDirectory, "Assets", "ViewButtons", fileName);
        if (!File.Exists(path))
        {
            return;
        }

        BitmapImage bitmap = new();
        bitmap.BeginInit();
        bitmap.CacheOption = BitmapCacheOption.OnLoad;
        bitmap.UriSource = new Uri(path, UriKind.Absolute);
        bitmap.EndInit();
        bitmap.Freeze();
        image.Source = bitmap;
    }

    private void CloseButton_Click(object sender, RoutedEventArgs e)
    {
        Close();
    }

    private void ManageTemplateButton_Click(object sender, RoutedEventArgs e)
    {
        OpenFileDialog dialog = new()
        {
            Title = "\u9009\u62e9\u56fe\u7eb8\u6a21\u677f\u6587\u4ef6",
            Filter = "NX Part Template (*.prt)|*.prt|All files (*.*)|*.*",
            CheckFileExists = true,
            Multiselect = false
        };

        string defaultDirectory = GetDataDirectory();
        if (Directory.Exists(defaultDirectory))
        {
            dialog.InitialDirectory = defaultDirectory;
        }

        if (dialog.ShowDialog(this) != true)
        {
            return;
        }

        string selectedPath = dialog.FileName;
        ComboBoxItem? existingItem = null;
        foreach (object item in TemplateComboBox.Items)
        {
            if (item is ComboBoxItem comboBoxItem &&
                comboBoxItem.Tag is string tag &&
                string.Equals(tag, selectedPath, StringComparison.OrdinalIgnoreCase))
            {
                existingItem = comboBoxItem;
                break;
            }
        }

        if (existingItem == null)
        {
            existingItem = new ComboBoxItem
            {
                Content = Path.GetFileName(selectedPath),
                Tag = selectedPath
            };
            TemplateComboBox.Items.Add(existingItem);
        }

        TemplateComboBox.SelectedItem = existingItem;
    }

    private void NormalizeTemplatePaths()
    {
        string dataDirectory = GetDataDirectory();
        foreach (object item in TemplateComboBox.Items)
        {
            if (item is not ComboBoxItem comboBoxItem ||
                comboBoxItem.Content is not string fileName ||
                !fileName.EndsWith(".prt", StringComparison.OrdinalIgnoreCase))
            {
                continue;
            }

            comboBoxItem.Tag = Path.Combine(dataDirectory, fileName);
        }
    }

    private static string GetDataDirectory()
    {
        DirectoryInfo? baseDirectory = new(AppContext.BaseDirectory);
        DirectoryInfo? pluginRoot = baseDirectory.Parent?.Parent;
        return Path.Combine(pluginRoot?.FullName ?? AppContext.BaseDirectory, "DATA");
    }

    private void GenerateButton_Click(object sender, RoutedEventArgs e)
    {
        bool progressStarted = false;
        try
        {
            bool delayProgressWindow = RequestUsesManualFrontDirection();
            if (ShouldShowProgressForCurrentRequest())
            {
                StartProgressMonitor(delayProgressWindow);
                progressStarted = true;
            }
            WriteCreateDrawingRequest();
            Hide();
        }
        catch (Exception ex)
        {
            if (progressStarted)
            {
                StopProgressMonitor("Failed.");
            }
            else
            {
                GenerateButton.IsEnabled = true;
            }
            MessageBox.Show(ex.Message, "\u81ea\u52a8\u521b\u5efa\u4e09\u89c6\u56fe", MessageBoxButton.OK, MessageBoxImage.Error);
        }
    }

    private bool RequestUsesManualFrontDirection()
    {
        Dictionary<string, string> sharedSettings = CaptureCurrentDrawingSettings();
        List<AssemblyNode> checkedNodes = GetCheckedAssemblyNodes();
        if (!assemblySelectionAvailable)
        {
            return IsManualFrontDirectionMode(ReadText(sharedSettings, "frontDirectionMode", ""));
        }
        if (checkedNodes.Count == 0 && selectedAssemblyNode != null)
        {
            checkedNodes.Add(selectedAssemblyNode);
        }
        if (checkedNodes.Count == 0)
        {
            return IsManualFrontDirectionMode(ReadText(sharedSettings, "frontDirectionMode", ""));
        }

        foreach (AssemblyNode node in checkedNodes)
        {
            Dictionary<string, string> partSettings = sharedSettings;
            if (ShareAllPartsOptionsCheckBox.IsChecked != true &&
                partOptionsByOccurrence.TryGetValue(node.OccurrenceTag, out Dictionary<string, string>? savedPartSettings))
            {
                partSettings = savedPartSettings;
            }
            if (IsManualFrontDirectionMode(ReadText(partSettings, "frontDirectionMode", "")))
            {
                return true;
            }
        }

        return false;
    }

    private static bool IsManualFrontDirectionMode(string value)
    {
        string mode = (value ?? "").Trim();
        return mode.Equals("manualFaceX", StringComparison.OrdinalIgnoreCase) ||
               mode.Equals("manual", StringComparison.OrdinalIgnoreCase) ||
               mode.Equals("selectedFaceX", StringComparison.OrdinalIgnoreCase);
    }

    private bool ShouldShowProgressForCurrentRequest()
    {
        if (!assemblySelectionAvailable)
        {
            return false;
        }

        List<AssemblyNode> checkedNodes = GetCheckedAssemblyNodes();
        if (checkedNodes.Count == 0 && selectedAssemblyNode != null)
        {
            checkedNodes.Add(selectedAssemblyNode);
        }

        if (checkedNodes.Count > 1)
        {
            return true;
        }

        if (checkedNodes.Count != 1)
        {
            return false;
        }

        AssemblyNode node = checkedNodes[0];
        return node.HasChildren;
    }

    private void StartProgressMonitor(bool delayProgressWindow = false)
    {
        string progressPath = GetProgressPath();
        try
        {
            if (File.Exists(progressPath))
            {
                File.Delete(progressPath);
            }
        }
        catch
        {
        }

        if (!delayProgressWindow)
        {
            ShowProgressWindow();
            SetProgressDisplay("Preparing drawing...", 0, 0, true);
        }
        GenerateButton.IsEnabled = false;
        progressTimer.Start();
    }

    private void StopProgressMonitor(string message)
    {
        progressTimer.Stop();
        SetProgressDisplay(message, 1, 1, false);
        GenerateButton.IsEnabled = true;
    }

    private void ShowProgressWindow()
    {
        if (progressWindow != null)
        {
            progressWindow.Show();
            progressWindow.Activate();
            return;
        }

        progressWindowTextBlock = new TextBlock
        {
            Text = "Preparing drawing...",
            Foreground = new SolidColorBrush(Color.FromRgb(64, 80, 96)),
            Margin = new Thickness(0, 0, 0, 12),
            TextTrimming = TextTrimming.CharacterEllipsis
        };
        progressWindowBar = new ProgressBar
        {
            Height = 18,
            Minimum = 0,
            Maximum = 100,
            Value = 0,
            IsIndeterminate = true
        };
        StackPanel panel = new()
        {
            Margin = new Thickness(22)
        };
        panel.Children.Add(new TextBlock
        {
            Text = "\u751f\u6210\u56fe\u7eb8",
            FontSize = 16,
            FontWeight = FontWeights.SemiBold,
            Foreground = new SolidColorBrush(Color.FromRgb(29, 45, 58)),
            Margin = new Thickness(0, 0, 0, 12)
        });
        panel.Children.Add(progressWindowTextBlock);
        panel.Children.Add(progressWindowBar);

        progressWindow = new Window
        {
            Title = "\u751f\u6210\u56fe\u7eb8",
            Width = 440,
            Height = 160,
            ResizeMode = ResizeMode.NoResize,
            WindowStartupLocation = WindowStartupLocation.CenterScreen,
            Topmost = true,
            Content = panel
        };
        progressWindow.Show();
    }

    private void SetProgressDisplay(string message, int current, int total, bool indeterminate)
    {
        if (progressWindowTextBlock != null)
        {
            progressWindowTextBlock.Text = total > 0 ? $"{message} ({current}/{total})" : message;
        }
        if (progressWindowBar != null)
        {
            progressWindowBar.IsIndeterminate = indeterminate;
            if (total > 0)
            {
                progressWindowBar.Maximum = total;
                progressWindowBar.Value = Math.Max(0, Math.Min(current, total));
            }
            else
            {
                progressWindowBar.Value = 0;
            }
        }
    }

    private void ProgressTimer_Tick(object? sender, EventArgs e)
    {
        string progressPath = GetProgressPath();
        if (!File.Exists(progressPath))
        {
            return;
        }

        try
        {
            Dictionary<string, string> values = ReadKeyValueFile(progressPath);
            int current = ParseInt(values.TryGetValue("current", out string? currentText) ? currentText : "", 0);
            int total = ParseInt(values.TryGetValue("total", out string? totalText) ? totalText : "", 0);
            string message = values.TryGetValue("message", out string? messageText) ? messageText : "Drawing...";
            bool done = values.TryGetValue("done", out string? doneText) && IsTruthy(doneText);

            if (progressWindow == null && total > 0 && !done)
            {
                ShowProgressWindow();
            }

            if (total > 0)
            {
                SetProgressDisplay(message, current, total, false);
            }
            else
            {
                SetProgressDisplay(message, current, total, true);
            }

            if (done)
            {
                StopProgressMonitor("Drawing finished.");
                progressWindow?.Close();
                Close();
            }
        }
        catch
        {
        }
    }

    private void ApplyPartSettingsButton_Click(object sender, RoutedEventArgs e)
    {
        SaveCurrentPartOptions(showMessage: true);
    }

    private void SelectAllPartsCheckBox_Changed(object sender, RoutedEventArgs e)
    {
        if (!IsLoaded || isUpdatingAssemblyChecks)
        {
            return;
        }

        SetAllAssemblyNodeChecks(SelectAllPartsCheckBox.IsChecked == true);
        UpdateSelectAllCheckBoxState();
        UpdateSelectedAssemblyText();
    }

    private void ShareAllPartsOptionsCheckBox_Changed(object sender, RoutedEventArgs e)
    {
        if (!IsLoaded)
        {
            return;
        }

        if (ShareAllPartsOptionsCheckBox.IsChecked != true)
        {
            LoadOptionsForSelectedPart();
        }

        UpdateSelectedAssemblyText();
    }

    private void ZoomInButton_Click(object sender, RoutedEventArgs e)
    {
        SendDisplayCommand("zoom", "1.25");
    }

    private void ZoomOutButton_Click(object sender, RoutedEventArgs e)
    {
        SendDisplayCommand("zoom", "0.8");
    }

    private void ZoomFitButton_Click(object sender, RoutedEventArgs e)
    {
        SendDisplayCommand("fit", "");
    }

    private void MainTabControl_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        if (!ReferenceEquals(e.OriginalSource, MainTabControl))
        {
            return;
        }

        ApplySelectedTabPanel();
    }

    private void ProjectionMode_Checked(object sender, RoutedEventArgs e)
    {
        if (!IsLoaded)
        {
            return;
        }

        ApplyProjectionMode();
    }

    private void PreviewOption_Changed(object sender, RoutedEventArgs e)
    {
        if (!IsLoaded || isUpdatingPreview)
        {
            return;
        }

        isUpdatingPreview = true;
        try
        {
            if (sender is ToggleButton button && button.IsChecked == true)
            {
                if (button.Name.StartsWith("Iso", StringComparison.Ordinal))
                {
                    UncheckOtherButtons(button, IsoTopLeftButton, IsoTopRightButton, IsoBottomLeftButton, IsoBottomRightButton);
                }
                else if (button.Name.StartsWith("Flat", StringComparison.Ordinal))
                {
                    UncheckOtherButtons(button, FlatTopLeftButton, FlatTopRightButton, FlatBottomLeftButton, FlatBottomRightButton);
                }

                PreventSameAuxiliaryCorner(button);
            }

            UpdatePreviewFromOptions();
        }
        finally
        {
            isUpdatingPreview = false;
        }
    }

    private void TechnicalRequirementCorner_Changed(object sender, RoutedEventArgs e)
    {
        if (!IsLoaded || isUpdatingPreview)
        {
            return;
        }

        isUpdatingPreview = true;
        try
        {
            if (sender is ToggleButton button && button.IsChecked == true)
            {
                UncheckOtherButtons(
                    button,
                    TechnicalTopLeftButton,
                    TechnicalTopRightButton,
                    TechnicalBottomLeftButton,
                    TechnicalBottomRightButton);
            }

            if (!IsAnyChecked(
                    TechnicalTopLeftButton,
                    TechnicalTopRightButton,
                    TechnicalBottomLeftButton,
                    TechnicalBottomRightButton))
            {
                TechnicalTopLeftButton.IsChecked = true;
            }

            UpdatePreviewFromOptions();
        }
        finally
        {
            isUpdatingPreview = false;
        }
    }

    private void TechnicalRequirementOption_Changed(object sender, RoutedEventArgs e)
    {
        if (!IsLoaded || isUpdatingPreview)
        {
            return;
        }

        UpdatePreviewFromOptions();
    }

    private void AssemblyTree_SelectedItemChanged(object sender, RoutedPropertyChangedEventArgs<object> e)
    {
        selectedAssemblyNode = (e.NewValue as TreeViewItem)?.Tag as AssemblyNode;
        if (selectedAssemblyNode != null && !isUpdatingAssemblyChecks)
        {
            if (Keyboard.Modifiers.HasFlag(ModifierKeys.Shift) &&
                shiftAnchorAssemblyNode != null &&
                !ReferenceEquals(shiftAnchorAssemblyNode, selectedAssemblyNode))
            {
                SetAssemblyRangeChecked(shiftAnchorAssemblyNode, selectedAssemblyNode, true);
                UpdateSelectAllCheckBoxState();
            }
            else
            {
                shiftAnchorAssemblyNode = selectedAssemblyNode;
            }
        }

        LoadOptionsForSelectedPart();
        UpdateSelectedAssemblyText();
        if (IsLoaded && selectedAssemblyNode != null)
        {
            SendDisplayCommand("display", "");
        }
    }

    private void AssemblyTree_MouseDoubleClick(object sender, MouseButtonEventArgs e)
    {
        if (FindVisualParent<TreeViewItem>(e.OriginalSource as DependencyObject) is TreeViewItem item &&
            item.Tag is AssemblyNode node)
        {
            selectedAssemblyNode = node;
            item.IsSelected = true;
        }

        if (selectedAssemblyNode == null || string.IsNullOrWhiteSpace(selectedAssemblyNode.OccurrenceTag))
        {
            return;
        }

        SendDisplayCommand("display", "");
    }

    private void AssemblyNodeCheckBox_Changed(object sender, RoutedEventArgs e)
    {
        if (sender is not CheckBox checkBox || checkBox.Tag is not AssemblyNode node)
        {
            return;
        }

        node.IsChecked = checkBox.IsChecked == true;
        if (!isUpdatingAssemblyChecks && node.TreeItem != null)
        {
            selectedAssemblyNode = node;
            shiftAnchorAssemblyNode = node;
            node.TreeItem.IsSelected = true;
            UpdateSelectAllCheckBoxState();
            UpdateSelectedAssemblyText();
        }
    }

    private void AssemblyNodeCheckBox_PreviewMouseLeftButtonDown(object sender, MouseButtonEventArgs e)
    {
        if (sender is not CheckBox checkBox ||
            checkBox.Tag is not AssemblyNode node ||
            !Keyboard.Modifiers.HasFlag(ModifierKeys.Shift) ||
            shiftAnchorAssemblyNode == null ||
            ReferenceEquals(shiftAnchorAssemblyNode, node))
        {
            return;
        }

        bool targetChecked = checkBox.IsChecked != true;
        SetAssemblyRangeChecked(shiftAnchorAssemblyNode, node, targetChecked);
        selectedAssemblyNode = node;
        if (node.TreeItem != null)
        {
            isUpdatingAssemblyChecks = true;
            try
            {
                node.TreeItem.IsSelected = true;
            }
            finally
            {
                isUpdatingAssemblyChecks = false;
            }
        }

        LoadOptionsForSelectedPart();
        UpdateSelectAllCheckBoxState();
        UpdateSelectedAssemblyText();
        e.Handled = true;
    }

    private void SendDisplayCommand(string action, string value)
    {
        if (selectedAssemblyNode == null || string.IsNullOrWhiteSpace(selectedAssemblyNode.OccurrenceTag))
        {
            return;
        }

        try
        {
            string displayCommandPath = GetDisplayCommandPath();
            Directory.CreateDirectory(Path.GetDirectoryName(displayCommandPath)!);
            StringBuilder builder = new();
            builder.Append("action=").AppendLine(action);
            builder.Append("occurrenceTag=").AppendLine(selectedAssemblyNode.OccurrenceTag);
            if (!string.IsNullOrWhiteSpace(value))
            {
                builder.Append("value=").AppendLine(value);
            }

            WriteAtomicText(displayCommandPath, builder.ToString());
        }
        catch
        {
            // NX side will simply keep the current display if the command file cannot be written.
        }
    }

    private void SendClearDisplayCommand()
    {
        try
        {
            string displayCommandPath = GetDisplayCommandPath();
            Directory.CreateDirectory(Path.GetDirectoryName(displayCommandPath)!);
            WriteAtomicText(displayCommandPath, "action=clear\n");
        }
        catch
        {
            // NX side also clears when it observes this process exiting.
        }
    }

    private static T? FindVisualParent<T>(DependencyObject? child)
        where T : DependencyObject
    {
        while (child != null)
        {
            if (child is T typed)
            {
                return typed;
            }

            child = VisualTreeHelper.GetParent(child);
        }

        return null;
    }

    protected override void OnClosed(EventArgs e)
    {
        SendClearDisplayCommand();
        SaveOptions();

        base.OnClosed(e);

        Application.Current.Shutdown();
        Dispatcher.CurrentDispatcher.BeginInvokeShutdown(DispatcherPriority.Background);
    }

    protected override void OnContentRendered(EventArgs e)
    {
        base.OnContentRendered(e);
        ApplySelectedTabPanel();
        ApplyProjectionMode();
        UpdatePreviewFromOptions();
    }

    private void ApplySelectedTabPanel()
    {
        if (MainTabControl == null ||
            PartSelectionPanel == null ||
            ViewSettingsPanel == null ||
            AnnotationSettingsPanel == null ||
            TechnicalRequirementsPanel == null)
        {
            return;
        }

        bool partSelected = ReferenceEquals(MainTabControl.SelectedItem, PartSelectionTab);
        bool viewSelected = ReferenceEquals(MainTabControl.SelectedItem, ViewTab);
        bool annotationSelected = ReferenceEquals(MainTabControl.SelectedItem, AnnotationTab);
        bool technicalSelected = ReferenceEquals(MainTabControl.SelectedItem, TechnicalRequirementsTab);
        PartSelectionPanel.Visibility = partSelected ? Visibility.Visible : Visibility.Collapsed;
        ViewSettingsPanel.Visibility = viewSelected ? Visibility.Visible : Visibility.Collapsed;
        AnnotationSettingsPanel.Visibility = annotationSelected ? Visibility.Visible : Visibility.Collapsed;
        TechnicalRequirementsPanel.Visibility = technicalSelected ? Visibility.Visible : Visibility.Collapsed;
    }

    private void ApplyProjectionMode()
    {
        TopViewButton.Content = null;
        BottomViewButton.Content = null;
        LeftPositionViewButton.Content = null;
        RightPositionViewButton.Content = null;

        if (FirstAngleRadio.IsChecked == true)
        {
            TopViewButton.Content = bottomViewContent;
            BottomViewButton.Content = topViewContent;
            LeftPositionViewButton.Content = rightPositionViewContent;
            RightPositionViewButton.Content = leftPositionViewContent;
            TopViewButton.ToolTip = "\u4ef0\u89c6\u56fe";
            BottomViewButton.ToolTip = "\u4fef\u89c6\u56fe";
            LeftPositionViewButton.ToolTip = "\u53f3\u89c6\u56fe";
            RightPositionViewButton.ToolTip = "\u5de6\u89c6\u56fe";
            ProjectionHintText.Text = "\u7b2c\u4e00\u89d2\u6cd5\u6807\u51c6\u5e03\u5c40\uff1a\u4f7f\u7528\u65b0\u6309\u94ae\u56fe\u505a\u4e0a\u4e0b\u3001\u5de6\u53f3\u5bf9\u5e94\u5207\u6362";
            UpdatePreviewFromOptions();
            return;
        }

        TopViewButton.Content = topViewContent;
        BottomViewButton.Content = bottomViewContent;
        LeftPositionViewButton.Content = leftPositionViewContent;
        RightPositionViewButton.Content = rightPositionViewContent;
        TopViewButton.ToolTip = "\u4e0a\u89c6\u56fe";
        BottomViewButton.ToolTip = "\u4e0b\u89c6\u56fe";
        LeftPositionViewButton.ToolTip = "\u5de6\u89c6\u56fe";
        RightPositionViewButton.ToolTip = "\u53f3\u89c6\u56fe";
        ProjectionHintText.Text = "\u7b2c\u4e09\u89d2\u6cd5\u6807\u51c6\u5e03\u5c40\uff1a\u6309\u793a\u610f\u56fe\u56fa\u5b9a\u663e\u793a\u4e0a\u3001\u4e0b\u3001\u5de6\u3001\u53f3\u3001\u540e\u3001\u4e0b\u540e\u89c6\u56fe";
        UpdatePreviewFromOptions();
    }

    private static void UncheckOtherButtons(ToggleButton selectedButton, params ToggleButton[] buttons)
    {
        foreach (ToggleButton button in buttons)
        {
            if (!ReferenceEquals(button, selectedButton))
            {
                button.IsChecked = false;
            }
        }
    }

    private void UpdatePreviewFromOptions()
    {
        bool isFirstAngle = FirstAngleRadio.IsChecked == true;

        PreviewProjectionText.Text = isFirstAngle ? "\u7b2c\u4e00\u89d2\u6cd5" : "\u7b2c\u4e09\u89d2\u6cd5";

        List<PreviewItem> items = BuildProjectionPreviewItems(isFirstAngle);
        bool showIso = IsAnyChecked(IsoTopLeftButton, IsoTopRightButton, IsoBottomLeftButton, IsoBottomRightButton);
        bool showFlat = IsAnyChecked(FlatTopLeftButton, FlatTopRightButton, FlatBottomLeftButton, FlatBottomRightButton);
        string isoCorner = GetCorner(IsoTopLeftButton, IsoTopRightButton, IsoBottomLeftButton, IsoBottomRightButton);
        string flatCorner = GetCorner(FlatTopLeftButton, FlatTopRightButton, FlatBottomLeftButton, FlatBottomRightButton);
        bool autoCompactAuxiliary = AuxAutoCompactCheckBox.IsChecked == true;
        if (autoCompactAuxiliary)
        {
            isoCorner = "TopRight";
            flatCorner = "BottomRight";
        }

        AddAuxiliaryPreviewItems(items, showIso, showFlat, isoCorner, flatCorner, autoCompactAuxiliary);
        CompactPreviewItems(items);
        UpdateTechnicalRequirementPreview();
    }

    private void UpdateTechnicalRequirementPreview()
    {
        if (PreviewTechnicalRequirement == null)
        {
            return;
        }

        if (TechnicalRequirementEnabledCheckBox.IsChecked != true)
        {
            PreviewTechnicalRequirement.Visibility = Visibility.Collapsed;
            return;
        }

        const double frameLeft = 20;
        const double frameTop = 20;
        const double frameRight = 600;
        const double frameBottom = 470;
        const double noteWidth = 128;
        const double noteHeight = 82;
        double margin = ReadPreviewDouble(SheetMarginTextBox, 20) * 0.5;
        margin = Math.Clamp(margin, 8, 24);
        string corner = GetTechnicalRequirementCorner();

        double left = corner is "TopRight" or "BottomRight"
            ? frameRight - margin - noteWidth
            : frameLeft + margin;
        double top = corner is "BottomLeft" or "BottomRight"
            ? frameBottom - margin - noteHeight
            : frameTop + margin;

        SetCanvasPosition(PreviewTechnicalRequirement, left, top);
        PreviewTechnicalRequirement.RenderTransform = Transform.Identity;
        PreviewTechnicalRequirement.Visibility = Visibility.Visible;
    }

    private List<PreviewItem> BuildProjectionPreviewItems(bool isFirstAngle)
    {
        const double frontWidth = 150;
        const double frontHeight = 72;
        double gap = ReadPreviewDouble(ViewSpacingTextBox, 20);

        FrameworkElement topSlot = isFirstAngle ? PreviewBottomView : PreviewTopView;
        FrameworkElement bottomSlot = isFirstAngle ? PreviewTopView : PreviewBottomView;
        FrameworkElement leftSlot = isFirstAngle ? PreviewRightPositionView : PreviewLeftPositionView;
        FrameworkElement rightSlot = isFirstAngle ? PreviewLeftPositionView : PreviewRightPositionView;

        HideAllPreviewViews();

        List<PreviewItem> items = new();

        if (FrontViewButton.IsChecked == true)
        {
            items.Add(new PreviewItem(PreviewFrontView, 0, 0, frontWidth, frontHeight));
        }

        if (TopViewButton.IsChecked == true)
        {
            items.Add(new PreviewItem(topSlot, 15, -62 - gap, 120, 62));
        }

        if (BottomViewButton.IsChecked == true)
        {
            items.Add(new PreviewItem(bottomSlot, 15, frontHeight + gap, 120, 70));
        }

        if (LeftPositionViewButton.IsChecked == true)
        {
            items.Add(new PreviewItem(leftSlot, -86 - gap, 0, 86, 72));
        }

        if (RightPositionViewButton.IsChecked == true)
        {
            items.Add(new PreviewItem(rightSlot, frontWidth + gap, 0, 88, 72));
        }

        if (BackViewButton.IsChecked == true)
        {
            double backX = frontWidth + gap;
            if (RightPositionViewButton.IsChecked == true)
            {
                backX += 88 + gap;
            }

            items.Add(new PreviewItem(PreviewBackView, backX, 0, 120, 72));
        }

        if (BackBottomViewButton.IsChecked == true)
        {
            double backX = 15;
            double backY = frontHeight + gap;
            if (BottomViewButton.IsChecked == true)
            {
                backY += 70 + gap;
            }

            items.Add(new PreviewItem(PreviewBackBottomView, backX, backY, 120, 72));
        }

        return items;
    }

    private void AddAuxiliaryPreviewItems(
        List<PreviewItem> items,
        bool showIso,
        bool showFlat,
        string isoCorner,
        string flatCorner,
        bool autoCompactAuxiliary)
    {
        double gap = ReadPreviewDouble(ViewSpacingTextBox, 20);
        Rect baseBounds = items.Count > 0 ? GetPreviewBounds(items) : new Rect(0, 0, 0, 0);
        Rect auxiliaryLayoutBounds = CreateAuxiliaryPreviewLayoutBounds(baseBounds, gap);
        Dictionary<string, int> cornerCounts = new();

        if (showIso)
        {
            items.Add(autoCompactAuxiliary
                ? CreateAuxiliaryPreviewItem(items, PreviewIsoView, auxiliaryLayoutBounds, isoCorner, 94, 66, gap, cornerCounts, true)
                : CreateManualAuxiliaryPreviewItem(PreviewIsoView, baseBounds, isoCorner, 94, 66, gap, cornerCounts));
        }

        if (showFlat)
        {
            items.Add(autoCompactAuxiliary
                ? CreateAuxiliaryPreviewItem(items, PreviewFlatView, auxiliaryLayoutBounds, flatCorner, 140, 70, gap, cornerCounts, true)
                : CreateManualAuxiliaryPreviewItem(PreviewFlatView, baseBounds, flatCorner, 140, 70, gap, cornerCounts));
        }
    }

    private static Rect CreateAuxiliaryPreviewLayoutBounds(Rect baseBounds, double gap)
    {
        double sideRoom = Math.Max(160, gap + 145);
        double verticalRoom = Math.Max(110, gap + 90);
        double left = baseBounds.Left - sideRoom;
        double top = baseBounds.Top - verticalRoom;
        double right = baseBounds.Right + sideRoom;
        double bottom = baseBounds.Bottom + verticalRoom;
        return new Rect(left, top, right - left, bottom - top);
    }

    private static PreviewItem CreateManualAuxiliaryPreviewItem(
        FrameworkElement element,
        Rect baseBounds,
        string corner,
        double width,
        double height,
        double gap,
        Dictionary<string, int> cornerCounts)
    {
        double compactGap = Math.Clamp(gap, 8, 20);
        cornerCounts.TryGetValue(corner, out int stackIndex);
        cornerCounts[corner] = stackIndex + 1;

        double left = corner is "TopLeft" or "BottomLeft"
            ? baseBounds.Left - width - compactGap
            : baseBounds.Right + compactGap;
        double top = corner is "TopLeft" or "TopRight"
            ? baseBounds.Top - height - compactGap - stackIndex * (height + compactGap)
            : baseBounds.Bottom + compactGap + stackIndex * (height + compactGap);

        return new PreviewItem(element, left, top, width, height);
    }

    private static PreviewItem CreateAuxiliaryPreviewItem(
        IReadOnlyList<PreviewItem> existingItems,
        FrameworkElement element,
        Rect layoutBounds,
        string corner,
        double width,
        double height,
        double gap,
        Dictionary<string, int> cornerCounts,
        bool allowReflow)
    {
        cornerCounts.TryGetValue(corner, out int stackIndex);
        cornerCounts[corner] = stackIndex + 1;

        double left = corner is "TopLeft" or "BottomLeft"
            ? layoutBounds.Left
            : layoutBounds.Right - width;
        double top = corner is "TopLeft" or "TopRight"
            ? layoutBounds.Top + stackIndex * (height + gap)
            : layoutBounds.Bottom - height - stackIndex * (height + gap);

        if (!allowReflow)
        {
            return new PreviewItem(element, left, top, width, height);
        }

        Rect candidate = new(left, top, width, height);
        bool stackDown = corner is "TopLeft" or "TopRight";
        int guard = 0;
        while (OverlapsAny(candidate, existingItems, gap) && guard++ < 20)
        {
            top += stackDown ? height + gap : -(height + gap);
            if (stackDown && top + height > layoutBounds.Bottom)
            {
                top = layoutBounds.Top;
                left -= width + gap;
            }
            else if (!stackDown && top < layoutBounds.Top)
            {
                top = layoutBounds.Bottom - height;
                left -= width + gap;
            }
            candidate = new Rect(left, top, width, height);
        }

        return new PreviewItem(element, left, top, width, height);
    }

    private static bool OverlapsAny(Rect candidate, IReadOnlyList<PreviewItem> items, double gap)
    {
        Rect padded = new(candidate.Left - gap, candidate.Top - gap, candidate.Width + gap * 2, candidate.Height + gap * 2);
        foreach (PreviewItem item in items)
        {
            Rect existing = new(item.Left, item.Top, item.Width, item.Height);
            if (padded.IntersectsWith(existing))
            {
                return true;
            }
        }

        return false;
    }

    private void CompactPreviewItems(List<PreviewItem> items)
    {
        if (items.Count == 0)
        {
            return;
        }

        const double frameLeft = 20;
        const double frameTop = 20;
        const double frameRight = 600;
        const double titleTop = 390;
        double margin = ReadPreviewDouble(SheetMarginTextBox, 20);
        Rect bounds = GetPreviewBounds(items);
        double usableLeft = frameLeft + margin;
        double usableTop = frameTop + margin;
        double usableRight = frameRight - margin;
        double usableBottom = titleTop - margin;
        double usableWidth = Math.Max(1, usableRight - usableLeft);
        double usableHeight = Math.Max(1, usableBottom - usableTop);
        double scale = Math.Min(1.0, Math.Min(usableWidth / Math.Max(1, bounds.Width), usableHeight / Math.Max(1, bounds.Height)));
        double scaledWidth = bounds.Width * scale;
        double scaledHeight = bounds.Height * scale;
        double targetLeft = usableLeft + (usableWidth - scaledWidth) / 2;
        double targetTop = usableTop + (usableHeight - scaledHeight) / 2;

        foreach (PreviewItem item in items)
        {
            SetCanvasPosition(
                item.Element,
                targetLeft + (item.Left - bounds.Left) * scale,
                targetTop + (item.Top - bounds.Top) * scale);
            item.Element.RenderTransformOrigin = new Point(0, 0);
            item.Element.RenderTransform = scale < 0.999 ? new ScaleTransform(scale, scale) : Transform.Identity;
            item.Element.Visibility = Visibility.Visible;
        }
    }

    private void HideAllPreviewViews()
    {
        PreviewTopView.Visibility = Visibility.Collapsed;
        PreviewBottomView.Visibility = Visibility.Collapsed;
        PreviewLeftPositionView.Visibility = Visibility.Collapsed;
        PreviewRightPositionView.Visibility = Visibility.Collapsed;
        PreviewFrontView.Visibility = Visibility.Collapsed;
        PreviewBackView.Visibility = Visibility.Collapsed;
        PreviewBackBottomView.Visibility = Visibility.Collapsed;
        PreviewIsoView.Visibility = Visibility.Collapsed;
        PreviewFlatView.Visibility = Visibility.Collapsed;
        PreviewTechnicalRequirement.Visibility = Visibility.Collapsed;
        foreach (FrameworkElement element in new FrameworkElement[]
                 {
                     PreviewTopView,
                     PreviewBottomView,
                     PreviewLeftPositionView,
                     PreviewRightPositionView,
                     PreviewFrontView,
                     PreviewBackView,
                     PreviewBackBottomView,
                     PreviewIsoView,
                     PreviewFlatView,
                     PreviewTechnicalRequirement
                 })
        {
            element.RenderTransform = Transform.Identity;
        }
    }

    private static Rect GetPreviewBounds(IReadOnlyList<PreviewItem> items)
    {
        double left = items[0].Left;
        double top = items[0].Top;
        double right = items[0].Left + items[0].Width;
        double bottom = items[0].Top + items[0].Height;

        foreach (PreviewItem item in items)
        {
            left = Math.Min(left, item.Left);
            top = Math.Min(top, item.Top);
            right = Math.Max(right, item.Left + item.Width);
            bottom = Math.Max(bottom, item.Top + item.Height);
        }

        return new Rect(left, top, right - left, bottom - top);
    }

    private static string GetCorner(ToggleButton topLeftButton, ToggleButton topRightButton, ToggleButton bottomLeftButton, ToggleButton bottomRightButton)
    {
        if (topRightButton.IsChecked == true)
        {
            return "TopRight";
        }

        if (bottomLeftButton.IsChecked == true)
        {
            return "BottomLeft";
        }

        if (bottomRightButton.IsChecked == true)
        {
            return "BottomRight";
        }

        return "TopLeft";
    }

    private static bool IsAnyChecked(params ToggleButton[] buttons)
    {
        foreach (ToggleButton button in buttons)
        {
            if (button.IsChecked == true)
            {
                return true;
            }
        }

        return false;
    }

    private static string SelectedComboBoxTag(ComboBox comboBox, string fallback)
    {
        if (comboBox.SelectedItem is ComboBoxItem item && item.Tag is string tag && !string.IsNullOrWhiteSpace(tag))
        {
            return tag;
        }

        return fallback;
    }

    private static void SelectComboBoxByTag(ComboBox comboBox, string tag)
    {
        foreach (object item in comboBox.Items)
        {
            if (item is ComboBoxItem comboBoxItem &&
                comboBoxItem.Tag is string itemTag &&
                string.Equals(itemTag, tag, StringComparison.OrdinalIgnoreCase))
            {
                comboBox.SelectedItem = comboBoxItem;
                return;
            }
        }

        if (comboBox.Items.Count > 0)
        {
            comboBox.SelectedIndex = 0;
        }
    }

    private void PreventSameAuxiliaryCorner(ToggleButton changedButton)
    {
        bool isoEnabled = IsAnyChecked(IsoTopLeftButton, IsoTopRightButton, IsoBottomLeftButton, IsoBottomRightButton);
        bool flatEnabled = IsAnyChecked(FlatTopLeftButton, FlatTopRightButton, FlatBottomLeftButton, FlatBottomRightButton);
        if (!isoEnabled || !flatEnabled)
        {
            return;
        }

        string isoCorner = GetCorner(IsoTopLeftButton, IsoTopRightButton, IsoBottomLeftButton, IsoBottomRightButton);
        string flatCorner = GetCorner(FlatTopLeftButton, FlatTopRightButton, FlatBottomLeftButton, FlatBottomRightButton);
        if (!string.Equals(isoCorner, flatCorner, StringComparison.OrdinalIgnoreCase))
        {
            return;
        }

        bool changedIso = changedButton.Name.StartsWith("Iso", StringComparison.Ordinal);
        if (changedIso)
        {
            SetCornerButtons(true, AlternateCorner(isoCorner), FlatTopLeftButton, FlatTopRightButton, FlatBottomLeftButton, FlatBottomRightButton);
        }
        else
        {
            SetCornerButtons(true, AlternateCorner(flatCorner), IsoTopLeftButton, IsoTopRightButton, IsoBottomLeftButton, IsoBottomRightButton);
        }
    }

    private void EnsureDifferentAuxiliaryCorners()
    {
        bool isoEnabled = IsAnyChecked(IsoTopLeftButton, IsoTopRightButton, IsoBottomLeftButton, IsoBottomRightButton);
        bool flatEnabled = IsAnyChecked(FlatTopLeftButton, FlatTopRightButton, FlatBottomLeftButton, FlatBottomRightButton);
        if (!isoEnabled || !flatEnabled)
        {
            return;
        }

        string isoCorner = GetCorner(IsoTopLeftButton, IsoTopRightButton, IsoBottomLeftButton, IsoBottomRightButton);
        string flatCorner = GetCorner(FlatTopLeftButton, FlatTopRightButton, FlatBottomLeftButton, FlatBottomRightButton);
        if (string.Equals(isoCorner, flatCorner, StringComparison.OrdinalIgnoreCase))
        {
            SetCornerButtons(true, AlternateCorner(flatCorner), FlatTopLeftButton, FlatTopRightButton, FlatBottomLeftButton, FlatBottomRightButton);
        }
    }

    private static string AlternateCorner(string corner)
    {
        return corner switch
        {
            "TopLeft" => "BottomLeft",
            "BottomLeft" => "TopLeft",
            "TopRight" => "BottomRight",
            "BottomRight" => "TopRight",
            _ => "BottomRight"
        };
    }

    private static double ReadPreviewDouble(TextBox textBox, double fallback)
    {
        if (double.TryParse(textBox.Text, out double value))
        {
            return Math.Max(5, value);
        }

        return fallback;
    }

    private static void SetCanvasPosition(UIElement element, double left, double top)
    {
        Canvas.SetLeft(element, left);
        Canvas.SetTop(element, top);
    }

    private Dictionary<string, string> CaptureCurrentDrawingSettings()
    {
        EnsureDifferentAuxiliaryCorners();

        bool isFirstAngle = FirstAngleRadio.IsChecked == true;
        string requestTechnicalRequirementText = TechnicalRequirementEnabledCheckBox.IsChecked == true
            ? NormalizeTechnicalRequirementText(TechnicalRequirementTextBox.Text)
            : "";

        return new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
        {
            ["templatePath"] = SelectedTemplatePath(),
            ["projection"] = isFirstAngle ? "first" : "third",
            ["frontDirectionMode"] = SelectedComboBoxTag(FrontDirectionModeComboBox, "largestFaceLongestEdge"),
            ["viewSpacing"] = ViewSpacingTextBox.Text,
            ["sheetMargin"] = SheetMarginTextBox.Text,
            ["front"] = BoolText(FrontViewButton.IsChecked == true),
            ["top"] = BoolText(TopViewButton.IsChecked == true),
            ["bottom"] = BoolText(BottomViewButton.IsChecked == true),
            ["left"] = BoolText(LeftPositionViewButton.IsChecked == true),
            ["right"] = BoolText(RightPositionViewButton.IsChecked == true),
            ["back"] = BoolText(BackViewButton.IsChecked == true),
            ["backBottom"] = BoolText(BackBottomViewButton.IsChecked == true),
            ["iso"] = BoolText(IsAnyChecked(IsoTopLeftButton, IsoTopRightButton, IsoBottomLeftButton, IsoBottomRightButton)),
            ["flat"] = BoolText(IsAnyChecked(FlatTopLeftButton, FlatTopRightButton, FlatBottomLeftButton, FlatBottomRightButton)),
            ["auxAutoCompact"] = BoolText(AuxAutoCompactCheckBox.IsChecked == true),
            ["isoCorner"] = GetCorner(IsoTopLeftButton, IsoTopRightButton, IsoBottomLeftButton, IsoBottomRightButton),
            ["flatCorner"] = GetCorner(FlatTopLeftButton, FlatTopRightButton, FlatBottomLeftButton, FlatBottomRightButton),
            ["autoDimensions"] = BoolText(AnyDimensionEnabled()),
            ["dimensionOverall"] = BoolText(DimensionOverallCheckBox.IsChecked == true),
            ["dimensionLinear"] = BoolText(false),
            ["dimensionAngle"] = BoolText(DimensionAngleCheckBox.IsChecked == true),
            ["dimensionHole"] = BoolText(DimensionHoleCheckBox.IsChecked == true),
            ["dimensionHoleLocation"] = BoolText(DimensionHoleLocationCheckBox.IsChecked == true),
            ["dimensionInnerClosedCurve"] = BoolText(DimensionInnerClosedCurveCheckBox.IsChecked == true),
            ["technicalRequirementEnabled"] = BoolText(TechnicalRequirementEnabledCheckBox.IsChecked == true),
            ["technicalRequirementIndexed"] = BoolText(TechnicalRequirementIndexCheckBox.IsChecked == true),
            ["technicalRequirementCorner"] = GetTechnicalRequirementCorner(),
            ["technicalRequirementText"] = EncodeText(requestTechnicalRequirementText)
        };
    }

    private void ApplyDrawingSettings(Dictionary<string, string> values)
    {
        isLoadingPartOptions = true;
        isUpdatingPreview = true;
        try
        {
            FirstAngleRadio.IsChecked = ReadText(values, "projection", FirstAngleRadio.IsChecked == true ? "first" : "third") != "third";
            ThirdAngleRadio.IsChecked = FirstAngleRadio.IsChecked != true;
            SelectComboBoxByTag(FrontDirectionModeComboBox, ReadText(values, "frontDirectionMode", "largestFaceLongestEdge"));
            ViewSpacingTextBox.Text = ReadText(values, "viewSpacing", ViewSpacingTextBox.Text);
            SheetMarginTextBox.Text = ReadText(values, "sheetMargin", SheetMarginTextBox.Text);
            FrontViewButton.IsChecked = ReadBool(values, "front", FrontViewButton.IsChecked == true);
            TopViewButton.IsChecked = ReadBool(values, "top", TopViewButton.IsChecked == true);
            BottomViewButton.IsChecked = ReadBool(values, "bottom", BottomViewButton.IsChecked == true);
            LeftPositionViewButton.IsChecked = ReadBool(values, "left", LeftPositionViewButton.IsChecked == true);
            RightPositionViewButton.IsChecked = ReadBool(values, "right", RightPositionViewButton.IsChecked == true);
            BackViewButton.IsChecked = ReadBool(values, "back", BackViewButton.IsChecked == true);
            BackBottomViewButton.IsChecked = ReadBool(values, "backBottom", BackBottomViewButton.IsChecked == true);
            AuxAutoCompactCheckBox.IsChecked = ReadBool(values, "auxAutoCompact", AuxAutoCompactCheckBox.IsChecked == true);
            DimensionOverallCheckBox.IsChecked = ReadBool(values, "dimensionOverall", DimensionOverallCheckBox.IsChecked == true);
            DimensionLinearCheckBox.IsChecked = false;
            DimensionAngleCheckBox.IsChecked = ReadBool(values, "dimensionAngle", DimensionAngleCheckBox.IsChecked == true);
            DimensionHoleCheckBox.IsChecked = ReadBool(values, "dimensionHole", DimensionHoleCheckBox.IsChecked == true);
            DimensionHoleLocationCheckBox.IsChecked = ReadBool(values, "dimensionHoleLocation", DimensionHoleLocationCheckBox.IsChecked == true);
            DimensionInnerClosedCurveCheckBox.IsChecked = ReadBool(values, "dimensionInnerClosedCurve", DimensionInnerClosedCurveCheckBox.IsChecked == true);
            ShareAllPartsOptionsCheckBox.IsChecked = ReadBool(values, "shareAllPartOptions", ShareAllPartsOptionsCheckBox.IsChecked == true);
            TechnicalRequirementEnabledCheckBox.IsChecked = ReadBool(values, "technicalRequirementEnabled", TechnicalRequirementEnabledCheckBox.IsChecked == true);
            TechnicalRequirementIndexCheckBox.IsChecked = ReadBool(values, "technicalRequirementIndexed", TechnicalRequirementIndexCheckBox.IsChecked == true);
            SetCornerButtons(
                true,
                ReadText(values, "technicalRequirementCorner", GetTechnicalRequirementCorner()),
                TechnicalTopLeftButton,
                TechnicalTopRightButton,
                TechnicalBottomLeftButton,
                TechnicalBottomRightButton);
            TechnicalRequirementTextBox.Text = NormalizeTechnicalRequirementText(DecodeText(ReadText(values, "technicalRequirementText", "")));

            SetCornerButtons(
                ReadBool(values, "iso", IsAnyChecked(IsoTopLeftButton, IsoTopRightButton, IsoBottomLeftButton, IsoBottomRightButton)),
                ReadText(values, "isoCorner", GetCorner(IsoTopLeftButton, IsoTopRightButton, IsoBottomLeftButton, IsoBottomRightButton)),
                IsoTopLeftButton,
                IsoTopRightButton,
                IsoBottomLeftButton,
                IsoBottomRightButton);

            SetCornerButtons(
                ReadBool(values, "flat", IsAnyChecked(FlatTopLeftButton, FlatTopRightButton, FlatBottomLeftButton, FlatBottomRightButton)),
                ReadText(values, "flatCorner", GetCorner(FlatTopLeftButton, FlatTopRightButton, FlatBottomLeftButton, FlatBottomRightButton)),
                FlatTopLeftButton,
                FlatTopRightButton,
                FlatBottomLeftButton,
                FlatBottomRightButton);

            EnsureDifferentAuxiliaryCorners();
        }
        finally
        {
            isUpdatingPreview = false;
            isLoadingPartOptions = false;
        }

        ApplyProjectionMode();
        UpdatePreviewFromOptions();
    }

    private void SaveCurrentPartOptions(bool showMessage)
    {
        Dictionary<string, string> values = CaptureCurrentDrawingSettings();
        SaveOptions();

        if (ShareAllPartsOptionsCheckBox.IsChecked == true)
        {
            if (showMessage)
            {
                SelectedAssemblyTextBlock.Text = "\u6240\u6709\u96f6\u4ef6\u5171\u7528\u53c2\u6570\u5df2\u5e94\u7528";
            }
            return;
        }

        if (selectedAssemblyNode == null)
        {
            return;
        }

        partOptionsByOccurrence[selectedAssemblyNode.OccurrenceTag] = values;
        SetAssemblyNodeChecked(selectedAssemblyNode, true);
        UpdateSelectAllCheckBoxState();
        if (showMessage)
        {
            SelectedAssemblyTextBlock.Text = $"{selectedAssemblyNode.Name} \u53c2\u6570\u5df2\u5e94\u7528";
        }
    }
    private void LoadOptionsForSelectedPart()
    {
        if (isLoadingPartOptions ||
            ShareAllPartsOptionsCheckBox?.IsChecked == true ||
            selectedAssemblyNode == null)
        {
            return;
        }

        if (partOptionsByOccurrence.TryGetValue(selectedAssemblyNode.OccurrenceTag, out Dictionary<string, string>? values))
        {
            ApplyDrawingSettings(values);
        }
    }

    private void WriteCreateDrawingRequest()
    {
        SaveCurrentPartOptions(showMessage: false);

        string requestPath = GetRequestPath();
        Directory.CreateDirectory(Path.GetDirectoryName(requestPath)!);

        Dictionary<string, string> sharedSettings = CaptureCurrentDrawingSettings();
        List<AssemblyNode> checkedNodes = GetCheckedAssemblyNodes();
        if (!assemblySelectionAvailable)
        {
            checkedNodes.Clear();
        }
        if (assemblySelectionAvailable && checkedNodes.Count == 0 && selectedAssemblyNode != null)
        {
            checkedNodes.Add(selectedAssemblyNode);
        }

        Dictionary<string, string> values = new(StringComparer.OrdinalIgnoreCase)
        {
            ["action"] = "create",
            ["selectedPartCount"] = checkedNodes.Count.ToString(),
            ["shareAllPartOptions"] = BoolText(ShareAllPartsOptionsCheckBox.IsChecked == true),
            ["selectedOccurrenceTag"] = assemblySelectionAvailable
                ? checkedNodes.FirstOrDefault()?.OccurrenceTag ?? selectedAssemblyNode?.OccurrenceTag ?? ""
                : ""
        };

        foreach (KeyValuePair<string, string> setting in sharedSettings)
        {
            values[setting.Key] = setting.Value;
        }

        for (int index = 0; index < checkedNodes.Count; ++index)
        {
            AssemblyNode node = checkedNodes[index];
            Dictionary<string, string> partSettings = sharedSettings;
            if (ShareAllPartsOptionsCheckBox.IsChecked != true &&
                partOptionsByOccurrence.TryGetValue(node.OccurrenceTag, out Dictionary<string, string>? savedPartSettings))
            {
                partSettings = savedPartSettings;
            }

            string prefix = $"part{index}.";
            values[prefix + "occurrenceTag"] = node.OccurrenceTag;
            values[prefix + "name"] = EncodeText(node.Name);
            values[prefix + "partPath"] = EncodeText(node.PartPath);
            values[prefix + "quantity"] = node.Quantity.ToString();
            foreach (KeyValuePair<string, string> setting in partSettings)
            {
                values[prefix + setting.Key] = setting.Value;
            }
        }

        StringBuilder builder = new();
        foreach (KeyValuePair<string, string> value in values)
        {
            builder.Append(value.Key).Append('=').AppendLine(value.Value);
        }

        WriteAtomicText(requestPath, builder.ToString());
    }

    private void LoadAssemblyTree()
    {
        if (AssemblyTree == null)
        {
            return;
        }

        AssemblyTree.Items.Clear();
        string manifestPath = GetAssemblyManifestPath();
        if (!File.Exists(manifestPath))
        {
            TreeViewItem item = CreateAssemblyTreeItem(new AssemblyNode("0", "", "当前工作零件", ""));
            AssemblyTree.Items.Add(item);
            item.IsSelected = true;
            item.IsExpanded = true;
            if (item.Tag is AssemblyNode fallbackNode)
            {
                selectedAssemblyNode = fallbackNode;
            }
            assemblySelectionAvailable = false;
            UpdatePartSelectionAvailability();
            UpdateSelectedAssemblyText();
            return;
        }

        try
        {
            List<AssemblyManifestRecord> records = ReadAssemblyManifestRecords(manifestPath);
            Dictionary<string, List<AssemblyManifestRecord>> childrenByParent = records
                .GroupBy(record => record.ParentOccurrenceTag, StringComparer.OrdinalIgnoreCase)
                .ToDictionary(group => group.Key, group => group.ToList(), StringComparer.OrdinalIgnoreCase);
            HashSet<string> parentIds = records
                .Select(record => record.ParentOccurrenceTag)
                .Where(parent => !string.IsNullOrWhiteSpace(parent) && parent != "0")
                .ToHashSet(StringComparer.OrdinalIgnoreCase);
            AddAggregatedAssemblyNodes(childrenByParent, parentIds, "0", AssemblyTree.Items, 1);
            if (AssemblyTree.Items.Count == 0)
            {
                AddAggregatedAssemblyNodes(childrenByParent, parentIds, "", AssemblyTree.Items, 1);
            }

            TreeViewItem? firstItem = AssemblyTree.Items.OfType<TreeViewItem>().FirstOrDefault();
            if (firstItem != null)
            {
                firstItem.IsSelected = true;
                if (firstItem.Tag is AssemblyNode firstNode)
                {
                    selectedAssemblyNode = firstNode;
                }
            }
        }
        catch
        {
            TreeViewItem item = CreateAssemblyTreeItem(new AssemblyNode("0", "", "当前工作零件", ""));
            AssemblyTree.Items.Add(item);
            item.IsSelected = true;
            if (item.Tag is AssemblyNode fallbackNode)
            {
                selectedAssemblyNode = fallbackNode;
            }
        }

        assemblySelectionAvailable = GetSelectableAssemblyNodes().Count > 1;
        UpdatePartSelectionAvailability();
        UpdateSelectAllCheckBoxState();
        UpdateSelectedAssemblyText();
    }

    private void InitializeAssemblyFilterMenu()
    {
        if (AssemblyTree == null)
        {
            return;
        }

        ContextMenu contextMenu = new();
        MenuItem filterMenu = new() { Header = "过滤" };
        AddAssemblyFilterRule(filterMenu, "移除普通零件", BatchTargetFilterKind.RemovePart);
        AddAssemblyFilterRule(filterMenu, "移除装配体", BatchTargetFilterKind.RemoveAssembly);
        AddAssemblyFilterRule(filterMenu, "移除已出图模型", BatchTargetFilterKind.RemoveWithDrawingSheets);
        AddAssemblyFilterRule(filterMenu, "移除未出图模型", BatchTargetFilterKind.RemoveWithoutDrawingSheets);
        AddAssemblyFilterRule(filterMenu, "移除钣金", BatchTargetFilterKind.RemoveSheetMetal);
        AddAssemblyFilterRule(filterMenu, "移除非钣金", BatchTargetFilterKind.RemoveNonSheetMetal);
        AddAssemblyFilterRule(filterMenu, "移除隐藏组件", BatchTargetFilterKind.RemoveHiddenComponent);
        filterMenu.Items.Add(new Separator());
        AddAssemblyKeywordFilterRule(
            filterMenu,
            "移除包含关键词...",
            BatchTargetFilterKind.RemoveKeywordMatches);
        AddAssemblyKeywordFilterRule(
            filterMenu,
            "移除不含关键词...",
            BatchTargetFilterKind.RemoveKeywordNonMatches);
        AddAssemblyAttributeNameFilterRule(
            filterMenu,
            "移除有指定属性...",
            BatchTargetFilterKind.RemoveHasAttribute);
        AddAssemblyAttributeNameFilterRule(
            filterMenu,
            "移除不含指定属性...",
            BatchTargetFilterKind.RemoveMissingAttribute);
        AddAssemblyAttributeEqualsFilterRule(filterMenu);
        AddAssemblyAttributeValueFilterRule(filterMenu);
        filterMenu.Items.Add(new Separator());
        MenuItem applyFilters = new() { Header = "应用所选过滤" };
        applyFilters.Click += (_, _) => ApplySelectedAssemblyFilters();
        filterMenu.Items.Add(applyFilters);
        MenuItem clearFilters = new() { Header = "清除过滤选择" };
        clearFilters.Click += (_, _) => ClearAssemblyFilterSelections();
        filterMenu.Items.Add(clearFilters);
        contextMenu.Items.Add(filterMenu);

        contextMenu.Items.Add(new Separator());
        MenuItem restoreAll = new() { Header = "恢复全部勾选" };
        restoreAll.Click += (_, _) =>
        {
            SetAllAssemblyNodeChecks(true);
            UpdateSelectAllCheckBoxState();
            UpdateSelectedAssemblyText();
        };
        contextMenu.Items.Add(restoreAll);
        AssemblyTree.ContextMenu = contextMenu;
    }

    private void AddAssemblyFilterRule(
        MenuItem filterMenu,
        string header,
        BatchTargetFilterKind kind)
    {
        MenuItem item = new() { Header = header, IsCheckable = true, StaysOpenOnClick = true };
        assemblyFilterRulesByMenuItem[item] = new BatchTargetFilterRule { Kind = kind };
        filterMenu.Items.Add(item);
    }

    private void AddAssemblyKeywordFilterRule(
        MenuItem filterMenu,
        string header,
        BatchTargetFilterKind kind)
    {
        MenuItem item = new() { Header = header, IsCheckable = true, StaysOpenOnClick = true };
        item.Click += (_, _) =>
        {
            if (!item.IsChecked)
            {
                assemblyFilterRulesByMenuItem.Remove(item);
                return;
            }

            if (!BatchTargetFilterPrompt.TryAskText(this, "过滤关键词", "关键词", out string keyword))
            {
                item.IsChecked = false;
                assemblyFilterRulesByMenuItem.Remove(item);
                return;
            }

            assemblyFilterRulesByMenuItem[item] =
                new BatchTargetFilterRule { Kind = kind, Keyword = keyword };
        };
        filterMenu.Items.Add(item);
    }

    private void AddAssemblyAttributeNameFilterRule(
        MenuItem filterMenu,
        string header,
        BatchTargetFilterKind kind)
    {
        MenuItem item = new() { Header = header, IsCheckable = true, StaysOpenOnClick = true };
        item.Click += (_, _) =>
        {
            if (!item.IsChecked)
            {
                assemblyFilterRulesByMenuItem.Remove(item);
                return;
            }

            if (!BatchTargetFilterPrompt.TryAskText(this, "按属性过滤", "属性名", out string attributeName))
            {
                item.IsChecked = false;
                assemblyFilterRulesByMenuItem.Remove(item);
                return;
            }

            assemblyFilterRulesByMenuItem[item] =
                new BatchTargetFilterRule
                {
                    Kind = kind,
                    AttributeName = attributeName
                };
        };
        filterMenu.Items.Add(item);
    }

    private void AddAssemblyAttributeEqualsFilterRule(MenuItem filterMenu)
    {
        MenuItem item = new()
        {
            Header = "移除属性等于...",
            IsCheckable = true,
            StaysOpenOnClick = true
        };
        item.Click += (_, _) =>
        {
            if (!item.IsChecked)
            {
                assemblyFilterRulesByMenuItem.Remove(item);
                return;
            }

            if (!BatchTargetFilterPrompt.TryAskAttributeEquals(
                    this,
                    out string attributeName,
                    out string attributeValue))
            {
                item.IsChecked = false;
                assemblyFilterRulesByMenuItem.Remove(item);
                return;
            }

            assemblyFilterRulesByMenuItem[item] =
                new BatchTargetFilterRule
                {
                    Kind = BatchTargetFilterKind.RemoveAttributeEquals,
                    AttributeName = attributeName,
                    AttributeValue = attributeValue
                };
        };
        filterMenu.Items.Add(item);
    }

    private void AddAssemblyAttributeValueFilterRule(MenuItem filterMenu)
    {
        MenuItem item = new()
        {
            Header = "移除无指定属性值...",
            IsCheckable = true,
            StaysOpenOnClick = true
        };
        item.Click += (_, _) =>
        {
            if (!item.IsChecked)
            {
                assemblyFilterRulesByMenuItem.Remove(item);
                return;
            }

            if (!BatchTargetFilterPrompt.TryAskText(this, "按属性值过滤", "属性值", out string value))
            {
                item.IsChecked = false;
                assemblyFilterRulesByMenuItem.Remove(item);
                return;
            }

            assemblyFilterRulesByMenuItem[item] =
                new BatchTargetFilterRule
                {
                    Kind = BatchTargetFilterKind.RemoveWithoutAttributeValue,
                    AttributeValue = value
                };
        };
        filterMenu.Items.Add(item);
    }

    private void ClearAssemblyFilterSelections()
    {
        foreach (MenuItem item in assemblyFilterRulesByMenuItem.Keys.ToList())
        {
            item.IsChecked = false;
        }
    }

    private void ApplySelectedAssemblyFilters()
    {
        List<BatchTargetFilterRule> rules = assemblyFilterRulesByMenuItem
            .Where(pair => pair.Key.IsChecked)
            .Select(pair => pair.Value)
            .ToList();
        if (rules.Count == 0)
        {
            SelectedAssemblyTextBlock.Text = "请先勾选至少一个过滤条件";
            return;
        }

        List<AssemblyNode> allTargets = GetSelectableAssemblyNodes();
        if (allTargets.Count == 0)
        {
            return;
        }

        List<AssemblyNode> removed = allTargets
            .Where(
                node => rules.Any(
                    rule => BatchTargetFilterEngine.ShouldRemove(node.FilterTarget, rule)))
            .ToList();

        isUpdatingAssemblyChecks = true;
        try
        {
            foreach (AssemblyNode node in allTargets)
            {
                SetAssemblyNodeChecked(node, !removed.Contains(node));
            }
        }
        finally
        {
            isUpdatingAssemblyChecks = false;
        }

        UpdateSelectAllCheckBoxState();
        isUpdatingAssemblyChecks = true;
        try
        {
            foreach (AssemblyNode node in EnumerateTreeItems(AssemblyTree.Items)
                         .Select(item => item.Tag)
                         .OfType<AssemblyNode>())
            {
                if (rules.Any(rule => BatchTargetFilterEngine.ShouldRemove(node.FilterTarget, rule)))
                {
                    SetAssemblyNodeChecked(node, false);
                }
            }

            SelectAllPartsCheckBox.IsChecked =
                allTargets.Count > 0 && allTargets.All(node => node.IsChecked);
        }
        finally
        {
            isUpdatingAssemblyChecks = false;
        }

        UpdateSelectedAssemblyText();
        SelectedAssemblyTextBlock.Text += $"，按 {rules.Count} 项条件过滤取消 {removed.Count} 个";
    }

    private void UpdatePartSelectionAvailability()
    {
        if (PartSelectionTab == null || MainTabControl == null)
        {
            return;
        }

        PartSelectionTab.Visibility = assemblySelectionAvailable ? Visibility.Visible : Visibility.Collapsed;
        if (!assemblySelectionAvailable && ReferenceEquals(MainTabControl.SelectedItem, PartSelectionTab))
        {
            MainTabControl.SelectedItem = ViewTab;
        }

        ApplySelectedTabPanel();
    }

    private TreeViewItem CreateAssemblyTreeItem(AssemblyNode node)
    {
        string partStem = string.IsNullOrWhiteSpace(node.PartPath)
            ? ""
            : Path.GetFileNameWithoutExtension(node.PartPath);
        string displayName = CleanAssemblyDisplayName(node.Name, partStem);
        string compareName = Path.GetFileNameWithoutExtension(displayName);
        string headerText = string.IsNullOrWhiteSpace(partStem) ||
            string.Equals(displayName, partStem, StringComparison.OrdinalIgnoreCase) ||
            string.Equals(compareName, partStem, StringComparison.OrdinalIgnoreCase)
                ? displayName
                : $"{displayName}  [{partStem}]";
        if (node.Quantity > 1)
        {
            headerText += $"  x{node.Quantity}";
        }

        CheckBox checkBox = new()
        {
            Tag = node,
            VerticalAlignment = VerticalAlignment.Center,
            Margin = new Thickness(0, 0, 6, 0)
        };
        checkBox.PreviewMouseLeftButtonDown += AssemblyNodeCheckBox_PreviewMouseLeftButtonDown;
        checkBox.Checked += AssemblyNodeCheckBox_Changed;
        checkBox.Unchecked += AssemblyNodeCheckBox_Changed;

        TextBlock textBlock = new()
        {
            Text = headerText,
            VerticalAlignment = VerticalAlignment.Center,
            TextTrimming = TextTrimming.CharacterEllipsis
        };

        StackPanel header = new()
        {
            Orientation = Orientation.Horizontal
        };
        header.Children.Add(checkBox);
        header.Children.Add(textBlock);

        TreeViewItem item = new()
        {
            Header = header,
            Tag = node
        };
        node.CheckBox = checkBox;
        node.TreeItem = item;
        return item;
    }

    private void UpdateSelectedAssemblyText()
    {
        if (SelectedAssemblyTextBlock == null)
        {
            return;
        }

        if (selectedAssemblyNode == null)
        {
            SelectedAssemblyTextBlock.Text = "当前工作零件";
            return;
        }

        int checkedCount = GetCheckedAssemblyNodes().Count;
        string suffix = checkedCount > 0 ? $"\uff0c\u5df2\u52fe\u9009 {checkedCount} \u4e2a" : "";
        SelectedAssemblyTextBlock.Text = string.IsNullOrWhiteSpace(selectedAssemblyNode.PartPath)
            ? $"{selectedAssemblyNode.Name}{QuantitySuffix(selectedAssemblyNode)}{suffix}"
            : $"{selectedAssemblyNode.Name}{QuantitySuffix(selectedAssemblyNode)} - {selectedAssemblyNode.PartPath}{suffix}";
    }

    private static string QuantitySuffix(AssemblyNode node)
    {
        return node.Quantity > 1 ? $" x{node.Quantity}" : "";
    }

    private static string CleanAssemblyDisplayName(string name, string partStem)
    {
        string text = name?.Trim() ?? "";
        bool corrupt = text.Contains('\uFFFD') || text.Contains("???") || text.Contains("���");
        if ((corrupt || string.IsNullOrWhiteSpace(text)) && !string.IsNullOrWhiteSpace(partStem))
        {
            return partStem;
        }

        return text;
    }

    private void SetAllAssemblyNodeChecks(bool isChecked)
    {
        isUpdatingAssemblyChecks = true;
        try
        {
            foreach (AssemblyNode node in GetSelectableAssemblyNodes())
            {
                SetAssemblyNodeChecked(node, isChecked);
            }
        }
        finally
        {
            isUpdatingAssemblyChecks = false;
        }
    }

    private void SetAssemblyRangeChecked(AssemblyNode firstNode, AssemblyNode secondNode, bool isChecked)
    {
        List<TreeViewItem> items = EnumerateTreeItems(AssemblyTree.Items).ToList();
        int firstIndex = items.FindIndex(item => ReferenceEquals(item.Tag, firstNode));
        int secondIndex = items.FindIndex(item => ReferenceEquals(item.Tag, secondNode));
        if (firstIndex < 0 || secondIndex < 0)
        {
            SetAssemblyNodeChecked(secondNode, isChecked);
            return;
        }

        if (firstIndex > secondIndex)
        {
            (firstIndex, secondIndex) = (secondIndex, firstIndex);
        }

        isUpdatingAssemblyChecks = true;
        try
        {
            for (int index = firstIndex; index <= secondIndex; ++index)
            {
                if (items[index].Tag is AssemblyNode node)
                {
                    SetAssemblyNodeChecked(node, isChecked);
                }
            }
        }
        finally
        {
            isUpdatingAssemblyChecks = false;
        }
    }

    private void SetAssemblyNodeChecked(AssemblyNode node, bool isChecked)
    {
        node.IsChecked = isChecked;
        if (node.CheckBox != null)
        {
            node.CheckBox.IsChecked = isChecked;
        }
    }

    private List<AssemblyNode> GetCheckedAssemblyNodes()
    {
        List<AssemblyNode> nodes = new();
        foreach (AssemblyNode node in GetSelectableAssemblyNodes())
        {
            if (node.IsChecked)
            {
                nodes.Add(node);
            }
        }

        return nodes;
    }

    private void UpdateSelectAllCheckBoxState()
    {
        if (SelectAllPartsCheckBox == null || AssemblyTree == null)
        {
            return;
        }

        isUpdatingAssemblyChecks = true;
        try
        {
            List<AssemblyNode> selectableNodes = GetSelectableAssemblyNodes();
            SelectAllPartsCheckBox.IsChecked = selectableNodes.Count > 0 && selectableNodes.All(node => node.IsChecked);
        }
        finally
        {
            isUpdatingAssemblyChecks = false;
        }
    }

    private List<AssemblyNode> GetSelectableAssemblyNodes()
    {
        return EnumerateTreeItems(AssemblyTree.Items)
            .Select(item => item.Tag)
            .OfType<AssemblyNode>()
            .ToList();
    }

    private static bool IsRootAssemblyNode(AssemblyNode node)
    {
        return IsRootOccurrenceTag(node.ParentOccurrenceTag);
    }

    private static bool IsRootOccurrenceTag(string? parentOccurrenceTag)
    {
        return string.IsNullOrWhiteSpace(parentOccurrenceTag) ||
               string.Equals(parentOccurrenceTag, "0", StringComparison.OrdinalIgnoreCase);
    }

    private List<AssemblyManifestRecord> ReadAssemblyManifestRecords(string manifestPath)
    {
        List<AssemblyManifestRecord> records = new();
        foreach (string rawLine in File.ReadLines(manifestPath, Encoding.UTF8))
        {
            Dictionary<string, string> values = ReadTabbedKeyValueLine(rawLine);
            if (!values.TryGetValue("id", out string? id) || string.IsNullOrWhiteSpace(id))
            {
                continue;
            }

            values.TryGetValue("parent", out string? parentId);
            values.TryGetValue("name", out string? name);
            values.TryGetValue("part", out string? part);
            values.TryGetValue("attributes", out string? attributes);
            records.Add(new AssemblyManifestRecord(
                id,
                parentId ?? "",
                string.IsNullOrWhiteSpace(name) ? $"零件 {id}" : name,
                part ?? "",
                ReadManifestBool(values, "assembly"),
                ReadManifestBool(values, "drawing"),
                ReadManifestBool(values, "sheetMetal"),
                ReadManifestBool(values, "hidden"),
                ParseManifestAttributes(attributes ?? "")));
        }

        return records;
    }

    private void AddAggregatedAssemblyNodes(
        Dictionary<string, List<AssemblyManifestRecord>> childrenByParent,
        HashSet<string> parentIds,
        string parentOccurrenceTag,
        ItemCollection targetItems,
        int parentQuantity)
    {
        if (!childrenByParent.TryGetValue(parentOccurrenceTag, out List<AssemblyManifestRecord>? children))
        {
            return;
        }

        IEnumerable<IGrouping<string, AssemblyManifestRecord>> groups = children
            .GroupBy(child => BuildAssemblyPartIdentityKey(child.Name, child.PartPath), StringComparer.OrdinalIgnoreCase);
        foreach (IGrouping<string, AssemblyManifestRecord> group in groups)
        {
            AssemblyManifestRecord representative = group.First();
            List<AssemblyManifestRecord> groupedRecords = group.ToList();
            int quantity = Math.Max(1, parentQuantity) * group.Count();
            bool hasChildren = parentIds.Contains(representative.OccurrenceTag);
            AssemblyNode node = new(
                representative.OccurrenceTag,
                representative.ParentOccurrenceTag,
                representative.Name,
                representative.PartPath,
                quantity,
                hasChildren,
                representative.IsAssembly || hasChildren,
                groupedRecords.Any(record => record.HasDrawingSheets),
                groupedRecords.Any(record => record.IsSheetMetal),
                groupedRecords.All(record => record.IsHiddenComponent),
                MergeManifestAttributes(groupedRecords));
            TreeViewItem item = CreateAssemblyTreeItem(node);
            targetItems.Add(item);
            item.IsExpanded = true;

            if (hasChildren)
            {
                AddAggregatedAssemblyNodes(
                    childrenByParent,
                    parentIds,
                    representative.OccurrenceTag,
                    item.Items,
                    quantity);
            }
        }
    }

    private static bool ReadManifestBool(
        IReadOnlyDictionary<string, string> values,
        string key)
    {
        return values.TryGetValue(key, out string? value) &&
               (string.Equals(value, "true", StringComparison.OrdinalIgnoreCase) || value == "1");
    }

    private static Dictionary<string, string> ParseManifestAttributes(string payload)
    {
        Dictionary<string, string> attributes = new(StringComparer.OrdinalIgnoreCase);
        foreach (string entry in payload.Split('\x1e', StringSplitOptions.RemoveEmptyEntries))
        {
            int separator = entry.IndexOf('\x1f');
            if (separator <= 0)
            {
                continue;
            }

            string name = entry[..separator].Trim();
            if (name.Length == 0 || attributes.ContainsKey(name))
            {
                continue;
            }

            attributes[name] = entry[(separator + 1)..].Trim();
        }

        return attributes;
    }

    private static Dictionary<string, string> MergeManifestAttributes(
        IEnumerable<AssemblyManifestRecord> records)
    {
        Dictionary<string, string> attributes = new(StringComparer.OrdinalIgnoreCase);
        foreach (AssemblyManifestRecord record in records)
        {
            foreach (KeyValuePair<string, string> pair in record.AttributeValues)
            {
                attributes.TryAdd(pair.Key, pair.Value);
            }
        }

        return attributes;
    }

    private static HashSet<string> ReadAssemblyParentIds(string manifestPath)
    {
        HashSet<string> parentIds = new(StringComparer.OrdinalIgnoreCase);
        foreach (string rawLine in File.ReadLines(manifestPath, Encoding.UTF8))
        {
            Dictionary<string, string> values = ReadTabbedKeyValueLine(rawLine);
            if (values.TryGetValue("parent", out string? parentId) &&
                !string.IsNullOrWhiteSpace(parentId) &&
                parentId != "0")
            {
                parentIds.Add(parentId);
            }
        }

        return parentIds;
    }

    private static Dictionary<string, int> CountAssemblyLeafQuantities(string manifestPath)
    {
        List<Dictionary<string, string>> rows = File.ReadLines(manifestPath, Encoding.UTF8)
            .Select(ReadTabbedKeyValueLine)
            .Where(values => values.ContainsKey("id"))
            .ToList();
        HashSet<string> parentIds = rows
            .Select(values => values.TryGetValue("parent", out string? parentId) ? parentId : "")
            .Where(parentId => !string.IsNullOrWhiteSpace(parentId) && parentId != "0")
            .ToHashSet(StringComparer.OrdinalIgnoreCase);
        Dictionary<string, int> quantities = new(StringComparer.OrdinalIgnoreCase);
        foreach (Dictionary<string, string> values in rows)
        {
            string id = values.TryGetValue("id", out string? idText) ? idText : "";
            string parentId = values.TryGetValue("parent", out string? parentText) ? parentText : "";
            if (string.IsNullOrWhiteSpace(id) ||
                parentIds.Contains(id) ||
                IsRootOccurrenceTag(parentId))
            {
                continue;
            }

            values.TryGetValue("name", out string? name);
            values.TryGetValue("part", out string? part);
            string identityKey = BuildAssemblyPartIdentityKey(name ?? "", part ?? "");
            quantities[identityKey] = quantities.TryGetValue(identityKey, out int quantity)
                ? quantity + 1
                : 1;
        }

        return quantities;
    }

    private static string BuildAssemblyPartIdentityKey(string name, string partPath)
    {
        string normalizedPart = (partPath ?? "").Trim().Replace('\\', '/');
        if (!string.IsNullOrWhiteSpace(normalizedPart))
        {
            return "part:" + normalizedPart;
        }

        return "name:" + (name ?? "").Trim();
    }

    private void UpdateAssemblyParentCheckStates()
    {
        List<TreeViewItem> items = EnumerateTreeItems(AssemblyTree.Items).Reverse().ToList();
        foreach (TreeViewItem item in items)
        {
            if (item.Items.Count == 0 || item.Tag is not AssemblyNode node || IsRootAssemblyNode(node))
            {
                continue;
            }

            List<AssemblyNode> childNodes = item.Items
                .OfType<TreeViewItem>()
                .Select(child => child.Tag)
                .OfType<AssemblyNode>()
                .ToList();
            if (childNodes.Count == 0)
            {
                continue;
            }

            SetAssemblyNodeChecked(node, childNodes.All(child => child.IsChecked));
        }
    }

    private static IEnumerable<TreeViewItem> EnumerateTreeItems(ItemCollection items)
    {
        foreach (object item in items)
        {
            if (item is not TreeViewItem treeViewItem)
            {
                continue;
            }

            yield return treeViewItem;
            foreach (TreeViewItem child in EnumerateTreeItems(treeViewItem.Items))
            {
                yield return child;
            }
        }
    }

    private static Dictionary<string, string> ReadTabbedKeyValueLine(string line)
    {
        Dictionary<string, string> values = new(StringComparer.OrdinalIgnoreCase);
        foreach (string part in line.Split('\t'))
        {
            int equalIndex = part.IndexOf('=');
            if (equalIndex <= 0)
            {
                continue;
            }

            string key = part[..equalIndex].Trim();
            string value = UnescapeManifestValue(part[(equalIndex + 1)..].Trim());
            values[key] = value;
        }

        return values;
    }

    private static string UnescapeManifestValue(string value)
    {
        if (value.IndexOf('%') < 0)
        {
            return value;
        }

        List<byte> bytes = new();
        for (int i = 0; i < value.Length; ++i)
        {
            if (value[i] == '%' &&
                i + 2 < value.Length &&
                Uri.IsHexDigit(value[i + 1]) &&
                Uri.IsHexDigit(value[i + 2]))
            {
                string hex = value.Substring(i + 1, 2);
                bytes.Add((byte)Convert.ToInt32(hex, 16));
                i += 2;
            }
            else
            {
                byte[] textBytes = Encoding.UTF8.GetBytes(value[i].ToString());
                bytes.AddRange(textBytes);
            }
        }

        return Encoding.UTF8.GetString(bytes.ToArray());
    }

    private void LoadSavedOptions()
    {
        string settingsPath = GetSettingsPath();
        if (!File.Exists(settingsPath))
        {
            return;
        }

        try
        {
            Dictionary<string, string> values = ReadKeyValueFile(settingsPath);

            FirstAngleRadio.IsChecked = ReadBool(values, "firstAngle", FirstAngleRadio.IsChecked == true);
            ThirdAngleRadio.IsChecked = FirstAngleRadio.IsChecked != true;

            SelectComboBoxByTag(FrontDirectionModeComboBox, ReadText(values, "frontDirectionMode", "largestFaceLongestEdge"));
            ViewSpacingTextBox.Text = ReadText(values, "viewSpacing", ViewSpacingTextBox.Text);
            SheetMarginTextBox.Text = ReadText(values, "sheetMargin", SheetMarginTextBox.Text);

            FrontViewButton.IsChecked = ReadBool(values, "frontSlot", FrontViewButton.IsChecked == true);
            TopViewButton.IsChecked = ReadBool(values, "topSlot", TopViewButton.IsChecked == true);
            BottomViewButton.IsChecked = ReadBool(values, "bottomSlot", BottomViewButton.IsChecked == true);
            LeftPositionViewButton.IsChecked = ReadBool(values, "leftSlot", LeftPositionViewButton.IsChecked == true);
            RightPositionViewButton.IsChecked = ReadBool(values, "rightSlot", RightPositionViewButton.IsChecked == true);
            BackViewButton.IsChecked = ReadBool(values, "backSlot", BackViewButton.IsChecked == true);
            BackBottomViewButton.IsChecked = ReadBool(values, "backBottomSlot", BackBottomViewButton.IsChecked == true);
            AuxAutoCompactCheckBox.IsChecked = ReadBool(values, "auxAutoCompact", AuxAutoCompactCheckBox.IsChecked == true);
            DimensionOverallCheckBox.IsChecked = ReadBool(values, "dimensionOverall", DimensionOverallCheckBox.IsChecked == true);
            DimensionLinearCheckBox.IsChecked = false;
            DimensionAngleCheckBox.IsChecked = ReadBool(values, "dimensionAngle", DimensionAngleCheckBox.IsChecked == true);
            DimensionHoleCheckBox.IsChecked = ReadBool(values, "dimensionHole", DimensionHoleCheckBox.IsChecked == true);
            DimensionHoleLocationCheckBox.IsChecked = ReadBool(values, "dimensionHoleLocation", DimensionHoleLocationCheckBox.IsChecked == true);
            DimensionInnerClosedCurveCheckBox.IsChecked = ReadBool(values, "dimensionInnerClosedCurve", DimensionInnerClosedCurveCheckBox.IsChecked == true);
            TechnicalRequirementEnabledCheckBox.IsChecked = ReadBool(values, "technicalRequirementEnabled", TechnicalRequirementEnabledCheckBox.IsChecked == true);
            TechnicalRequirementIndexCheckBox.IsChecked = ReadBool(values, "technicalRequirementIndexed", TechnicalRequirementIndexCheckBox.IsChecked == true);
            SetCornerButtons(
                true,
                ReadText(values, "technicalRequirementCorner", GetTechnicalRequirementCorner()),
                TechnicalTopLeftButton,
                TechnicalTopRightButton,
                TechnicalBottomLeftButton,
                TechnicalBottomRightButton);
            TechnicalRequirementTextBox.Text = NormalizeTechnicalRequirementText(DecodeText(ReadText(values, "technicalRequirementText", "")));

            SetCornerButtons(
                ReadBool(values, "iso", IsAnyChecked(IsoTopLeftButton, IsoTopRightButton, IsoBottomLeftButton, IsoBottomRightButton)),
                ReadText(values, "isoCorner", GetCorner(IsoTopLeftButton, IsoTopRightButton, IsoBottomLeftButton, IsoBottomRightButton)),
                IsoTopLeftButton,
                IsoTopRightButton,
                IsoBottomLeftButton,
                IsoBottomRightButton);

            SetCornerButtons(
                ReadBool(values, "flat", IsAnyChecked(FlatTopLeftButton, FlatTopRightButton, FlatBottomLeftButton, FlatBottomRightButton)),
                ReadText(values, "flatCorner", GetCorner(FlatTopLeftButton, FlatTopRightButton, FlatBottomLeftButton, FlatBottomRightButton)),
                FlatTopLeftButton,
                FlatTopRightButton,
                FlatBottomLeftButton,
                FlatBottomRightButton);

            EnsureDifferentAuxiliaryCorners();
        }
        catch
        {
            // Ignore a stale settings file and keep the built-in defaults.
        }
    }

    private void SaveOptions()
    {
        try
        {
            EnsureDifferentAuxiliaryCorners();
            Dictionary<string, string> values = new()
            {
                ["firstAngle"] = BoolText(FirstAngleRadio.IsChecked == true),
                ["frontDirectionMode"] = SelectedComboBoxTag(FrontDirectionModeComboBox, "largestFaceLongestEdge"),
                ["viewSpacing"] = ViewSpacingTextBox.Text,
                ["sheetMargin"] = SheetMarginTextBox.Text,
                ["frontSlot"] = BoolText(FrontViewButton.IsChecked == true),
                ["topSlot"] = BoolText(TopViewButton.IsChecked == true),
                ["bottomSlot"] = BoolText(BottomViewButton.IsChecked == true),
                ["leftSlot"] = BoolText(LeftPositionViewButton.IsChecked == true),
                ["rightSlot"] = BoolText(RightPositionViewButton.IsChecked == true),
                ["backSlot"] = BoolText(BackViewButton.IsChecked == true),
                ["backBottomSlot"] = BoolText(BackBottomViewButton.IsChecked == true),
                ["iso"] = BoolText(IsAnyChecked(IsoTopLeftButton, IsoTopRightButton, IsoBottomLeftButton, IsoBottomRightButton)),
                ["flat"] = BoolText(IsAnyChecked(FlatTopLeftButton, FlatTopRightButton, FlatBottomLeftButton, FlatBottomRightButton)),
                ["auxAutoCompact"] = BoolText(AuxAutoCompactCheckBox.IsChecked == true),
                ["isoCorner"] = GetCorner(IsoTopLeftButton, IsoTopRightButton, IsoBottomLeftButton, IsoBottomRightButton),
                ["flatCorner"] = GetCorner(FlatTopLeftButton, FlatTopRightButton, FlatBottomLeftButton, FlatBottomRightButton),
                ["dimensionOverall"] = BoolText(DimensionOverallCheckBox.IsChecked == true),
                ["dimensionLinear"] = BoolText(false),
                ["dimensionAngle"] = BoolText(DimensionAngleCheckBox.IsChecked == true),
                ["dimensionHole"] = BoolText(DimensionHoleCheckBox.IsChecked == true),
                ["dimensionHoleLocation"] = BoolText(DimensionHoleLocationCheckBox.IsChecked == true),
                ["dimensionInnerClosedCurve"] = BoolText(DimensionInnerClosedCurveCheckBox.IsChecked == true),
                ["shareAllPartOptions"] = BoolText(ShareAllPartsOptionsCheckBox.IsChecked == true),
                ["technicalRequirementEnabled"] = BoolText(TechnicalRequirementEnabledCheckBox.IsChecked == true),
                ["technicalRequirementIndexed"] = BoolText(TechnicalRequirementIndexCheckBox.IsChecked == true),
                ["technicalRequirementCorner"] = GetTechnicalRequirementCorner(),
                ["technicalRequirementText"] = EncodeText(NormalizeTechnicalRequirementText(TechnicalRequirementTextBox.Text))
            };

            StringBuilder builder = new();
            foreach (KeyValuePair<string, string> value in values)
            {
                builder.Append(value.Key).Append('=').AppendLine(value.Value);
            }

            File.WriteAllText(GetSettingsPath(), builder.ToString(), new UTF8Encoding(false));
        }
        catch
        {
        }
    }

    private static Dictionary<string, string> ReadKeyValueFile(string path)
    {
        Dictionary<string, string> values = new(StringComparer.OrdinalIgnoreCase);
        foreach (string line in File.ReadLines(path, Encoding.UTF8))
        {
            int equalIndex = line.IndexOf('=');
            if (equalIndex <= 0)
            {
                continue;
            }

            values[line[..equalIndex].Trim()] = line[(equalIndex + 1)..].Trim();
        }

        return values;
    }

    private static void WriteAtomicText(string path, string content)
    {
        string? directory = Path.GetDirectoryName(path);
        if (!string.IsNullOrWhiteSpace(directory))
        {
            Directory.CreateDirectory(directory);
        }

        string tempPath = path + ".tmp";
        File.WriteAllText(tempPath, content, new UTF8Encoding(false));
        if (File.Exists(path))
        {
            File.Delete(path);
        }

        File.Move(tempPath, path);
    }

    private static string ReadText(Dictionary<string, string> values, string key, string fallback)
    {
        return values.TryGetValue(key, out string? value) && !string.IsNullOrWhiteSpace(value)
            ? value
            : fallback;
    }

    private static bool ReadBool(Dictionary<string, string> values, string key, bool fallback)
    {
        if (!values.TryGetValue(key, out string? value))
        {
            return fallback;
        }

        return string.Equals(value, "1", StringComparison.OrdinalIgnoreCase) ||
               string.Equals(value, "true", StringComparison.OrdinalIgnoreCase) ||
               string.Equals(value, "yes", StringComparison.OrdinalIgnoreCase);
    }

    private void LoadTechnicalRequirements()
    {
        TechnicalRequirementTree.Items.Clear();
        string libraryPath = File.Exists(UserTechnicalRequirementsPath)
            ? UserTechnicalRequirementsPath
            : DefaultTechnicalRequirementsPath;

        if (!File.Exists(libraryPath))
        {
            TechnicalRequirementTree.Items.Add(new TreeViewItem
            {
                Header = "未找到插件自带技术要求库",
                IsEnabled = false
            });
            return;
        }

        bool inTechnicalNotes = false;
        TreeViewItem? currentCategory = null;
        foreach (string rawLine in ReadChineseConfigLines(libraryPath))
        {
            string line = rawLine.Trim();
            if (line.Length == 0 || line.StartsWith("!", StringComparison.Ordinal))
            {
                continue;
            }

            if (string.Equals(line, "KEY_WORD TECHNICAL_NOTES_START", StringComparison.OrdinalIgnoreCase))
            {
                inTechnicalNotes = true;
                continue;
            }

            if (string.Equals(line, "KEY_WORD TECHNICAL_NOTES_END", StringComparison.OrdinalIgnoreCase))
            {
                break;
            }

            if (!inTechnicalNotes)
            {
                continue;
            }

            if (line.StartsWith("+", StringComparison.Ordinal))
            {
                if (currentCategory == null)
                {
                    continue;
                }

                string note = line[1..].Trim();
                if (note.Length == 0)
                {
                    continue;
                }

                currentCategory.Items.Add(new TreeViewItem
                {
                    Header = note,
                    Tag = new TechnicalRequirementNode(note, true)
                });
                continue;
            }

            currentCategory = new TreeViewItem
            {
                Header = line,
                Tag = new TechnicalRequirementNode(line, false),
                IsExpanded = false
            };
            TechnicalRequirementTree.Items.Add(currentCategory);
        }
    }

    private static string[] ReadChineseConfigLines(string path)
    {
        byte[] bytes = File.ReadAllBytes(path);
        Encoding encoding = DetectConfigEncoding(bytes);
        string text = encoding.GetString(bytes);
        return text.Replace("\r\n", "\n", StringComparison.Ordinal)
            .Replace('\r', '\n')
            .Split('\n');
    }

    private static Encoding DetectConfigEncoding(byte[] bytes)
    {
        if (bytes.Length >= 3 &&
            bytes[0] == 0xEF &&
            bytes[1] == 0xBB &&
            bytes[2] == 0xBF)
        {
            return new UTF8Encoding(encoderShouldEmitUTF8Identifier: true, throwOnInvalidBytes: true);
        }

        UTF8Encoding strictUtf8 = new(encoderShouldEmitUTF8Identifier: false, throwOnInvalidBytes: true);
        try
        {
            strictUtf8.GetString(bytes);
            return strictUtf8;
        }
        catch (DecoderFallbackException)
        {
            return Encoding.GetEncoding("GB18030");
        }
    }

    private void TechnicalRequirementTree_SelectedItemChanged(object sender, RoutedPropertyChangedEventArgs<object> e)
    {
        if (e.NewValue is not TreeViewItem { Tag: TechnicalRequirementNode node })
        {
            return;
        }

        TechnicalRequirementEditTextBox.Text = node.Text;
        if (node.IsDetail)
        {
            AppendTechnicalRequirement(node.Text);
        }
    }

    private void AppendTechnicalRequirement(string note)
    {
        string trimmedNote = note.Trim();
        if (trimmedNote.Length == 0)
        {
            return;
        }

        List<string> details = ExtractTechnicalRequirementDetails(TechnicalRequirementTextBox.Text);

        if (details.Any(line => string.Equals(RemoveLeadingIndex(line), trimmedNote, StringComparison.Ordinal)))
        {
            return;
        }

        string detailToAppend = TechnicalRequirementIndexCheckBox.IsChecked == true
            ? $"{details.Count + 1}. {trimmedNote}"
            : trimmedNote;

        details.Add(detailToAppend);
        TechnicalRequirementTextBox.Text = BuildTechnicalRequirementText(details);
        TechnicalRequirementTextBox.CaretIndex = TechnicalRequirementTextBox.Text.Length;
        TechnicalRequirementTextBox.ScrollToEnd();
    }

    private void AddTechnicalCategoryButton_Click(object sender, RoutedEventArgs e)
    {
        string text = TechnicalRequirementEditTextBox.Text.Trim();
        if (text.Length == 0)
        {
            return;
        }

        TreeViewItem item = CreateTechnicalRequirementTreeItem(text, false);
        item.IsExpanded = true;
        TechnicalRequirementTree.Items.Add(item);
        item.IsSelected = true;
    }

    private void AddTechnicalNoteButton_Click(object sender, RoutedEventArgs e)
    {
        string text = TechnicalRequirementEditTextBox.Text.Trim();
        if (text.Length == 0)
        {
            return;
        }

        TreeViewItem? category = GetSelectedTechnicalCategory();
        if (category == null)
        {
            MessageBox.Show(this, "请先选择一个分类。", "AutoCreateThreeViews", MessageBoxButton.OK, MessageBoxImage.Information);
            return;
        }

        TreeViewItem item = CreateTechnicalRequirementTreeItem(text, true);
        category.Items.Add(item);
        category.IsExpanded = true;
        item.IsSelected = true;
    }

    private void UpdateTechnicalRequirementButton_Click(object sender, RoutedEventArgs e)
    {
        if (TechnicalRequirementTree.SelectedItem is not TreeViewItem { Tag: TechnicalRequirementNode node } selected)
        {
            return;
        }

        string text = TechnicalRequirementEditTextBox.Text.Trim();
        if (text.Length == 0)
        {
            return;
        }

        node.Text = text;
        selected.Header = text;
    }

    private void DeleteTechnicalRequirementButton_Click(object sender, RoutedEventArgs e)
    {
        if (TechnicalRequirementTree.SelectedItem is not TreeViewItem selected)
        {
            return;
        }

        ItemsControl? parent = ItemsControl.ItemsControlFromItemContainer(selected);
        if (parent == null)
        {
            return;
        }

        parent.Items.Remove(selected);
    }

    private void SaveTechnicalLibraryButton_Click(object sender, RoutedEventArgs e)
    {
        try
        {
            SaveTechnicalRequirementLibrary();
            MessageBox.Show(this, "技术要求库已保存。", "AutoCreateThreeViews", MessageBoxButton.OK, MessageBoxImage.Information);
        }
        catch (Exception ex)
        {
            MessageBox.Show(this, "技术要求库保存失败：" + ex.Message, "AutoCreateThreeViews", MessageBoxButton.OK, MessageBoxImage.Error);
        }
    }

    private static TreeViewItem CreateTechnicalRequirementTreeItem(string text, bool isDetail)
    {
        return new TreeViewItem
        {
            Header = text,
            Tag = new TechnicalRequirementNode(text, isDetail),
            IsExpanded = !isDetail
        };
    }

    private TreeViewItem? GetSelectedTechnicalCategory()
    {
        if (TechnicalRequirementTree.SelectedItem is TreeViewItem selected)
        {
            if (selected.Tag is TechnicalRequirementNode { IsDetail: false })
            {
                return selected;
            }

            ItemsControl? parent = ItemsControl.ItemsControlFromItemContainer(selected);
            if (parent is TreeViewItem parentTreeItem &&
                parentTreeItem.Tag is TechnicalRequirementNode { IsDetail: false })
            {
                return parentTreeItem;
            }
        }

        return TechnicalRequirementTree.Items.OfType<TreeViewItem>().FirstOrDefault();
    }

    private void SaveTechnicalRequirementLibrary()
    {
        Directory.CreateDirectory(Path.GetDirectoryName(UserTechnicalRequirementsPath)!);
        File.WriteAllText(
            UserTechnicalRequirementsPath,
            BuildTechnicalRequirementLibraryText(),
            new UTF8Encoding(false));
    }

    private string BuildTechnicalRequirementLibraryText()
    {
        StringBuilder builder = new();
        builder.AppendLine("KEY_WORD TECHNICAL_NOTES_START");

        foreach (TreeViewItem categoryItem in TechnicalRequirementTree.Items.OfType<TreeViewItem>())
        {
            if (categoryItem.Tag is not TechnicalRequirementNode categoryNode ||
                categoryNode.IsDetail ||
                string.IsNullOrWhiteSpace(categoryNode.Text))
            {
                continue;
            }

            builder.AppendLine(categoryNode.Text.Trim());
            foreach (TreeViewItem detailItem in categoryItem.Items.OfType<TreeViewItem>())
            {
                if (detailItem.Tag is TechnicalRequirementNode { IsDetail: true } detailNode &&
                    !string.IsNullOrWhiteSpace(detailNode.Text))
                {
                    builder.Append('+').AppendLine(detailNode.Text.Trim());
                }
            }
        }

        builder.AppendLine("KEY_WORD TECHNICAL_NOTES_END");
        return builder.ToString();
    }

    private static string NormalizeTechnicalRequirementText(string text)
    {
        return BuildTechnicalRequirementText(ExtractTechnicalRequirementDetails(text));
    }

    private static string BuildTechnicalRequirementText(IReadOnlyList<string> details)
    {
        List<string> cleanDetails = details
            .Select(line => line.Trim())
            .Where(line => line.Length > 0 && !string.Equals(line, "技术要求", StringComparison.Ordinal))
            .ToList();

        if (cleanDetails.Count == 0)
        {
            return "";
        }

        return "技术要求" + Environment.NewLine + string.Join(Environment.NewLine, cleanDetails);
    }

    private static List<string> ExtractTechnicalRequirementDetails(string text)
    {
        return text
            .Replace("\r\n", "\n", StringComparison.Ordinal)
            .Replace('\r', '\n')
            .Split('\n')
            .Select(line => line.Trim())
            .Where(line => line.Length > 0 && !string.Equals(line, "技术要求", StringComparison.Ordinal))
            .ToList();
    }

    private static string RemoveLeadingIndex(string text)
    {
        int dotIndex = text.IndexOf('.');
        if (dotIndex > 0 && text[..dotIndex].All(char.IsDigit))
        {
            return text[(dotIndex + 1)..].Trim();
        }

        return text;
    }

    private static string EncodeText(string text)
    {
        return Convert.ToBase64String(Encoding.UTF8.GetBytes(text));
    }

    private static string DecodeText(string encodedText)
    {
        if (string.IsNullOrWhiteSpace(encodedText))
        {
            return "";
        }

        try
        {
            return Encoding.UTF8.GetString(Convert.FromBase64String(encodedText));
        }
        catch
        {
            return encodedText;
        }
    }

    private static void SetCornerButtons(
        bool isEnabled,
        string corner,
        ToggleButton topLeftButton,
        ToggleButton topRightButton,
        ToggleButton bottomLeftButton,
        ToggleButton bottomRightButton)
    {
        topLeftButton.IsChecked = isEnabled && string.Equals(corner, "TopLeft", StringComparison.OrdinalIgnoreCase);
        topRightButton.IsChecked = isEnabled && string.Equals(corner, "TopRight", StringComparison.OrdinalIgnoreCase);
        bottomLeftButton.IsChecked = isEnabled && string.Equals(corner, "BottomLeft", StringComparison.OrdinalIgnoreCase);
        bottomRightButton.IsChecked = isEnabled && string.Equals(corner, "BottomRight", StringComparison.OrdinalIgnoreCase);
    }

    private string GetTechnicalRequirementCorner()
    {
        return GetCorner(
            TechnicalTopLeftButton,
            TechnicalTopRightButton,
            TechnicalBottomLeftButton,
            TechnicalBottomRightButton);
    }

    private static string BoolText(bool value)
    {
        return value ? "1" : "0";
    }

    private static bool IsTruthy(string value)
    {
        return string.Equals(value, "1", StringComparison.OrdinalIgnoreCase) ||
               string.Equals(value, "true", StringComparison.OrdinalIgnoreCase) ||
               string.Equals(value, "yes", StringComparison.OrdinalIgnoreCase);
    }

    private static int ParseInt(string value, int fallback)
    {
        return int.TryParse(value, out int parsed) ? parsed : fallback;
    }

    private bool AnyDimensionEnabled()
    {
        return DimensionOverallCheckBox.IsChecked == true ||
               DimensionAngleCheckBox.IsChecked == true ||
               DimensionHoleCheckBox.IsChecked == true ||
               DimensionHoleLocationCheckBox.IsChecked == true ||
               DimensionInnerClosedCurveCheckBox.IsChecked == true;
    }

    private string SelectedTemplatePath()
    {
        if (TemplateComboBox.SelectedItem is ComboBoxItem item && item.Tag is string path)
        {
            return path;
        }

        return "";
    }

    private static string GetRequestPath()
    {
        string? commandLinePath = GetCommandLineValue("--request");

        if (!string.IsNullOrWhiteSpace(commandLinePath))
        {
            return commandLinePath;
        }

        return Path.Combine(AppContext.BaseDirectory, "AutoCreateThreeViews.request");
    }

    private static string GetProgressPath()
    {
        string requestPath = GetRequestPath();
        string? directory = Path.GetDirectoryName(requestPath);
        return Path.Combine(directory ?? AppContext.BaseDirectory, "AutoCreateThreeViews.progress");
    }

    private static string GetAssemblyManifestPath()
    {
        string? commandLinePath = GetCommandLineValue("--assembly");
        return string.IsNullOrWhiteSpace(commandLinePath)
            ? Path.Combine(AppContext.BaseDirectory, "AutoCreateThreeViews.assembly")
            : commandLinePath;
    }

    private static string GetDisplayCommandPath()
    {
        string? commandLinePath = GetCommandLineValue("--display");
        return string.IsNullOrWhiteSpace(commandLinePath)
            ? Path.Combine(AppContext.BaseDirectory, "AutoCreateThreeViews.display")
            : commandLinePath;
    }

    private static string? GetCommandLineValue(string name)
    {
        string[] args = Environment.GetCommandLineArgs();
        for (int i = 0; i < args.Length - 1; ++i)
        {
            if (string.Equals(args[i], name, StringComparison.OrdinalIgnoreCase))
            {
                return args[i + 1];
            }
        }

        return null;
    }

    private static string GetSettingsPath()
    {
        return Path.Combine(GetConfigDirectory(), "AutoCreateThreeViews.settings");
    }

    private static string GetConfigDirectory()
    {
        return @"D:\UG智辉钣金插件\config";
    }

    private sealed class TechnicalRequirementNode
    {
        public TechnicalRequirementNode(string text, bool isDetail)
        {
            Text = text;
            IsDetail = isDetail;
        }

        public string Text { get; set; }

        public bool IsDetail { get; }
    }

    private sealed class AssemblyManifestRecord
    {
        public AssemblyManifestRecord(
            string occurrenceTag,
            string parentOccurrenceTag,
            string name,
            string partPath,
            bool isAssembly,
            bool hasDrawingSheets,
            bool isSheetMetal,
            bool isHiddenComponent,
            IReadOnlyDictionary<string, string> attributeValues)
        {
            OccurrenceTag = occurrenceTag;
            ParentOccurrenceTag = parentOccurrenceTag;
            Name = name;
            PartPath = partPath;
            IsAssembly = isAssembly;
            HasDrawingSheets = hasDrawingSheets;
            IsSheetMetal = isSheetMetal;
            IsHiddenComponent = isHiddenComponent;
            AttributeValues = attributeValues;
        }

        public string OccurrenceTag { get; }

        public string ParentOccurrenceTag { get; }

        public string Name { get; }

        public string PartPath { get; }

        public bool IsAssembly { get; }

        public bool HasDrawingSheets { get; }

        public bool IsSheetMetal { get; }

        public bool IsHiddenComponent { get; }

        public IReadOnlyDictionary<string, string> AttributeValues { get; }
    }

    private sealed class AssemblyNode
    {
        public AssemblyNode(
            string occurrenceTag,
            string parentOccurrenceTag,
            string name,
            string partPath,
            int quantity = 1,
            bool hasChildren = false,
            bool isAssembly = false,
            bool hasDrawingSheets = false,
            bool isSheetMetal = false,
            bool isHiddenComponent = false,
            IReadOnlyDictionary<string, string>? attributeValues = null)
        {
            OccurrenceTag = occurrenceTag;
            ParentOccurrenceTag = parentOccurrenceTag;
            Name = name;
            PartPath = partPath;
            Quantity = Math.Max(1, quantity);
            HasChildren = hasChildren;
            FilterTarget = new BatchTargetFilterTarget
            {
                DisplayName = name,
                FilePath = partPath,
                IsAssembly = isAssembly || hasChildren,
                HasDrawingSheets = hasDrawingSheets,
                IsSheetMetal = isSheetMetal,
                IsHiddenComponent = isHiddenComponent,
                AttributeValues = attributeValues ??
                    new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
            };
        }

        public string OccurrenceTag { get; }

        public string ParentOccurrenceTag { get; }

        public string Name { get; }

        public string PartPath { get; }

        public int Quantity { get; }

        public bool HasChildren { get; }

        public BatchTargetFilterTarget FilterTarget { get; }

        public bool IsChecked { get; set; }

        public CheckBox? CheckBox { get; set; }

        public TreeViewItem? TreeItem { get; set; }
    }

    private sealed record PreviewItem(FrameworkElement Element, double Left, double Top, double Width, double Height);
}
