#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <algorithm>
#include <cstdio>
#include <cwctype>
#include <string>
#include <vector>

namespace zhihui::kjj
{

struct ShortcutEntry
{
    std::wstring alias;
    std::wstring displayName;
    std::wstring commandId;
    std::wstring category;
    std::wstring module = L"全部";
    bool enabled = true;
};

struct ShortcutConfig
{
    bool enabled = false;
    bool repeatOnEmptySpace = true;
    bool showHud = true;
    std::wstring toggleHotkey = L"ALT+F1";
    std::vector<ShortcutEntry> entries;
};

inline std::wstring Utf8ToWide(const std::string& utf8)
{
    if (utf8.empty())
    {
        return std::wstring();
    }
    const int len = MultiByteToWideChar(
        CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (len <= 0)
    {
        return std::wstring();
    }
    std::wstring wide(static_cast<std::size_t>(len), L'\0');
    MultiByteToWideChar(
        CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(), len);
    return wide;
}

inline std::string WideToUtf8(const std::wstring& wide)
{
    if (wide.empty())
    {
        return std::string();
    }
    const int len = WideCharToMultiByte(
        CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    if (len <= 0)
    {
        return std::string();
    }
    std::string utf8(static_cast<std::size_t>(len), '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), utf8.data(), len, nullptr, nullptr);
    return utf8;
}

inline std::string WideToAnsi(const std::wstring& wide)
{
    if (wide.empty())
    {
        return std::string();
    }
    const int len = WideCharToMultiByte(
        CP_ACP, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    if (len <= 0)
    {
        return std::string();
    }
    std::string ansi(static_cast<std::size_t>(len), '\0');
    WideCharToMultiByte(
        CP_ACP, 0, wide.data(), static_cast<int>(wide.size()), ansi.data(), len, nullptr, nullptr);
    return ansi;
}

inline std::wstring TrimWide(const std::wstring& value)
{
    std::size_t start = 0;
    while (start < value.size() && std::iswspace(value[start]))
    {
        ++start;
    }
    std::size_t end = value.size();
    while (end > start && std::iswspace(value[end - 1]))
    {
        --end;
    }
    return value.substr(start, end - start);
}

inline std::wstring ToLowerAscii(std::wstring value)
{
    for (wchar_t& ch : value)
    {
        if (ch >= L'A' && ch <= L'Z')
        {
            ch = static_cast<wchar_t>(ch - L'A' + L'a');
        }
    }
    return value;
}

inline std::wstring ToUpperAscii(std::wstring value)
{
    for (wchar_t& ch : value)
    {
        if (ch >= L'a' && ch <= L'z')
        {
            ch = static_cast<wchar_t>(ch - L'a' + L'A');
        }
    }
    return value;
}

inline std::wstring DirectoryOf(const std::wstring& path)
{
    const std::size_t pos = path.find_last_of(L"\\/");
    return (pos == std::wstring::npos) ? std::wstring() : path.substr(0, pos);
}

inline bool FileExistsW(const std::wstring& path)
{
    const DWORD attrs = GetFileAttributesW(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

inline void EnsureDirectoryW(const std::wstring& dir)
{
    if (dir.empty())
    {
        return;
    }
    CreateDirectoryW(dir.c_str(), nullptr);
}

inline bool GetFileWriteTimeW(const std::wstring& path, FILETIME& outTime)
{
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data))
    {
        return false;
    }
    outTime = data.ftLastWriteTime;
    return true;
}

inline bool ReadUtf8FileW(const std::wstring& path, std::wstring& outText)
{
    FILE* fp = _wfopen(path.c_str(), L"rb");
    if (!fp)
    {
        return false;
    }
    fseek(fp, 0, SEEK_END);
    const long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (size < 0)
    {
        fclose(fp);
        return false;
    }
    std::string raw(static_cast<std::size_t>(size), '\0');
    if (size > 0)
    {
        const std::size_t readBytes = fread(raw.data(), 1, static_cast<std::size_t>(size), fp);
        raw.resize(readBytes);
    }
    fclose(fp);
    if (raw.size() >= 3 &&
        static_cast<unsigned char>(raw[0]) == 0xEF &&
        static_cast<unsigned char>(raw[1]) == 0xBB &&
        static_cast<unsigned char>(raw[2]) == 0xBF)
    {
        raw.erase(0, 3);
    }
    outText = Utf8ToWide(raw);
    return true;
}

inline bool WriteUtf8FileW(const std::wstring& path, const std::wstring& text)
{
    EnsureDirectoryW(DirectoryOf(path));
    FILE* fp = _wfopen(path.c_str(), L"wb");
    if (!fp)
    {
        return false;
    }
    const unsigned char bom[3] = {0xEF, 0xBB, 0xBF};
    fwrite(bom, 1, 3, fp);
    const std::string utf8 = WideToUtf8(text);
    if (!utf8.empty())
    {
        fwrite(utf8.data(), 1, utf8.size(), fp);
    }
    fclose(fp);
    return true;
}

inline void AppendDebugLogW(const std::wstring& pluginRoot, const std::wstring& message)
{
    if (pluginRoot.empty())
    {
        return;
    }
    const std::wstring logDir = pluginRoot + L"\\logs";
    EnsureDirectoryW(logDir);
    const std::wstring logPath = logDir + L"\\KuaiJieJian_debug.log";
    FILE* fp = _wfopen(logPath.c_str(), L"ab");
    if (!fp)
    {
        return;
    }
    SYSTEMTIME st{};
    GetLocalTime(&st);
    wchar_t prefix[64]{};
    swprintf_s(prefix, L"[%04d-%02d-%02d %02d:%02d:%02d.%03d] ",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    const std::string line = WideToUtf8(std::wstring(prefix) + message + L"\r\n");
    fwrite(line.data(), 1, line.size(), fp);
    fclose(fp);
}

inline std::vector<ShortcutEntry> DefaultShortcuts()
{
    return {
        // NX 原生常用命令
        {L"e",   L"拉伸",       L"UG_MODELING_EXTRUDED_FEATURE",      L"NX原生", L"全部", true},
        {L"rev", L"旋转",       L"UG_MODELING_REVOLVED_FEATURE",      L"NX原生", L"全部", true},
        {L"h",   L"孔",         L"UG_MODELING_HOLE_FEATURE",          L"NX原生", L"全部", true},
        {L"f",   L"边倒圆",     L"UG_MODELING_BLEND_FEATURE",         L"NX原生", L"全部", true},
        {L"cha", L"倒斜角",     L"UG_MODELING_CHAMFER_FEATURE",       L"NX原生", L"全部", true},
        {L"tr",  L"修剪体",     L"UG_MODELING_TRIM_FEATURE",          L"NX原生", L"全部", true},
        {L"sp",  L"拆分体",     L"UG_MODELING_SPLIT_FEATURE",         L"NX原生", L"全部", true},
        {L"mi",  L"镜像特征",   L"UG_MODELING_MIRRORFEATURE_FEATURE", L"NX原生", L"全部", true},
        {L"u",   L"求和",       L"UG_MODELING_UNITE_FEATURE",         L"NX原生", L"全部", true},
        {L"su",  L"求差",       L"UG_MODELING_SUBTRACT_FEATURE",      L"NX原生", L"全部", true},
        {L"m",   L"移动对象",   L"UG_EDIT_MOVE_OBJECT",               L"NX原生", L"全部", true},
        {L"d",   L"删除",       L"UG_EDIT_DELETE",                    L"NX原生", L"全部", true},
        {L"z",   L"撤销",       L"UG_EDIT_UNDO",                      L"NX原生", L"全部", true},
        {L"y",   L"重做",       L"UG_EDIT_REDO",                      L"NX原生", L"全部", true},
        {L"s",   L"保存",       L"UG_FILE_SAVE_PART",                 L"NX原生", L"全部", true},
        {L"vf",  L"适合窗口",   L"UG_VIEW_FIT",                       L"NX原生", L"全部", true},
        {L"tb",  L"突出块",     L"UG_SBSM_TAB_FEATURE",               L"NX原生", L"钣金", true},
        {L"fl",  L"弯边",       L"UG_SBSM_FLANGE_FEATURE",            L"NX原生", L"钣金", true},
        {L"nc",  L"法向开孔",   L"UG_SBSM_NORMAL_CUTOUT_FEATURE",     L"NX原生", L"钣金", true},
        {L"ub",  L"伸直",       L"UG_SBSM_UNBEND_FEATURE",            L"NX原生", L"钣金", true},
        {L"rb",  L"重新折弯",   L"UG_SBSM_REBEND_FEATURE",            L"NX原生", L"钣金", true},
        {L"fs",  L"展平实体",   L"UG_SBSM_FLAT_SOLID_FEATURE",        L"NX原生", L"钣金", true},

        // 智辉钣金插件命令
        {L"xrsx",    L"写入部件属性",     L"Write_Prat_Attr",        L"智辉插件", L"全部", true},
        {L"plsx",    L"批量写入部件属性", L"PiLian_Write_Prat_Attr", L"智辉插件", L"全部", true},
        {L"tjhj",    L"统计焊件数量",     L"ZiDonFenCen",            L"智辉插件", L"全部", true},
        {L"bjx",     L"标记线",           L"BiaoJIXIan",             L"智辉插件", L"全部", true},
        {L"sdzbj",   L"手动转钣金",       L"SouDonZuanBanJin",       L"智辉插件", L"全部", true},
        {L"plzbj",   L"批量转钣金",       L"PiLianZuanBanJin",       L"智辉插件", L"全部", true},
        {L"ksx",     L"创建孔属性",       L"HoleAttribute",          L"智辉插件", L"全部", true},
        {L"zdct",    L"自动折弯图",       L"ZiDonCuTu",              L"智辉插件", L"制图", true},
        {L"kbz",     L"孔标注",           L"KonBiaoZu",              L"智辉插件", L"制图", true},
        {L"zdcc",    L"最大尺寸",         L"MaxDim",                 L"智辉插件", L"制图", true},
        {L"zdbz",    L"工程图自动标注",   L"ZiDonBiaoZhu",           L"智辉插件", L"制图", true},
        {L"bh",      L"编号",             L"BianHao",                L"智辉插件", L"全部", true},
        {L"hcl",     L"号材量",           L"BODYNOT",                L"智辉插件", L"全部", true},
        {L"xghcl",   L"修改号材量",       L"ATTRmodify",             L"智辉插件", L"全部", true},
        {L"mbsb",    L"90度角撕边",       L"MenBanSiBian",           L"智辉插件", L"全部", true},
        {L"blfg",    L"包容体分割",       L"BaoLonTiFenGe",          L"智辉插件", L"全部", true},
        {L"blqc",    L"包容体切除",       L"BaoRongQieChu",          L"智辉插件", L"全部", true},
        {L"zwbc",    L"折弯避让槽",       L"ZheWanBiRangCao",        L"智辉插件", L"全部", true},
        {L"bjcb",    L"钣金裁边",         L"BanJjinCaiBian",         L"智辉插件", L"全部", true},
        {L"bjct",    L"钣金拆图",         L"BanJinCaiTu",            L"智辉插件", L"全部", true},
        {L"ftkk",    L"方通卡口",         L"FangTongKaKou",          L"智辉插件", L"全部", true},
        {L"gctj",    L"管材统计",         L"XuanZeGuanJian",         L"智辉插件", L"全部", true},
        {L"dtzzp",   L"多体转装配",       L"DuoSiTiZuanZuanPei",     L"智辉插件", L"全部", true},
        {L"zjzt",    L"组件转实体",       L"ZuJianZuanTi",           L"智辉插件", L"全部", true},
        {L"jszj",    L"解散组件",         L"JieSanZuJian",           L"智辉插件", L"全部", true},
        {L"kfl",     L"孔防拉料检查",     L"KonFanLaLiao",           L"智辉插件", L"全部", true},
        {L"zwbj",    L"折弯标记缺口",     L"BendMarkNotch",          L"智辉插件", L"全部", true},
        {L"zwfz",    L"折弯辅助板",       L"ZeWanFuZu",              L"智辉插件", L"全部", true},
        {L"zwmn",    L"折弯模拟",         L"ZeWanMoNi",              L"智辉插件", L"全部", true},
        {L"qmzk",    L"球面展开",         L"QiuMianZanKai",          L"智辉插件", L"全部", true},
        {L"ftsz",    L"方通/圆管伸直",    L"FanTonSenZi",            L"智辉插件", L"全部", true},
        {L"tydf",    L"天圆地方",         L"TianYuanDiFan",          L"智辉插件", L"全部", true},
        {L"bzk",     L"智辉标准件库",     L"StandardPartsLibrary",   L"智辉插件", L"全部", true},
        {L"2pbc",    L"2点避让槽",        L"TwoPointBiLanCao",       L"智辉插件", L"全部", true},
        {L"ddsb",    L"多段角撕边",       L"TwoPointSiBian",         L"智辉插件", L"全部", true},
        {L"cpb",     L"拆平板",           L"CaiPinBan",              L"智辉插件", L"全部", true},
        {L"crb",     L"拆圆弧板",         L"CaiRBan",                L"智辉插件", L"全部", true},
        {L"fgyj",    L"分割圆角1",        L"CaiR1",                  L"智辉插件", L"全部", true},
        {L"bjtc",    L"板件调尺",         L"TiaoZenBanLeiCiCun",     L"智辉插件", L"全部", true},
        {L"mjx",     L"查看调整门间隙",   L"TiaoZenMenJianXi",       L"智辉插件", L"全部", true},
        {L"zdsst",   L"自动三视图",       L"AutoCreateThreeViews",   L"智辉插件", L"全部", true},
        {L"dcdwg",   L"导出DWG",          L"DaoCuDWG",               L"智辉插件", L"全部", true},
        {L"pldczkt", L"批量导出展开图",   L"PILianDaoCuZKT",         L"智辉插件", L"全部", true},
        {L"kk",      L"卡口",             L"KaKou",                  L"智辉插件", L"全部", true},
        {L"kz",      L"刻字",             L"KeZi",                   L"智辉插件", L"全部", true},
        {L"ftgmk",   L"方通公母口",       L"FanTonGMK",              L"智辉插件", L"全部", true},
        {L"mxb",     L"明细表",           L"MinXiBiao",              L"智辉插件", L"全部", true},
        {L"tbfx",    L"填补缝隙",         L"TianFenXI",              L"智辉插件", L"全部", true},
        {L"gjxj",    L"管件斜接处理",     L"QinLiGuanJian",          L"智辉插件", L"全部", true},
        {L"kjj",     L"快捷键设置",       L"KuaiJieJian",            L"智辉插件", L"全部", true},
        {L"gcbz",    L"公差标注",         L"GonCaBiaoZu",            L"智辉插件", L"制图", true},
    };
}

inline ShortcutConfig DefaultShortcutConfig()
{
    ShortcutConfig cfg;
    cfg.enabled = false;
    cfg.repeatOnEmptySpace = true;
    cfg.showHud = true;
    cfg.toggleHotkey = L"ALT+F1";
    cfg.entries = DefaultShortcuts();
    return cfg;
}

inline bool SaveShortcutConfig(const std::wstring& path, const ShortcutConfig& cfg)
{
    std::wstring out;
    out += L"# 智辉钣金插件 - 快捷键（按空格执行）配置文件\r\n";
    out += L"[Settings]\r\n";
    out += L"Enabled=" + std::wstring(cfg.enabled ? L"1" : L"0") + L"\r\n";
    out += L"RepeatOnEmptySpace=" + std::wstring(cfg.repeatOnEmptySpace ? L"1" : L"0") + L"\r\n";
    out += L"ShowHud=" + std::wstring(cfg.showHud ? L"1" : L"0") + L"\r\n";
    out += L"ToggleHotkey=" + cfg.toggleHotkey + L"\r\n";
    out += L"\r\n[Shortcuts]\r\n";
    out += L"# 格式: 简码=命令ID|显示名称|分类|启用(1/0)|适用模块\r\n";
    for (const auto& entry : cfg.entries)
    {
        if (entry.alias.empty() || entry.commandId.empty())
        {
            continue;
        }
        out += ToLowerAscii(TrimWide(entry.alias)) + L"=" +
               TrimWide(entry.commandId) + L"|" +
               TrimWide(entry.displayName) + L"|" +
               (entry.category.empty() ? L"自定义" : TrimWide(entry.category)) + L"|" +
               (entry.enabled ? L"1" : L"0") + L"|" +
               (entry.module.empty() ? L"全部" : TrimWide(entry.module)) + L"\r\n";
    }
    return WriteUtf8FileW(path, out);
}

inline bool LoadShortcutConfig(const std::wstring& path, ShortcutConfig& outCfg)
{
    std::wstring text;
    if (!ReadUtf8FileW(path, text))
    {
        return false;
    }
    ShortcutConfig cfg;
    cfg.entries.clear();
    std::wstring section;
    std::size_t pos = 0;
    while (pos <= text.size())
    {
        std::size_t eol = text.find(L'\n', pos);
        if (eol == std::wstring::npos)
        {
            eol = text.size();
        }
        std::wstring line = text.substr(pos, eol - pos);
        if (!line.empty() && line.back() == L'\r')
        {
            line.pop_back();
        }
        line = TrimWide(line);
        pos = eol + 1;

        if (line.empty() || line[0] == L'#' || line[0] == L';')
        {
            continue;
        }
        if (line.front() == L'[' && line.back() == L']')
        {
            section = ToLowerAscii(line.substr(1, line.size() - 2));
            continue;
        }
        const std::size_t eq = line.find(L'=');
        if (eq == std::wstring::npos)
        {
            continue;
        }
        const std::wstring key = TrimWide(line.substr(0, eq));
        const std::wstring val = TrimWide(line.substr(eq + 1));

        if (section == L"settings")
        {
            const std::wstring k = ToLowerAscii(key);
            if (k == L"enabled")
            {
                cfg.enabled = (val != L"0" && ToLowerAscii(val) != L"false");
            }
            else if (k == L"repeatonemptyspace")
            {
                cfg.repeatOnEmptySpace = (val != L"0" && ToLowerAscii(val) != L"false");
            }
            else if (k == L"showhud")
            {
                cfg.showHud = (val != L"0" && ToLowerAscii(val) != L"false");
            }
            else if (k == L"togglehotkey" && !val.empty())
            {
                cfg.toggleHotkey = ToUpperAscii(val);
            }
        }
        else if (section == L"shortcuts")
        {
            ShortcutEntry entry;
            entry.alias = ToLowerAscii(key);
            std::vector<std::wstring> parts;
            std::size_t start = 0;
            while (start <= val.size())
            {
                std::size_t bar = val.find(L'|', start);
                if (bar == std::wstring::npos)
                {
                    parts.push_back(TrimWide(val.substr(start)));
                    break;
                }
                parts.push_back(TrimWide(val.substr(start, bar - start)));
                start = bar + 1;
            }
            if (!parts.empty()) entry.commandId = parts[0];
            if (parts.size() >= 2) entry.displayName = parts[1];
            if (parts.size() >= 3 && !parts[2].empty()) entry.category = parts[2];
            else entry.category = L"自定义";
            if (parts.size() >= 4) entry.enabled = (parts[3] != L"0");
            if (parts.size() >= 5 && !parts[4].empty()) entry.module = parts[4];
            else entry.module = L"全部";
            if (entry.displayName.empty()) entry.displayName = entry.commandId;
            if (!entry.alias.empty() && !entry.commandId.empty())
            {
                cfg.entries.push_back(std::move(entry));
            }
        }
    }
    outCfg = std::move(cfg);
    return true;
}

inline ShortcutConfig LoadOrCreateShortcutConfig(const std::wstring& path)
{
    ShortcutConfig cfg;
    if (!FileExistsW(path) || !LoadShortcutConfig(path, cfg))
    {
        cfg = DefaultShortcutConfig();
        SaveShortcutConfig(path, cfg);
    }
    return cfg;
}

inline UINT ReloadWindowMessage()
{
    return RegisterWindowMessageW(L"Zhihui_KuaiJieJian_ReloadConfig");
}

inline const wchar_t* HudWindowClassName()
{
    return L"ZhihuiKuaiJianHudWindowClass";
}

struct LastCommandInfo
{
    std::wstring commandId;
    std::wstring displayName;
    std::wstring category;
};

inline bool SaveLastCommandW(const std::wstring& path, const LastCommandInfo& info)
{
    if (info.commandId.empty())
    {
        return false;
    }
    const std::wstring line = TrimWide(info.commandId) + L"|" +
        (info.displayName.empty() ? TrimWide(info.commandId) : TrimWide(info.displayName)) + L"|" +
        (info.category.empty() ? L"自定义" : TrimWide(info.category));
    return WriteUtf8FileW(path, line);
}

inline bool LoadLastCommandW(const std::wstring& path, LastCommandInfo& outInfo)
{
    std::wstring text;
    if (!ReadUtf8FileW(path, text))
    {
        return false;
    }
    text = TrimWide(text);
    if (text.empty())
    {
        return false;
    }
    const std::size_t p1 = text.find(L'|');
    if (p1 == std::wstring::npos)
    {
        outInfo.commandId = text;
        outInfo.displayName = text;
        outInfo.category = L"自定义";
        return true;
    }
    const std::size_t p2 = text.find(L'|', p1 + 1);
    outInfo.commandId = TrimWide(text.substr(0, p1));
    if (p2 == std::wstring::npos)
    {
        outInfo.displayName = TrimWide(text.substr(p1 + 1));
        outInfo.category = L"自定义";
    }
    else
    {
        outInfo.displayName = TrimWide(text.substr(p1 + 1, p2 - p1 - 1));
        outInfo.category = TrimWide(text.substr(p2 + 1));
    }
    if (outInfo.displayName.empty())
    {
        outInfo.displayName = outInfo.commandId;
    }
    if (outInfo.category.empty())
    {
        outInfo.category = L"自定义";
    }
    return !outInfo.commandId.empty();
}

} // namespace zhihui::kjj
