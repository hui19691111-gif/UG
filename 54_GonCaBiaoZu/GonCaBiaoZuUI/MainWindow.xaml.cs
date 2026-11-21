using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Interop;
using System.Windows.Media;
using System.Windows.Shapes;

namespace GonCaBiaoZuUI;

public partial class MainWindow : Window
{
    private readonly string _sessionId;
    private readonly string _ipcDir;
    private readonly IntPtr _nxHwnd;
    private bool _initializing = true;
    private bool _busy = false;
    private bool _closing = false;
    private bool _hasLastGdt = false;
    private bool _hasLastDatum = false;
    private int _switchSeq = 0;
    private Task? _activeRequestTask;

    private int _dimSubmode = 0;
    private int _dimPresetIndex = 1;
    private int _gdtChar = 1;
    private int _gdtFrameColor = 0;
    private int _gdtTextColor = 0;
    private int _datumFrameColor = 0;
    private int _datumTextColor = 0;

    private readonly List<Button> _dimModeButtons = new();
    private readonly List<Button> _dimPresetButtons = new();
    private readonly List<Button> _gdtSymbolButtons = new();

    private static readonly string ConfigPath = @"D:\UG智辉钣金插件\config\GonCaBiaoZu_UI.json";

    private record DimPresetItem(string Label, string Sub, double Upper, double Lower, bool Sym);
    private record GdtSymbolItem(string Name, string PathData);
    private record GdtTemplateItem(string Name, int CharIdx, int Zone, double Val, int Mat, int D1, int D2, int D3);
    private record NxSwatch(int ColorId, string Name, string Hex);

    private static readonly DimPresetItem[] DimPresets =
    {
        new("±0.05", "精密间距", 0.05, -0.05, true),
        new("±0.10", "常用孔距", 0.10, -0.10, true),
        new("±0.15", "折弯内空", 0.15, -0.15, true),
        new("±0.20", "常规折弯", 0.20, -0.20, true),
        new("±0.30", "一般外形", 0.30, -0.30, true),
        new("±0.50", "大件自由", 0.50, -0.50, true),
        new("+0.05/0", "精密底孔", 0.05, 0.0, false),
        new("+0.10/0", "压铆/激光", 0.10, 0.0, false),
        new("+0.15/0", "常规过孔", 0.15, 0.0, false),
        new("+0.20/0", "宽松装配", 0.20, 0.0, false),
        new("0/-0.05", "精密外轴", 0.0, -0.05, false),
        new("0/-0.10", "插接公口", 0.0, -0.10, false),
        new("0/-0.20", "内嵌包边", 0.0, -0.20, false),
        new("0/-0.50", "避空门板", 0.0, -0.50, false),
        new("+0.1/-0.05", "非对称1", 0.10, -0.05, false),
        new("+0.2/-0.1", "非对称2", 0.20, -0.10, false),
    };

    private static readonly string[] DimModes =
        { "快捷公差", "孔配合", "轴配合", "装配配合", "GB/T 1804", "手动偏差", "清除公差" };

    private static readonly string[] HoleFits =
    {
        "H6 (精密基准孔)", "H7 (常用定位销孔)", "H8 (常规配合孔)", "H9 (轴套滑动孔)",
        "H10 (宽松装配孔)", "H11 (钣金冲孔)", "JS7 (对称过渡孔)", "K7 (轴承外圈孔)",
        "M7 (过渡配合孔)", "N7 (轻过盈孔)", "P7 (过盈压入孔)", "G7 (精密间隙孔)", "F8 (活动转动孔)"
    };

    private static readonly string[] ShaftFits =
    {
        "h5 (高精基准轴)", "h6 (常用基准轴)", "h7 (常规基准轴)", "h8 (一般配合轴)",
        "h9 (宽松导轴)", "g6 (常用滑动轴)", "f6 (精密转动轴)", "f7 (普通转动轴)",
        "e8 (大间隙轴)", "js6 (过渡轴)", "k6 (轴承内圈轴)", "m6 (紧过渡轴)",
        "n6 (轻压入轴)", "p6 (过盈压入轴)", "s6 (重过盈轴)"
    };

    private static readonly GdtSymbolItem[] GdtSymbols =
    {
        new("直线度", "M 3,12 L 21,12"),
        new("平面度", "M 6,7 L 20,7 L 16,17 L 2,17 Z"),
        new("圆度", "M 12,4 A 8,8 0 1 0 12.01,4 Z"),
        new("圆柱度", "M 12,6 A 5,5 0 1 0 12.01,6 Z M 7,19 L 13,5 M 11,19 L 17,5"),
        new("线轮廓度", "M 4,16 A 8,8 0 0 1 20,16"),
        new("面轮廓度", "M 4,16 A 8,8 0 0 1 20,16 Z"),
        new("倾斜度", "M 18,6 L 5,18 L 20,18"),
        new("垂直度", "M 12,4 L 12,18 M 4,18 L 20,18"),
        new("平行度", "M 7,18 L 13,5 M 13,18 L 19,5"),
        new("位置度", "M 12,6 A 6,6 0 1 0 12.01,6 Z M 12,2 L 12,22 M 2,12 L 22,12"),
        new("同轴度", "M 12,4 A 8,8 0 1 0 12.01,4 Z M 12,8 A 4,4 0 1 0 12.01,8 Z"),
        new("对称度", "M 3,12 L 21,12 M 7,7 L 17,7 M 7,17 L 17,17"),
        new("圆跳动", "M 6,19 L 17,6 M 12,7 L 17,6 L 16,11"),
        new("全跳动", "M 3,18 L 11,6 M 7,7 L 11,6 L 10,10 M 11,18 L 19,6 M 15,7 L 19,6 L 18,10 M 3,18 L 11,18"),
    };

    private static readonly GdtTemplateItem[] GdtTemplates =
    {
        new("— 快捷模板 —", -1, 0, 0.1, 0, 0, 0, 0),
        new("平面度 0.1 (安装面)", 1, 0, 0.1, 0, 0, 0, 0),
        new("平面度 0.2 (常规钣金)", 1, 0, 0.2, 0, 0, 0, 0),
        new("垂直度 0.1 | A (折弯)", 7, 0, 0.1, 0, 1, 0, 0),
        new("垂直度 0.2 | A (机箱)", 7, 0, 0.2, 0, 1, 0, 0),
        new("平行度 0.1 | A", 8, 0, 0.1, 0, 1, 0, 0),
        new("位置度 ∅0.1 | A | B", 9, 1, 0.1, 0, 1, 2, 0),
        new("位置度 ∅0.2 | A | B | C", 9, 1, 0.2, 0, 1, 2, 3),
        new("位置度 ∅0.2 Ⓜ | A | B", 9, 1, 0.2, 1, 1, 2, 0),
        new("同轴度 ∅0.05 | A", 10, 1, 0.05, 0, 1, 0, 0),
        new("对称度 0.1 | A", 11, 0, 0.1, 0, 1, 0, 0),
        new("圆跳动 0.05 | A", 12, 0, 0.05, 0, 1, 0, 0),
        new("面轮廓度 0.2 | A | B", 5, 0, 0.2, 0, 1, 2, 0),
    };

