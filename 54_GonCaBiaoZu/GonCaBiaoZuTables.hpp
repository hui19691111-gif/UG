#pragma once

#include "ZhihuiDialogMemory.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace zhihui::gcbz
{

struct SheetMetalDimPreset
{
    std::string label;
    double upper = 0.0;
    double lower = 0.0;
    bool symmetric = false;
};

struct GdtQuickPreset
{
    std::string label;
    int characteristic = 1; // 0=直线度, 1=平面度, 2=圆度, 3=圆柱度, 4=线轮廓, 5=面轮廓, 6=倾斜度, 7=垂直度, 8=平行度, 9=位置度, 10=同轴度, 11=对称度, 12=圆跳动, 13=全跳动
    int zoneShape = 0;      // 0=无, 1=直径 Φ, 2=球径 SΦ, 3=正方形 □
    double value = 0.1;
    int materialMod = 0;    // 0=无, 1=Ⓜ 最大实体, 2=Ⓛ 最小实体
    std::string datumA;
    std::string datumB;
    std::string datumC;
};

inline std::wstring ConfigIniPath()
{
    return zhihui_dialog_memory::StatePath(L"GonCaBiaoZu_Presets.ini");
}

inline std::vector<SheetMetalDimPreset> DefaultDimPresets()
{
    return {
        {"±0.05 (精密间距)",       0.05, -0.05, true},
        {"±0.10 (常用钣金孔距)",   0.10, -0.10, true},
        {"±0.15 (折弯内空/间距)",  0.15, -0.15, true},
        {"±0.20 (常规折弯尺寸)",   0.20, -0.20, true},
        {"±0.30 (一般外形尺寸)",   0.30, -0.30, true},
        {"±0.50 (大件自由公差)",   0.50, -0.50, true},
        {"+0.05 / 0 (精密销/底孔)", 0.05,  0.00, false},
        {"+0.10 / 0 (压铆/激光孔)", 0.10,  0.00, false},
        {"+0.15 / 0 (常规过孔)",   0.15,  0.00, false},
        {"+0.20 / 0 (宽松装配孔)", 0.20,  0.00, false},
        {"0 / -0.05 (精密外轴)",   0.00, -0.05, false},
        {"0 / -0.10 (插接公口)",   0.00, -0.10, false},
        {"0 / -0.20 (内嵌包边板)", 0.00, -0.20, false},
        {"0 / -0.50 (避空下偏门板)", 0.00, -0.50, false},
        {"+0.10 / -0.05 (非对称)", 0.10, -0.05, false},
        {"+0.20 / -0.10 (非对称)", 0.20, -0.10, false},
    };
}

inline std::vector<GdtQuickPreset> DefaultGdtPresets()
{
    return {
        {"— [不套用快捷模板，按下方手动设置]", -1, 0, 0.10, 0, "", "", ""},
        {"▱ 平面度 0.05 (精密安装面)",       1,  0, 0.05, 0, "", "", ""},
        {"▱ 平面度 0.10 (常规钣金底面)",     1,  0, 0.10, 0, "", "", ""},
        {"▱ 平面度 0.20 (大板平面度)",       1,  0, 0.20, 0, "", "", ""},
        {"⊥ 垂直度 0.10 | A (折弯边垂直)",   7,  0, 0.10, 0, "A", "", ""},
        {"⊥ 垂直度 0.20 | A (大折弯垂直)",   7,  0, 0.20, 0, "A", "", ""},
        {"⊥ 垂直度 ∅0.05 | A (孔轴线垂直)",  7,  1, 0.05, 0, "A", "", ""},
        {"∥ 平行度 0.05 | A (精密对面平行)", 8,  0, 0.05, 0, "A", "", ""},
        {"∥ 平行度 0.10 | A (折弯对面平行)", 8,  0, 0.10, 0, "A", "", ""},
        {"⌖ 位置度 ∅0.10 | A | B (精密孔组)", 9,  1, 0.10, 0, "A", "B", ""},
        {"⌖ 位置度 ∅0.15 Ⓜ | A | B (压铆孔)", 9,  1, 0.15, 1, "A", "B", ""},
        {"⌖ 位置度 ∅0.20 | A | B | C (安装孔)", 9, 1, 0.20, 0, "A", "B", "C"},
        {"◎ 同轴度 ∅0.02 | A (精密轴承同轴)", 10, 1, 0.02, 0, "A", "", ""},
        {"◎ 同轴度 ∅0.05 | A (常规孔轴同轴)", 10, 1, 0.05, 0, "A", "", ""},
        {"≡ 对称度 0.10 | A (卡槽/折弯对称)", 11, 0, 0.10, 0, "A", "", ""},
        {"⌓ 面轮廓度 0.20 | A | B (成形曲面)", 5,  0, 0.20, 0, "A", "B", ""},
    };
}

inline void EnsureDefaultConfigIni()
{
    const std::wstring path = ConfigIniPath();
    const DWORD attrs = GetFileAttributesW(path.c_str());
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0)
    {
        return;
    }

    FILE* fp = _wfopen(path.c_str(), L"wb");
    if (!fp) return;
    const unsigned char bom[3] = {0xEF, 0xBB, 0xBF};
    fwrite(bom, 1, 3, fp);

    std::ostringstream out;
    out << "; 智辉钣金插件 - 工程图公差标注 (54_GonCaBiaoZu) 预设配置文件\r\n"
        << "; 支持自定义常用钣金/机加尺寸公差与形位公差预设。\r\n\r\n"
        << "[DimensionPresets]\r\n"
        << "; 格式: PresetN = 显示名称|上偏差|下偏差|是否对称(1/0)\r\n";

    const auto dims = DefaultDimPresets();
    for (std::size_t i = 0; i < dims.size(); ++i)
    {
        out << "Preset" << (i + 1) << "=" << dims[i].label << "|"
            << dims[i].upper << "|" << dims[i].lower << "|"
            << (dims[i].symmetric ? 1 : 0) << "\r\n";
    }

    out << "\r\n[GdtPresets]\r\n"
        << "; 格式: GdtN = 显示名称|特征枚举(0~13)|公差带(0无/1直径/2球径/3方)|公差值|修饰符(0无/1M/2L)|基准A|基准B|基准C\r\n";

    const auto gdts = DefaultGdtPresets();
    for (std::size_t i = 1; i < gdts.size(); ++i)
    {
        out << "Gdt" << i << "=" << gdts[i].label << "|"
            << gdts[i].characteristic << "|" << gdts[i].zoneShape << "|"
            << gdts[i].value << "|" << gdts[i].materialMod << "|"
            << gdts[i].datumA << "|" << gdts[i].datumB << "|" << gdts[i].datumC << "\r\n";
    }

    const std::string content = out.str();
    fwrite(content.data(), 1, content.size(), fp);
    fclose(fp);
}

