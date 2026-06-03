# KeZi 体刻字

NX2412 C++ 插件，用于在实体平面或自动识别的方通外侧大面上生成编号刻字。

## 功能

- 手动模式：选择面和点位后刻字。
- 方通自动模式：选择实体后自动寻找最长方向的大外侧平面。
- 编号混合来源：体属性、零件属性优先，缺失时按前缀、起始号、位数自动生成。
- 使用 NX TextBuilder 生成文字轮廓，并尝试用 ExtrudeBuilder 对目标体做减料凹刻。

## 编译

```bat
build_manual\build.bat
```

Visual Studio 可直接打开 `KeZi.vcxproj` 编译。
