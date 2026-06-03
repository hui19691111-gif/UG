# 标记线插件项目

这是一个适用于 `UG/NX 2412` 的 C++ DLL 插件项目，用于在与所选实体相贴、且更大的实体上创建浅凹槽标记线。

## 当前保留的最终版本

- 工程文件：`BiaoJiXian.vcxproj`
- 解决方案：`BiaoJiXian.sln`
- 主源码：`src/BiaoJiXian.cpp`
- 对话框：`BiaoJiXian.dlx`
- 图标：`assets/mark-line-icon.svg`
- 输出目录：`bin/Release`

## 功能

- 支持一次多选多个实体批量执行
- 创建方式包含：
  - `底面`
  - `轮廓`
- `底面` 模式：
  - 查找与所选实体相贴且更大的目标体
  - 提取相贴底面的外围轮廓
  - 投影、拉伸、求差，创建深度 `0.001` 的凹槽
- `轮廓` 模式：
  - 使用 `NXOpen::Features::ShadowCurveBuilder` 在接触大面上生成阴影轮廓
  - 去参数后删除内环
  - 再按与底面模式相同的后半段流程创建凹槽

## 编译

1. 用 Visual Studio 2022 打开 `BiaoJiXian.sln`
2. 选择 `Release | x64`
3. 编译后得到：
   - `bin\\Release\\BiaoJiXian.dll`
   - `bin\\Release\\BiaoJiXian.dlx`

## 在 NX 中使用

1. 在 NX 中执行 UG/Open 用户函数
2. 加载 `BiaoJiXian.dll`
3. 在对话框中选择实体和创建方式
4. 点击确定执行
