using System.Collections.ObjectModel;
using System.Globalization;
using System.IO;
using System.Text.Encodings.Web;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Text.Unicode;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;

namespace MinXiBiaoUI;

public partial class MainWindow : Window
{
    private const int AttributeColumnCount = 8;
    private const string EmptyText = "<空>";

    private readonly List<ComboBox> headerCombos = new();
    private readonly List<BodyRecord> bodies = new();
    private readonly ObservableCollection<PreviewRow> previewRows = new();
    private readonly ObservableCollection<string> availableAttributes = new();
    private string outputPath = "";
    private bool updating;
    private string headerLocation = "above";
    private double textHeight = 3.5;

    public MainWindow()
    {
        InitializeComponent();
        Loaded += MainWindow_Loaded;
    }

    private List<string> SelectedColumns { get; set; } = Enumerable.Repeat("", AttributeColumnCount).ToList();
    private List<string> HeaderTitles { get; set; } = DefaultHeaderTitles();

    private static string SettingsPath => Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
        "UGZhihui",
        "MinXiBiaoUI.settings.json");

    private static List<string> DefaultHeaderTitles()
    {
        return new[] { "编号", "材料", "数量", "备注" }
            .Concat(Enumerable.Repeat("", AttributeColumnCount))
            .Take(AttributeColumnCount)
            .ToList();
    }

    private void MainWindow_Loaded(object sender, RoutedEventArgs e)
    {
        try
        {
            string inputPath = GetArgumentValue("--input");
            outputPath = GetArgumentValue("--output");
            if (string.IsNullOrWhiteSpace(inputPath) || string.IsNullOrWhiteSpace(outputPath))
            {
                throw new InvalidOperationException("Missing --input or --output.");
            }

            LoadInput(inputPath);
            LoadSettings();
            ApplyOptionControls();
            RefreshPreview();
            PreviewGrid.PreviewKeyDown += PreviewGrid_PreviewKeyDown;
        }
        catch (Exception ex)
        {
            MessageBox.Show(this, ex.Message, "工程图明细表", MessageBoxButton.OK, MessageBoxImage.Error);
            Close();
        }
    }

    private static string GetArgumentValue(string name)
    {
        string[] args = Environment.GetCommandLineArgs();
        for (int i = 0; i + 1 < args.Length; i++)
        {
            if (string.Equals(args[i], name, StringComparison.OrdinalIgnoreCase))
            {
                return args[i + 1];
            }
        }
        return "";
    }

    private void LoadInput(string inputPath)
    {
        string json = File.ReadAllText(inputPath);
        UiInput input = JsonSerializer.Deserialize<UiInput>(json) ?? new UiInput();

        availableAttributes.Clear();
        availableAttributes.Add(EmptyText);
        foreach (string name in input.Attributes.Where(static n => !string.IsNullOrWhiteSpace(n)).Distinct(StringComparer.OrdinalIgnoreCase))
        {
            availableAttributes.Add(name);
        }

        bodies.Clear();
        bodies.AddRange(input.Bodies);

        while (input.SelectedColumns.Count < AttributeColumnCount)
        {
            input.SelectedColumns.Add("");
        }

        SelectedColumns = input.SelectedColumns.Take(AttributeColumnCount).ToList();
        StatusText.Text = $"{bodies.Count} 个实体，{Math.Max(0, availableAttributes.Count - 1)} 个可用属性";
    }

    private void LoadSettings()
    {
        try
        {
            if (!File.Exists(SettingsPath))
            {
                return;
            }

            UiSettings settings = JsonSerializer.Deserialize<UiSettings>(File.ReadAllText(SettingsPath)) ?? new UiSettings();
            while (settings.SelectedColumns.Count < AttributeColumnCount)
            {
                settings.SelectedColumns.Add("");
            }
            while (settings.HeaderTitles.Count < AttributeColumnCount)
            {
                settings.HeaderTitles.Add("");
            }

            SelectedColumns = settings.SelectedColumns.Take(AttributeColumnCount).ToList();
            HeaderTitles = settings.HeaderTitles.Take(AttributeColumnCount).ToList();
            headerLocation = string.Equals(settings.HeaderLocation, "below", StringComparison.OrdinalIgnoreCase) ? "below" : "above";
            if (settings.TextHeight > 0.1)
            {
                textHeight = settings.TextHeight;
            }
            if (HeaderTitles.All(static title => string.IsNullOrWhiteSpace(title)))
            {
                HeaderTitles = DefaultHeaderTitles();
            }
        }
        catch
        {
            HeaderTitles = DefaultHeaderTitles();
        }
    }

    private void SaveSettings()
    {
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(SettingsPath)!);
            UiSettings settings = new()
            {
                SelectedColumns = SelectedColumns.Take(AttributeColumnCount).ToList(),
                HeaderTitles = HeaderTitles.Take(AttributeColumnCount).ToList(),
                HeaderLocation = headerLocation,
                TextHeight = textHeight
            };
            JsonSerializerOptions options = new()
            {
                WriteIndented = true,
                Encoder = JavaScriptEncoder.Create(UnicodeRanges.All)
            };
            File.WriteAllText(SettingsPath, JsonSerializer.Serialize(settings, options));
        }
        catch
        {
        }
    }

    private void ApplyOptionControls()
    {
        updating = true;
        HeaderLocationCombo.SelectedIndex = string.Equals(headerLocation, "below", StringComparison.OrdinalIgnoreCase) ? 1 : 0;
        TextHeightBox.Text = textHeight.ToString("0.###", CultureInfo.InvariantCulture);
        updating = false;
    }

    private void HeaderCombo_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        if (updating || sender is not ComboBox combo || combo.Tag is not int index)
        {
            return;
        }

        string value = combo.SelectedItem as string ?? "";
        SelectedColumns[index] = value == EmptyText ? "" : value;
        RefreshPreview();
        RefreshAvailableAttributesFromPreview();
    }

    private static string FormatDecimalText(string text)
    {
        string trimmed = text.Trim();
        if (!trimmed.Contains('.') ||
            !double.TryParse(trimmed, NumberStyles.Float, CultureInfo.InvariantCulture, out double value))
        {
            return text;
        }
        return value.ToString("0.0", CultureInfo.InvariantCulture);
    }

    private void RefreshPreview()
    {
        previewRows.Clear();

        PreviewGrid.Columns.Clear();
        headerCombos.Clear();
        PreviewGrid.Columns.Add(new DataGridTextColumn
        {
            Header = "序号",
            Binding = new System.Windows.Data.Binding(nameof(PreviewRow.Sequence)),
            Width = new DataGridLength(64),
            ElementStyle = (Style)FindResource("NoWrapCellText"),
            IsReadOnly = true
        });

        for (int i = 0; i < AttributeColumnCount; i++)
        {
            int columnIndex = i;
            PreviewGrid.Columns.Add(new DataGridTextColumn
            {
                Header = BuildColumnHeader(i),
                Binding = new System.Windows.Data.Binding($"Values[{columnIndex}]"),
                Width = DataGridLength.Auto,
                MinWidth = 120,
                ElementStyle = (Style)FindResource("NoWrapCellText"),
                IsReadOnly = true
            });
        }

        int sequence = 1;
        foreach (BodyRecord body in bodies)
        {
            if (!ShouldIncludeBody(body))
            {
                continue;
            }

            PreviewRow row = new()
            {
                BodyName = body.Name
            };
            for (int i = 0; i < AttributeColumnCount; i++)
            {
                string name = SelectedColumns[i];
                row.Values.Add(string.IsNullOrWhiteSpace(name) ? "" : FormatDecimalText(body.Attributes.GetValueOrDefault(name, "")));
            }

            row.Sequence = sequence.ToString();
            previewRows.Add(row);
            sequence++;
        }

        PreviewGrid.ItemsSource = previewRows;
        UpdateStatus();
    }

    private StackPanel BuildColumnHeader(int index)
    {
        StackPanel panel = new()
        {
            Orientation = Orientation.Vertical,
            Margin = new Thickness(0),
            MinWidth = 120
        };
        panel.Children.Add(BuildEditableHeader(index));

        ComboBox combo = new()
        {
            ItemsSource = availableAttributes,
            SelectedItem = string.IsNullOrWhiteSpace(SelectedColumns[index]) ? EmptyText : SelectedColumns[index],
            Tag = index,
            IsEditable = false,
            MinWidth = 90,
            Height = 24,
            Margin = new Thickness(0, 2, 0, 0)
        };
        combo.SelectionChanged += HeaderCombo_SelectionChanged;
        headerCombos.Add(combo);
        panel.Children.Add(combo);
        return panel;
    }

    private TextBox BuildEditableHeader(int index)
    {
        TextBox textBox = new()
        {
            Text = GetHeaderDisplayText(index),
            Tag = index,
            BorderThickness = new Thickness(0),
            MinWidth = 116,
            Padding = new Thickness(2, 0, 2, 0),
            Background = System.Windows.Media.Brushes.Transparent,
            VerticalContentAlignment = VerticalAlignment.Center
        };
        textBox.Foreground = string.IsNullOrWhiteSpace(HeaderTitles[index])
            ? System.Windows.Media.Brushes.Gray
            : System.Windows.Media.Brushes.Black;
        textBox.GotKeyboardFocus += HeaderTitle_GotKeyboardFocus;
        textBox.LostKeyboardFocus += HeaderTitle_LostKeyboardFocus;
        textBox.TextChanged += HeaderTitle_TextChanged;
        return textBox;
    }

    private string GetHeaderDisplayText(int index)
    {
        if (index < 0 || index >= HeaderTitles.Count)
        {
            return EmptyText;
        }
        return string.IsNullOrWhiteSpace(HeaderTitles[index]) ? EmptyText : HeaderTitles[index];
    }

    private void HeaderTitle_GotKeyboardFocus(object sender, KeyboardFocusChangedEventArgs e)
    {
        if (sender is TextBox textBox && string.Equals(textBox.Text, EmptyText, StringComparison.Ordinal))
        {
            textBox.Text = "";
        }
    }

    private void HeaderTitle_LostKeyboardFocus(object sender, KeyboardFocusChangedEventArgs e)
    {
        if (sender is TextBox textBox)
        {
            textBox.Text = string.IsNullOrWhiteSpace(textBox.Text) ? EmptyText : textBox.Text;
        }
    }

    private void HeaderTitle_TextChanged(object sender, TextChangedEventArgs e)
    {
        if (updating || sender is not TextBox textBox || textBox.Tag is not int index || index < 0 || index >= HeaderTitles.Count)
        {
            return;
        }

        HeaderTitles[index] = string.Equals(textBox.Text, EmptyText, StringComparison.Ordinal) ? "" : textBox.Text;
        textBox.Foreground = string.IsNullOrWhiteSpace(HeaderTitles[index])
            ? System.Windows.Media.Brushes.Gray
            : System.Windows.Media.Brushes.Black;
        UpdateStatus();
    }

    private bool IsActiveColumn(int index)
    {
        return index >= 0 &&
               index < SelectedColumns.Count &&
               index < HeaderTitles.Count &&
               !string.IsNullOrWhiteSpace(HeaderTitles[index]);
    }

    private bool HasTitleColumn(int index)
    {
        return index >= 0 &&
               index < HeaderTitles.Count &&
               !string.IsNullOrWhiteSpace(HeaderTitles[index]);
    }

    private void RefreshAvailableAttributesFromPreview()
    {
        if (updating)
        {
            return;
        }

        HashSet<string> names = new(StringComparer.OrdinalIgnoreCase);
        foreach (BodyRecord body in bodies.Where(ShouldIncludeBody))
        {
            foreach (KeyValuePair<string, string> attribute in body.Attributes)
            {
                if (!string.IsNullOrWhiteSpace(attribute.Key) && !string.IsNullOrWhiteSpace(attribute.Value))
                {
                    names.Add(attribute.Key);
                }
            }
        }

        foreach (string selected in SelectedColumns.Where(static name => !string.IsNullOrWhiteSpace(name)))
        {
            names.Add(selected);
        }

        updating = true;
        availableAttributes.Clear();
        availableAttributes.Add(EmptyText);
        foreach (string name in names.OrderBy(static name => name, StringComparer.CurrentCultureIgnoreCase))
        {
            availableAttributes.Add(name);
        }

        for (int i = 0; i < headerCombos.Count && i < SelectedColumns.Count; i++)
        {
            headerCombos[i].ItemsSource = null;
            headerCombos[i].ItemsSource = availableAttributes;
            headerCombos[i].SelectedItem = string.IsNullOrWhiteSpace(SelectedColumns[i]) ? EmptyText : SelectedColumns[i];
        }
        updating = false;
    }

    private bool ShouldIncludeBody(BodyRecord body)
    {
        for (int index = 0; index < AttributeColumnCount; index++)
        {
            if (!IsActiveColumn(index))
            {
                continue;
            }

            string name = SelectedColumns[index];
            if (!string.IsNullOrWhiteSpace(name) &&
                body.Attributes.TryGetValue(name, out string? value) &&
                !string.IsNullOrWhiteSpace(value))
            {
                return true;
            }
        }
        return false;
    }

    private void OkButton_Click(object sender, RoutedEventArgs e)
    {
        try
        {
            if (!Enumerable.Range(0, AttributeColumnCount).Any(IsActiveColumn))
            {
                MessageBox.Show(this, "请至少设置一个标题列。", "工程图明细表", MessageBoxButton.OK, MessageBoxImage.Information);
                return;
            }

            UiOutput output = new()
            {
                Confirmed = true,
                SelectedColumns = SelectedColumns,
                HeaderTitles = HeaderTitles,
                HeaderLocation = headerLocation,
                TextHeight = textHeight,
                IncludedBodyNames = previewRows.Select(static row => row.BodyName).Where(static name => !string.IsNullOrWhiteSpace(name)).ToList()
            };

            JsonSerializerOptions options = new()
            {
                WriteIndented = true,
                Encoder = JavaScriptEncoder.Create(UnicodeRanges.All)
            };
            File.WriteAllText(outputPath, JsonSerializer.Serialize(output, options));
            SaveSettings();
            CloseAndShutdown();
        }
        catch (Exception ex)
        {
            MessageBox.Show(this, ex.Message, "工程图明细表", MessageBoxButton.OK, MessageBoxImage.Error);
        }
    }

    private void HeaderLocationCombo_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        if (updating || HeaderLocationCombo.SelectedItem is not ComboBoxItem item)
        {
            return;
        }

        headerLocation = item.Tag as string == "below" ? "below" : "above";
    }

    private void TextHeightBox_TextChanged(object sender, TextChangedEventArgs e)
    {
        if (updating)
        {
            return;
        }

        if (double.TryParse(TextHeightBox.Text, NumberStyles.Float, CultureInfo.InvariantCulture, out double value) ||
            double.TryParse(TextHeightBox.Text, NumberStyles.Float, CultureInfo.CurrentCulture, out value))
        {
            textHeight = Math.Clamp(value, 1.0, 20.0);
        }
    }

    private void CancelButton_Click(object sender, RoutedEventArgs e)
    {
        UiOutput output = new() { Confirmed = false };
        File.WriteAllText(outputPath, JsonSerializer.Serialize(output));
        CloseAndShutdown();
    }

    private void DeleteRowsButton_Click(object sender, RoutedEventArgs e)
    {
        DeleteSelectedRows();
    }

    private void PreviewGrid_PreviewKeyDown(object sender, KeyEventArgs e)
    {
        if (e.Key == Key.Delete)
        {
            DeleteSelectedRows();
            e.Handled = true;
        }
    }

    private void DeleteSelectedRows()
    {
        List<PreviewRow> selectedRows = PreviewGrid.SelectedItems.OfType<PreviewRow>().ToList();
        foreach (PreviewRow row in selectedRows)
        {
            previewRows.Remove(row);
        }
        RenumberRows();
        UpdateStatus();
    }

    private void RenumberRows()
    {
        for (int i = 0; i < previewRows.Count; i++)
        {
            previewRows[i].Sequence = (i + 1).ToString();
        }
        PreviewGrid.Items.Refresh();
    }

    private void UpdateStatus()
    {
        StatusText.Text = $"{previewRows.Count} 行明细，{Enumerable.Range(0, AttributeColumnCount).Count(HasTitleColumn)} 个标题列";
    }

    protected override void OnClosed(EventArgs e)
    {
        if (!string.IsNullOrWhiteSpace(outputPath) && !File.Exists(outputPath))
        {
            File.WriteAllText(outputPath, JsonSerializer.Serialize(new UiOutput { Confirmed = false }));
        }
        base.OnClosed(e);
        Application.Current.Shutdown();
    }

    private void CloseAndShutdown()
    {
        Close();
        Application.Current.Shutdown();
    }
}

