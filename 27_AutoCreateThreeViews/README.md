# 自动创建三视图

这是一个 NXOpen C++ / Block UI Styler 插件骨架，用来承载“钣金自动创建三视图”的对话框和后续出图逻辑。

## 当前已包含

- `ufusr` 入口，可作为 NX 用户出口 DLL 加载。
- `AutoCreateThreeViewsDialog` 对话框包装类。
- `AutoCreateThreeViews.dlx` 对话框草稿，包含基本、视图、标注、输出、规则库五类参数。
- `AutoCreateThreeViews.bmp` 插件工具栏图标，打包时随 application 文件一起发布。
- `AutoCreateThreeViews.rules.json` 默认规则库配置。
- Release x64 Visual Studio 工程。

## 后续接入点

主要业务逻辑集中放到：

```cpp
int AutoCreateThreeViewsDialog::ExecuteCreateDrawing()
```

建议按这个顺序补功能：

1. 获取当前工作部件和主实体。
2. 根据规则库选择图纸模板。
3. 用 NXOpen Drafting 创建图纸页。
4. 创建正视图、俯视图、右视图、等轴测视图。
5. 根据标注页参数补尺寸、孔位、折弯信息。
6. 根据输出页参数导出 PDF / DWG。

## 使用方式

1. 用 Visual Studio 打开 `自动创建三视图.sln`。
2. 确认 `UGII_BASE_DIR` 指向当前 NX 安装目录。
3. 编译 `Release|x64`。
4. 将 `bin\\Release\\AutoCreateThreeViews.dll`、`AutoCreateThreeViews.dlx`、`AutoCreateThreeViews.bmp` 放到 NX 插件 application 目录。
