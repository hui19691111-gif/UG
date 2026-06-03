#pragma once

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace zhihui_bend_rules_ini
{
    inline const char* Utf8(const char* text)
    {
        return text;
    }

#if defined(__cpp_char8_t)
    inline const char* Utf8(const char8_t* text)
    {
        return reinterpret_cast<const char*>(text);
    }
#endif

    struct CoefficientRow
    {
        std::string material;
        double thickness = 0.0;
        bool hasQ1 = false; double q1 = 0.0;
        bool hasQ2 = false; double q2 = 0.0;
        bool hasQ3 = false; double q3 = 0.0;
        bool hasK1 = false; double k1 = 0.0;
        bool hasK2 = false; double k2 = 0.0;
        bool hasK3 = false; double k3 = 0.0;
        bool hasA1 = false; double a1 = 0.0;
        bool hasA2 = false; double a2 = 0.0;
        bool hasA3 = false; double a3 = 0.0;
        bool hasLQ1 = false; double lq1 = 0.0;
        bool hasLQ2 = false; double lq2 = 0.0;
        bool hasLQ3 = false; double lq3 = 0.0;
        bool hasLK1 = false; double lk1 = 0.0;
        bool hasLK2 = false; double lk2 = 0.0;
        bool hasLK3 = false; double lk3 = 0.0;
        bool hasLA1 = false; double la1 = 0.0;
        bool hasR = false; double r = 0.0;
        std::string f1 = "f1";
        std::string f2 = "f1";
    };

    struct ConditionRow
    {
        std::string conditionKey;
        std::string angleLabel;
        std::string smallArcCode;
        std::string largeArcCode;
    };

    inline std::string Trim(const std::string& value)
    {
        size_t first = 0;
        while (first < value.size() && static_cast<unsigned char>(value[first]) <= ' ')
        {
            ++first;
        }

        size_t last = value.size();
        while (last > first && static_cast<unsigned char>(value[last - 1]) <= ' ')
        {
            --last;
        }

        return value.substr(first, last - first);
    }

    inline std::string RemoveUtf8Bom(const std::string& text)
    {
        if (text.size() >= 3 &&
            static_cast<unsigned char>(text[0]) == 0xEF &&
            static_cast<unsigned char>(text[1]) == 0xBB &&
            static_cast<unsigned char>(text[2]) == 0xBF)
        {
            return text.substr(3);
        }
        return text;
    }

    inline std::vector<std::string> Split(const std::string& text, char separator)
    {
        std::vector<std::string> items;
        std::string item;
        std::stringstream stream(text);
        while (std::getline(stream, item, separator))
        {
            items.push_back(Trim(item));
        }
        return items;
    }

    inline std::map<std::string, std::string> ParseFields(const std::string& text)
    {
        std::map<std::string, std::string> fields;
        std::vector<std::string> items = Split(text, ';');
        for (size_t i = 0; i < items.size(); ++i)
        {
            size_t eq = items[i].find('=');
            if (eq == std::string::npos)
            {
                continue;
            }

            std::string key = Trim(items[i].substr(0, eq));
            std::string value = Trim(items[i].substr(eq + 1));
            if (!key.empty())
            {
                fields[key] = value;
            }
        }
        return fields;
    }

    inline bool BoolValue(const std::string& value, bool fallback)
    {
        std::string text = Trim(value);
        std::string lower = text;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (lower == "true" || lower == "1" || text == Utf8(u8"是") || text == Utf8(u8"启用"))
        {
            return true;
        }
        if (lower == "false" || lower == "0" || text == Utf8(u8"否") || text == Utf8(u8"停用"))
        {
            return false;
        }
        return fallback;
    }

    inline bool OptionalNumber(const std::map<std::string, std::string>& fields, const std::string& key, double* value)
    {
        std::map<std::string, std::string>::const_iterator it = fields.find(key);
        if (it == fields.end())
        {
            return false;
        }

        std::string raw = Trim(it->second);
        if (raw.empty() || raw == Utf8(u8"空"))
        {
            return false;
        }

        char* end = NULL;
        double parsed = std::strtod(raw.c_str(), &end);
        if (end == raw.c_str())
        {
            return false;
        }

        *value = parsed;
        return true;
    }

    inline double NumberValue(const std::map<std::string, std::string>& fields, const std::string& key, double fallback)
    {
        double value = fallback;
        return OptionalNumber(fields, key, &value) ? value : fallback;
    }

    inline std::string StringValue(const std::map<std::string, std::string>& fields, const std::string& key, const std::string& fallback)
    {
        std::map<std::string, std::string>::const_iterator it = fields.find(key);
        return it == fields.end() ? fallback : Trim(it->second);
    }

    inline std::string Upper(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), ::toupper);
        return value;
    }

    inline std::string NumberText(double value)
    {
        std::ostringstream stream;
        stream.setf(std::ios::fixed);
        stream.precision(6);
        stream << value;
        std::string text = stream.str();
        while (text.size() > 1 && text[text.size() - 1] == '0')
        {
            text.erase(text.size() - 1);
        }
        if (!text.empty() && text[text.size() - 1] == '.')
        {
            text.erase(text.size() - 1);
        }
        return text;
    }

    inline std::string FixedNumberText(double value, int precision)
    {
        std::ostringstream stream;
        stream.setf(std::ios::fixed);
        stream.precision(precision);
        stream << value;
        return stream.str();
    }

    inline std::vector<ConditionRow> DefaultConditionRows()
    {
        std::vector<ConditionRow> rows;
        ConditionRow row;
        row.conditionKey = "= 90"; row.angleLabel = "90"; row.smallArcCode = "Q1"; row.largeArcCode = "K3"; rows.push_back(row);
        row.conditionKey = Utf8(u8"∈"); row.angleLabel = "(0,90)"; row.smallArcCode = "K2"; row.largeArcCode = "K3"; rows.push_back(row);
        row.conditionKey = Utf8(u8"∈"); row.angleLabel = "(90,180)"; row.smallArcCode = "K2"; row.largeArcCode = "K3"; rows.push_back(row);
        row.conditionKey = Utf8(u8"∈"); row.angleLabel = "(180,360)"; row.smallArcCode = "K3"; row.largeArcCode = "K3"; rows.push_back(row);
        return rows;
    }

    inline void ConditionAngleRange(const std::string& angleLabel, bool* hasAngleMin, double* angleMin, bool* hasAngleMax, double* angleMax)
    {
        *hasAngleMin = false;
        *hasAngleMax = false;
        *angleMin = 0.0;
        *angleMax = 0.0;

        std::string label = Trim(angleLabel);
        char* end = NULL;
        double exactAngle = std::strtod(label.c_str(), &end);
        if (end != label.c_str() && Trim(std::string(end)).empty())
        {
            *hasAngleMin = true; *angleMin = exactAngle - 0.001;
            *hasAngleMax = true; *angleMax = exactAngle + 0.001;
            return;
        }

        if (label == "(0,90)")
        {
            *hasAngleMin = true; *angleMin = 0.001;
            *hasAngleMax = true; *angleMax = 89.999;
        }
        else if (label == "(90,180)")
        {
            *hasAngleMin = true; *angleMin = 90.001;
            *hasAngleMax = true; *angleMax = 179.999;
        }
        else if (label == "(180,360)")
        {
            *hasAngleMin = true; *angleMin = 180.001;
            *hasAngleMax = true; *angleMax = 360.0;
        }
    }

    inline bool CoeffCodeValue(const CoefficientRow& row, const std::string& code, double* value)
    {
        std::string key = Upper(Trim(code));
        if (key == "Q1" && row.hasQ1) { *value = row.q1; return true; }
        if (key == "Q2" && row.hasQ2) { *value = row.q2; return true; }
        if (key == "Q3" && row.hasQ3) { *value = row.q3; return true; }
        if (key == "K1" && row.hasK1) { *value = row.k1; return true; }
        if (key == "K2" && row.hasK2) { *value = row.k2; return true; }
        if (key == "K3" && row.hasK3) { *value = row.k3; return true; }
        if (key == "A1" && row.hasA1) { *value = row.a1; return true; }
        if (key == "A2" && row.hasA2) { *value = row.a2; return true; }
        if (key == "A3" && row.hasA3) { *value = row.a3; return true; }
        if (key == "LQ1" && row.hasLQ1) { *value = row.lq1; return true; }
        if (key == "LQ2" && row.hasLQ2) { *value = row.lq2; return true; }
        if (key == "LQ3" && row.hasLQ3) { *value = row.lq3; return true; }
        if (key == "LK1" && row.hasLK1) { *value = row.lk1; return true; }
        if (key == "LK2" && row.hasLK2) { *value = row.lk2; return true; }
        if (key == "LK3" && row.hasLK3) { *value = row.lk3; return true; }
        if (key == "LA1" && row.hasLA1) { *value = row.la1; return true; }
        return false;
    }

    inline std::string MethodForCode(const std::string& code)
    {
        std::string key = Upper(Trim(code));
        return (key == "K1" || key == "K2" || key == "K3" ||
            key == "LK1" || key == "LK2" || key == "LK3") ? "KFactor" : "Deduction";
    }

    template <typename RuleConfigT>
    inline void LoadSettingsFields(const std::string& key, const std::string& value, RuleConfigT* config)
    {
        if (config == NULL)
        {
            return;
        }

        if (key == Utf8(u8"使用绝对内R"))
        {
            config->useAbsoluteLargeArc = BoolValue(value, config->useAbsoluteLargeArc);
        }
        else if (key == Utf8(u8"绝对内R阈值") || key == Utf8(u8"最小半径") || key == Utf8(u8"多刀折圆最小半径"))
        {
            config->absoluteLargeArcRadius = std::max(0.0, std::atof(value.c_str()));
        }
        else if (key == Utf8(u8"多刀折圆最小直径"))
        {
            config->absoluteLargeArcRadius = std::max(0.0, std::atof(value.c_str()) / 2.0);
        }
        else if (key == Utf8(u8"使用R比厚度"))
        {
            config->useRatioLargeArc = BoolValue(value, config->useRatioLargeArc);
        }
        else if (key == Utf8(u8"R比厚度阈值"))
        {
            config->ratioLargeArc = std::max(0.0, std::atof(value.c_str()));
        }
    }

    inline void ReadCoefficientRowFields(const std::string& material, const std::string& value, std::vector<CoefficientRow>* rows)
    {
        if (rows == NULL)
        {
            return;
        }

        std::map<std::string, std::string> fields = ParseFields(value);
        CoefficientRow row;
        row.material = material;
        row.thickness = NumberValue(fields, Utf8(u8"厚度"), 0.0);
        row.hasQ1 = OptionalNumber(fields, "Q1", &row.q1);
        row.hasQ2 = OptionalNumber(fields, "Q2", &row.q2);
        row.hasQ3 = OptionalNumber(fields, "Q3", &row.q3);
        row.hasK1 = OptionalNumber(fields, "K1", &row.k1);
        row.hasK2 = OptionalNumber(fields, "K2", &row.k2);
        row.hasK3 = OptionalNumber(fields, "K3", &row.k3);
        row.hasA1 = OptionalNumber(fields, "A1", &row.a1);
        row.hasA2 = OptionalNumber(fields, "A2", &row.a2);
        row.hasA3 = OptionalNumber(fields, "A3", &row.a3);
        row.hasLQ1 = OptionalNumber(fields, "LQ1", &row.lq1);
        row.hasLQ2 = OptionalNumber(fields, "LQ2", &row.lq2);
        row.hasLQ3 = OptionalNumber(fields, "LQ3", &row.lq3);
        row.hasLK1 = OptionalNumber(fields, "LK1", &row.lk1);
        row.hasLK2 = OptionalNumber(fields, "LK2", &row.lk2);
        row.hasLK3 = OptionalNumber(fields, "LK3", &row.lk3);
        row.hasLA1 = OptionalNumber(fields, "LA1", &row.la1);
        row.hasR = OptionalNumber(fields, "R", &row.r);
        row.f1 = StringValue(fields, "F1", "f1");
        row.f2 = StringValue(fields, "F2", "f1");
        if (row.thickness > 0.0)
        {
            rows->push_back(row);
        }
    }

    inline std::vector<CoefficientRow> LoadCoefficientRowsFromText(const std::string& iniText)
    {
        std::vector<CoefficientRow> rows;
        std::string section;
        std::stringstream stream(RemoveUtf8Bom(iniText));
        std::string line;
        while (std::getline(stream, line))
        {
            line = Trim(line);
            if (line.empty() || line[0] == '#' || line[0] == ';')
            {
                continue;
            }

            if (line.size() >= 2 && line[0] == '[' && line[line.size() - 1] == ']')
            {
                section = Trim(line.substr(1, line.size() - 2));
                continue;
            }

            if (section.find(Utf8(u8"材料:")) != 0)
            {
                continue;
            }

            size_t eq = line.find('=');
            if (eq == std::string::npos)
            {
                continue;
            }

            ReadCoefficientRowFields(Trim(section.substr(std::string(Utf8(u8"材料:")).size())), Trim(line.substr(eq + 1)), &rows);
        }
        return rows;
    }

    inline std::vector<ConditionRow> LoadConditionRowsFromText(const std::string& iniText)
    {
        std::vector<ConditionRow> rows;
        std::string section;
        std::stringstream stream(RemoveUtf8Bom(iniText));
        std::string line;
        while (std::getline(stream, line))
        {
            line = Trim(line);
            if (line.empty() || line[0] == '#' || line[0] == ';')
            {
                continue;
            }

            if (line.size() >= 2 && line[0] == '[' && line[line.size() - 1] == ']')
            {
                section = Trim(line.substr(1, line.size() - 2));
                continue;
            }

            if (section != Utf8(u8"条件表"))
            {
                continue;
            }

            size_t eq = line.find('=');
            if (eq == std::string::npos)
            {
                continue;
            }

            std::map<std::string, std::string> fields = ParseFields(Trim(line.substr(eq + 1)));
            ConditionRow row;
            row.conditionKey = StringValue(fields, Utf8(u8"条件"), std::string());
            row.angleLabel = StringValue(fields, Utf8(u8"角度"), std::string());
            row.smallArcCode = StringValue(fields, Utf8(u8"小圆弧"), StringValue(fields, Utf8(u8"普通折弯"), std::string()));
            row.largeArcCode = StringValue(fields, Utf8(u8"大圆弧"), StringValue(fields, Utf8(u8"多刀折圆"), std::string()));
            if (!row.angleLabel.empty() && !row.smallArcCode.empty() && !row.largeArcCode.empty())
            {
                rows.push_back(row);
            }
        }
        return rows.empty() ? DefaultConditionRows() : rows;
    }

    inline std::string DefaultConditionKeyForAngle(const std::string& angleLabel)
    {
        std::string label = Trim(angleLabel);
        return label == "90" ? std::string("= 90") : std::string(Utf8(u8"∈"));
    }

    inline void AppendCoefficientOptional(std::ostringstream& stream, const char* name, bool hasValue, double value)
    {
        stream << ";" << name << "=";
        if (hasValue)
        {
            stream << FixedNumberText(value, 2);
        }
    }

    template <typename RuleConfigT>
    inline bool SaveCoefficientRowsToIniFile(const RuleConfigT& settings, const std::vector<CoefficientRow>& rows,
        const std::vector<ConditionRow>& conditionRows, const std::string& path, std::string* error)
    {
        std::vector<ConditionRow> conditions = conditionRows.empty() ? DefaultConditionRows() : conditionRows;

        std::ofstream file(path.c_str(), std::ios::out | std::ios::binary);
        if (!file)
        {
            if (error != NULL)
            {
                *error = std::string(Utf8(u8"无法写入规则表：")) + path;
            }
            return false;
        }

        std::ostringstream stream;
        stream << "# 智辉钣金批量转钣金折弯规则表\n";
        stream << "# UG 内置表格维护材料系数，保存时自动生成 [规则]。\n\n";

        stream << Utf8(u8"[大圆弧判定]\n");
        stream << Utf8(u8"使用绝对内R=") << (settings.useAbsoluteLargeArc ? Utf8(u8"是") : Utf8(u8"否")) << "\n";
        stream << Utf8(u8"最小半径=") << NumberText(settings.absoluteLargeArcRadius) << "\n";
        stream << Utf8(u8"使用R比厚度=") << (settings.useRatioLargeArc ? Utf8(u8"是") : Utf8(u8"否")) << "\n";
        stream << Utf8(u8"R比厚度阈值=") << NumberText(settings.ratioLargeArc) << "\n\n";

        stream << Utf8(u8"[条件表]\n");
        for (size_t i = 0; i < conditions.size(); ++i)
        {
            std::string conditionKey = Trim(conditions[i].conditionKey).empty() ?
                DefaultConditionKeyForAngle(conditions[i].angleLabel) : Trim(conditions[i].conditionKey);
            stream << Utf8(u8"规则") << (i + 1) << Utf8(u8"=条件=") << conditionKey;
            stream << Utf8(u8";角度=") << conditions[i].angleLabel;
            stream << Utf8(u8";大圆弧=") << conditions[i].largeArcCode;
            stream << Utf8(u8";小圆弧=") << conditions[i].smallArcCode << "\n";
        }
        stream << "\n";

        std::map<std::string, bool> materialWritten;
        for (size_t i = 0; i < rows.size(); ++i)
        {
            std::string material = Trim(rows[i].material).empty() ? std::string(Utf8(u8"材质 <未指定>")) : Trim(rows[i].material);
            if (materialWritten[material])
            {
                continue;
            }
            materialWritten[material] = true;

            std::vector<size_t> materialRows;
            for (size_t j = 0; j < rows.size(); ++j)
            {
                std::string rowMaterial = Trim(rows[j].material).empty() ? std::string(Utf8(u8"材质 <未指定>")) : Trim(rows[j].material);
                if (rowMaterial == material)
                {
                    materialRows.push_back(j);
                }
            }

            std::stable_sort(materialRows.begin(), materialRows.end(),
                [&rows](size_t left, size_t right)
            {
                if (std::fabs(rows[left].thickness - rows[right].thickness) > 1.0e-9)
                {
                    return rows[left].thickness < rows[right].thickness;
                }
                if (rows[left].hasR != rows[right].hasR)
                {
                    return rows[left].hasR;
                }
                return rows[left].r < rows[right].r;
            });

            stream << Utf8(u8"[材料:") << material << "]\n";
            for (size_t j = 0; j < materialRows.size(); ++j)
            {
                const CoefficientRow& row = rows[materialRows[j]];
                stream << Utf8(u8"厚度") << (j + 1) << Utf8(u8"=厚度=") << FixedNumberText(row.thickness, 2);
                AppendCoefficientOptional(stream, "Q1", row.hasQ1, row.q1);
                AppendCoefficientOptional(stream, "Q2", row.hasQ2, row.q2);
                AppendCoefficientOptional(stream, "Q3", row.hasQ3, row.q3);
                AppendCoefficientOptional(stream, "K1", row.hasK1, row.k1);
                AppendCoefficientOptional(stream, "K2", row.hasK2, row.k2);
                AppendCoefficientOptional(stream, "K3", row.hasK3, row.k3);
                AppendCoefficientOptional(stream, "A1", row.hasA1, row.a1);
                AppendCoefficientOptional(stream, "A2", row.hasA2, row.a2);
                AppendCoefficientOptional(stream, "A3", row.hasA3, row.a3);
                AppendCoefficientOptional(stream, "LQ1", row.hasLQ1, row.lq1);
                AppendCoefficientOptional(stream, "LQ2", row.hasLQ2, row.lq2);
                AppendCoefficientOptional(stream, "LQ3", row.hasLQ3, row.lq3);
                AppendCoefficientOptional(stream, "LK1", row.hasLK1, row.lk1);
                AppendCoefficientOptional(stream, "LK2", row.hasLK2, row.lk2);
                AppendCoefficientOptional(stream, "LK3", row.hasLK3, row.lk3);
                AppendCoefficientOptional(stream, "LA1", row.hasLA1, row.la1);
                stream << ";R=";
                if (row.hasR)
                {
                    stream << NumberText(row.r);
                }
                stream << ";F1=" << (row.f1.empty() ? "f1" : row.f1);
                stream << ";F2=" << (row.f2.empty() ? "f1" : row.f2) << "\n";
            }
            stream << "\n";
        }

        std::string text = stream.str();
        file.write(text.c_str(), static_cast<std::streamsize>(text.size()));
        return true;
    }

    template <typename BendRuleT>
    inline void AddGeneratedRule(std::vector<BendRuleT>& rules, const CoefficientRow& row, const ConditionRow& condition, bool largeArc)
    {
        std::string code = largeArc ? condition.largeArcCode : condition.smallArcCode;
        double value = 0.0;
        if (!CoeffCodeValue(row, code, &value))
        {
            return;
        }

        bool hasAngleMin = false;
        bool hasAngleMax = false;
        double angleMin = 0.0;
        double angleMax = 0.0;
        ConditionAngleRange(condition.angleLabel, &hasAngleMin, &angleMin, &hasAngleMax, &angleMax);

        double halfWidth = std::max(0.05, row.thickness * 0.05);
        BendRuleT rule;
        rule.enabled = true;
        rule.fallback = false;
        rule.name = NumberText(row.thickness) + "_" +
            (largeArc ? std::string(Utf8(u8"大圆弧_")) : std::string(Utf8(u8"小圆弧_"))) +
            condition.angleLabel + "_" + code;
        rule.material = Trim(row.material).empty() ? std::string(Utf8(u8"材质 <未指定>")) : Trim(row.material);
        rule.method = MethodForCode(code);
        rule.note = largeArc ? std::string(Utf8(u8"INI系数表生成[LargeArc][")) + code + "]" :
            std::string(Utf8(u8"INI系数表生成[SmallArc][")) + code + "]";
        rule.value = value;
        rule.hasAngleMin = hasAngleMin;
        rule.hasAngleMax = hasAngleMax;
        rule.angleMin = angleMin;
        rule.angleMax = angleMax;
        rule.hasThicknessMin = true;
        rule.hasThicknessMax = true;
        rule.thicknessMin = row.thickness - halfWidth;
        rule.thicknessMax = row.thickness + halfWidth;
        rule.hasRadiusMin = row.hasR;
        rule.hasRadiusMax = row.hasR;
        rule.radiusMin = row.hasR ? std::max(0.0, row.r - halfWidth) : 0.0;
        rule.radiusMax = row.hasR ? row.r + halfWidth : 0.0;
        rule.hasRadiusOverride = row.hasR;
        rule.radiusOverride = row.r;
        rules.push_back(rule);
    }

    template <typename BendRuleT>
    inline std::vector<BendRuleT> BuildRulesFromCoefficientRows(const std::vector<CoefficientRow>& coefficientRows, const std::vector<ConditionRow>& conditionRows)
    {
        std::vector<BendRuleT> rules;
        std::vector<ConditionRow> conditions = conditionRows.empty() ? DefaultConditionRows() : conditionRows;
        for (size_t i = 0; i < coefficientRows.size(); ++i)
        {
            if (coefficientRows[i].thickness <= 0.0)
            {
                continue;
            }

            for (size_t c = 0; c < conditions.size(); ++c)
            {
                AddGeneratedRule(rules, coefficientRows[i], conditions[c], false);
                AddGeneratedRule(rules, coefficientRows[i], conditions[c], true);
            }
        }
        return rules;
    }

    template <typename RuleConfigT, typename BendRuleT>
    inline bool LoadIntoRuleConfig(const std::string& iniText, RuleConfigT* config)
    {
        if (config == NULL)
        {
            return false;
        }

        std::string section;
        std::vector<CoefficientRow> coefficientRows;
        std::vector<ConditionRow> conditionRows;
        std::vector<BendRuleT> flatRules;

        std::stringstream stream(RemoveUtf8Bom(iniText));
        std::string line;
        while (std::getline(stream, line))
        {
            line = Trim(line);
            if (line.empty() || line[0] == '#' || line[0] == ';')
            {
                continue;
            }

            if (line.size() >= 2 && line[0] == '[' && line[line.size() - 1] == ']')
            {
                section = Trim(line.substr(1, line.size() - 2));
                continue;
            }

            size_t eq = line.find('=');
            if (eq == std::string::npos)
            {
                continue;
            }

            std::string key = Trim(line.substr(0, eq));
            std::string value = Trim(line.substr(eq + 1));

            if (section == Utf8(u8"大圆弧判定"))
            {
                LoadSettingsFields(key, value, config);
                continue;
            }

            if (section == Utf8(u8"条件表"))
            {
                std::map<std::string, std::string> fields = ParseFields(value);
                ConditionRow row;
                row.conditionKey = StringValue(fields, Utf8(u8"条件"), std::string());
                row.angleLabel = StringValue(fields, Utf8(u8"角度"), std::string());
                row.smallArcCode = StringValue(fields, Utf8(u8"小圆弧"), StringValue(fields, Utf8(u8"普通折弯"), std::string()));
                row.largeArcCode = StringValue(fields, Utf8(u8"大圆弧"), StringValue(fields, Utf8(u8"多刀折圆"), std::string()));
                if (!row.angleLabel.empty() && !row.smallArcCode.empty() && !row.largeArcCode.empty())
                {
                    conditionRows.push_back(row);
                }
                continue;
            }

            if (section.find(Utf8(u8"材料:")) == 0)
            {
                ReadCoefficientRowFields(Trim(section.substr(std::string(Utf8(u8"材料:")).size())), value, &coefficientRows);
                continue;
            }

            if (section == Utf8(u8"规则"))
            {
                std::map<std::string, std::string> fields = ParseFields(value);
                BendRuleT rule;
                rule.enabled = BoolValue(StringValue(fields, Utf8(u8"启用"), "true"), true);
                rule.fallback = BoolValue(StringValue(fields, Utf8(u8"兜底"), "false"), false);
                rule.name = StringValue(fields, Utf8(u8"名称"), key);
                rule.material = StringValue(fields, Utf8(u8"材料"), std::string());
                rule.method = StringValue(fields, Utf8(u8"方法"), "KFactor");
                if (rule.method.find(Utf8(u8"扣")) != std::string::npos)
                {
                    rule.method = "Deduction";
                }
                else if (rule.method.find("K") != std::string::npos || rule.method.find(Utf8(u8"因子")) != std::string::npos)
                {
                    rule.method = "KFactor";
                }
                rule.note = StringValue(fields, Utf8(u8"备注"), std::string());
                rule.value = NumberValue(fields, Utf8(u8"值"), 0.5);
                rule.hasAngleMin = OptionalNumber(fields, Utf8(u8"角度最小"), &rule.angleMin);
                rule.hasAngleMax = OptionalNumber(fields, Utf8(u8"角度最大"), &rule.angleMax);
                rule.hasThicknessMin = OptionalNumber(fields, Utf8(u8"厚度最小"), &rule.thicknessMin);
                rule.hasThicknessMax = OptionalNumber(fields, Utf8(u8"厚度最大"), &rule.thicknessMax);
                rule.hasRadiusMin = OptionalNumber(fields, Utf8(u8"内R最小"), &rule.radiusMin);
                rule.hasRadiusMax = OptionalNumber(fields, Utf8(u8"内R最大"), &rule.radiusMax);
                rule.hasRadiusOverride = OptionalNumber(fields, Utf8(u8"半径覆盖"), &rule.radiusOverride);
                if (rule.enabled)
                {
                    flatRules.push_back(rule);
                }
            }
        }

        if (!coefficientRows.empty())
        {
            config->rules = BuildRulesFromCoefficientRows<BendRuleT>(coefficientRows, conditionRows);
            return !config->rules.empty();
        }

        if (!flatRules.empty())
        {
            config->rules = flatRules;
            return true;
        }

        return false;
    }
}