// ============================================================================
// 1. GB/T 1804-2000 一般公差（未注线性尺寸极限偏差，单位 mm）
// gradeIndex: 0 = f (精密), 1 = m (中等), 2 = c (粗糙), 3 = v (最粗)
// ============================================================================
inline bool LookupGb1804Linear(double nominalSizeMm, int gradeIndex, double& outPlusMinus)
{
    const double d = std::fabs(nominalSizeMm);
    if (d < 0.5) return false;

    struct RangeRow
    {
        double maxSize;
        double tol[4]; // f, m, c, v
    };

    static const RangeRow kTable[] = {
        {   3.0, {0.05, 0.10, 0.20, 0.50} },
        {   6.0, {0.05, 0.10, 0.30, 0.50} },
        {  30.0, {0.10, 0.20, 0.50, 1.00} },
        { 120.0, {0.15, 0.30, 0.80, 1.50} },
        { 400.0, {0.20, 0.50, 1.20, 2.50} },
        {1000.0, {0.30, 0.80, 2.00, 4.00} },
        {2000.0, {0.50, 1.20, 3.00, 6.00} },
        {4000.0, {0.80, 2.00, 4.00, 8.00} },
    };

    const int g = std::clamp(gradeIndex, 0, 3);
    for (const auto& row : kTable)
    {
        if (d <= row.maxSize + 1.0e-6)
        {
            outPlusMinus = row.tol[g];
            return true;
        }
    }
    outPlusMinus = kTable[7].tol[g];
    return true;
}

// ============================================================================
// 2. GB/T 1800.2 标准极限与配合查表（基本尺寸 0 ~ 500 mm，单位 mm）
// 内置常用孔代号與轴代号，返回上偏差 ES/es (mm) 和下偏差 EI/ei (mm)
// ============================================================================
struct FitSymbolDef
{
    const char* code;       // 如 "H7"
    const char* deviation;  // 如 "H"
    int grade;              // 如 7
    bool isHole;
    // 13 个尺寸分段 (<=3, <=6, <=10, <=18, <=30, <=50, <=80, <=120, <=180, <=250, <=315, <=400, <=500)
    // 单位为微米 (um): {upper_um, lower_um}
    int bands[13][2];
};

