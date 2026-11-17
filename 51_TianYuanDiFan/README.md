# 天圆地方（NX 2412）

在工作零件原点生成天圆地方钣金体，并生成一张展平图样。方口在 Z=0，圆口朝 +Z，两者同心。

圆直径、底边长、底边宽和高度按外轮廓输入，单位是毫米，英寸零件会自动换算。底边长沿 X，底边宽沿 Y。板厚加在轮廓内侧。方口圆角必须大于板厚。成形方式与手动录制一致：Y=0 以下的半边方口对半圆弧做一次放样弯边。两条截面是放样弯边内部的草图。沿 XC-ZC 镜像几何体，两侧 +X 端面各偏置 -0.1 mm 后合并，再对半边端面做展平图样。展平图样在独立视图 FLAT-PATTERN 中。分段可选整体、中分2段、中分4段、对角分2段或对角分4段。中分从边的中点切开。对角分仍用中分的半边曲线放样并镜像，再沿方口两条对角平面切开，把落在同一侧的块合并。每块各有展平图样。勾选「使用多段折弯」后可设置折弯段数目和索引标记长度，索引标记只出现在展平图样里。

展开长度由 NX 按当前零件的钣金设置计算。

## 使用

1. 打开零件，启动菜单“天圆地方”。
2. 输入圆直径、底边长、底边宽、高度、板厚、接缝宽度和方口圆角。需要分段折弯时勾选「使用多段折弯」。
3. 确定或应用。零件导航器中出现“天圆地方”和“天圆地方展开”。

再次应用会在同一位置再生成一个。已应用的结果由 NX 撤销管理。对话框会记住上次的尺寸。

## 构建

```powershell
cmake -S . -B build-release -A x64 -DZH_TRANSITION_TESTS=ON -DNX_ROOT="D:\Program Files\Siemens\NX2412"
cmake --build build-release --config Release --target TianYuanDiFan TransitionIntegration
$env:UGII_BASE_DIR = "D:\Program Files\Siemens\NX2412"
$env:PATH = "$env:UGII_BASE_DIR\NXBIN;$env:PATH"
.\build-release\Release\TransitionIntegration.exe G:\TianYuanDiFanTest
python tools/deploy.py
```

测试程序只在给定的新目录里建立零件，不打开用户模型，也不部署。
