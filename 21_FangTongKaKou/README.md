# FangTongKaKou 方通卡口

NX C++ 插件项目，用于识别方通，并为后续在方通之间自动创建安装定位焊接卡口提供基础。

## 当前状态

- DLL 入口：`ufusr`
- 输出：`FangTongKaKou.dll`
- 当前功能：
  - 选择实体、面或边；
  - 归并到对应实体；
  - 按包围盒比例和平面数量识别候选方通；
  - 在 Listing Window 和消息框输出识别报告。

## 下一步预留

- 识别方通截面外轮廓和内孔；
- 识别两根方通之间的相交/搭接关系；
- 按间隙、深度、宽度规则生成卡口切除体；
- 执行布尔切除，并保留可回退的日志。

## 编译

手动编译：

```bat
FangTongKaKou\build_manual\build.bat
```

Visual Studio 编译：

```text
FangTongKaKou\FangTongKaKou.vcxproj
```