inline int GetGb1800SizeBand(double sizeMm)
{
    const double d = std::fabs(sizeMm);
    static const double kLimits[13] = {
        3.0, 6.0, 10.0, 18.0, 30.0, 50.0, 80.0, 120.0, 180.0, 250.0, 315.0, 400.0, 500.0
    };
    for (int i = 0; i < 13; ++i)
    {
        if (d <= kLimits[i] + 1.0e-6)
        {
            return i;
        }
    }
    return 12;
}

inline const std::vector<FitSymbolDef>& GetHoleFitTable()
{
    static const std::vector<FitSymbolDef> kHoles = {
        {"H6", "H", 6, true, {
            {6,0}, {8,0}, {9,0}, {11,0}, {13,0}, {16,0}, {19,0}, {22,0}, {25,0}, {29,0}, {32,0}, {36,0}, {40,0}
        }},
        {"H7", "H", 7, true, {
            {10,0}, {12,0}, {15,0}, {18,0}, {21,0}, {25,0}, {30,0}, {35,0}, {40,0}, {46,0}, {52,0}, {57,0}, {63,0}
        }},
        {"H8", "H", 8, true, {
            {14,0}, {18,0}, {22,0}, {27,0}, {33,0}, {39,0}, {46,0}, {54,0}, {63,0}, {72,0}, {81,0}, {89,0}, {97,0}
        }},
        {"H9", "H", 9, true, {
            {25,0}, {30,0}, {36,0}, {43,0}, {52,0}, {62,0}, {74,0}, {87,0}, {100,0}, {115,0}, {130,0}, {140,0}, {155,0}
        }},
        {"H10", "H", 10, true, {
            {40,0}, {48,0}, {58,0}, {70,0}, {84,0}, {100,0}, {120,0}, {140,0}, {160,0}, {185,0}, {210,0}, {230,0}, {250,0}
        }},
        {"H11", "H", 11, true, {
            {60,0}, {75,0}, {90,0}, {110,0}, {130,0}, {160,0}, {190,0}, {220,0}, {250,0}, {290,0}, {320,0}, {360,0}, {400,0}
        }},
        {"JS7", "JS", 7, true, {
            {5,-5}, {6,-6}, {7,-7}, {9,-9}, {10,-10}, {12,-12}, {15,-15}, {17,-17}, {20,-20}, {23,-23}, {26,-26}, {28,-28}, {31,-31}
        }},
        {"K7", "K", 7, true, {
            {0,-10}, {3,-9}, {5,-10}, {6,-12}, {6,-15}, {7,-18}, {9,-21}, {10,-25}, {12,-28}, {13,-33}, {16,-36}, {17,-40}, {18,-45}
        }},
        {"M7", "M", 7, true, {
            {-2,-12}, {-1,-13}, {0,-15}, {0,-18}, {0,-21}, {0,-25}, {0,-30}, {0,-35}, {0,-40}, {0,-46}, {0,-52}, {0,-57}, {0,-63}
        }},
        {"N7", "N", 7, true, {
            {-4,-14}, {-4,-16}, {-4,-19}, {-5,-23}, {-7,-28}, {-8,-33}, {-9,-39}, {-10,-45}, {-12,-52}, {-14,-60}, {-14,-66}, {-16,-73}, {-17,-80}
        }},
        {"P7", "P", 7, true, {
            {-6,-16}, {-8,-20}, {-9,-24}, {-11,-29}, {-14,-35}, {-17,-42}, {-21,-51}, {-24,-59}, {-28,-68}, {-33,-79}, {-36,-88}, {-41,-98}, {-45,-108}
        }},
        {"G7", "G", 7, true, {
            {12,2}, {16,4}, {20,5}, {24,6}, {28,7}, {34,9}, {40,10}, {47,12}, {54,14}, {61,15}, {69,17}, {75,18}, {83,20}
        }},
        {"F8", "F", 8, true, {
            {20,6}, {28,10}, {35,13}, {43,16}, {53,20}, {64,25}, {76,30}, {90,36}, {106,43}, {122,50}, {137,56}, {151,62}, {165,68}
        }},
    };
    return kHoles;
}

