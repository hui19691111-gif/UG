using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;

namespace Zhaofu.Nx.SheetMetal.BendFactorBatch.Shared
{
    public static class BendFactorDefaultTables
    {
        public static List<ConditionTableRowRecord> CreateDefaultConditionRows()
        {
            return new List<ConditionTableRowRecord>
            {
                new ConditionTableRowRecord { ConditionKey = "= 90", AngleLabel = "90", SmallArcCode = "Q1", LargeArcCode = "K1" },
                new ConditionTableRowRecord { ConditionKey = "∈", AngleLabel = "(0,90)", SmallArcCode = "K2", LargeArcCode = "K1" },
                new ConditionTableRowRecord { ConditionKey = "∈", AngleLabel = "(90,180)", SmallArcCode = "K1", LargeArcCode = "K1" },
                new ConditionTableRowRecord { ConditionKey = "∈", AngleLabel = "(180,360)", SmallArcCode = "K1", LargeArcCode = "K1" }
            };
        }

        public static List<CoefficientTableRowRecord> CreateDefaultCoefficientRows()
        {
            return new List<CoefficientTableRowRecord>
            {
                Row("材质 <未指定>", 0.5, 0.85, 0.85, 0.86, 0.4, 0.24, 0.63, 0.85, 0.85, 0.85, null, "f1", "f1"),
                Row("材质 <未指定>", 0.8, 1.35, 1.35, 1.36, 0.4, 0.24, 0.63, 1.35, 1.35, 1.35, null, "f1", "f1"),
                Row("材质 <未指定>", 1, 1.7, 1.7, 1.8, 0.5, 0.24, 0.63, 1.7, 1.7, 1.7, null, "f1", "f1"),
                Row("材质 <未指定>", 1.2, 2, 2, 2, 0.5, 0.24, 0.63, 2, 2, 2, null, "f1", "f1"),
                Row("材质 <未指定>", 1.5, 2.5, 2.5, 2.5, 0.5, 0.24, 0.612, 2.6, 2.6, 2.6, null, "f1", "f1"),
                Row("材质 <未指定>", 1.8, 3.1, 3.1, 3.2, 0.5, 0.24, 0.63, 3.1, 3.1, 3.1, null, "f1", "f1"),
                Row("材质 <未指定>", 2, 3.3, 3.3, 3.3, 0.52, 0.24, 0.63, 3.51, 3.51, 3.51, null, "f1", "f1"),
                Row("材质 <未指定>", 2.5, 4.3, 4.3, 4.4, 0.5, 0.24, 0.63, 4.3, 4.3, 4.3, null, "f1", "f1"),
                Row("材质 <未指定>", 3, 4.8, 4.8, 4.8, 0.5, 0.24, 0.63, 5, 5, 5, null, "f1", "f1"),
                Row("材质 <未指定>", 3.2, 5.4, 5.4, 5.5, 0.5, 0.24, 0.63, 5.4, 5.4, 5.4, null, "f1", "f1"),
                Row("材质 <未指定>", 3.5, 5.9, 5.9, 5.1, 0.5, 0.24, 0.63, 5.9, 5.9, 5.9, null, "f1", "f1"),
                Row("材质 <未指定>", 4, 6.4, 6.4, 6.4, 0.5, 0.24, 0.63, 6.8, 6.8, 6.8, null, "f1", "f1"),
                Row("材质 <未指定>", 4.5, 7.6, 7.6, 7.7, 0.5, 0.24, 0.63, 7.6, 7.6, 7.6, null, "f1", "f1"),
                Row("材质 <未指定>", 5, 8.2, 8.2, 8.2, 0.5, 0.24, 0.63, 8.5, 8.5, 8.5, null, "f1", "f1"),
                Row("材质 <未指定>", 6, 9.8, 9.8, 9.8, 0.5, 0.24, 0.63, 9.9, 9.9, 9.9, null, "f1", "f1"),
                Row("材质 <未指定>", 8, 13.3, 13.3, 13.4, 0.5, 0.24, 0.63, 13.3, 13.3, 13.3, null, "f1", "f1"),
                Row("材质 <未指定>", 10, 16.8, 16.81, 16.82, 0.5, 0.24, 0.56, 16.8, 16.8, 16.8, null, "f1", "f1"),
                Row("材质 <未指定>", 12, 20, 20, 21, 0.5, 0.24, 0.63, 20, 20, 20, null, "f1", "f1"),
                Row("材质 <未指定>", 14, 24, 24, 25, 0.5, 0.24, 0.63, 24, 24, 24, null, "f1", "f1"),
                Row("材质 <未指定>", 20, 28, 28, 29, 0.5, 0.24, 0.63, 28, 28, 28, null, "f1", "f1"),
                Row("材质 <未指定>", 0.5, 0.85, 0.85, 0.86, 0.4, 0.24, 0.63, 0.85, 0.85, 0.85, null, "f1", "f1"),
                Row("材质 <未指定>", 0.8, 1.35, 1.35, 1.36, 0.4, 0.24, 0.63, 1.35, 1.35, 1.35, null, "f1", "f1"),
                Row("材质 <未指定>", 1, 1.7, 1.7, 1.8, 0.5, 0.24, 0.63, 1.7, 1.7, 1.7, null, "f1", "f1"),
                Row("材质 <未指定>", 1.2, 2, 2, 2.1, 0.5, 0.24, 0.63, 2, 2, 2, null, "f1", "f1"),
                Row("材质 <未指定>", 1.5, 2.5, 2.5, 2.6, 0.5, 0.24, 0.612, 2.5, 2.5, 2.5, null, "f1", "f1"),
                Row("Q235", 1.8, 3.1, 3.1, 3.2, 0.5, 0.24, 0.63, 3.1, 3.1, 3.1, null, "f1", "f1"),
                Row("Q235", 2, 3.5, 3.5, 3.6, 0.5, 0.24, 0.63, 3.5, 3.5, 3.5, null, "f1", "f1"),
                Row("Q235", 2.5, 4.2, 4.2, 4.3, 0.5, 0.24, 0.63, 4.2, 4.2, 4.2, null, "f1", "f1"),
                Row("Q235", 3, 5, 5, 6, 0.5, 0.24, 0.63, 5, 5, 5, null, "f1", "f1"),
                Row("Q235", 3.2, 5.5, 5.5, 5.6, 0.5, 0.24, 0.63, 5.5, 5.5, 5.5, null, "f1", "f1"),
                Row("Q235", 3.5, 6, 6, 7, 0.5, 0.24, 0.63, 6, 6, 6, null, "f1", "f1"),
                Row("Q235", 4, 6.8, 6.8, 6.9, 0.5, 0.24, 0.63, 6.8, 6.8, 6.8, null, "f1", "f1"),
                Row("Q235", 4.5, 7.6, 7.6, 7.7, 0.5, 0.24, 0.63, 7.6, 7.6, 7.6, null, "f1", "f1"),
                Row("Q235", 5, 8.5, 8.5, 8.6, 0.5, 0.24, 0.63, 8.5, 8.5, 8.5, null, "f1", "f1"),
                Row("Q235", 6, 10, 10, 11, 0.5, 0.24, 0.63, 10, 10, 10, null, "f1", "f1"),
                Row("Q235", 8, 13, 13, 14, 0.5, 0.24, 0.63, 13, 13, 13, null, "f1", "f1"),
                Row("Q235", 10, 17, 17, 18, 0.5, 0.24, 0.552, 17, 17, 17, null, "f1", "f1"),
                Row("Q235", 12, 20, 20, 21, 0.5, 0.24, 0.63, 20, 20, 20, null, "f1", "f1"),
                Row("Q235", 14, 24, 24, 25, 0.5, 0.24, 0.63, 24, 24, 24, null, "f1", "f1"),
                Row("Q235", 16, 28, 28, 29, 0.5, 0.24, 0.63, 28, 28, 28, null, "f1", "f1"),
                Row("自定义材质3", 0.5, 0.85, 0.85, 0.86, 0.4, 0.24, 0.63, 0.85, 0.85, 0.85, null, "f1", "f1"),
                Row("自定义材质3", 0.8, 1.35, 1.35, 1.36, 0.4, 0.24, 0.63, 1.35, 1.35, 1.35, null, "f1", "f1"),
                Row("自定义材质3", 1, 1.7, 1.7, 1.8, 0.5, 0.24, 0.63, 1.7, 1.7, 1.7, null, "f1", "f1"),
                Row("自定义材质3", 1.2, 2, 2, 2.1, 0.5, 0.24, 0.63, 2, 2, 2, null, "f1", "f1"),
                Row("自定义材质3", 1.5, 2.5, 2.5, 2.6, 0.5, 0.24, 0.612, 2.5, 2.5, 2.5, null, "f1", "f1"),
                Row("自定义材质3", 1.8, 3.1, 3.1, 3.2, 0.5, 0.24, 0.63, 3.1, 3.1, 3.1, null, "f1", "f1"),
                Row("自定义材质3", 2, 3.5, 3.5, 3.6, 0.5, 0.24, 0.63, 3.5, 3.5, 3.5, null, "f1", "f1"),
                Row("自定义材质3", 2.5, 4.2, 4.2, 4.3, 0.5, 0.24, 0.63, 4.2, 4.2, 4.2, null, "f1", "f1"),
                Row("自定义材质3", 3, 5, 5, 6, 0.5, 0.24, 0.63, 5, 5, 5, null, "f1", "f1"),
                Row("自定义材质3", 3.2, 5.5, 5.5, 5.6, 0.5, 0.24, 0.63, 5.5, 5.5, 5.5, null, "f1", "f1"),
                Row("自定义材质3", 3.5, 6, 6, 7, 0.5, 0.24, 0.63, 6, 6, 6, null, "f1", "f1"),
                Row("自定义材质3", 4, 6.8, 6.8, 6.9, 0.5, 0.24, 0.63, 6.8, 6.8, 6.8, null, "f1", "f1"),
                Row("自定义材质4", 4.5, 7.6, 7.6, 7.7, 0.5, 0.24, 0.63, 7.6, 7.6, 7.6, null, "f1", "f1"),
                Row("自定义材质5", 5, 8.5, 8.5, 8.6, 0.5, 0.24, 0.63, 8.5, 8.5, 8.5, null, "f1", "f1"),
                Row("自定义材质3", 6, 10, 10, 11, 0.5, 0.24, 0.63, 10, 10, 10, null, "f1", "f1"),
                Row("自定义材质3", 8, 13, 13, 14, 0.5, 0.24, 0.63, 13, 13, 13, null, "f1", "f1"),
                Row("自定义材质3", 10, 17, 17, 18, 0.5, 0.24, 0.56, 17, 17, 17, null, "f1", "f1"),
                Row("自定义材质3", 12, 20, 20, 21, 0.5, 0.24, 0.63, 20, 20, 20, null, "f1", "f1"),
                Row("自定义材质3", 14, 24, 24, 25, 0.5, 0.24, 0.63, 24, 24, 24, null, "f1", "f1")
            };
        }

