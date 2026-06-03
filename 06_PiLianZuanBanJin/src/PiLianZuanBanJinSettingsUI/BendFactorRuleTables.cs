using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using Zhaofu.Nx.SheetMetal.BendFactorBatch.Shared;

namespace ZhsmToolboxSettingsUI;

internal static class BendFactorRuleTables
{
    public static IReadOnlyList<ConditionTableRowViewModel> CreateDefaultConditionRows()
    {
        return BendFactorDefaultTables.CreateDefaultConditionRows()
            .Select(ConditionTableRowViewModel.FromRecord)
            .ToList();
    }

    public static IReadOnlyList<CoefficientTableRowModel> CreateDefaultCoefficientRows()
    {
        return BendFactorDefaultTables.CreateDefaultCoefficientRows()
            .Select(CoefficientTableRowModel.FromRecord)
            .ToList();
    }

    public static List<BendRuleRecord> BuildRules(
        IEnumerable<ConditionTableRowViewModel> conditionRows,
        IEnumerable<CoefficientTableRowModel> coefficientRows)
    {
        List<ConditionTableRowRecord> conditions = (conditionRows ?? Enumerable.Empty<ConditionTableRowViewModel>())
            .Where(item => item != null)
            .Select(item => item.ToRecord())
            .ToList();
        List<CoefficientTableRowRecord> coefficients = (coefficientRows ?? Enumerable.Empty<CoefficientTableRowModel>())
            .Where(item => item != null)
            .Select(item => item.ToRecord())
            .ToList();

        return BendFactorDefaultTables.BuildRules(conditions, coefficients);
    }

    private static void AddRule(
        ICollection<BendRuleRecord> rules,
        CoefficientTableRowModel row,
        string material,
        double thicknessMin,
        double thicknessMax,
        double? radiusMin,
        double? radiusMax,
        ConditionTableRowViewModel condition,
        bool largeArc,
        string code)
    {
        if (string.IsNullOrWhiteSpace(code))
        {
            return;
        }

        double? value = ResolveCodeValue(row, code);
        if (!value.HasValue)
        {
            return;
        }

        GetAngleRange(condition.AngleLabel, out double? angleMin, out double? angleMax);
        bool isDeduction = code.StartsWith("Q", StringComparison.OrdinalIgnoreCase);
        string bendTypeLabel = largeArc ? "大圆弧" : "小圆弧";
        string normalizedCode = code.Trim().ToUpperInvariant();

        rules.Add(new BendRuleRecord
        {
            Enabled = true,
            RuleName = $"{row.Thickness:0.###}_{bendTypeLabel}_{condition.AngleLabel}_{normalizedCode}",
            Material = material,
            AngleMinDeg = angleMin,
            AngleMaxDeg = angleMax,
            ThicknessMin = thicknessMin,
            ThicknessMax = thicknessMax,
            InnerRadiusMin = radiusMin,
            InnerRadiusMax = radiusMax,
            Method = isDeduction ? "Deduction" : "KFactor",
            Value = value.Value,
            RadiusOverride = row.R,
            Note = largeArc
                ? $"设置窗口条件表生成[LargeArc][{normalizedCode}]"
                : $"设置窗口条件表生成[SmallArc][{normalizedCode}]"
        });
    }

    private static double? ResolveCodeValue(CoefficientTableRowModel row, string code)
    {
        return code.Trim().ToUpperInvariant() switch
        {
            "Q1" => row.Q1,
            "Q2" => row.Q2,
            "Q3" => row.Q3,
            "K1" => row.K1,
            "K2" => row.K2,
            "K3" => row.K3,
            "A1" => row.A1,
            "A2" => row.A2,
            "A3" => row.A3,
            _ => null
        };
    }

    private static void GetAngleRange(string angleLabel, out double? angleMin, out double? angleMax)
    {
        angleMin = null;
        angleMax = null;
        switch ((angleLabel ?? string.Empty).Trim())
        {
            case "90":
                angleMin = 89.999;
                angleMax = 90.001;
                break;
            case "(0,90)":
                angleMin = 0.001;
                angleMax = 89.999;
                break;
            case "(90,180)":
                angleMin = 90.001;
                angleMax = 179.999;
                break;
            case "180":
                angleMin = 179.999;
                angleMax = 180.001;
                break;
            case "(180,360)":
                angleMin = 180.001;
                angleMax = 360.0;
                break;
        }
    }

    private static CoefficientTableRowModel Row(
        double thickness,
        double q1,
        double q2,
        double q3,
        double k1,
        double k2,
        double k3,
        double a)
    {
        return new CoefficientTableRowModel
        {
            Material = "材质 <未指定>",
            Thickness = thickness,
            Q1 = q1,
            Q2 = q2,
            Q3 = q3,
            K1 = k1,
            K2 = k2,
            K3 = k3,
            A1 = a,
            A2 = a,
            A3 = a,
            F1 = "f1",
            F2 = "f1"
        };
    }
}