    private static readonly string[] DatumChoices = { "无", "A", "B", "C", "D", "A-B" };

    // 常用 NX 颜色号与视觉映射（0=跟随工程图默认色，兼容黑底/白底）
    private static readonly NxSwatch[] NxSwatches =
    {
        new(0,   "默认 (随图面)", "#475569"),
        new(6,   "黄 (6)",   "#EAB308"),
        new(1,   "白 (1)",   "#F8FAFC"),
        new(108, "青 (108)", "#0891B2"),
        new(36,  "绿 (36)",  "#16A34A"),
        new(186, "红 (186)", "#DC2626"),
        new(211, "蓝 (211)", "#2563EB"),
        new(78,  "橙 (78)",  "#EA580C"),
        new(181, "洋红 (181)", "#C026D3"),
        new(216, "黑 (216)", "#111827"),
    };

    public MainWindow(string sessionId, string ipcDir, IntPtr nxHwnd)
    {
        _sessionId = sessionId;
        _ipcDir = ipcDir;
        _nxHwnd = nxHwnd;

        InitializeComponent();
        BuildDynamicControls();
        LoadState();
        _initializing = false;

        RefreshDimUi();
        RefreshGdtPreview();
        RefreshDatumPreview();

        SourceInitialized += (_, _) =>
        {
            // 注意：严禁跨进程设置 WindowInteropHelper.Owner = _nxHwnd，否则会绑定两个进程的 Win32 输入队列，
            // 导致 NX 调用 SelectTaggedObject 时发生跨进程消息死锁/转圈圈。仅使用 Topmost 悬浮即可。
            Topmost = PinToggle.IsChecked == true;
            // 默认靠左上方停靠，不挡工程图中央
            Left = Math.Max(20, SystemParameters.WorkArea.Left + 24);
            Top = Math.Max(60, SystemParameters.WorkArea.Top + 90);
        };

        ContentRendered += async (_, _) =>
        {
            // 打开对话框后立即自动进入当前页签的连续标注选择模式，无需再多点一次按钮
            await StartActiveTabAutoPickAsync();
        };

        Closing += (_, _) =>
        {
            _closing = true;
            if (_busy) SignalCancelActiveNxDialog();
        };
    }

    private void BuildDynamicControls()
    {
        // 1. 尺寸模式胶囊按钮
        for (int i = 0; i < DimModes.Length; i++)
        {
            int idx = i;
            var btn = new Button { Content = DimModes[i], Style = (Style)FindResource("ChipBtn") };
            btn.Click += (_, _) => { _dimSubmode = idx; RefreshDimUi(); SaveState(); };
            _dimModeButtons.Add(btn);
            DimModePanel.Children.Add(btn);
        }

        // 2. 4x4 钣金快捷公差卡片墙
        for (int i = 0; i < DimPresets.Length; i++)
        {
            int idx = i;
            var p = DimPresets[i];
            var sp = new StackPanel { HorizontalAlignment = HorizontalAlignment.Center };
            sp.Children.Add(new TextBlock { Text = p.Label, FontSize = 12, FontWeight = FontWeights.SemiBold, HorizontalAlignment = HorizontalAlignment.Center });
            sp.Children.Add(new TextBlock { Text = p.Sub, FontSize = 9.5, Foreground = new SolidColorBrush(Color.FromRgb(100, 116, 139)), HorizontalAlignment = HorizontalAlignment.Center });

            var btn = new Button
            {
                Content = sp,
                Margin = new Thickness(2.5),
                Padding = new Thickness(4, 6, 4, 6),
                Cursor = Cursors.Hand,
                Style = (Style)FindResource("ChipBtn")
            };
            btn.Click += (_, _) =>
            {
                _dimPresetIndex = idx;
                TxtUpperTol.Text = p.Upper.ToString("0.###", CultureInfo.InvariantCulture);
                TxtLowerTol.Text = p.Lower.ToString("0.###", CultureInfo.InvariantCulture);
                RefreshDimUi();
                SaveState();
            };
            _dimPresetButtons.Add(btn);
            DimPresetGrid.Children.Add(btn);
        }

        // 孔/轴配合下拉
        foreach (var h in HoleFits) CmbHoleFit.Items.Add(h);
        foreach (var s in ShaftFits) CmbShaftFit.Items.Add(s);
        CmbHoleFit.SelectedIndex = 1;
        CmbShaftFit.SelectedIndex = 5;
        CmbFitStyle.SelectedIndex = 0;
        CmbGb1804.SelectedIndex = 1;
        CmbPrefix.SelectedIndex = 0;
        CmbSuffix.SelectedIndex = 0;

        // 3. 形位公差 7x2 矢量符号按钮
        for (int i = 0; i < GdtSymbols.Length; i++)
        {
            int idx = i;
            var sym = GdtSymbols[i];
            var path = new System.Windows.Shapes.Path
            {
                Data = Geometry.Parse(sym.PathData),
                Stroke = new SolidColorBrush(Color.FromRgb(15, 23, 42)),
                StrokeThickness = 1.7,
                StrokeStartLineCap = PenLineCap.Round,
                StrokeEndLineCap = PenLineCap.Round,
                Width = 24,
                Height = 24,
                Stretch = Stretch.Uniform
            };
            var btn = new Button
            {
                Content = path,
                ToolTip = sym.Name,
                Height = 36,
                Margin = new Thickness(2.5),
                Padding = new Thickness(4),
                Style = (Style)FindResource("ChipBtn")
            };
            btn.Click += (_, _) =>
            {
                _gdtChar = idx;
                _initializing = true;
                CmbGdtPreset.SelectedIndex = 0;
                _initializing = false;
                RefreshGdtPreview();
                SaveState();
            };
            _gdtSymbolButtons.Add(btn);
            GdtSymbolGrid.Children.Add(btn);
        }

        foreach (var t in GdtTemplates) CmbGdtPreset.Items.Add(t.Name);
        CmbGdtPreset.SelectedIndex = 0;
        CmbGdtZone.SelectedIndex = 0;
        CmbGdtMat.SelectedIndex = 0;
        CmbGb1184.SelectedIndex = 0;

        foreach (var d in DatumChoices)
        {
            CmbGdtD1.Items.Add(d);
            CmbGdtD2.Items.Add(d);
            CmbGdtD3.Items.Add(d);
        }
        CmbGdtD1.SelectedIndex = 0;
        CmbGdtD2.SelectedIndex = 0;
        CmbGdtD3.SelectedIndex = 0;

        // 快捷公差值按钮
        foreach (var v in new[] { "0.05", "0.1", "0.2", "0.3", "0.5" })
        {
            string val = v;
            var b = new Button { Content = val, Style = (Style)FindResource("ChipBtn"), Margin = new Thickness(0, 0, 4, 0), Padding = new Thickness(7, 3, 7, 3) };
            b.Click += (_, _) => { TxtGdtVal.Text = val; };
            GdtQuickValPanel.Children.Add(b);
        }

        // 字高快捷按钮 (GD&T & Datum)
        foreach (var h in new[] { "2.0", "2.5", "3.5", "5.0" })
        {
            string hv = h;
            var b1 = new Button { Content = hv, Style = (Style)FindResource("ChipBtn"), Margin = new Thickness(0, 0, 4, 0), Padding = new Thickness(8, 3, 8, 3) };
            b1.Click += (_, _) => { TxtGdtHeight.Text = hv; };
            GdtHeightChips.Children.Add(b1);

            var b2 = new Button { Content = hv, Style = (Style)FindResource("ChipBtn"), Margin = new Thickness(0, 0, 4, 0), Padding = new Thickness(8, 3, 8, 3) };
            b2.Click += (_, _) => { TxtDatumHeight.Text = hv; };
            DatumHeightChips.Children.Add(b2);
        }

        // 基准字母快捷按钮
        foreach (var lt in new[] { "A", "B", "C", "D", "E", "F" })
        {
            string letter = lt;
            var b = new Button { Content = letter, Style = (Style)FindResource("ChipBtn"), Margin = new Thickness(0, 0, 5, 0), Padding = new Thickness(10, 4, 10, 4), FontWeight = FontWeights.SemiBold };
            b.Click += (_, _) => { TxtDatumLetter.Text = letter; };
            DatumLetterChips.Children.Add(b);
        }

        // NX 色板色块
        BuildColorSwatches(GdtFrameColorSwatches, id => { _gdtFrameColor = id; TxtGdtFrameColor.Text = id.ToString(); RefreshGdtPreview(); SaveState(); });
        BuildColorSwatches(GdtTextColorSwatches, id => { _gdtTextColor = id; TxtGdtTextColor.Text = id.ToString(); RefreshGdtPreview(); SaveState(); });
        BuildColorSwatches(DatumFrameColorSwatches, id => { _datumFrameColor = id; TxtDatumFrameColor.Text = id.ToString(); RefreshDatumPreview(); SaveState(); });
        BuildColorSwatches(DatumTextColorSwatches, id => { _datumTextColor = id; TxtDatumTextColor.Text = id.ToString(); RefreshDatumPreview(); SaveState(); });
    }