        public static List<BendRuleRecord> CreateDefaultRules()
        {
            return BuildRules(CreateDefaultConditionRows(), CreateDefaultCoefficientRows());
        }

        public static List<BendRuleRecord> BuildRules(IEnumerable<ConditionTableRowRecord> conditionRows, IEnumerable<CoefficientTableRowRecord> coefficientRows)
        {
            List<BendRuleRecord> rules = new List<BendRuleRecord>();
            List<ConditionTableRowRecord> conditions = (conditionRows ?? Enumerable.Empty<ConditionTableRowRecord>())
                .Where(item => item != null)
                .ToList();

            foreach (CoefficientTableRowRecord row in coefficientRows ?? Enumerable.Empty<CoefficientTableRowRecord>())
            {
                if (row == null || row.Thickness <= 0.0)
                {
                    continue;
                }

                string material = string.IsNullOrWhiteSpace(row.Material) ? "材质 <未指定>" : row.Material.Trim();
                double halfWidth = Math.Max(0.05, row.Thickness * 0.05);
                double? radiusMin = row.R.HasValue ? Math.Max(0.0, row.R.Value - halfWidth) : (double?)null;
                double? radiusMax = row.R.HasValue ? row.R.Value + halfWidth : (double?)null;

                foreach (ConditionTableRowRecord condition in conditions)
                {
                    AddRule(rules, row, material, row.Thickness - halfWidth, row.Thickness + halfWidth, radiusMin, radiusMax, condition, false, condition.SmallArcCode);
                    AddRule(rules, row, material, row.Thickness - halfWidth, row.Thickness + halfWidth, radiusMin, radiusMax, condition, true, condition.LargeArcCode);
                }

                if (row.K1.HasValue)
                {
                    rules.Add(new BendRuleRecord
                    {
                        Enabled = true,
                        RuleName = row.Thickness.ToString("0.###", CultureInfo.InvariantCulture) + " 压死边",
                        Material = material,
                        ThicknessMin = row.Thickness - halfWidth,
                        ThicknessMax = row.Thickness + halfWidth,
                        Method = "KFactor",
                        Value = row.K1.Value,
                        RadiusOverride = row.R,
                        Note = "内置完整系数表生成[PressEdge][K1]"
                    });
                }
            }

            return rules;
        }

