# 孔属性插件

UG/NX 2412 C++ 孔属性插件。

## 功能

- 框选或点选孔内圆柱面，并自动排除外圆柱面、非完整圆柱面。
- 按直径和孔面高度分组，列表显示直径、数量、类型、规格、备注。
- 选中列表行时，高亮对应孔面。
- 编辑直径时，用 `UF_MODL_create_resize_face` 修改该组孔面的直径。
- 类型从 Excel 规则表读取。插件启动时加载一次，用户修改表格后可点“加载表格数据”手动重载。
- 规格和备注写入面属性：`类型`、`规格`。

## 规则表

规则表与 `KonBiaoZu` 共用，文件名为 `KonBiaoZuRules.xlsx`。程序优先查找 DLL 同级或上级目录里的 `DATA\KonBiaoZuRules.xlsx`，开发环境下会自动查找 `D:\UGPluginRepo\KonBiaoZu\data\KonBiaoZuRules.xlsx`。

插件直接调用 Excel 读取所有工作表。支持 `KonBiaoZuRules.xlsx` 的配置表格式：第 2 行为类别，第 3 行为标注文本模板，第 4 行为注释模板，第 7 行开始为底孔和规格映射。

| 列 | 含义 |
|---|---|
| A | 直径，支持精确值，也支持 `>3<4`、`3-4`、`3~4` |
| B | 类型 |
| C | 规格 |
| D | 备注 |

板厚由孔圆柱面的高度确定。压铆螺母这类规则可在一个规格单元格里写枚举，例如：`-1=S-M3-1; -2=S-M3-2; -3=S-M3-3`。程序按孔面高度自动选规格：`-1 = 0.8-1.4`，`-2 = 1.4-2.3`，`-3 = 2.3-3.2`。

类型为“孔”时，规格和备注保持空。找不到匹配规则时，规格和备注也保持空。

## 使用方式

1. 确认环境变量 `UGII_BASE_DIR` 指向 `D:\Program Files\Siemens\NX2412`。
2. 用 Visual Studio 打开 `HoleAttribute.vcxproj`，选择 `x64|Debug` 或 `x64|Release` 编译。
3. 运行时让 `HoleAttribute.dll`、`HoleAttribute.dlx` 和共享的 `DATA\KonBiaoZuRules.xlsx` 保持在同一套部署目录中。
4. 在 NX 中通过“文件 -> 执行 -> NX Open”运行 `HoleAttribute.dll`。

## 界面控件

- `selection0`：选择孔圆柱面。
- `tree_control0`：孔属性列表。
- `addCrossSelectionNodeButton`：从选择框刷新列表。
- `addNodeButton`：打开 Excel 规则表。
- `deleteNodeButton`：加载表格数据。