    private void BuildColorSwatches(Panel host, Action<int> onPick)
    {
        foreach (var sw in NxSwatches)
        {
            int cid = sw.ColorId;
            var color = (Color)ColorConverter.ConvertFromString(sw.Hex);
            var btn = new Button
            {
                Width = 22,
                Height = 22,
                Margin = new Thickness(0, 0, 5, 0),
                ToolTip = sw.Name,
                Cursor = Cursors.Hand,
                Background = new SolidColorBrush(color),
                BorderBrush = new SolidColorBrush(Color.FromRgb(203, 213, 225)),
                BorderThickness = new Thickness(1),
                Tag = cid
            };
            btn.Template = CreateSwatchTemplate();
            btn.Click += (_, _) => onPick(cid);
            host.Children.Add(btn);
        }
    }

    private static ControlTemplate CreateSwatchTemplate()
    {
        var tmpl = new ControlTemplate(typeof(Button));
        var bd = new FrameworkElementFactory(typeof(Border));
        bd.SetValue(Border.CornerRadiusProperty, new CornerRadius(4));
        bd.SetBinding(Border.BackgroundProperty, new System.Windows.Data.Binding("Background") { RelativeSource = new System.Windows.Data.RelativeSource(System.Windows.Data.RelativeSourceMode.TemplatedParent) });
        bd.SetBinding(Border.BorderBrushProperty, new System.Windows.Data.Binding("BorderBrush") { RelativeSource = new System.Windows.Data.RelativeSource(System.Windows.Data.RelativeSourceMode.TemplatedParent) });
        bd.SetBinding(Border.BorderThicknessProperty, new System.Windows.Data.Binding("BorderThickness") { RelativeSource = new System.Windows.Data.RelativeSource(System.Windows.Data.RelativeSourceMode.TemplatedParent) });
        tmpl.VisualTree = bd;
        return tmpl;
    }

    private static SolidColorBrush BrushFromNxColor(int colorId)
    {
        foreach (var sw in NxSwatches)
        {
            if (sw.ColorId == colorId) return new SolidColorBrush((Color)ColorConverter.ConvertFromString(sw.Hex));
        }
        return new SolidColorBrush(Color.FromRgb(15, 23, 42));
    }

    private void HighlightSwatches(Panel host, int activeColorId)
    {
        foreach (UIElement el in host.Children)
        {
            if (el is Button b && b.Tag is int cid)
            {
                bool active = cid == activeColorId;
                b.BorderBrush = active ? new SolidColorBrush(Color.FromRgb(37, 99, 235)) : new SolidColorBrush(Color.FromRgb(203, 213, 225));
                b.BorderThickness = active ? new Thickness(2.5) : new Thickness(1);
            }
        }
    }

    private void Header_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
    {
        if (e.ChangedButton == MouseButton.Left) DragMove();
    }

    private void PinToggle_Click(object sender, RoutedEventArgs e)
    {
        Topmost = PinToggle.IsChecked == true;
    }

    private void SignalCancelActiveNxDialog()
    {
        if (string.IsNullOrEmpty(_sessionId)) return;
        try
        {
            using var cancelEv = EventWaitHandle.OpenExisting($"Local\\Zhihui_GCBZ_Cancel_{_sessionId}");
            cancelEv.Set();
        }
        catch { }
    }

