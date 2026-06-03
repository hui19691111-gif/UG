using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text;
using Microsoft.Win32;

namespace ZhsmToolboxSettingsUI;

internal static class CoefficientTableInterop
{
    private static readonly string[] Headers =
    {
        "材质", "厚度", "Q1", "Q2", "Q3", "K1", "K2", "K3", "A1", "A2", "A3", "R", "F1", "F2"
    };

    public static IReadOnlyList<CoefficientTableRowModel> LoadRows(string path)
    {
        if (string.IsNullOrWhiteSpace(path) || !File.Exists(path))
        {
            return Array.Empty<CoefficientTableRowModel>();
        }

        return string.Equals(Path.GetExtension(path), ".csv", StringComparison.OrdinalIgnoreCase)
            ? LoadRowsFromCsv(path)
            : LoadRowsFromExcelCom(path);
    }

    public static void ExportRows(string path, IReadOnlyList<CoefficientTableRowModel> rows)
    {
        if (string.IsNullOrWhiteSpace(path))
        {
            return;
        }

        if (string.Equals(Path.GetExtension(path), ".csv", StringComparison.OrdinalIgnoreCase))
        {
            ExportRowsToCsv(path, rows);
            return;
        }

        ExportRowsToExcelCom(path, rows);
    }

    public static string PickImportPath(string initialPath)
    {
        OpenFileDialog dialog = new OpenFileDialog
        {
            Title = "选择系数表",
            Filter = "系数表|*.xls;*.xlsx;*.csv|Excel 工作簿|*.xls;*.xlsx|CSV 文件|*.csv",
            FileName = initialPath ?? string.Empty,
            CheckFileExists = true
        };

        return dialog.ShowDialog() == true ? dialog.FileName : string.Empty;
    }

    public static string PickExportPath(string initialPath)
    {
        SaveFileDialog dialog = new SaveFileDialog
        {
            Title = "导出系数表",
            Filter = "Excel 97-2003 工作簿|*.xls|Excel 工作簿|*.xlsx|CSV 文件|*.csv",
            FileName = string.IsNullOrWhiteSpace(initialPath) ? "系数表导出.xls" : Path.GetFileName(initialPath),
            AddExtension = true,
            DefaultExt = "xls"
        };

        if (!string.IsNullOrWhiteSpace(initialPath))
        {
            dialog.InitialDirectory = Path.GetDirectoryName(initialPath);
        }

        return dialog.ShowDialog() == true ? dialog.FileName : string.Empty;
    }

    private static IReadOnlyList<CoefficientTableRowModel> LoadRowsFromCsv(string path)
    {
        List<CoefficientTableRowModel> rows = new List<CoefficientTableRowModel>();
        string[] lines = File.ReadAllLines(path, Encoding.UTF8);
        for (int index = 1; index < lines.Length; index++)
        {
            CoefficientTableRowModel row = ParseRow(ParseCsvLine(lines[index]));
            if (row.Thickness > 0.0)
            {
                rows.Add(row);
            }
        }

        return rows;
    }

    private static void ExportRowsToCsv(string path, IReadOnlyList<CoefficientTableRowModel> rows)
    {
        string directory = Path.GetDirectoryName(path) ?? string.Empty;
        if (!string.IsNullOrWhiteSpace(directory))
        {
            Directory.CreateDirectory(directory);
        }

        StringBuilder builder = new StringBuilder();
        builder.AppendLine(string.Join(",", Headers));
        foreach (CoefficientTableRowModel row in rows ?? Array.Empty<CoefficientTableRowModel>())
        {
            string[] cells =
            {
                CsvCell(row.Material),
                CsvCell(row.Thickness),
                CsvCell(row.Q1),
                CsvCell(row.Q2),
                CsvCell(row.Q3),
                CsvCell(row.K1),
                CsvCell(row.K2),
                CsvCell(row.K3),
                CsvCell(row.A1),
                CsvCell(row.A2),
                CsvCell(row.A3),
                CsvCell(row.R),
                CsvCell(row.F1),
                CsvCell(row.F2)
            };
            builder.AppendLine(string.Join(",", cells));
        }

        File.WriteAllText(path, builder.ToString(), new UTF8Encoding(false));
    }

    private static IReadOnlyList<CoefficientTableRowModel> LoadRowsFromExcelCom(string path)
    {
        List<CoefficientTableRowModel> rows = new List<CoefficientTableRowModel>();
        Type? excelType = Type.GetTypeFromProgID("Excel.Application");
        if (excelType == null)
        {
            throw new InvalidOperationException("当前系统未安装 Excel，无法读取 xls/xlsx。");
        }

        dynamic? excel = null;
        dynamic? workbook = null;
        try
        {
            excel = Activator.CreateInstance(excelType) ?? throw new InvalidOperationException("无法启动 Excel。");
            excel.Visible = false;
            excel.DisplayAlerts = false;
            workbook = excel.Workbooks.Open(path);
            dynamic worksheet = workbook.Worksheets(1);
            int rowCount = worksheet.UsedRange.Rows.Count;
            int columnCount = worksheet.UsedRange.Columns.Count;
            for (int rowIndex = 2; rowIndex <= rowCount; rowIndex++)
            {
                string[] cells = new string[Math.Max(columnCount, Headers.Length)];
                for (int columnIndex = 1; columnIndex <= cells.Length; columnIndex++)
                {
                    object? value = columnIndex <= columnCount ? worksheet.Cells(rowIndex, columnIndex).Value : null;
                    cells[columnIndex - 1] = value == null ? string.Empty : Convert.ToString(value, CultureInfo.InvariantCulture) ?? string.Empty;
                }

                CoefficientTableRowModel row = ParseRow(cells);
                if (row.Thickness > 0.0)
                {
                    rows.Add(row);
                }
            }
        }
        finally
        {
            TryCloseExcel(workbook, excel);
        }

        return rows;
    }

