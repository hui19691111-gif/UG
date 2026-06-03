import fs from "node:fs/promises";
import { SpreadsheetFile, Workbook } from "@oai/artifact-tool";

const outputDir = "D:/UGPluginRepo/CuanJianKonSuXin/outputs/semiconductor_precision";
const outputPath = `${outputDir}/半导体平台精度参数逐项分析与建议值.xlsx`;

const wb = Workbook.create();

const colors = {
  blue: "#1F4E78",
  header: "#5B9BD5",
  pale: "#D9EAF7",
  green: "#70AD47",
  lightGreen: "#E2F0D9",
  orange: "#F4B183",
  lightOrange: "#FCE4D6",
  red: "#C00000",
  yellow: "#FFF2CC",
  white: "#FFFFFF",
};

function idx(cell) {
  const m = cell.match(/^([A-Z]+)(\d+)$/);
  let col = 0;
  for (const ch of m[1]) col = col * 26 + ch.charCodeAt(0) - 64;
  return { row: Number(m[2]) - 1, col: col - 1 };
}

function setWidths(ws, widths) {
  widths.forEach((w, i) => {
    ws.getRangeByIndexes(0, i, 1, 1).format.columnWidthPx = w;
  });
}

function title(ws, text, sub = "") {
  ws.getRange("A1:I1").merge();
  ws.getRange("A1:I1").values = [[text]];
  ws.getRange("A1:I1").format = { fill: colors.blue, font: { bold: true, color: colors.white }, wrapText: true };
  if (sub) {
    ws.getRange("A2:I2").merge();
    ws.getRange("A2:I2").values = [[sub]];
    ws.getRange("A2:I2").format = { fill: colors.pale, font: { italic: true }, wrapText: true };
  }
}

function table(ws, start, data, opt = {}) {
  const p = idx(start);
  const r = ws.getRangeByIndexes(p.row, p.col, data.length, data[0].length);
  r.values = data;
  r.format = { wrapText: true };
  const headerRows = opt.headerRows ?? 1;
  if (headerRows > 0) {
    const h = ws.getRangeByIndexes(p.row, p.col, headerRows, data[0].length);
    h.format = { fill: opt.headerFill ?? colors.header, font: { bold: true, color: opt.headerColor ?? colors.white }, wrapText: true };
  }
  if (opt.firstColFill) {
    ws.getRangeByIndexes(p.row + headerRows, p.col, data.length - headerRows, 1).format = {
      fill: opt.firstColFill,
      font: { bold: true },
      wrapText: true,
    };
  }
  return r;
}

function finish(ws, widths) {
  setWidths(ws, widths);
  ws.showGridLines = false;
  ws.freezePanes.freezeRows(3);
  ws.getUsedRange().format.font = { name: "Microsoft YaHei" };
  ws.getUsedRange().format.wrapText = true;
}

const overview = wb.worksheets.add("01_结论总览");
title(overview, "半导体平台精度参数逐项分析与建议值", "目标：曝光有效面积 ≥310×310 mm；X/Y/Z 重复定位 ≤±0.1 μm；补偿后定位精度 ≤±0.5 μm");
table(overview, "A4", [
  ["结论项", "建议结论", "说明"],
  ["总体策略", "平台模组机械参数可适度放宽，但必须以补偿后 310×310 mm 全区域实测 map 达标为最终验收依据。", "不要只用单轴规格判断整机精度。"],
  ["重复定位", "X/Y/Z 单轴重复定位建议 ≤±0.05~0.08 μm；可接受上限 ≤±0.1 μm。", "最终整机重复 ≤±0.1 μm 时，单轴最好留余量。"],
  ["定位精度", "平台模组补偿后定位精度建议 ≤±0.3 μm/310 mm；可接受上限 ≤±0.5 μm/310 mm。", "如果供应商只能承诺补偿后 ±0.5 μm，则其它误差源必须更小。"],
  ["最大瓶颈", "X/Y 正交度、直线度、Pitch/Yaw、台面平面度、旋转轴角度误差。", "这些参数会把角秒级误差放大成微米级位移。"],
  ["检测系统", "CCD 和激光测距系统测量不确定度建议 ≤±0.1 μm，理想 ≤±0.05 μm。", "1 μm 级测量系统不能验收 ±0.5 μm 或 ±0.1 μm 指标。"],
  ["放宽原则", "机械基础参数可放宽到建议上限，但要提供补偿前/补偿后报告，并保证最终 Mark map 与 Z height map 合格。", "补偿不能替代稳定性，热漂移和重复性不能靠一次性补偿解决。"],
], { firstColFill: colors.pale });
table(overview, "A13", [
  ["推荐最终验收口径"],
  ["在设备最终安装、温度稳定、视觉标定、激光测高标定和运动补偿完成后，310 mm × 310 mm 工作区域内所有检测点 X/Y/Z 坐标误差均需满足：重复定位 ≤±0.1 μm；补偿后定位精度 ≤±0.5 μm。任一点超差即判定不合格。"],
], { headerRows: 1, headerFill: colors.green });
overview.getRange("A13:I13").merge();
overview.getRange("A14:I17").merge();
finish(overview, [150, 430, 420, 80, 80, 80, 80, 80, 80]);