    private void CloseBtn_Click(object sender, RoutedEventArgs e)
    {
        _closing = true;
        if (_busy) SignalCancelActiveNxDialog();
        SaveState();
        Close();
    }

    private async void MainTab_Checked(object sender, RoutedEventArgs e)
    {
        if (_initializing || PanelDim == null) return;
        PanelDim.Visibility = TabDim.IsChecked == true ? Visibility.Visible : Visibility.Collapsed;
        PanelGdt.Visibility = TabGdt.IsChecked == true ? Visibility.Visible : Visibility.Collapsed;
        PanelDatum.Visibility = TabDatum.IsChecked == true ? Visibility.Visible : Visibility.Collapsed;
        SaveState();
        await StartActiveTabAutoPickAsync();
    }

    private async Task CancelRunningAndWaitAsync()
    {
        if (_busy)
        {
            SignalCancelActiveNxDialog();
        }
        var t = _activeRequestTask;
        if (t != null && !t.IsCompleted)
        {
            try { await t; } catch { }
        }
    }

    private async Task StartActiveTabAutoPickAsync()
    {
        if (_closing || string.IsNullOrEmpty(_sessionId) || string.IsNullOrEmpty(_ipcDir)) return;
        int mySeq = ++_switchSeq;
        await CancelRunningAndWaitAsync();
        if (_closing || mySeq != _switchSeq) return;

        if (TabDim.IsChecked == true)
        {
            await ExecuteDimAsync("dim_continuous");
        }
        else if (TabGdt.IsChecked == true)
        {
            await RunGdtPlaceAsync();
        }
        else if (TabDatum.IsChecked == true)
        {
            await RunDatumPlaceAsync();
        }
    }

    private void RefreshDimUi()
    {
        for (int i = 0; i < _dimModeButtons.Count; i++)
        {
            bool active = i == _dimSubmode;
            _dimModeButtons[i].Background = active ? new SolidColorBrush(Color.FromRgb(37, 99, 235)) : new SolidColorBrush(Color.FromRgb(248, 250, 252));
            _dimModeButtons[i].Foreground = active ? Brushes.White : new SolidColorBrush(Color.FromRgb(51, 65, 85));
            _dimModeButtons[i].BorderBrush = active ? new SolidColorBrush(Color.FromRgb(37, 99, 235)) : new SolidColorBrush(Color.FromRgb(203, 213, 225));
        }

        for (int i = 0; i < _dimPresetButtons.Count; i++)
        {
            bool active = i == _dimPresetIndex;
            _dimPresetButtons[i].Background = active ? new SolidColorBrush(Color.FromRgb(239, 246, 255)) : new SolidColorBrush(Color.FromRgb(248, 250, 252));
            _dimPresetButtons[i].BorderBrush = active ? new SolidColorBrush(Color.FromRgb(37, 99, 235)) : new SolidColorBrush(Color.FromRgb(226, 232, 240));
            _dimPresetButtons[i].BorderThickness = active ? new Thickness(1.8) : new Thickness(1);
        }

        DimQuickCard.Visibility = _dimSubmode == 0 ? Visibility.Visible : Visibility.Collapsed;
        bool needDetail = _dimSubmode is 1 or 2 or 3 or 4 or 5;
        DimDetailCard.Visibility = needDetail ? Visibility.Visible : Visibility.Collapsed;
        RowHoleFit.Visibility = _dimSubmode is 1 or 3 ? Visibility.Visible : Visibility.Collapsed;
        RowShaftFit.Visibility = _dimSubmode is 2 or 3 ? Visibility.Visible : Visibility.Collapsed;
        RowFitStyle.Visibility = _dimSubmode is 1 or 2 or 3 ? Visibility.Visible : Visibility.Collapsed;
        RowGb1804.Visibility = _dimSubmode == 4 ? Visibility.Visible : Visibility.Collapsed;
        RowManualTol.Visibility = _dimSubmode == 5 ? Visibility.Visible : Visibility.Collapsed;

        // 更新预览文本
        string prefix = CmbPrefix.SelectedIndex switch { 2 => "2×", 3 => "4×", 4 => "6×", 5 => "8×", 6 => "□", _ => "" };
        string suffix = CmbSuffix.SelectedIndex switch { 2 => " THRU", 3 => " EQS", 4 => " TYP", 5 => " 配作", _ => "" };
        string body = _dimSubmode switch
        {
            0 => $"25  {DimPresets[Math.Clamp(_dimPresetIndex, 0, DimPresets.Length - 1)].Label}",
            1 => $"∅25  {HoleFits[Math.Max(0, CmbHoleFit.SelectedIndex)].Split(' ')[0]}",
            2 => $"∅25  {ShaftFits[Math.Max(0, CmbShaftFit.SelectedIndex)].Split(' ')[0]}",
            3 => $"∅25  {HoleFits[Math.Max(0, CmbHoleFit.SelectedIndex)].Split(' ')[0]}/{ShaftFits[Math.Max(0, CmbShaftFit.SelectedIndex)].Split(' ')[0]}",
            4 => $"25  GB/T 1804-{new[] { "f", "m", "c", "v" }[Math.Clamp(CmbGb1804.SelectedIndex, 0, 3)]}",
            5 => $"25  ({TxtUpperTol.Text} / {TxtLowerTol.Text})",
            _ => "清除所选尺寸公差与括号"
        };
        if (ChkRefDim.IsChecked == true && _dimSubmode != 6) body = $"({body})";
        DimPreviewText.Text = _dimSubmode == 6 ? body : $"{prefix}{body}{suffix}";
        WriteLiveDimConfig();
    }

    private void WriteLiveDimConfig()
    {
        if (string.IsNullOrEmpty(_ipcDir)) return;
        try
        {
            var sb = new StringBuilder();
            sb.AppendLine($"dim_submode={_dimSubmode}");
            sb.AppendLine($"dim_preset={_dimPresetIndex}");
            sb.AppendLine($"dim_hole_fit={Math.Max(0, CmbHoleFit.SelectedIndex)}");
            sb.AppendLine($"dim_shaft_fit={Math.Max(0, CmbShaftFit.SelectedIndex)}");
            sb.AppendLine($"dim_fit_style={Math.Max(0, CmbFitStyle.SelectedIndex)}");
            sb.AppendLine($"dim_gb1804={Math.Max(0, CmbGb1804.SelectedIndex)}");
            sb.AppendLine($"dim_upper={ParseDouble(TxtUpperTol.Text, 0.1).ToString(CultureInfo.InvariantCulture)}");
            sb.AppendLine($"dim_lower={ParseDouble(TxtLowerTol.Text, -0.1).ToString(CultureInfo.InvariantCulture)}");
            sb.AppendLine($"dim_prefix={Math.Max(0, CmbPrefix.SelectedIndex)}");
            sb.AppendLine($"dim_suffix={Math.Max(0, CmbSuffix.SelectedIndex)}");
            sb.AppendLine($"dim_ref={(ChkRefDim.IsChecked == true ? 1 : 0)}");
            File.WriteAllText(System.IO.Path.Combine(_ipcDir, "live_dim.ini"), sb.ToString(), new UTF8Encoding(false));
        }
        catch { }
    }

