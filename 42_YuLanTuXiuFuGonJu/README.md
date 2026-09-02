# UG2412 预览图修复工具（UG 内部版）

本工具必须在 NX 2412 已完全启动后，从“智辉钣金”菜单运行。

## 功能

- 当前零件：将模型空间工作视图设为正等轴测、适合窗口并保存原生预览图，随后恢复进入工具前的视图。
- 批量文件夹：逐个打开 `.prt`、修复预览图并关闭，结束后恢复原显示零件和原视图。
- 使用 NX 原生预览设置：同时开启“保存预览”和“存储预览”，创建时间均设为“部件保存时”。
- 可选择是否递归处理子文件夹。
- 不在零件目录创建 `.bak`。
- 批量处理中，临时恢复副本仅写入 Windows 临时目录；预览验证成功后立即删除，失败时自动恢复原 `.prt`。
- 已在 NX 会话中修改但尚未保存的零件会跳过，避免覆盖用户工作。
- 逐文件的生成尺寸、像素变化数、校验值和错误会写入 `D:\UGZhiHuiLogs\YuLanTuXiuFuGonJu.log`。
- `NxPartThumbnailProvider.dll` 是 Windows 11 资源管理器缩略图处理器，只读取 `.prt` 内已存在的 NX 原生 JPEG 预览，不启动 NX、不修改零件。

注意：NX 必须保持可见的图形上下文才能在保存时生成原生模型预览，因此批量期间不能同时操作这一 NX 会话。

## 构建

```powershell
cmake -S . -B build-internal -A x64
cmake --build build-internal --config Release
```

输出：`build-internal/Release/YuLanTuXiuFuGonJu.dll`
