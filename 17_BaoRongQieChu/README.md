# BaoRongQieChu

`BaoRongQieChu` 是一个 NXOpen C++ / Block Styler 插件，用于对所选对象执行局部包容切除。

## 当前行为

- 支持选择同一个实体上的实体、面、边。
- 根据所选对象的联合包围盒，向外偏置 `0.1mm` 创建一个包容块。
- 勾选 `布尔减体` 时，立即用所属实体减去该包容块，不复制目标体，也不保留工具体副本。
- 取消勾选 `布尔减体` 时，只创建包容块，便于检查范围。
- 选择对象后立即执行，成功后自动清空当前选择。

## 构建

```powershell
$env:UGII_BASE_DIR="D:\Program Files\Siemens\NX2412"
cmake -S D:\UGPluginRepo\BaoRongQieChu -B D:\UGPluginRepo\BaoRongQieChu\build
cmake --build D:\UGPluginRepo\BaoRongQieChu\build --config Release
```