    private void DimParam_Changed(object sender, RoutedEventArgs e)
    {
        if (_initializing) return;
        RefreshDimUi();
        SaveState();
    }

    private void DimManual_Changed(object sender, TextChangedEventArgs e)
    {
        if (_initializing) return;
        RefreshDimUi();
        SaveState();
    }

    private void CmbGdtPreset_Changed(object sender, SelectionChangedEventArgs e)
    {
        if (_initializing || CmbGdtPreset.SelectedIndex <= 0) return;
        var t = GdtTemplates[CmbGdtPreset.SelectedIndex];
        _initializing = true;
        _gdtChar = t.CharIdx;
        CmbGdtZone.SelectedIndex = t.Zone;
        TxtGdtVal.Text = t.Val.ToString("0.###", CultureInfo.InvariantCulture);
        CmbGdtMat.SelectedIndex = t.Mat;
        CmbGb1184.SelectedIndex = 0;
        CmbGdtD1.SelectedIndex = t.D1;
        CmbGdtD2.SelectedIndex = t.D2;
        CmbGdtD3.SelectedIndex = t.D3;
        _initializing = false;
        RefreshGdtPreview();
        SaveState();
    }

    private void GdtParam_Changed(object sender, EventArgs e)
    {
        if (_initializing) return;
        RefreshGdtPreview();
        SaveState();
    }

    private void GdtStyle_Changed(object sender, TextChangedEventArgs e)
    {
        if (_initializing) return;
        RefreshGdtPreview();
        SaveState();
    }

    private void GdtColorId_Changed(object sender, TextChangedEventArgs e)
    {
        if (_initializing) return;
        if (int.TryParse(TxtGdtFrameColor.Text, out int fc)) _gdtFrameColor = Math.Clamp(fc, 0, 216);
        if (int.TryParse(TxtGdtTextColor.Text, out int tc)) _gdtTextColor = Math.Clamp(tc, 0, 216);
        RefreshGdtPreview();
        SaveState();
    }

    private void RefreshGdtPreview()
    {
        for (int i = 0; i < _gdtSymbolButtons.Count; i++)
        {
            bool active = i == _gdtChar;
            _gdtSymbolButtons[i].Background = active ? new SolidColorBrush(Color.FromRgb(219, 234, 254)) : new SolidColorBrush(Color.FromRgb(248, 250, 252));
            _gdtSymbolButtons[i].BorderBrush = active ? new SolidColorBrush(Color.FromRgb(37, 99, 235)) : new SolidColorBrush(Color.FromRgb(226, 232, 240));
            _gdtSymbolButtons[i].BorderThickness = active ? new Thickness(2) : new Thickness(1);
        }
        HighlightSwatches(GdtFrameColorSwatches, _gdtFrameColor);
        HighlightSwatches(GdtTextColorSwatches, _gdtTextColor);

        double h = ParseDouble(TxtGdtHeight.Text, 3.5);
        GdtPreviewInfo.Text = $"{GdtSymbols[_gdtChar].Name} · 字高 {h:0.#} mm · 框色 {_gdtFrameColor} / 字色 {_gdtTextColor}";

        var frameBrush = BrushFromNxColor(_gdtFrameColor);
        var textBrush = BrushFromNxColor(_gdtTextColor);
        double fontSize = Math.Clamp(12 + (h - 2.0) * 2.2, 12, 22);
        double cellH = fontSize + 14;

        GdtGraphicFrame.Children.Clear();

        // 1. 符号格
        var symPath = new System.Windows.Shapes.Path
        {
            Data = Geometry.Parse(GdtSymbols[_gdtChar].PathData),
            Stroke = textBrush,
            StrokeThickness = 1.8,
            Width = fontSize + 2,
            Height = fontSize + 2,
            Stretch = Stretch.Uniform
        };
        GdtGraphicFrame.Children.Add(MakeFcfCell(symPath, frameBrush, cellH, false));

        // 2. 公差值格
        string zone = CmbGdtZone.SelectedIndex switch { 1 => "∅", 2 => "S∅", 3 => "□", _ => "" };
        string mat = CmbGdtMat.SelectedIndex switch { 1 => " Ⓜ", 2 => " Ⓛ", _ => "" };
        string valStr = CmbGb1184.SelectedIndex > 0 ? $"自动({new[] { "", "H", "K", "L" }[CmbGb1184.SelectedIndex]})" : TxtGdtVal.Text;
        var valTb = new TextBlock { Text = $"{zone}{valStr}{mat}", Foreground = textBrush, FontSize = fontSize, FontWeight = FontWeights.SemiBold, VerticalAlignment = VerticalAlignment.Center };
        GdtGraphicFrame.Children.Add(MakeFcfCell(valTb, frameBrush, cellH, true));

        // 3. 基准格
        foreach (var cmb in new[] { CmbGdtD1, CmbGdtD2, CmbGdtD3 })
        {
            if (cmb.SelectedIndex > 0)
            {
                var dtb = new TextBlock { Text = DatumChoices[cmb.SelectedIndex], Foreground = textBrush, FontSize = fontSize, FontWeight = FontWeights.SemiBold, VerticalAlignment = VerticalAlignment.Center };
                GdtGraphicFrame.Children.Add(MakeFcfCell(dtb, frameBrush, cellH, true));
            }
        }
        WriteLiveGdtConfig();
    }

    private void WriteLiveGdtConfig()
    {
        if (string.IsNullOrEmpty(_ipcDir)) return;
        try
        {
            var sb = new StringBuilder();
            foreach (var kv in BuildGdtPayload("gdt_place")) sb.AppendLine($"{kv.Key}={kv.Value}");
            File.WriteAllText(System.IO.Path.Combine(_ipcDir, "live_gdt.ini"), sb.ToString(), new UTF8Encoding(false));
        }
        catch { }
    }

