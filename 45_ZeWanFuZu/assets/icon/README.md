# 折弯辅助板专用图标

蓝灰色折弯板表示原板，橙色梯形板表示辅助板，两处橙色短连接表示微连接。原板上沿为明显的斜边，与底部折弯线不平行；辅助板沿此斜边连接，外侧定位边与折弯线平行。无文字，供 NX 小尺寸工具栏使用。

- `ZeWanFuZu.master.png`：内置 ImageGen 生成的透明原图。
- `ZeWanFuZu.256.png`：透明预览图。
- `ZeWanFuZu.32.png`：32 × 32 透明图标。
- `../../ZeWanFuZu.bmp`：部署到 NX 的 32 × 32、24 位 RGB 图标，沿用此前图标的 RGB(240, 243, 246) 背景。
- `Export-ToolbarIcon.ps1`：仅执行尺寸和格式转换，保留透明原图。

运行导出脚本后重新编译 Release，再运行 `../../tools/deploy.py`，备份旧资源并更新部署清单。NX 可能缓存工具栏图标，重新启动后加载新版。

## 初版生成提示词（内置 ImageGen）

Use case: logo-brand. Asset type: one dedicated Siemens NX CAD toolbar command icon for 'bend assist plate', final use 32x32 pixels; generate a clean large transparent PNG master, square composition. Subject: an isometric L-shaped folded sheet-metal part in cool blue-gray steel, with a clearly distinct orange removable bend-assist extension attached to the sloping free edge of its upright flange by EXACTLY TWO short narrow rectangular micro-connection tabs. The upright original sheet flange has a slanted free edge. The orange extension is a single wide trapezoidal patch in the SAME PLANE as this flange, above that slanted edge; its straight outer locating edge is parallel to the straight bend at the bottom of the flange. Leave a clearly visible thin transparent gap between the orange auxiliary extension and original blue-gray flange, interrupted only by the two orange bridge tabs. A short base flange turns backward at a right angle at the lower bend, visibly conveying folded sheet metal. Style: crisp CAD engineering toolbar pictogram, simplified isometric geometry, bold clean dark slate outlines, restrained flat color facets with very subtle metallic shading, not photorealistic, no dramatic lighting, no soft shadow. Ensure the orange extension and two bridges are the visual focus and recognizable at 32px. Composition one cohesive symbol centered, occupies 88 percent of square with comfortable transparent margins, front upright flange dominates and base flange is small. No dimension arrows, no tools, no detached parts, no screws, no text, no letters, no badges, no border container, no watermark, no background. True transparent background.
