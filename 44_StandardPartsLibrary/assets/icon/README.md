# 标准件库图标

2026-09-08 使用内置 imagegen 生成新版：蓝色零件托盘表示库，内六角螺丝与螺母表示标准件，橙色滑块表示用户定义参数。

- `StandardPartsLibrary.master.png`：原始透明 PNG，1254×1254。
- `StandardPartsLibrary.256.png`：透明预览图。
- `StandardPartsLibrary.32.png`：工具栏实际尺寸的透明 PNG。
- `../../StandardPartsLibrary.bmp`：NX 部署文件，32×32、32 位 RGB，沿用旧图标的 RGB(209,209,209) 背景键。
- `Export-ToolbarIcon.ps1`：从原图重现以上 PNG/BMP 导出。只做尺寸和格式转换，原图保持不变。

部署路径为 `D:\UG智辉钣金插件\application\StandardPartsLibrary.bmp`。NX 已运行的工具栏可能缓存旧图标，重新启动 NX 后加载新版。

## 生成提示词

Use case: logo-brand. Create a single polished compact toolbar icon for a Siemens NX CAD plugin called Standard Parts Library (no text should appear). It stores reusable standard and custom mechanical parts, with user-defined dimensions to select sizes and insert models. Design one coherent icon, not a presentation or a sheet of options. A sturdy blue low-sided open parts tray, containing one upright silver socket-head cap screw on the left and one substantial silver hex nut on the right. In the front lower-right integrate a simple orange parameter-adjustment symbol: two short horizontal slider rails and two bold square slider handles. The screw and nut must read clearly as different mechanical parts. Use an engineering CAD toolbar aesthetic, very simplified solid isometric forms, restrained metal facet shading, navy outlines and crisp geometry. Designed to stay recognizable when rasterized at 32x32 pixels: large silhouette, very few details, no hairlines, no fine threaded texture (only two broad thread marks if needed), no measurements, no extra arrows, no letters or numbers, no watermark, no enclosing app tile or border. Center the single icon square in frame and fill about 88% width/height. Isolated on a truly transparent background with clean alpha; no ground shadow, no vignette. Output one production-ready square icon.