const analysis = wb.worksheets.add("02_客户参数逐项分析");
title(analysis, "客户提供参数逐项分析", "逐项说明原参数对半导体平台精度的影响，并给出建议值与可放宽值");
table(analysis, "A4", [
  ["参数", "客户/现有值", "对精度的影响", "是否可补偿", "理想建议值", "可接受放宽值", "验收建议"],
  ["曝光有效面积", "≥310×310 mm", "决定全区域 Mark map 和 Z height map 的检测范围。", "不可补偿", "≥310×310 mm", "不可小于 310×310 mm", "以中心区域 ±155 mm 定义有效检测范围。"],
  ["X/Y 行程", "X≥320 mm，Y≥320 mm", "需要覆盖 310 mm 有效区域并保留加减速、边界和安装余量。", "不可补偿", "≥330 mm", "≥320 mm，建议边界预留 ≥10 mm", "行程不足会导致边缘精度和运动稳定性风险。"],
  ["Z 行程", "Z≥10 mm", "影响对焦、测高和工艺间隙调整。", "不可补偿", "≥10 mm，建议 12 mm", "≥10 mm", "确认全行程内 Z 精度，而不是只测某一点。"],
  ["重复定位精度", "≤±0.1 μm", "半导体平台核心指标，补偿无法消除随机重复误差。", "基本不可补偿", "≤±0.05~0.08 μm", "≤±0.1 μm", "必须做多点、多次往返重复测试。"],
  ["定位精度", "≤±0.5 μm", "决定 Mark 点绝对坐标误差。", "可通过 map 补偿改善", "补偿后 ≤±0.3 μm/310 mm", "补偿后 ≤±0.5 μm/310 mm", "必须明确是补偿前还是补偿后。建议最终按补偿后验收。"],
  ["X/Y 绝对精度", "±0.5 μm/400 mm", "310 mm 内约 ±0.39 μm，单轴本身可接近目标。", "可补偿", "≤±0.3 μm/400 mm", "≤±0.5 μm/400 mm", "该参数可接受，但不能单独代表整机精度。"],
  ["X/Y 直线度", "±2 μm 或 ±2/400 mm", "会直接造成全区域点位偏差，当前值对 ±0.5 μm 偏大。", "部分可补偿", "≤±0.3~0.5 μm/310 mm", "≤±1 μm/310 mm", "若只承诺补偿后，可放宽到 ±1 μm，但需 map 后残差达标。"],
  ["Pitch 俯仰", "±2 arc-sec", "会造成 Z 高度变化，也会通过 Abbe 高差影响 X/Y。310 mm 全跨约 3 μm PV。", "部分可补偿", "≤±0.3~0.5 arc-sec", "≤±1 arc-sec", "Z 高度 map 必须覆盖全区域。"],
  ["Yaw 偏摆", "±2 arc-sec", "对 310×310 角点 XY 综合影响约 2.1 μm。", "部分可补偿", "≤±0.3~0.5 arc-sec", "≤±1 arc-sec", "若最终补偿后 ≤±0.5 μm，Yaw 残差必须受控。"],
  ["X/Y 正交度", "10 arc-sec", "最大瓶颈；310 mm 全跨可造成约 15 μm 级几何误差。", "可补偿大部分", "≤0.5 arc-sec", "≤1~2 arc-sec；若完全依赖补偿，机械值最多建议 ≤3 arc-sec", "即使补偿，也需提供补偿前/后正交误差报告。"],
  ["平面度", "±2 μm", "影响激光测高、焦面和 Z 判定。对 ±0.5 μm 目标偏大。", "可做高度 map 补偿，但不能消除台面变形漂移", "≤±0.3~0.5 μm/310 mm", "≤±1 μm/310 mm", "若工艺依赖焦面，建议不放宽。"],
  ["伺服跳动", "±15 nm", "对 0.1 μm 重复性影响小，参数较好。", "不可补偿随机波动", "≤±15 nm", "≤±25 nm", "关注实际工况下的稳定性和 settling time。"],
  ["旋转轴重复定位", "±2 arc-sec", "若参与定位，310×310 角点 XY 综合误差约 2.1 μm。", "可通过视觉重新找 Mark 补偿部分", "≤±0.1~0.2 arc-sec", "≤±0.5 arc-sec；若不参与定位可作为报告项", "如果旋转后仍要求任意点 ±0.5 μm，当前 ±2 arc-sec 不建议接受。"],
  ["CCD/激光测量系统", "约 1 μm", "测量系统误差大于目标精度，无法验收 ±0.5/±0.1 μm。", "不可用补偿替代量测能力", "≤±0.05 μm", "≤±0.1 μm", "必须先标定并给出测量不确定度。"],
], { firstColFill: colors.pale });
finish(analysis, [150, 160, 320, 120, 180, 220, 330, 80, 80]);

