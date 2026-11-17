# 折弯标记缺口（43_BendMarkNotch）错误归档

## 2026-11-17 大折弯相切边界缺口后重新折弯失败

- 错误现象：`D:\UG智辉钣金插件\logs\BendMarkNotch_debug.log` 中，2026-11-16 16:04 和 2026-11-17 08:30 两次运行都报 `ProcessBody NXException stage=commit rebend code=3675017 message=Cannot create rebend. Try changing input selections.`，整条命令回退，模型没有打上缺口。
- 失败零件：单折弯钣金体，R171、板厚 4（R/t = 42.75，按大折弯处理），段数 1，所以只在两条相切边界上打缺口：2 条边界 × 2 端 = 4 个 Ø3 通孔。伸直、草图、拉伸、布尔减都成功了，只有重新折弯（Rebend）被 NX 拒绝。重新折弯用的是伸直时选的同一对折弯面。
- 对照：2026-10-09 的大折弯（R76.5、板厚 3、段数 20，也包含相切边界缺口）重新折弯成功。所以相切边界缺口并不是每次都失败。本次失败的确切几何原因从日志里查不出来（未查到官方依据）。
- 本轮实现（按用户要求，缺口仍打在两条相切边界上）：Rebend 提交失败时，只回退该钣金体的内部操作，跳过该体，并给出中文提示；其他钣金体照常处理。全部钣金体都被跳过时报错，模型不修改。如果该体原来已有折弯标记缺口节点（重复运行或编辑），则整条命令取消，保留原有缺口，避免旧节点一直处于抑制状态。
- 同轮修复：`TryGetFlatPatternUpwardFace`、`ResolveSameSkinWebFace`、`RepairFlatPatternUpwardFace` 原来只有声明和调用、没有定义，现已补齐实现；删除了未使用的 `ProcessBodyWithSpanFilter` 声明。
- 实测结果（2026-11-17 09:30，已通过）：日志显示大圆弧 `body=62731` 在 `original-pairs` 报 `3675017` 后，回退到布尔后重查的成对存活折弯面 `live-all-bends`（`faces=2`），校验 `updateErr=0 suppressed=0 flat=0 bent=1`，成功弯回（`rebendFeature=69606`、`flat pattern upward face repaired feature=66073 web=63139`）；同零件其余 3 个钣金体均走 `original-pairs` 成功，最终 `updateErrors=0`。
- 根因总结（为什么之前手动可以、程序先后出现 3 种异常）：
  1. **布尔减替换面标签**：4 个 Ø3 缺口打在相切边界上做完布尔减后，伸直前记录的 `innerFace`/`outerFace`/`referenceFace` 标签失效。交互界面点选的是布尔后**当前存活的新面**，并自动配对另一侧；程序原本把布尔前的旧标签传给 `Rebend`，因此报 `3675017`。
  2. **仅传单侧折弯面会静默不弯回**：`RebendBuilder` 只传单侧 `live-flat-inner`（`faces=1`）时虽然不抛异常，但折弯状态仍保持 `SheetmetalBendStateFlat`（图形区仍是平板未弯回）。必须传布尔后重新查到的成对存活折弯面 `live-all-bends`（`faces=2`），并在提交后校验 `flat == 0 && bent > 0`。
  3. **展平图样修复缺 `XAxisEdge`**：大圆弧原展平图样朝上面指在折弯圆柱面（`63135`）上，`RepairFlatPatternUpwardFace` 改回平板面（`63139`）时未同步补设 `XAxisEdge`，触发 `3675241`，导致滚回当前特征时 `Update undo happened` 整条回退。
  4. **弹窗编码**：`CMakeLists.txt` 启用了 `/utf-8`，传给 `NXMessageBox::Show` 的标题和正文必须显式包装 `NXOpen::NXString(..., NXOpen::NXString::UTF8)`，否则按系统 GBK 解析会乱码。
- 防再犯：对相切边界打缺口后的 `Rebend`，若原始面集合失败，必须从布尔后的当前体重新查询 `SheetmetalFaceTypeBend` 成对存活面，并在 `DoUpdate` 后校验 `SheetmetalBendStateFlat == 0`，不能仅以 `CommitFeature()` 未抛异常当成已弯回。