    private static Border MakeFcfCell(UIElement content, Brush borderBrush, double height, bool overlapLeft)
    {
        return new Border
        {
            Child = content,
            BorderBrush = borderBrush,
            BorderThickness = new Thickness(overlapLeft ? 0 : 1.6, 1.6, 1.6, 1.6),
            Padding = new Thickness(10, 2, 10, 2),
            Height = height,
            MinWidth = height,
            Background = Brushes.White
        };
    }

    private void DatumParam_Changed(object sender, EventArgs e)
    {
        if (_initializing) return;
        RefreshDatumPreview();
        SaveState();
    }

    private void DatumStyle_Changed(object sender, TextChangedEventArgs e)
    {
        if (_initializing) return;
        RefreshDatumPreview();
        SaveState();
    }

    private void DatumColorId_Changed(object sender, TextChangedEventArgs e)
    {
        if (_initializing) return;
        if (int.TryParse(TxtDatumFrameColor.Text, out int fc)) _datumFrameColor = Math.Clamp(fc, 0, 216);
        if (int.TryParse(TxtDatumTextColor.Text, out int tc)) _datumTextColor = Math.Clamp(tc, 0, 216);
        RefreshDatumPreview();
        SaveState();
    }

    private void RefreshDatumPreview()
    {
        HighlightSwatches(DatumFrameColorSwatches, _datumFrameColor);
        HighlightSwatches(DatumTextColorSwatches, _datumTextColor);

        string letter = string.IsNullOrWhiteSpace(TxtDatumLetter.Text) ? "A" : TxtDatumLetter.Text.Trim();
        double h = ParseDouble(TxtDatumHeight.Text, 3.5);
        DatumPreviewInfo.Text = $"基准 {letter} · 字高 {h:0.#} mm · 框色 {_datumFrameColor} / 字色 {_datumTextColor}";

        var frameBrush = BrushFromNxColor(_datumFrameColor);
        var textBrush = BrushFromNxColor(_datumTextColor);
        double fontSize = Math.Clamp(13 + (h - 2.0) * 2.2, 13, 22);
        double boxSize = fontSize + 14;

        DatumGraphicPreview.Children.Clear();
        DatumGraphicPreview.Children.Add(new Border
        {
            Width = boxSize,
            Height = boxSize,
            BorderBrush = frameBrush,
            BorderThickness = new Thickness(1.8),
            Background = Brushes.White,
            HorizontalAlignment = HorizontalAlignment.Center,
            Child = new TextBlock { Text = letter, Foreground = textBrush, FontSize = fontSize, FontWeight = FontWeights.Bold, HorizontalAlignment = HorizontalAlignment.Center, VerticalAlignment = VerticalAlignment.Center }
        });
        DatumGraphicPreview.Children.Add(new Rectangle
        {
            Width = 1.8,
            Height = 14,
            Fill = frameBrush,
            HorizontalAlignment = HorizontalAlignment.Center
        });
        DatumGraphicPreview.Children.Add(new Polygon
        {
            Points = new PointCollection { new(0, 10), new(8, 0), new(16, 10) },
            Fill = frameBrush,
            HorizontalAlignment = HorizontalAlignment.Center
        });
        WriteLiveDatumConfig();
    }

    private void WriteLiveDatumConfig()
    {
        if (string.IsNullOrEmpty(_ipcDir)) return;
        try
        {
            var sb = new StringBuilder();
            foreach (var kv in BuildDatumPayload("datum_place")) sb.AppendLine($"{kv.Key}={kv.Value}");
            File.WriteAllText(System.IO.Path.Combine(_ipcDir, "live_datum.ini"), sb.ToString(), new UTF8Encoding(false));
        }
        catch { }
    }

    private async void BtnDimContinuous_Click(object sender, RoutedEventArgs e)
    {
        await CancelRunningAndWaitAsync();
        await ExecuteDimAsync("dim_continuous");
    }

    private async void BtnDimBatch_Click(object sender, RoutedEventArgs e)
    {
        await CancelRunningAndWaitAsync();
        await ExecuteDimAsync("dim_batch");
    }

    private async Task ExecuteDimAsync(string action)
    {
        var req = new Dictionary<string, string>
        {
            ["action"] = action,
            ["dim_submode"] = _dimSubmode.ToString(),
            ["dim_preset"] = _dimPresetIndex.ToString(),
            ["dim_hole_fit"] = Math.Max(0, CmbHoleFit.SelectedIndex).ToString(),
            ["dim_shaft_fit"] = Math.Max(0, CmbShaftFit.SelectedIndex).ToString(),
            ["dim_fit_style"] = Math.Max(0, CmbFitStyle.SelectedIndex).ToString(),
            ["dim_gb1804"] = Math.Max(0, CmbGb1804.SelectedIndex).ToString(),
            ["dim_upper"] = ParseDouble(TxtUpperTol.Text, 0.1).ToString(CultureInfo.InvariantCulture),
            ["dim_lower"] = ParseDouble(TxtLowerTol.Text, -0.1).ToString(CultureInfo.InvariantCulture),
            ["dim_prefix"] = Math.Max(0, CmbPrefix.SelectedIndex).ToString(),
            ["dim_suffix"] = Math.Max(0, CmbSuffix.SelectedIndex).ToString(),
            ["dim_ref"] = (ChkRefDim.IsChecked == true ? 1 : 0).ToString()
        };
        await SendRequestAsync(req, action == "dim_continuous"
            ? "请在 NX 工程图中连续点选尺寸（中键/ESC 结束）..."
            : "请在 NX 工程图中框选尺寸后点确定...");
    }

    private async void BtnGdtPlace_Click(object sender, RoutedEventArgs e)
    {
        await CancelRunningAndWaitAsync();
        await RunGdtPlaceAsync();
    }

    private async Task RunGdtPlaceAsync()
    {
        WriteLiveGdtConfig();
        var res = await SendRequestAsync(BuildGdtPayload("gdt_place"), "形位公差就绪：1.选边(中键跳过) -> 2.点击放点（可连续标注，ESC暂停）");
        if (res != null && res.TryGetValue("ok", out var ok) && ok == "1")
        {
            _hasLastGdt = true;
            BtnGdtUpdateLast.IsEnabled = true;
        }
    }

    private async void BtnGdtUpdateLast_Click(object sender, RoutedEventArgs e)
    {
        if (!_hasLastGdt) return;
        await CancelRunningAndWaitAsync();
        await SendRequestAsync(BuildGdtPayload("gdt_update_last"), "正在刷新刚标注的形位公差...");
    }

