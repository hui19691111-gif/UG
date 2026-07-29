using System.Windows;
using System.Windows.Controls;

namespace AutoCreateThreeViewsUI;

internal enum BatchTargetFilterKind
{
    RemovePart,
    RemoveAssembly,
    RemoveWithDrawingSheets,
    RemoveWithoutDrawingSheets,
    RemoveSheetMetal,
    RemoveNonSheetMetal,
    RemoveHiddenComponent,
    RemoveKeywordMatches,
    RemoveKeywordNonMatches,
    RemoveHasAttribute,
    RemoveMissingAttribute,
    RemoveAttributeEquals,
    RemoveWithoutAttributeValue
}

internal sealed class BatchTargetFilterTarget
{
    public string DisplayName { get; init; } = "";

    public string FilePath { get; init; } = "";

    public bool IsAssembly { get; init; }

    public bool HasDrawingSheets { get; init; }

    public bool IsSheetMetal { get; init; }

    public bool IsHiddenComponent { get; init; }

    public IReadOnlyDictionary<string, string> AttributeValues { get; init; } =
        new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

    public string KeywordText
    {
        get
        {
            List<string> values = [DisplayName, FilePath];
            foreach (KeyValuePair<string, string> pair in AttributeValues)
            {
                values.Add(pair.Key);
                values.Add(pair.Value);
            }

            return string.Join('\n', values);
        }
    }
}

internal sealed class BatchTargetFilterRule
{
    public BatchTargetFilterKind Kind { get; init; }

    public string Keyword { get; init; } = "";

    public string AttributeName { get; init; } = "";

    public string AttributeValue { get; init; } = "";
}

internal static class BatchTargetFilterEngine
{
    public static bool ShouldRemove(BatchTargetFilterTarget target, BatchTargetFilterRule rule)
    {
        return rule.Kind switch
        {
            BatchTargetFilterKind.RemovePart => !target.IsAssembly && !target.IsSheetMetal,
            BatchTargetFilterKind.RemoveAssembly => target.IsAssembly,
            BatchTargetFilterKind.RemoveWithDrawingSheets => target.HasDrawingSheets,
            BatchTargetFilterKind.RemoveWithoutDrawingSheets => !target.HasDrawingSheets,
            BatchTargetFilterKind.RemoveSheetMetal => target.IsSheetMetal,
            BatchTargetFilterKind.RemoveNonSheetMetal => !target.IsSheetMetal,
            BatchTargetFilterKind.RemoveHiddenComponent => target.IsHiddenComponent,
            BatchTargetFilterKind.RemoveKeywordMatches => ContainsKeyword(target, rule.Keyword),
            BatchTargetFilterKind.RemoveKeywordNonMatches => !ContainsKeyword(target, rule.Keyword),
            BatchTargetFilterKind.RemoveHasAttribute => HasAttribute(target, rule.AttributeName),
            BatchTargetFilterKind.RemoveMissingAttribute => !HasAttribute(target, rule.AttributeName),
            BatchTargetFilterKind.RemoveAttributeEquals => AttributeEquals(
                target,
                rule.AttributeName,
                rule.AttributeValue),
            BatchTargetFilterKind.RemoveWithoutAttributeValue => !HasAttributeValue(
                target,
                rule.AttributeValue),
            _ => false
        };
    }

    private static bool ContainsKeyword(BatchTargetFilterTarget target, string keyword)
    {
        string value = Normalize(keyword);
        return value.Length > 0 &&
               target.KeywordText.Contains(value, StringComparison.OrdinalIgnoreCase);
    }

    private static bool HasAttribute(BatchTargetFilterTarget target, string attributeName)
    {
        string name = Normalize(attributeName);
        return name.Length > 0 && target.AttributeValues.ContainsKey(name);
    }

    private static bool AttributeEquals(
        BatchTargetFilterTarget target,
        string attributeName,
        string attributeValue)
    {
        string name = Normalize(attributeName);
        return name.Length > 0 &&
               target.AttributeValues.TryGetValue(name, out string? actual) &&
               string.Equals(
                   Normalize(actual),
                   Normalize(attributeValue),
                   StringComparison.OrdinalIgnoreCase);
    }

    private static bool HasAttributeValue(BatchTargetFilterTarget target, string attributeValue)
    {
        string expected = Normalize(attributeValue);
        return expected.Length > 0 &&
               target.AttributeValues.Values.Any(
                   value => string.Equals(
                       Normalize(value),
                       expected,
                       StringComparison.OrdinalIgnoreCase));
    }

    private static string Normalize(string? value)
    {
        return string.IsNullOrWhiteSpace(value) ? "" : value.Trim();
    }
}

internal static class BatchTargetFilterPrompt
{
    public static bool TryAskText(
        Window owner,
        string title,
        string label,
        out string value)
    {
        TextBox textBox = new() { MinWidth = 340, Margin = new Thickness(0, 5, 0, 12) };
        Window dialog = CreateDialog(owner, title, 390, 150);
        StackPanel panel = new() { Margin = new Thickness(14) };
        panel.Children.Add(new TextBlock { Text = label });
        panel.Children.Add(textBox);
        panel.Children.Add(CreateButtons(dialog));
        dialog.Content = panel;
        textBox.Focus();

        bool accepted = dialog.ShowDialog() == true;
        value = accepted ? textBox.Text.Trim() : "";
        return accepted && value.Length > 0;
    }

    public static bool TryAskAttributeEquals(
        Window owner,
        out string attributeName,
        out string attributeValue)
    {
        TextBox nameBox = new() { MinWidth = 340, Margin = new Thickness(0, 5, 0, 8) };
        TextBox valueBox = new() { MinWidth = 340, Margin = new Thickness(0, 5, 0, 12) };
        Window dialog = CreateDialog(owner, "按属性过滤", 390, 205);
        StackPanel panel = new() { Margin = new Thickness(14) };
        panel.Children.Add(new TextBlock { Text = "属性名" });
        panel.Children.Add(nameBox);
        panel.Children.Add(new TextBlock { Text = "属性值" });
        panel.Children.Add(valueBox);
        panel.Children.Add(CreateButtons(dialog));
        dialog.Content = panel;
        nameBox.Focus();

        bool accepted = dialog.ShowDialog() == true;
        attributeName = accepted ? nameBox.Text.Trim() : "";
        attributeValue = accepted ? valueBox.Text.Trim() : "";
        return accepted && attributeName.Length > 0;
    }

    private static Window CreateDialog(
        Window owner,
        string title,
        double width,
        double height)
    {
        return new Window
        {
            Owner = owner,
            Title = title,
            Width = width,
            Height = height,
            WindowStartupLocation = WindowStartupLocation.CenterOwner,
            ResizeMode = ResizeMode.NoResize,
            ShowInTaskbar = false,
            Background = System.Windows.Media.Brushes.White
        };
    }

    private static FrameworkElement CreateButtons(Window dialog)
    {
        Button ok = new()
        {
            Content = "确定",
            Width = 82,
            Height = 28,
            IsDefault = true,
            Margin = new Thickness(0, 0, 8, 0)
        };
        Button cancel = new()
        {
            Content = "取消",
            Width = 82,
            Height = 28,
            IsCancel = true
        };
        ok.Click += (_, _) => dialog.DialogResult = true;
        cancel.Click += (_, _) => dialog.DialogResult = false;

        StackPanel buttons = new()
        {
            Orientation = Orientation.Horizontal,
            HorizontalAlignment = HorizontalAlignment.Right
        };
        buttons.Children.Add(ok);
        buttons.Children.Add(cancel);
        return buttons;
    }
}
