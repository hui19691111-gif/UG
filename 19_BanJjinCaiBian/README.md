# BanJjinCaiBian

这是一个 Siemens NX / UG 的 C++ 插件骨架工程，当前先完成第一个最小目标：

- 通过 `NXOpen::UI::SelectionManager()` 调起 NX 自带选择对话框
- 对话框只允许选择两类对象：
  - 点 `Point`
  - 边 `Edge`
- 选择结果会输出到 `Listing Window`

## 当前工程结构

```text
BanJjinCaiBian/
├─ BanJjinCaiBian.vcxproj
├─ BanJjinCaiBian.vcxproj.filters
├─ README.md
└─ src/
   └─ BanJjinCaiBian.cpp
```

## 开发环境假设

- Windows
- Visual Studio 2022
- Siemens NX 已安装
- 已配置环境变量 `UGII_BASE_DIR`

示例：

```powershell
$env:UGII_BASE_DIR = "C:\Program Files\Siemens\NX2306"
```

## 编译说明

1. 用 Visual Studio 打开 [BanJjinCaiBian.vcxproj](D:/UGPluginRepo/BanJjinCaiBian/BanJjinCaiBian.vcxproj)
2. 选择 `x64`
3. 编译 `Debug` 或 `Release`
4. 输出 DLL 位于 `build\<Configuration>\BanJjinCaiBian.dll`

## 在 NX 中运行

1. 打开 NX
2. 执行 `工具 -> Journal -> 播放` 或 `文件 -> 执行 -> NX Open`
3. 选择编译好的 `BanJjinCaiBian.dll`
4. 插件会弹出 NX 自带选择对话框，只允许选择点和边

## 代码要点

核心筛选逻辑在 [src/BanJjinCaiBian.cpp](D:/UGPluginRepo/BanJjinCaiBian/src/BanJjinCaiBian.cpp)：

- 点过滤：`UF_point_type`
- 边过滤：`UF_solid_type + UF_UI_SEL_FEATURE_ANY_EDGE`

对应接口使用的是 NXOpen C++ 的：

- `NXOpen::UI`
- `NXOpen::Selection`
- `NXOpen::Selection::MaskTriple`

## 下一步建议

这个最小版本跑通后，我们可以继续往下做：

1. 改成真正的 Block Styler 图形对话框
2. 分成两个独立选择框：基准点、目标撕边
3. 增加钣金合法性检查
4. 开始实现撕边几何逻辑