    private Dictionary<string, string> BuildGdtPayload(string action) => new()
    {
        ["action"] = action,
        ["gdt_char"] = _gdtChar.ToString(),
        ["gdt_zone"] = Math.Max(0, CmbGdtZone.SelectedIndex).ToString(),
        ["gdt_val"] = ParseDouble(TxtGdtVal.Text, 0.1).ToString(CultureInfo.InvariantCulture),
        ["gdt_mat"] = Math.Max(0, CmbGdtMat.SelectedIndex).ToString(),
        ["gdt_gb1184"] = Math.Max(0, CmbGb1184.SelectedIndex).ToString(),
        ["gdt_d1"] = Math.Max(0, CmbGdtD1.SelectedIndex).ToString(),
        ["gdt_d2"] = Math.Max(0, CmbGdtD2.SelectedIndex).ToString(),
        ["gdt_d3"] = Math.Max(0, CmbGdtD3.SelectedIndex).ToString(),
        ["gdt_height"] = ParseDouble(TxtGdtHeight.Text, 3.5).ToString(CultureInfo.InvariantCulture),
        ["gdt_frame_color"] = _gdtFrameColor.ToString(),
        ["gdt_text_color"] = _gdtTextColor.ToString()
    };

    private async void BtnDatumPlace_Click(object sender, RoutedEventArgs e)
    {
        await CancelRunningAndWaitAsync();
        await RunDatumPlaceAsync();
    }

    private async Task RunDatumPlaceAsync()
    {
        WriteLiveDatumConfig();
        var res = await SendRequestAsync(BuildDatumPayload("datum_place"), "基准符号就绪：1.选基准边(中键跳过) -> 2.点击放点（自动递增，ESC暂停）");
        if (res != null && res.TryGetValue("ok", out var ok) && ok == "1")
        {
            _hasLastDatum = true;
            BtnDatumUpdateLast.IsEnabled = true;
            if (res.TryGetValue("next_letter", out var next) && !string.IsNullOrWhiteSpace(next))
            {
                _initializing = true;
                TxtDatumLetter.Text = next;
                _initializing = false;
                RefreshDatumPreview();
                SaveState();
            }
        }
    }

    private async void BtnDatumUpdateLast_Click(object sender, RoutedEventArgs e)
    {
        if (!_hasLastDatum) return;
        await CancelRunningAndWaitAsync();
        await SendRequestAsync(BuildDatumPayload("datum_update_last"), "正在刷新刚标注的基准符号...");
    }

    private Dictionary<string, string> BuildDatumPayload(string action) => new()
    {
        ["action"] = action,
        ["datum_letter"] = string.IsNullOrWhiteSpace(TxtDatumLetter.Text) ? "A" : TxtDatumLetter.Text.Trim(),
        ["datum_auto_inc"] = (ChkDatumAutoInc.IsChecked == true ? 1 : 0).ToString(),
        ["datum_height"] = ParseDouble(TxtDatumHeight.Text, 3.5).ToString(CultureInfo.InvariantCulture),
        ["datum_frame_color"] = _datumFrameColor.ToString(),
        ["datum_text_color"] = _datumTextColor.ToString()
    };

    private async void BtnOpenConfig_Click(object sender, RoutedEventArgs e)
    {
        await SendRequestAsync(new Dictionary<string, string> { ["action"] = "open_config" }, "正在打开预设配置文件...");
    }

    private async Task<Dictionary<string, string>?> SendRequestAsync(Dictionary<string, string> payload, string busyMessage)
    {
        if (_busy) return null;
        if (string.IsNullOrEmpty(_sessionId) || string.IsNullOrEmpty(_ipcDir))
        {
            StatusText.Text = "独立预览模式（未连接 NX 会话）。";
            return null;
        }

        _busy = true;
        StatusText.Text = busyMessage;

        var tcs = new TaskCompletionSource<bool>();
        _activeRequestTask = tcs.Task;

        try
        {
            string reqPath = System.IO.Path.Combine(_ipcDir, "request.ini");
            string resPath = System.IO.Path.Combine(_ipcDir, "response.ini");

            var sb = new StringBuilder();
            foreach (var kv in payload) sb.AppendLine($"{kv.Key}={kv.Value}");
            File.WriteAllText(reqPath, sb.ToString(), new UTF8Encoding(false));

            var result = await Task.Run(() =>
            {
                using var reqEv = EventWaitHandle.OpenExisting($"Local\\Zhihui_GCBZ_Req_{_sessionId}");
                using var resEv = EventWaitHandle.OpenExisting($"Local\\Zhihui_GCBZ_Res_{_sessionId}");
                resEv.Reset();
                reqEv.Set();
                resEv.WaitOne();

                var map = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
                if (File.Exists(resPath))
                {
                    foreach (var line in File.ReadAllLines(resPath, Encoding.UTF8))
                    {
                        int eq = line.IndexOf('=');
                        if (eq > 0) map[line[..eq].Trim()] = line[(eq + 1)..].Trim();
                    }
                }
                return map;
            });

            if (result.TryGetValue("message", out var msg) && !string.IsNullOrEmpty(msg))
            {
                StatusText.Text = msg;
            }
            return result;
        }
        catch (Exception ex)
        {
            StatusText.Text = $"与 NX 通信异常：{ex.Message}";
            return null;
        }
        finally
        {
            _busy = false;
            tcs.TrySetResult(true);
            SetActionsEnabled(true);
        }
    }

    private void SetActionsEnabled(bool enabled)
    {
        BtnDimContinuous.IsEnabled = enabled;
        BtnDimBatch.IsEnabled = enabled;
        BtnGdtPlace.IsEnabled = enabled;
        BtnGdtUpdateLast.IsEnabled = enabled && _hasLastGdt;
        BtnDatumPlace.IsEnabled = enabled;
        BtnDatumUpdateLast.IsEnabled = enabled && _hasLastDatum;
    }