const calcs = wb.worksheets.add("03_误差影响估算");
title(calcs, "关键角度参数对 310×310 mm 区域的影响", "角秒参数会随距离放大成微米级误差，是半导体平台需要重点控制的原因");
table(calcs, "A4", [
  ["项目", "角度", "半径/距离", "估算影响", "说明"],
  ["1 arc-sec", "4.848 μrad", "155 mm 边缘", "约 0.75 μm", "区域中心到边缘"],
  ["1 arc-sec", "4.848 μrad", "219.2 mm 角点", "约 1.06 μm", "区域中心到角点"],
  ["2 arc-sec", "9.696 μrad", "155 mm 边缘", "约 1.50 μm", "Pitch/Yaw 或旋转误差在边缘的影响"],
  ["2 arc-sec", "9.696 μrad", "219.2 mm 角点", "约 2.13 μm", "旋转或 Yaw 对角点综合位移影响"],
  ["10 arc-sec", "48.48 μrad", "155 mm 边缘", "约 7.5 μm", "当前正交度的半幅影响"],
  ["10 arc-sec", "48.48 μrad", "310 mm 全跨", "约 15.0 μm", "当前正交度按全跨估算"],
  ["Pitch 2 arc-sec", "9.696 μrad", "310 mm 全跨", "约 3.0 μm PV", "对台面 Z 高度差的影响"],
], { firstColFill: colors.lightOrange, headerFill: colors.orange, headerColor: "#000000" });
table(calcs, "A14", [
  ["结论"],
  ["如果最终要求补偿后定位精度 ≤±0.5 μm，角秒级误差不能只看数字大小。正交度 10 arc-sec、Pitch/Yaw 2 arc-sec、旋转轴 2 arc-sec 都会在 310×310 mm 区域放大成微米级误差。补偿后验收可以适度放宽机械基础，但热漂移、重复性和旋转重复误差仍必须控制。"],
], { headerRows: 1, headerFill: colors.green });
calcs.getRange("A14:I14").merge();
calcs.getRange("A15:I18").merge();
finish(calcs, [160, 130, 140, 150, 450, 80, 80, 80, 80]);

const spec = wb.worksheets.add("04_建议规格表");
title(spec, "建议给供应商的规格分配", "分为理想值、可接受放宽值、最终验收值三档");
table(spec, "A4", [
  ["项目", "理想规格", "可接受放宽规格", "最终验收必须满足", "备注"],
  ["X/Y 单轴重复定位", "≤±0.05~0.08 μm", "≤±0.1 μm", "整机重复定位 ≤±0.1 μm", "重复性不建议依赖补偿。"],
  ["Z 单轴重复定位", "≤±0.05 μm", "≤±0.1 μm", "Z 重复定位 ≤±0.1 μm", "需用高精度激光或电容测头验证。"],
  ["X/Y 补偿后定位精度", "≤±0.3 μm/310 mm", "≤±0.5 μm/310 mm", "全区域 X/Y ≤±0.5 μm", "最终以 CCD Mark map 为准。"],
  ["Z 补偿后高度精度", "≤±0.3 μm/310 mm", "≤±0.5 μm/310 mm", "全区域 Z ≤±0.5 μm", "最终以 Z height map 为准。"],
  ["X/Y 正交度", "≤0.5 arc-sec", "≤1~2 arc-sec；补偿型最多建议 ≤3 arc-sec", "补偿后 XY map 达标", "当前 10 arc-sec 建议优化。"],
  ["X/Y 直线度", "≤±0.3~0.5 μm/310 mm", "≤±1 μm/310 mm", "补偿后 XY map 达标", "当前 ±2 μm 偏大。"],
  ["Pitch/Yaw", "≤±0.3~0.5 arc-sec", "≤±1 arc-sec", "补偿后 XY/Z map 达标", "当前 ±2 arc-sec 偏大。"],
  ["台面平面度", "≤±0.3~0.5 μm/310 mm", "≤±1 μm/310 mm", "补偿后 Z map 达标", "若曝光焦深严，不建议放宽。"],
  ["旋转轴重复定位", "≤±0.1~0.2 arc-sec", "≤±0.5 arc-sec", "旋转后仍满足 XY map", "若旋转轴不参与定位，可作为报告项。"],
  ["检测系统不确定度", "≤±0.05 μm", "≤±0.1 μm", "必须优于验收指标", "1 μm 级不适合验收该指标。"],
  ["温度稳定性", "≤±0.1°C", "≤±0.3°C", "测试环境需记录", "热漂移会破坏补偿效果。"],
], { firstColFill: colors.lightGreen, headerFill: colors.green });
finish(spec, [170, 200, 260, 210, 330, 80, 80, 80, 80]);