        private static void AddRule(ICollection<BendRuleRecord> rules, CoefficientTableRowRecord row, string material, double thicknessMin, double thicknessMax, double? radiusMin, double? radiusMax, ConditionTableRowRecord condition, bool largeArc, string code)
        {
            if (rules == null || row == null || condition == null || string.IsNullOrWhiteSpace(code))
            {
                return;
            }

            double? value = ResolveCodeValue(row, code);
            if (!value.HasValue)
            {
                return;
            }

            GetAngleRange(condition.AngleLabel, out double? angleMin, out double? angleMax);
            string normalizedCode = code.Trim().ToUpperInvariant();
            bool isDeduction = normalizedCode.StartsWith("Q", StringComparison.OrdinalIgnoreCase);
            string bendTypeLabel = largeArc ? "大圆弧" : "小圆弧";

            rules.Add(new BendRuleRecord
            {
                Enabled = true,
                RuleName = row.Thickness.ToString("0.###", CultureInfo.InvariantCulture) + "_" + bendTypeLabel + "_" + condition.AngleLabel + "_" + normalizedCode,
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
                    ? "内置完整系数表生成[LargeArc][" + normalizedCode + "]"
                    : "内置完整系数表生成[SmallArc][" + normalizedCode + "]"
            });
        }

        private static double? ResolveCodeValue(CoefficientTableRowRecord row, string code)
        {
            switch ((code ?? string.Empty).Trim().ToUpperInvariant())
            {
                case "Q1": return row.Q1;
                case "Q2": return row.Q2;
                case "Q3": return row.Q3;
                case "K1": return row.K1;
                case "K2": return row.K2;
                case "K3": return row.K3;
                case "A1": return row.A1;
                case "A2": return row.A2;
                case "A3": return row.A3;
                default: return null;
            }
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

        private static CoefficientTableRowRecord Row(string material, double thickness, double? q1, double? q2, double? q3, double? k1, double? k2, double? k3, double? a1, double? a2, double? a3, double? r, string f1, string f2)
        {
            return new CoefficientTableRowRecord
            {
                Material = material,
                Thickness = thickness,
                Q1 = q1,
                Q2 = q2,
                Q3 = q3,
                K1 = k1,
                K2 = k2,
                K3 = k3,
                A1 = a1,
                A2 = a2,
                A3 = a3,
                R = r,
                F1 = f1,
                F2 = f2
            };
        }
    }
}