    private static void ExportRowsToExcelCom(string path, IReadOnlyList<CoefficientTableRowModel> rows)
    {
        Type? excelType = Type.GetTypeFromProgID("Excel.Application");
        if (excelType == null)
        {
            throw new InvalidOperationException("当前系统未安装 Excel，无法导出 xls/xlsx。");
        }

        string directory = Path.GetDirectoryName(path) ?? string.Empty;
        if (!string.IsNullOrWhiteSpace(directory))
        {
            Directory.CreateDirectory(directory);
        }

        dynamic? excel = null;
        dynamic? workbook = null;
        try
        {
            excel = Activator.CreateInstance(excelType) ?? throw new InvalidOperationException("无法启动 Excel。");
            excel.Visible = false;
            excel.DisplayAlerts = false;
            workbook = excel.Workbooks.Add();
            dynamic worksheet = workbook.Worksheets(1);
            worksheet.Name = "系数纠错表";
            for (int columnIndex = 0; columnIndex < Headers.Length; columnIndex++)
            {
                worksheet.Cells(1, columnIndex + 1).Value = Headers[columnIndex];
            }

            int rowIndex = 2;
            foreach (CoefficientTableRowModel row in rows ?? Array.Empty<CoefficientTableRowModel>())
            {
                object?[] values =
                {
                    row.Material, row.Thickness, row.Q1, row.Q2, row.Q3, row.K1, row.K2, row.K3,
                    row.A1, row.A2, row.A3, row.R,
                    string.IsNullOrWhiteSpace(row.F1) ? "f1" : row.F1,
                    string.IsNullOrWhiteSpace(row.F2) ? "f1" : row.F2
                };

                for (int columnIndex = 0; columnIndex < values.Length; columnIndex++)
                {
                    worksheet.Cells(rowIndex, columnIndex + 1).Value = values[columnIndex];
                }

                rowIndex++;
            }

            int format = string.Equals(Path.GetExtension(path), ".xlsx", StringComparison.OrdinalIgnoreCase) ? 51 : 56;
            workbook.SaveAs(path, format);
        }
        finally
        {
            TryCloseExcel(workbook, excel);
        }
    }

    private static CoefficientTableRowModel ParseRow(IReadOnlyList<string> cells)
    {
        return new CoefficientTableRowModel
        {
            Material = Cell(cells, 0, "材质 <未指定>"),
            Thickness = ParseDouble(Cell(cells, 1)),
            Q1 = ParseNullableDouble(Cell(cells, 2)),
            Q2 = ParseNullableDouble(Cell(cells, 3)),
            Q3 = ParseNullableDouble(Cell(cells, 4)),
            K1 = ParseNullableDouble(Cell(cells, 5)),
            K2 = ParseNullableDouble(Cell(cells, 6)),
            K3 = ParseNullableDouble(Cell(cells, 7)),
            A1 = ParseNullableDouble(Cell(cells, 8)),
            A2 = ParseNullableDouble(Cell(cells, 9)),
            A3 = ParseNullableDouble(Cell(cells, 10)),
            R = ParseNullableDouble(Cell(cells, 11)),
            F1 = Cell(cells, 12, "f1"),
            F2 = Cell(cells, 13, "f1")
        };
    }

    private static string Cell(IReadOnlyList<string> cells, int index, string fallback = "")
    {
        if (cells == null || index < 0 || index >= cells.Count)
        {
            return fallback;
        }

        string value = cells[index]?.Trim() ?? string.Empty;
        return string.IsNullOrWhiteSpace(value) ? fallback : value;
    }

    private static string CsvCell(object? value)
    {
        string text = value == null ? string.Empty : Convert.ToString(value, CultureInfo.InvariantCulture) ?? string.Empty;
        return "\"" + text.Replace("\"", "\"\"") + "\"";
    }

    private static string[] ParseCsvLine(string line)
    {
        if (string.IsNullOrEmpty(line))
        {
            return Array.Empty<string>();
        }

        List<string> cells = new List<string>();
        StringBuilder current = new StringBuilder();
        bool inQuotes = false;
        for (int index = 0; index < line.Length; index++)
        {
            char ch = line[index];
            if (inQuotes)
            {
                if (ch == '"')
                {
                    bool escapedQuote = index + 1 < line.Length && line[index + 1] == '"';
                    if (escapedQuote)
                    {
                        current.Append('"');
                        index++;
                    }
                    else
                    {
                        inQuotes = false;
                    }
                }
                else
                {
                    current.Append(ch);
                }
            }
            else if (ch == '"')
            {
                inQuotes = true;
            }
            else if (ch == ',')
            {
                cells.Add(current.ToString());
                current.Clear();
            }
            else
            {
                current.Append(ch);
            }
        }

        cells.Add(current.ToString());
        return cells.ToArray();
    }

    private static double ParseDouble(string? text)
    {
        return double.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out double value) ? value : 0.0;
    }

    private static double? ParseNullableDouble(string? text)
    {
        return double.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out double value) ? value : null;
    }

    private static void TryCloseExcel(dynamic? workbook, dynamic? excel)
    {
        try
        {
            workbook?.Close(false);
        }
        catch
        {
        }

        try
        {
            excel?.Quit();
        }
        catch
        {
        }
    }
}