public sealed class UiInput
{
    [JsonPropertyName("attributes")]
    public List<string> Attributes { get; set; } = new();

    [JsonPropertyName("selectedColumns")]
    public List<string> SelectedColumns { get; set; } = new();

    [JsonPropertyName("bodies")]
    public List<BodyRecord> Bodies { get; set; } = new();
}

public sealed class BodyRecord
{
    [JsonPropertyName("name")]
    public string Name { get; set; } = "";

    [JsonPropertyName("attributes")]
    public Dictionary<string, string> Attributes { get; set; } = new(StringComparer.OrdinalIgnoreCase);
}

public sealed class PreviewRow
{
    public string Sequence { get; set; } = "";
    public string BodyName { get; set; } = "";
    public ObservableCollection<string> Values { get; } = new();
}

public sealed class UiOutput
{
    [JsonPropertyName("confirmed")]
    public bool Confirmed { get; set; }

    [JsonPropertyName("selectedColumns")]
    public List<string> SelectedColumns { get; set; } = new();

    [JsonPropertyName("headerTitles")]
    public List<string> HeaderTitles { get; set; } = new();

    [JsonPropertyName("headerLocation")]
    public string HeaderLocation { get; set; } = "above";

    [JsonPropertyName("textHeight")]
    public double TextHeight { get; set; } = 3.5;

    [JsonPropertyName("includedBodyNames")]
    public List<string> IncludedBodyNames { get; set; } = new();
}

public sealed class UiSettings
{
    [JsonPropertyName("selectedColumns")]
    public List<string> SelectedColumns { get; set; } = new();

    [JsonPropertyName("headerTitles")]
    public List<string> HeaderTitles { get; set; } = new();

    [JsonPropertyName("headerLocation")]
    public string HeaderLocation { get; set; } = "above";

    [JsonPropertyName("textHeight")]
    public double TextHeight { get; set; } = 3.5;
}