inline const std::vector<FitSymbolDef>& GetShaftFitTable()
{
    static const std::vector<FitSymbolDef> kShafts = {
        {"h5", "h", 5, false, {
            {0,-4}, {0,-5}, {0,-6}, {0,-8}, {0,-9}, {0,-11}, {0,-13}, {0,-15}, {0,-18}, {0,-20}, {0,-23}, {0,-25}, {0,-27}
        }},
        {"h6", "h", 6, false, {
            {0,-6}, {0,-8}, {0,-9}, {0,-11}, {0,-13}, {0,-16}, {0,-19}, {0,-22}, {0,-25}, {0,-29}, {0,-32}, {0,-36}, {0,-40}
        }},
        {"h7", "h", 7, false, {
            {0,-10}, {0,-12}, {0,-15}, {0,-18}, {0,-21}, {0,-25}, {0,-30}, {0,-35}, {0,-40}, {0,-46}, {0,-52}, {0,-57}, {0,-63}
        }},
        {"h8", "h", 8, false, {
            {0,-14}, {0,-18}, {0,-22}, {0,-27}, {0,-33}, {0,-39}, {0,-46}, {0,-54}, {0,-63}, {0,-72}, {0,-81}, {0,-89}, {0,-97}
        }},
        {"h9", "h", 9, false, {
            {0,-25}, {0,-30}, {0,-36}, {0,-43}, {0,-52}, {0,-62}, {0,-74}, {0,-87}, {0,-100}, {0,-115}, {0,-130}, {0,-140}, {0,-155}
        }},
        {"g6", "g", 6, false, {
            {-2,-8}, {-4,-12}, {-5,-14}, {-6,-17}, {-7,-20}, {-9,-25}, {-10,-29}, {-12,-34}, {-14,-39}, {-15,-44}, {-17,-49}, {-18,-54}, {-20,-60}
        }},
        {"f6", "f", 6, false, {
            {-6,-12}, {-10,-18}, {-13,-22}, {-16,-27}, {-20,-33}, {-25,-41}, {-30,-49}, {-36,-58}, {-43,-68}, {-50,-79}, {-56,-88}, {-62,-98}, {-68,-108}
        }},
        {"f7", "f", 7, false, {
            {-6,-16}, {-10,-22}, {-13,-28}, {-16,-34}, {-20,-41}, {-25,-50}, {-30,-60}, {-36,-71}, {-43,-83}, {-50,-96}, {-56,-108}, {-62,-119}, {-68,-131}
        }},
        {"e8", "e", 8, false, {
            {-14,-28}, {-20,-38}, {-25,-47}, {-32,-59}, {-40,-73}, {-50,-89}, {-60,-106}, {-72,-126}, {-85,-148}, {-100,-172}, {-110,-191}, {-125,-214}, {-135,-232}
        }},
        {"js6", "js", 6, false, {
            {3,-3}, {4,-4}, {4,-4}, {5,-5}, {6,-6}, {8,-8}, {9,-9}, {11,-11}, {12,-12}, {14,-14}, {16,-16}, {18,-18}, {20,-20}
        }},
        {"k6", "k", 6, false, {
            {6,0}, {9,1}, {10,1}, {12,1}, {15,2}, {18,2}, {21,2}, {25,3}, {28,3}, {33,4}, {36,4}, {40,4}, {45,5}
        }},
        {"m6", "m", 6, false, {
            {8,2}, {12,4}, {15,6}, {18,7}, {21,8}, {25,9}, {30,11}, {35,13}, {40,15}, {46,17}, {52,20}, {57,21}, {63,23}
        }},
        {"n6", "n", 6, false, {
            {10,4}, {16,8}, {19,10}, {23,12}, {28,15}, {33,17}, {39,20}, {45,23}, {52,27}, {60,31}, {66,34}, {73,37}, {80,40}
        }},
        {"p6", "p", 6, false, {
            {12,6}, {20,12}, {24,15}, {29,18}, {35,22}, {42,26}, {51,32}, {59,37}, {68,43}, {79,50}, {88,56}, {98,62}, {108,68}
        }},
        {"s6", "s", 6, false, {
            {20,14}, {27,19}, {32,23}, {39,28}, {48,35}, {59,43}, {72,53}, {93,71}, {125,100}, {159,130}, {190,158}, {226,190}, {272,232}
        }},
    };
    return kShafts;
}

inline bool LookupFitByIndex(bool isHole, int index, double sizeMm,
    std::string& outCode, std::string& outDev, int& outGrade, double& outUpperMm, double& outLowerMm)
{
    const auto& table = isHole ? GetHoleFitTable() : GetShaftFitTable();
    if (index < 0 || static_cast<std::size_t>(index) >= table.size())
    {
        return false;
    }
    const auto& def = table[static_cast<std::size_t>(index)];
    const int band = GetGb1800SizeBand(sizeMm);
    outCode = def.code;
    outDev = def.deviation;
    outGrade = def.grade;
    outUpperMm = static_cast<double>(def.bands[band][0]) / 1000.0;
    outLowerMm = static_cast<double>(def.bands[band][1]) / 1000.0;
    return true;
}