const acceptance = wb.worksheets.add("05_验收方式");
title(acceptance, "整机验收方式建议", "用 CCD 测 X/Y Mark，用激光测 Z，高精度设备必须看全区域 map 和重复性");
table(acceptance, "A4", [
  ["检测项目", "检测方式", "点位建议", "判定标准", "报告要求"],
  ["X/Y 定位精度", "上方 CCD 拍工作台 Mark 点，计算理论坐标与实测坐标差。", "建议 11×11；最低 7×7", "补偿后每点 X/Y ≤±0.5 μm", "提供补偿前/补偿后误差 map、最大值、最小值、RMS。"],
  ["X/Y 重复定位", "选中心、四角、边中和随机点，往返重复定位拍照。", "不少于 9 点，每点 ≥30 次", "每点 X/Y 重复性 ≤±0.1 μm", "提供 3σ、max-min、RMS。"],
  ["Z 高度精度", "激光测距传感器扫描工作台高度。", "建议 11×11；最低 7×7", "补偿后每点 Z ≤±0.5 μm", "提供 height map、PV、RMS、倾斜拟合结果。"],
  ["Z 重复定位", "同一点多次 Z 定位和测高。", "中心、四角、边中、随机点", "每点 Z 重复性 ≤±0.1 μm", "提供重复性统计。"],
  ["旋转影响", "若旋转轴参与定位，旋转回零或旋转到工作角度后再测 Mark。", "至少 0°/90°/180°/270° 或实际工艺角度", "旋转后仍满足 X/Y/Z 最终要求", "提供旋转前后 map 对比。"],
  ["温度稳定", "记录环境、平台、工件、测头温度。", "全程记录", "测试期间温度稳定满足双方约定", "提供测试环境和稳定时间。"],
], { firstColFill: colors.pale });
table(acceptance, "A13", [
  ["建议验收条款"],
  ["平台模组机械基础参数允许按“可接受放宽规格”执行，但供应商必须在最终装机状态下完成二维/高度补偿，并提交补偿前、补偿后的 X/Y Mark map 与 Z height map。最终以补偿后的 310 mm × 310 mm 全区域检测结果作为合格判定依据；任一点超过 X/Y/Z 定位精度或重复定位精度要求，均判定不合格。"],
], { headerRows: 1, headerFill: colors.green });
acceptance.getRange("A13:I13").merge();
acceptance.getRange("A14:I18").merge();
finish(acceptance, [150, 320, 180, 210, 340, 80, 80, 80, 80]);

await fs.mkdir(outputDir, { recursive: true });

const scan = await wb.inspect({
  kind: "match",
  searchTerm: "#REF!|#DIV/0!|#VALUE!|#NAME\\?|#N/A",
  options: { useRegex: true, maxResults: 100 },
});
console.log(scan.ndjson);

for (const sheetName of ["01_结论总览", "02_客户参数逐项分析", "03_误差影响估算", "04_建议规格表", "05_验收方式"]) {
  const preview = await wb.render({ sheetName, autoCrop: "all", scale: 1, format: "png" });
  await fs.writeFile(`${outputDir}/${sheetName}.png`, new Uint8Array(await preview.arrayBuffer()));
}

const xlsx = await SpreadsheetFile.exportXlsx(wb);
await xlsx.save(outputPath);
console.log(outputPath);
