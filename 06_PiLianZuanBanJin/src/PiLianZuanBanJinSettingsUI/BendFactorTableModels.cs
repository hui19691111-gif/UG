using System.Collections.Generic;
using System.ComponentModel;
using System.Runtime.CompilerServices;
using Zhaofu.Nx.SheetMetal.BendFactorBatch.Shared;

namespace ZhsmToolboxSettingsUI;

public sealed class ConditionTableRowViewModel : INotifyPropertyChanged
{
    private string _conditionKey = string.Empty;
    private string _angleLabel = string.Empty;
    private string _smallArcCode = string.Empty;
    private string _largeArcCode = string.Empty;

    public event PropertyChangedEventHandler? PropertyChanged;

    public string ConditionKey { get => _conditionKey; set => SetField(ref _conditionKey, value); }
    public string AngleLabel { get => _angleLabel; set => SetField(ref _angleLabel, value); }
    public string SmallArcCode { get => _smallArcCode; set => SetField(ref _smallArcCode, value); }
    public string LargeArcCode { get => _largeArcCode; set => SetField(ref _largeArcCode, value); }

    public ConditionTableRowRecord ToRecord()
    {
        return new ConditionTableRowRecord
        {
            ConditionKey = ConditionKey?.Trim() ?? string.Empty,
            AngleLabel = AngleLabel?.Trim() ?? string.Empty,
            SmallArcCode = SmallArcCode?.Trim() ?? string.Empty,
            LargeArcCode = LargeArcCode?.Trim() ?? string.Empty
        };
    }

    public static ConditionTableRowViewModel FromRecord(ConditionTableRowRecord? record)
    {
        return new ConditionTableRowViewModel
        {
            ConditionKey = record?.ConditionKey ?? string.Empty,
            AngleLabel = record?.AngleLabel ?? string.Empty,
            SmallArcCode = record?.SmallArcCode ?? string.Empty,
            LargeArcCode = record?.LargeArcCode ?? string.Empty
        };
    }

    private void SetField<T>(ref T field, T value, [CallerMemberName] string? propertyName = null)
    {
        if (EqualityComparer<T>.Default.Equals(field, value))
        {
            return;
        }

        field = value;
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
    }
}

public sealed class CoefficientTableRowModel
{
    public string Material { get; set; } = "材质 <未指定>";
    public double Thickness { get; set; }
    public double? Q1 { get; set; }
    public double? Q2 { get; set; }
    public double? Q3 { get; set; }
    public double? K1 { get; set; }
    public double? K2 { get; set; }
    public double? K3 { get; set; }
    public double? A1 { get; set; }
    public double? A2 { get; set; }
    public double? A3 { get; set; }
    public double? R { get; set; }
    public string F1 { get; set; } = "f1";
    public string F2 { get; set; } = "f1";

    public CoefficientTableRowRecord ToRecord()
    {
        return new CoefficientTableRowRecord
        {
            Material = string.IsNullOrWhiteSpace(Material) ? "材质 <未指定>" : Material.Trim(),
            Thickness = Thickness,
            Q1 = Q1,
            Q2 = Q2,
            Q3 = Q3,
            K1 = K1,
            K2 = K2,
            K3 = K3,
            A1 = A1,
            A2 = A2,
            A3 = A3,
            R = R,
            F1 = string.IsNullOrWhiteSpace(F1) ? "f1" : F1.Trim(),
            F2 = string.IsNullOrWhiteSpace(F2) ? "f1" : F2.Trim()
        };
    }

    public static CoefficientTableRowModel FromRecord(CoefficientTableRowRecord? record)
    {
        return new CoefficientTableRowModel
        {
            Material = record?.Material ?? "材质 <未指定>",
            Thickness = record?.Thickness ?? 0.0,
            Q1 = record?.Q1,
            Q2 = record?.Q2,
            Q3 = record?.Q3,
            K1 = record?.K1,
            K2 = record?.K2,
            K3 = record?.K3,
            A1 = record?.A1,
            A2 = record?.A2,
            A3 = record?.A3,
            R = record?.R,
            F1 = record?.F1 ?? "f1",
            F2 = record?.F2 ?? "f1"
        };
    }
}