    private static double ParseDouble(string text, double fallback)
    {
        return double.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out double v) ? v : fallback;
    }

    private class UiState
    {
        public int ActiveTab { get; set; }
        public int DimSubmode { get; set; }
        public int DimPresetIndex { get; set; } = 1;
        public int HoleFit { get; set; } = 1;
        public int ShaftFit { get; set; } = 5;
        public int FitStyle { get; set; }
        public int Gb1804 { get; set; } = 1;
        public string UpperTol { get; set; } = "0.10";
        public string LowerTol { get; set; } = "-0.10";
        public int Prefix { get; set; }
        public int Suffix { get; set; }
        public bool RefDim { get; set; }
        public int GdtChar { get; set; } = 1;
        public int GdtZone { get; set; }
        public string GdtVal { get; set; } = "0.1";
        public int GdtMat { get; set; }
        public int Gb1184 { get; set; }
        public int GdtD1 { get; set; }
        public int GdtD2 { get; set; }
        public int GdtD3 { get; set; }
        public string GdtHeight { get; set; } = "3.5";
        public int GdtFrameColor { get; set; } = 0;
        public int GdtTextColor { get; set; } = 0;
        public string DatumLetter { get; set; } = "A";
        public bool DatumAutoInc { get; set; } = true;
        public string DatumHeight { get; set; } = "3.5";
        public int DatumFrameColor { get; set; } = 0;
        public int DatumTextColor { get; set; } = 0;
    }

    private void LoadState()
    {
        try
        {
            if (!File.Exists(ConfigPath)) return;
            var s = JsonSerializer.Deserialize<UiState>(File.ReadAllText(ConfigPath, Encoding.UTF8));
            if (s == null) return;

            TabDim.IsChecked = s.ActiveTab == 0;
            TabGdt.IsChecked = s.ActiveTab == 1;
            TabDatum.IsChecked = s.ActiveTab == 2;
            PanelDim.Visibility = s.ActiveTab == 0 ? Visibility.Visible : Visibility.Collapsed;
            PanelGdt.Visibility = s.ActiveTab == 1 ? Visibility.Visible : Visibility.Collapsed;
            PanelDatum.Visibility = s.ActiveTab == 2 ? Visibility.Visible : Visibility.Collapsed;

            _dimSubmode = Math.Clamp(s.DimSubmode, 0, 6);
            _dimPresetIndex = Math.Clamp(s.DimPresetIndex, 0, DimPresets.Length - 1);
            CmbHoleFit.SelectedIndex = Math.Clamp(s.HoleFit, 0, HoleFits.Length - 1);
            CmbShaftFit.SelectedIndex = Math.Clamp(s.ShaftFit, 0, ShaftFits.Length - 1);
            CmbFitStyle.SelectedIndex = Math.Clamp(s.FitStyle, 0, 3);
            CmbGb1804.SelectedIndex = Math.Clamp(s.Gb1804, 0, 3);
            TxtUpperTol.Text = s.UpperTol;
            TxtLowerTol.Text = s.LowerTol;
            CmbPrefix.SelectedIndex = Math.Clamp(s.Prefix, 0, 6);
            CmbSuffix.SelectedIndex = Math.Clamp(s.Suffix, 0, 5);
            ChkRefDim.IsChecked = s.RefDim;

            _gdtChar = Math.Clamp(s.GdtChar, 0, GdtSymbols.Length - 1);
            CmbGdtZone.SelectedIndex = Math.Clamp(s.GdtZone, 0, 3);
            TxtGdtVal.Text = s.GdtVal;
            CmbGdtMat.SelectedIndex = Math.Clamp(s.GdtMat, 0, 2);
            CmbGb1184.SelectedIndex = Math.Clamp(s.Gb1184, 0, 3);
            CmbGdtD1.SelectedIndex = Math.Clamp(s.GdtD1, 0, DatumChoices.Length - 1);
            CmbGdtD2.SelectedIndex = Math.Clamp(s.GdtD2, 0, DatumChoices.Length - 1);
            CmbGdtD3.SelectedIndex = Math.Clamp(s.GdtD3, 0, DatumChoices.Length - 1);
            TxtGdtHeight.Text = s.GdtHeight;
            _gdtFrameColor = Math.Clamp(s.GdtFrameColor, 0, 216);
            _gdtTextColor = Math.Clamp(s.GdtTextColor, 0, 216);
            TxtGdtFrameColor.Text = _gdtFrameColor.ToString();
            TxtGdtTextColor.Text = _gdtTextColor.ToString();

            TxtDatumLetter.Text = string.IsNullOrWhiteSpace(s.DatumLetter) ? "A" : s.DatumLetter;
            ChkDatumAutoInc.IsChecked = s.DatumAutoInc;
            TxtDatumHeight.Text = s.DatumHeight;
            _datumFrameColor = Math.Clamp(s.DatumFrameColor, 0, 216);
            _datumTextColor = Math.Clamp(s.DatumTextColor, 0, 216);
            TxtDatumFrameColor.Text = _datumFrameColor.ToString();
            TxtDatumTextColor.Text = _datumTextColor.ToString();
        }
        catch { }
    }

    private void SaveState()
    {
        try
        {
            Directory.CreateDirectory(System.IO.Path.GetDirectoryName(ConfigPath)!);
            var s = new UiState
            {
                ActiveTab = TabGdt.IsChecked == true ? 1 : (TabDatum.IsChecked == true ? 2 : 0),
                DimSubmode = _dimSubmode,
                DimPresetIndex = _dimPresetIndex,
                HoleFit = Math.Max(0, CmbHoleFit.SelectedIndex),
                ShaftFit = Math.Max(0, CmbShaftFit.SelectedIndex),
                FitStyle = Math.Max(0, CmbFitStyle.SelectedIndex),
                Gb1804 = Math.Max(0, CmbGb1804.SelectedIndex),
                UpperTol = TxtUpperTol.Text,
                LowerTol = TxtLowerTol.Text,
                Prefix = Math.Max(0, CmbPrefix.SelectedIndex),
                Suffix = Math.Max(0, CmbSuffix.SelectedIndex),
                RefDim = ChkRefDim.IsChecked == true,
                GdtChar = _gdtChar,
                GdtZone = Math.Max(0, CmbGdtZone.SelectedIndex),
                GdtVal = TxtGdtVal.Text,
                GdtMat = Math.Max(0, CmbGdtMat.SelectedIndex),
                Gb1184 = Math.Max(0, CmbGb1184.SelectedIndex),
                GdtD1 = Math.Max(0, CmbGdtD1.SelectedIndex),
                GdtD2 = Math.Max(0, CmbGdtD2.SelectedIndex),
                GdtD3 = Math.Max(0, CmbGdtD3.SelectedIndex),
                GdtHeight = TxtGdtHeight.Text,
                GdtFrameColor = _gdtFrameColor,
                GdtTextColor = _gdtTextColor,
                DatumLetter = TxtDatumLetter.Text,
                DatumAutoInc = ChkDatumAutoInc.IsChecked == true,
                DatumHeight = TxtDatumHeight.Text,
                DatumFrameColor = _datumFrameColor,
                DatumTextColor = _datumTextColor
            };
            File.WriteAllText(ConfigPath, JsonSerializer.Serialize(s, new JsonSerializerOptions { WriteIndented = true }), new UTF8Encoding(false));
        }
        catch { }
    }
}
