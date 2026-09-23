# 智辉折弯模拟（ZeWanMoNi）

2026-09-21 首版：NX 2412 / Windows x64 原生单刀定位与静态干涉检查。已生成可加载的 Release DLL，完成真实 NX 几何测试和对话框连续操作验证，并部署至本机智辉插件目录。

支持内侧圆柱折弯面和内侧直线锐边自动识别、三种示例刀具、自定义截面、方向反转、自动定位与正视，以及穿透/接触/间隙不足/无干涉结果。检查使用临时刀具实体并回滚，不在源零件中保留刀具特征。

2026-09-21 精简与干涉标红更新：取消全部数值输入，刀具截面来自刀库，长度自动覆盖所选折弯，圆柱 R 读取模型、锐边按 R=0 原位定位。选折弯、换刀和反转后自动检查；黄色显示刀具轮廓，红色粗线标出真实实体求交得到的重叠边界，支持多个独立干涉区域。接触不误画成红色。放置输入只保留“反转刀具方向”。

2026-09-21 刀具列表更新：刀具名称和截面缩略图直接列出；选中后显示截面大图及宽高，并同步更新三维轮廓。自定义刀具使用同一列表。“应用”和重新加载后保留当前刀具选择。

2026-09-21 DWG 刀库更新：从用户提供的 `折弯模拟.dwg` 提取 10 个刀型，包括 RZ8895、RZ30104、RW8867、RW8890、RW88100、RW88120 和原图编号 000、001、003、004。按毫米原尺寸安装到用户刀库，源图不变。[刀库与转换记录](tool-library/dwg-20260921/README.md)说明定位基准、尺寸、圆弧误差和源图处理。临时刀具实体改用带独立公差的 NX ExtrudeBuilder，解决英寸旋转放置时短线段导致旧拉伸接口失败的问题，不修改零件建模公差。

2026-09-23 DWG 界面预览：选择本机 DWG 后通过 AutoCAD ObjectDBX 枚举闭合轮廓，在刀具列表中先看截面图和宽高，再将选中的上刀保存到 `D:\UG智辉钣金插件\刀图`，原图留存于 `刀图\原图`。旧用户刀库安装时迁移到新目录。导入阶段按毫米图纸处理，不自动猜测缩放或修补断线。

**目前检查当前姿态；完整工序动画、连续运动碰撞、下模和整机模拟尚未实现。** 内置刀具为演示尺寸，应以实际刀具截面检查真实零件。

## 使用与交付

- [使用说明](使用说明.md)：启动、选择、参数、检查结果和自定义刀具格式。
- [验证记录](验证记录.md)：已测试样件、对话框操作和验证边界。
- [参考调研](参考调研.md)：博士钣金官方视频中实际观察到的功能。
- [开发路线](开发路线.md)：前期方案存档；其中规划功能不等于当前已实现功能。

发布资源位于 `build-codex/Release`：`ZeWanMoNi.dll`、`ZeWanMoNi.dlx`、`ZeWanMoNi.bmp`、`ZeWanMoNiExample.ztool`，以及 `ZeWanMoNiTools` 中的 10 个 DWG 刀具文件。部署目标为 `D:\UG智辉钣金插件\application`；对应菜单命令为“折弯模拟”。新增菜单需重启 NX 加载。可以通过“执行 NX Open”直接加载已部署 DLL，仍执行正常授权检查。

## 源码结构

| 文件 | 职责 |
| --- | --- |
| ZeWanMoNiMain.cpp | 受保护的 NX 命令入口与卸载 |
| ZeWanMoNi.cpp / .hpp | 原生 Block Styler 对话框、回调、预览和视角恢复 |
| BendSimulation.cpp / .hpp | 折弯位置识别、临时刀具实体、精确干涉与回滚 |
| ToolProfiles.cpp | 示例刀具、自定义截面校验、定位及轮廓 |
| DwgImport.cpp / .hpp | AutoCAD ObjectDBX 只读提取候选闭合截面 |
| ToolThumbnails.cpp / .hpp | 从同一刀具截面生成列表缩略图及选中项大图 |
| tests/Integration.cpp | 在 NX 环境中创建样件并验证真实几何接口 |
| tools/generate_resources.py | 生成对话框及图标资源 |
| tools/deploy.py | 保护校验、时间戳备份、安装及清单更新 |

## 构建

使用 Visual Studio 2022 的 CMake 与 NX 2412 SDK。以下命令在本模块目录执行，`cmake` 应指向 Visual Studio 安装中的程序。

```powershell
cmake -S . -B build-codex -A x64 '-DNX_ROOT=D:/Program Files/Siemens/NX2412' -DZH_BEND_SIM_TESTS=ON
cmake --build build-codex --config Release
```

几何测试程序需要 NX 运行环境和可用许可证；输出目录必须尚不存在。先将 NX 的 `NXBIN` 加入本次进程的 PATH，设置 `UGII_BASE_DIR` 为安装目录，再运行：

```powershell
.\build-codex\Release\BendSimulationIntegration.exe G:\ZeWanMoNiTest_new
```

测试程序创建自己的零件和截面文件，不能将已有生产零件目录作为输出目录。

运行期修改后须先完成 Release 构建与 NX 验证，再运行 `python tools/deploy.py`。脚本只安装列出的资源和已审核的 10 个刀具，保留现有授权/防篡改元数据；在 `D:\UG智辉钣金插件\backup` 备份原资源及清单，并更新菜单、清单与仓库部署参考。详细文件哈希和备份路径写入 `build-codex/deployment-verification.json`。再运行 `python tools/install_tool_library.py` 安装到智辉目录下的 `刀图` 并迁移旧用户刀库，记录写入 `build-codex/tool-library-installation.json`。

DWG 刀库几何测试：`BendSimulationIntegration.exe 新输出目录 tool-library/dwg-20260921/tools`，验证 10 把刀的毫米及旋转英寸实体、体积、正反向检查、回滚和公差不变。测试 EXE 不部署。