// ============================================================================
// 3. GB/T 1184-1996 形状和位置公差 未注公差值自动查表 (单位 mm)
// gradeIndex: 0 = H级(精密), 1 = K级(中等), 2 = L级(粗糙)
// characteristic: 0=直线度, 1=平面度, 7=垂直度, 8=平行度, 10=同轴度, 11=对称度, 12=圆跳动, 13=全跳动
// ============================================================================
inline double LookupGb1184Tolerance(int characteristic, int gradeIndex, double featureLengthMm)
{
    const double len = std::max(1.0, std::fabs(featureLengthMm));
    const int g = std::clamp(gradeIndex, 0, 2);

    // 圆跳动 / 全跳动 / 同轴度：与尺寸长度无关，固定按 H=0.1, K=0.2, L=0.5 (同轴度精密取 0.05/0.1/0.2)
    if (characteristic == 10)
    {
        static const double kConcentricity[3] = {0.05, 0.10, 0.20};
        return kConcentricity[g];
    }
    if (characteristic == 12 || characteristic == 13)
    {
        static const double kRunout[3] = {0.10, 0.20, 0.50};
        return kRunout[g];
    }

    // 垂直度 / 倾斜度 (表2)
    if (characteristic == 6 || characteristic == 7)
    {
        struct PerpRow { double maxLen; double tol[3]; };
        static const PerpRow kPerpTable[] = {
            { 100.0, {0.20, 0.40, 0.60} },
            { 300.0, {0.30, 0.60, 1.00} },
            {1000.0, {0.40, 0.80, 1.50} },
            {3000.0, {0.50, 1.00, 2.00} },
        };
        for (const auto& row : kPerpTable)
        {
            if (len <= row.maxLen + 1.0e-6) return row.tol[g];
        }
        return kPerpTable[3].tol[g];
    }

    // 对称度 / 位置度 (表3)
    if (characteristic == 9 || characteristic == 11)
    {
        struct SymRow { double maxLen; double tol[3]; };
        static const SymRow kSymTable[] = {
            { 100.0, {0.50, 0.60, 0.60} },
            { 300.0, {0.50, 0.60, 1.00} },
            {1000.0, {0.50, 0.80, 1.50} },
            {3000.0, {0.50, 1.00, 2.00} },
        };
        for (const auto& row : kSymTable)
        {
            if (len <= row.maxLen + 1.0e-6) return row.tol[g];
        }
        return kSymTable[3].tol[g];
    }

    // 直线度 / 平面度 / 平行度 / 轮廓度 (表1)
    struct FlatRow { double maxLen; double tol[3]; };
    static const FlatRow kFlatTable[] = {
        {  10.0, {0.02, 0.05, 0.10} },
        {  30.0, {0.05, 0.10, 0.20} },
        { 100.0, {0.10, 0.20, 0.40} },
        { 300.0, {0.20, 0.40, 0.80} },
        {1000.0, {0.30, 0.60, 1.20} },
        {3000.0, {0.40, 0.80, 1.60} },
    };
    for (const auto& row : kFlatTable)
    {
        if (len <= row.maxLen + 1.0e-6) return row.tol[g];
    }
    return kFlatTable[5].tol[g];
}

inline std::string FormatSignedTol(double val)
{
    if (std::fabs(val) < 1.0e-7)
    {
        return "0";
    }
    std::ostringstream oss;
    oss << (val > 0.0 ? "+" : "") << std::fixed << std::setprecision(3) << val;
    std::string s = oss.str();
    while (s.size() > 2 && s.back() == '0' && s.find('.') != std::string::npos)
    {
        s.pop_back();
    }
    if (!s.empty() && s.back() == '.')
    {
        s.pop_back();
    }
    return s;
}

inline std::string FormatUnsignedTol(double val)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3) << std::fabs(val);
    std::string s = oss.str();
    while (s.size() > 1 && s.back() == '0' && s.find('.') != std::string::npos)
    {
        s.pop_back();
    }
    if (!s.empty() && s.back() == '.')
    {
        s.pop_back();
    }
    return s;
}

inline int CountDecimals(double val)
{
    const double v = std::fabs(val);
    if (v < 1.0e-7) return 1;
    const double v1000 = std::round(v * 1000.0);
    const int milli = static_cast<int>(v1000);
    if (milli % 10 != 0) return 3;
    if (milli % 100 != 0) return 2;
    return 1;
}

} // namespace zhihui::gcbz
