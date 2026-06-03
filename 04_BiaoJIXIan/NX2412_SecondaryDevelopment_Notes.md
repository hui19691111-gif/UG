# NX2412 二次开发笔记

## 资料入口
- NXOpen 文档镜像: [https://www.ugapi.com/doc/NXOpen/](https://www.ugapi.com/doc/NXOpen/)
- UFUN 文档镜像: [https://www.ugapi.com/doc/UFun/](https://www.ugapi.com/doc/UFun/)
- 本机 Sample: `D:\Program Files\Siemens\NX2412\UGOPEN\SampleNXOpenApplications\C++`
- 本机头文件目录: `D:\Program Files\Siemens\NX2412\UGOPEN`

## 这份笔记的目标
- 记录本项目 `BiaoJiXian` 已经确认可用的开发模式。
- 记录后面继续开发时最常用、最容易出错的 API。
- 尽量把“文档结论”转成“项目里直接能用的做法”。

## Block Styler / UI 基本模式

### Block Styler 典型结构
来自 Sample:
- `D:\Program Files\Siemens\NX2412\UGOPEN\SampleNXOpenApplications\C++\BlockStyler\ColoredBlock\ColoredBlock.cpp`
- `D:\Program Files\Siemens\NX2412\UGOPEN\SampleNXOpenApplications\C++\BlockStyler\ColoredBlock\README.txt`

稳定写法:
- 构造函数里先拿 `Session::GetSession()` 和 `UI::GetUI()`。
- 用 `UI->CreateDialog("xxx.dlx")` 创建对话框。
- 注册 `AddInitializeHandler`、`AddDialogShownHandler`、`AddApplyHandler`。
- `initialize_cb()` 里做 block 绑定和属性初始化。
- `apply_cb()` 里读取界面值并执行建模逻辑。
- `ufusr()` 里 new 对象并 `Launch()`。
- `ufusr_ask_unload()` 一般返回 `Session::LibraryUnloadOptionImmediately`。

项目结论:
- `.dlx` 文件不能只靠裸文件名查找，最好从 DLL 所在目录和 `ui` 子目录主动定位。
- 重新生成的 `*_BJ.cpp` / `*_BJ.hpp` 容易被覆盖，算法逻辑必须拆到独立文件，比如现在的 [MarkLineBuilder.cpp](G:\NX2412C++\BiaoJiXian\MarkLineBuilder.cpp)。

### UI Styler 选择相关模式
来自 Sample:
- `D:\Program Files\Siemens\NX2412\UGOPEN\SampleNXOpenApplications\C++\Selection_UIStylerUIStyler\UIStylerSelectionExample.cpp`
- `D:\Program Files\Siemens\NX2412\UGOPEN\SampleNXOpenApplications\C++\Selection_UIStylerUIStyler\UIStylerSelectionExample_Cpp_ReadMe.txt`

有用结论:
- 选择过滤通常通过 `SelectionManager()->SetSelectionMask(...)` 完成。
- 边选择可用:
  - `selectionMask.Type = UF_solid_type`
  - `selectionMask.Subtype = UF_solid_edge_subtype`
  - `selectionMask.SolidBodySubtype = UF_UI_SEL_FEATURE_ANY_EDGE`
- 选择回调常用:
  - `SetTaggedObjectSelectionCallbacks(...)`
  - 过滤回调 `filter_cb`
  - 选中回调 `sel_cb`

项目结论:
- 如果后面现有 Block Styler 选择块不够用，可以参考这个 Sample 把选择过滤再收紧。
- “只允许选边 / 只允许选实体”的限制，优先在 UI 层做，不要把大量无效对象丢给业务层。

## NXOpen 与 UFUN 混用策略

### 当前项目推荐原则
- UI、对象集合、builder 管理优先用 NXOpen C++。
- 精细几何和底层显示属性优先用 UFUN。
- UFUN 返回 `tag_t` 后，用 `NXOpen::NXObjectManager::Get(tag)` 转回 NXOpen 对象。

这样做的原因:
- NXOpen 代码结构更清楚，适合 UI 和 builder 生命周期管理。
- UFUN 在投影、测量、显示属性、曲线查询这类底层能力上更直接。

## 本项目已经验证过的关键 API

### 1. 曲线投影
头文件:
- `uf_curve.h`

已经确认的接口:
- `UF_CURVE_init_proj_curves_data`
- `UF_CURVE_create_proj_curves`
- `UF_CURVE_ask_proj_curves`

项目中的使用方式:
- 输入对象: 体边 / 接触面边 / 用户选中的边。
- 目标对象: 大体的相贴面。
- 投影方式:
  - `proj_type = 1`
  - 表示沿面法向投影。
- `copy_flag = 3`
  - 创建关联投影特征。
- 通过 `UF_CURVE_ask_proj_curves` 取回投影生成的结果曲线。

项目结论:
- “轮廓模式 / 底面模式 / 选边模式” 都可以统一走这条投影链。
- 先投影，再做最大外环筛选和分段，比先手工算投影稳定。

### 2. 曲线参数与长度
头文件:
- `uf_curve.h`

已经确认的接口:
- `UF_CURVE_ask_parameterization`
- `UF_CURVE_ask_arc_length`
- `UF_CURVE_evaluate_curve`

项目里的作用:
- 获取曲线起止参数。
- 获取曲线长度。
- 获取曲线首尾点。

项目结论:
- 当前“最大外环”第一版已经用这些接口做了连通分组和总长度比较。
- 后面做“两端保留，中间按间距分段”时，这几个接口会继续是核心。

### 3. 相贴面识别和距离判断
头文件:
- `uf_modl.h`

已经确认的接口:
- `UF_MODL_ask_minimum_dist_2`

项目里的作用:
- 判断两个面是否相贴。
- 目前通过最小距离 `<= tolerance` 识别接触关系。

项目结论:
- 这个接口适合做第一层接触筛选。
- 复杂件后面如果出现“误判为接触”，要继续增加法向一致性或重叠范围判断。

### 4. 面面积计算
头文件:
- `uf_modl.h`

已经确认的接口:
- `UF_MODL_ask_mass_props_3d`

项目里的作用:
- 以 sheet 分析方式读取单个面的面积。
- 当前项目用它来判断“大体相贴面面积 > 小体相贴面面积”。

项目结论:
- 面积判断必须用真实面积，不能再用边数或近似代理值。
- 这是识别“大体”的关键条件之一。

### 5. 显示属性
头文件:
- `uf_obj.h`

已经确认的接口:
- `UF_OBJ_set_color`
- `UF_OBJ_set_font`
- `UF_OBJ_set_line_width`
- `UF_OBJ_set_layer`

项目里的作用:
- 给生成的标记曲线设置颜色、线型、线宽、图层。

项目结论:
- 生成曲线后立刻设置显示属性最稳，不要依赖当前部件默认样式。

## 当前项目已形成的实现思路

### 模式分发
- `OuterContour`
  - 取小体全部边。
  - 投影到每个相贴大体的相贴面。
  - 再筛最大外环。
- `BottomFace`
  - 取小体相贴面的边界。
  - 投影到对应大体相贴面。
- `SelectedEdges`
  - 取用户选中的边。
  - 先反查所属小体。
  - 再投影到对应大体相贴面。

### 接触体识别
- 先从工作部件里遍历候选体。
- 逐面对逐面检查最小距离。
- 找到接触面后，再比较面面积。
- 如果同一个小体接触多个大体，所有满足规则的大体都要标记。

### 最大外环
- 现阶段做法:
  - 根据投影曲线首尾点连通关系分组。
  - 保留总长度最大的那组。
- 后面可优化为:
  - 真正按闭环求面积。
  - 选择包围面积最大的闭环。

## 后续编码时优先遵守的规则
- 几何算法尽量拆到服务层，别回填到生成的 `*_BJ.cpp`。
- 一切 `.dlx` / `.dlg` 相关路径都按“DLL 所在目录优先”处理。
- 能直接用现成 UFUN / NXOpen builder 的地方，不要自己重造几何。
- 需要选择过滤时，先在 UI 侧限死对象类型，再在业务层二次校验。
- 创建 0.001 深、0.001 宽这类小特征时，公差必须比尺寸更小。
- 每次建模前尽量加 Undo Mark，异常时便于回退和排错。

## 对本项目下一阶段最有用的文档主题
- `Features::ProjectCurveBuilder`
- `UF_CURVE_create_proj_curves`
- `Features::DivideCurveBuilder`
- `Features::TrimCurve2Builder`
- `Features::TubeBuilder`
- `Section`
- `ScRuleFactory`
- `SelectionIntentRule`

## 给后续开发的直接建议
- 分段优先研究:
  - `DivideCurveBuilder`
  - `TrimCurve2Builder`
- 浅槽优先研究:
  - `TubeBuilder` 配合布尔减
  - 或截面扫掠切除 builder
- 如果后面 Sample 中找到更贴近“投影后分段再切除”的案例，优先照 Siemens 的 Sample 结构来做。
