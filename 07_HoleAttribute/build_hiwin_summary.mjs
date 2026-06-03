import fs from "node:fs/promises";
import { SpreadsheetFile, Workbook } from "@oai/artifact-tool";

const outputDir = "D:/UGPluginRepo/CuanJianKonSuXin/outputs/hiwin_summary";
const outputPath = `${outputDir}/HIWIN三轴平台方案通俗整理.xlsx`;

const workbook = Workbook.create();

const colors = {
  dark: "#1F4E78",
  header: "#5B9BD5",
  pale: "#D9EAF7",
  green: "#70AD47",
  lightGreen: "#E2F0D9",
  orange: "#F4B183",
  lightOrange: "#FCE4D6",
  grey: "#F2F2F2",
  border: "#B7B7B7",
  white: "#FFFFFF",
};

function styleTitle(sheet, range, title, subtitle = "") {
  const titleRange = sheet.getRange(range);
  titleRange.merge();
  titleRange.values = [[title]];
  titleRange.format = {
    fill: colors.dark,
    font: { bold: true, color: colors.white },
  };
  titleRange.format.rowHeight = 34;
  if (subtitle) {
    const sub = sheet.getRange("A2:H2");
    sub.merge();
    sub.values = [[subtitle]];
    sub.format = {
      fill: colors.pale,
      font: { italic: true, color: "#333333" },
      wrapText: true,
    };
    sub.format.rowHeight = 26;
  }
}

function writeTable(sheet, startCell, rows, options = {}) {
  const rowCount = rows.length;
  const colCount = rows[0].length;
  const start = cellToIndexes(startCell);
  const range = sheet.getRangeByIndexes(start.row, start.col, rowCount, colCount);
  range.values = rows;
  range.format = {
    wrapText: true,
  };
  const headerRows = options.headerRows ?? 1;
  if (headerRows > 0) {
    const h = sheet.getRangeByIndexes(start.row, start.col, headerRows, colCount);
    h.format = {
      fill: options.headerFill ?? colors.header,
      font: { bold: true, color: options.headerFontColor ?? colors.white },
      wrapText: true,
    };
  }
  if (options.firstColFill) {
    const c = sheet.getRangeByIndexes(start.row + headerRows, start.col, rowCount - headerRows, 1);
    c.format = { fill: options.firstColFill, font: { bold: true }, wrapText: true };
  }
  return range;
}

function cellToIndexes(cell) {
  const match = cell.match(/^([A-Z]+)(\d+)$/);
  const letters = match[1];
  let col = 0;
  for (const ch of letters) col = col * 26 + (ch.charCodeAt(0) - 64);
  return { row: Number(match[2]) - 1, col: col - 1 };
}

function setWidths(sheet, widths) {
  widths.forEach((width, idx) => {
    sheet.getRangeByIndexes(0, idx, 1, 1).format.columnWidthPx = width;
  });
}

function finishSheet(sheet, widths = [120, 150, 150, 150, 150, 150, 150, 150]) {
  setWidths(sheet, widths);
  sheet.freezePanes.freezeRows(3);
  sheet.showGridLines = false;
  const used = sheet.getUsedRange();
  used.format.font = { name: "Microsoft YaHei" };
}

// 1. Overview
const summary = workbook.worksheets.add("01_方案概览");
styleTitle(summary, "A1:H1", "HIWIN 三轴平台方案通俗整理", "客户：科毅科技 | 文件：U4002-260440 / PF2604113 | 版本：A1");
writeTable(summary, "A4", [
  ["项目", "内容", "通俗说明"],
  ["供应商", "HIWIN MIKROSYSTEM CORP. / 大银微系统", "精密运动平台与线性马达系统供应商"],
  ["客户", "科毅科技", "本方案对应客户名称"],
  ["方案型式", "Custom Systems / 客制系统", "不是标准单轴或单纯 XY 平台，而是多轴组合方案"],
  ["平台型号", "LMAP2C-320-800+BS-5-5+DMTB2", "可拆解为 X/Y 线性平台 + Z 轴滚珠螺杆机构 + DMTB2 旋转平台"],
  ["核心用途", "高精度定位、检测、微组装或精密制程", "重点在定位精度和稳定性，不是高速搬运"],
  ["环境假设", "Class 1000 无尘环境，23±1°C", "最终精度会受现场温度、安装、地基和调试影响"],
], { firstColFill: colors.pale });

writeTable(summary, "A13", [
  ["一句话结论"],
  ["这是一套以高精度定位为核心的客制化多轴运动平台，X/Y 轴负责平面精密移动，Z 轴通过楔形机构做短行程转换，旋转平台负责高精度角度定位。整体更适合精密检测、半导体、光学、微组装等应用。"],
], { headerRows: 1, headerFill: colors.green });
summary.getRange("A14:H16").merge();
summary.getRange("A13:H13").merge();
finishSheet(summary, [130, 260, 420, 120, 120, 120, 120, 120]);

// 2. Axis specs
const axis = workbook.worksheets.add("02_三轴规格");
styleTitle(axis, "A1:H1", "X / Y / Z 轴主要规格", "把规格表转换成便于阅读和横向比较的形式");
writeTable(axis, "A4", [
  ["项目", "单位", "X 轴（上轴）", "Y 轴（下轴）", "Z 轴", "通俗说明"],
  ["轴定义", "-", "X axis / Upper axis", "Y axis / Lower axis", "Z axis", "X/Y 做平面移动，Z 做短行程或楔形转换动作"],
  ["驱动概念", "-", "Linear motor x1", "Linear motor x2，并联双动子", "BS robot + AC servo motor", "X/Y 是线马，Z 是滚珠螺杆加伺服"],
  ["总负载", "kg", "20 kg 负载 + 60 kg 移动质量 = 80 kg", "20 kg 负载 + 135 kg 移动质量 = 155 kg", "20 kg 负载 + 19.6 kg 移动质量 = 40 kg", "评估马达能力时看的是负载加移动质量"],
  ["有效行程", "mm", 320, 800, 5, "Y 轴行程最长，X 轴中等，Z 轴为短行程"],
  ["滚珠螺杆导程", "mm", "-", "-", 5, "只出现在 Z 轴"],
  ["最大速度", "m/s", 0.3, 0.3, 0.01, "X/Y 速度稳健，Z 轴速度较低"],
  ["最大加速度", "m/s²", 5, 5, 1, "X/Y 加速度相同，Z 轴原始加速度较低"],
  ["重复精度", "μm", "±0.1", "±0.1/400mm", "±0.2", "重复到同一点的能力，数值越小越好"],
  ["绝对精度", "μm", "±0.5/400mm", "±0.5/400mm", "±0.5", "经过补偿后的定位误差水平"],
  ["水平直线度", "μm", "±2", "±2/400mm", "-", "运动轨迹在水平方向的直线性"],
  ["垂直直线度", "μm", "±2", "±2/400mm", "-", "运动轨迹在垂直方向的直线性"],
  ["俯仰 Pitch", "arc-sec", "±2", "±2/400mm", "-", "平台运动时的角度摆动"],
  ["偏摆 Yaw", "arc-sec", "±2", "±2/400mm", "-", "平台运动时的角度偏转"],
  ["伺服跳动", "nm", "±15", "±15", "±25", "伺服保持或运动时的微小波动"],
  ["正交度", "arc-sec", "10（200x200mm 中央行程）", "10（200x200mm 中央行程）", "-", "X/Y 两轴垂直程度"],
  ["平面度", "μm", "±2（with AccuPLUS?）", "±2（with AccuPLUS?）", "-", "平台平面运动时的高度变化"],
  ["编码器", "-", "类比光学尺", "类比光学尺", "类比光学尺", "用于高精度位置反馈"],
], { firstColFill: colors.pale });
finishSheet(axis, [130, 90, 180, 200, 180, 330, 80, 80]);

// 3. Motion timing
const motion = workbook.worksheets.add("03_运动时间评估");
styleTitle(motion, "A1:H1", "运动曲线与时间估算", "资料中的 X/Y/Z 均按梯形速度曲线：加速 -> 匀速 -> 减速 -> 停留");
writeTable(motion, "A4", [
  ["轴", "行程 mm", "最大速度 m/s", "最大加速度 m/s²", "加速时间 s", "匀速时间 s", "减速时间 s", "总时间 s"],
  ["X 轴", 320, 0.3, 5, 0.11, 0.96, 0.11, 1.28],
  ["Y 轴", 800, 0.3, 5, 0.11, 2.56, 0.11, 2.88],
  ["Z 轴（楔形转换后）", 18.7, 0.04, 3.7, 0.02, 0.49, 0.02, 0.63],
], { firstColFill: colors.lightGreen, headerFill: colors.green });
writeTable(motion, "A10", [
  ["说明项", "内容"],
  ["梯形速度曲线", "先加速到目标速度，中间保持匀速，接近目标位置时减速，最后保留 0.1 秒停留时间。"],
  ["X 轴", "完整 320 mm 行程约 1.28 秒。"],
  ["Y 轴", "完整 800 mm 行程约 2.88 秒，是三轴中时间最长的动作。"],
  ["Z 轴", "转换后约 18.7 mm 行程，动作时间约 0.63 秒。"],
  ["注意", "如果客户后续调整速度、加速度或负载，需要请 HIWIN 重新评估马达容量、温升和寿命。"],
], { firstColFill: colors.pale });
motion.getRange("B11:H11").merge();
motion.getRange("B12:H12").merge();
motion.getRange("B13:H13").merge();
motion.getRange("B14:H14").merge();
motion.getRange("B15:H15").merge();
finishSheet(motion, [150, 110, 130, 150, 120, 120, 120, 110]);

// 4. Rotary platform
const rotary = workbook.worksheets.add("04_旋转平台DMTB2");
styleTitle(rotary, "A1:H1", "旋转平台 DMTB2 重点参数", "适合高精度角度定位，重复精度等级较高");
writeTable(rotary, "A4", [
  ["项目", "单位", "数值", "通俗说明"],
  ["型号", "-", "DMTB2-70SP00-S4-4QS-0-0", "旋转平台/马达型号"],
  ["连续扭矩", "Nm", 9.1, "可长期输出的扭矩"],
  ["连续电流", "Arms", 2.6, "长期运行电流"],
  ["瞬时扭矩", "Nm", 30.4, "短时间可输出的峰值扭矩"],
  ["瞬时电流", "Arms", 8.7, "短时间峰值电流"],
  ["转矩常数", "Nm/Arms", 3.46, "电流转换成扭矩的能力"],
  ["重复精度", "arc-sec", "±2", "角度重复定位精度很高"],
  ["刚性", "arc-sec", "±5", "受力后角度变形范围"],
  ["最大速度", "rad/s", 1.75, "旋转最高速度"],
  ["最大角加速度", "rad/s²", 23.19, "旋转加速能力"],
  ["负载惯量", "kg·m²", 0.3413, "旋转负载的惯性大小"],
  ["传动比", "-", 8.5, "资料中给出的传动比"],
  ["马达温度估算", "°C", 42.3, "在给定条件下温升估算"],
], { firstColFill: colors.pale });
writeTable(rotary, "A20", [
  ["结论"],
  ["旋转轴适合做精密角度定位；资料显示其重复精度为 ±2 arc-sec，属于精密等级。实际表现仍需要结合负载惯量、安装状态、控制参数和现场环境确认。"],
], { headerRows: 1, headerFill: colors.green });
rotary.getRange("A20:H20").merge();
rotary.getRange("A21:H23").merge();
finishSheet(rotary, [160, 100, 190, 380, 80, 80, 80, 80]);

// 5. Wedge
const wedge = workbook.worksheets.add("05_Z轴楔形转换");
styleTitle(wedge, "A1:H1", "Z 轴 15° 楔形转换说明", "把小行程运动通过斜面机构转换成另一方向的运动");
writeTable(wedge, "A4", [
  ["项目", "原始值", "转换后", "单位", "说明"],
  ["楔形角度", 15, "-", "deg", "资料给出的 wedge angle"],
  ["负载", 20, 39.6, "kg", "转换后等效负载增加"],
  ["移动质量", 22, 39.6, "kg", "转换后等效移动质量增加"],
  ["行程", 5, 18.7, "mm", "小位移转换成较大位移"],
  ["速度", 0.01, 0.037, "m/s", "转换后速度放大"],
  ["加速度", 1, 3.73, "m/s²", "转换后加速度放大"],
], { firstColFill: colors.lightOrange, headerFill: colors.orange, headerFontColor: "#000000" });
writeTable(wedge, "A13", [
  ["通俗解释"],
  ["楔形机构可以理解成一个精密斜坡：原本较短的垂直位移，经过 15° 斜面后变成较长的水平位移。好处是可用短行程机构实现需要的转换动作；代价是负载、受力、刚性和马达能力都要重新核算。"],
], { headerRows: 1, headerFill: colors.green });
wedge.getRange("A13:H13").merge();
wedge.getRange("A14:H17").merge();
finishSheet(wedge, [140, 120, 120, 90, 360, 80, 80, 80]);

// 6. Conditions and risks
const notes = workbook.worksheets.add("06_条件与注意事项");
styleTitle(notes, "A1:H1", "方案条件与后续确认事项", "把原始资料中的备注转换成项目沟通用清单");
writeTable(notes, "A4", [
  ["类别", "资料内容", "建议关注点"],
  ["使用环境", "Class 1000 无尘环境，23±1°C 下量测", "现场环境若不同，精度和温升可能变化"],
  ["润滑", "HIWIN G03 润滑油", "维护周期和补油方式需确认"],
  ["散热/安装", "水平安装", "安装平面、地基、热源位置会影响精度"],
  ["驱动与控制", "HIWIN E2 x4 + ACS 控制器，驱动电压 DC 90V", "需确认与上位机、I/O、安全回路的接口"],
  ["底座材质", "XY 轴铸铁，Z 轴铝材", "关注结构刚性、热变形和重量"],
  ["线材", "白色无尘线皮，延长线 10 m", "拖链、弯折半径、线缆寿命需确认"],
  ["防尘/空压", "资料多处为 N/A", "若实际工况有粉尘、油雾、气管需求，应补充规格"],
  ["客户空间", "TBD", "需要最终设备空间、干涉范围、维修空间"],
  ["出货状态", "未检出货", "需确认出货检验项目、验收标准和现场复测方式"],
  ["精度风险", "最终结果取决于空气流动、安装精度、环境状态", "建议提前定义验收环境、检测方法和补偿策略"],
], { firstColFill: colors.pale });
writeTable(notes, "A18", [
  ["优先级", "待确认事项"],
  ["高", "客户实际速度、加速度、负载是否会变化；若变化必须重新核算。"],
  ["高", "最终验收标准：测哪些轴、测哪些精度、在哪个温度和环境下测。"],
  ["中", "Z 轴楔形机构的实际受力、刚性、寿命和安装空间。"],
  ["中", "ACS 控制器与整机控制系统的通讯方式和安全联锁。"],
  ["中", "防尘、防水、空压、拖链、线缆走线是否需要补充。"],
], { firstColFill: colors.lightOrange, headerFill: colors.orange, headerFontColor: "#000000" });
finishSheet(notes, [130, 320, 420, 80, 80, 80, 80, 80]);

// 7. Source images
const sources = workbook.worksheets.add("07_资料来源");
styleTitle(sources, "A1:H1", "资料来源与页面对应", "根据用户提供的 7 张截图整理，便于追溯");
writeTable(sources, "A4", [
  ["截图", "主要内容", "整理到的页签"],
  ["ScreenShot_2026-04-28_114925_496.png", "封面、客户、版本记录", "01_方案概览"],
  ["ScreenShot_2026-04-28_114951_372.png", "系统型式、X/Y/Z 轴总体规格", "02_三轴规格"],
  ["ScreenShot_2026-04-28_115001_313.png", "备注、使用环境、线材、控制器、验收条件", "06_条件与注意事项"],
  ["ScreenShot_2026-04-28_115012_524.png", "X 轴运动曲线、负载、评估结果", "03_运动时间评估"],
  ["ScreenShot_2026-04-28_115021_418.png", "Y 轴运动曲线、负载、评估结果", "03_运动时间评估"],
  ["ScreenShot_2026-04-28_115031_191.png", "Z 轴楔形转换与运动评估", "05_Z轴楔形转换 / 03_运动时间评估"],
  ["ScreenShot_2026-04-28_115040_029.png", "DMTB2 旋转平台 DM 选用报告", "04_旋转平台DMTB2"],
], { firstColFill: colors.pale });
writeTable(sources, "A15", [
  ["说明"],
  ["本 Excel 是根据截图人工整理的通俗版资料，适合沟通、评审和初步理解。若用于正式采购、验收或设计冻结，应以 HIWIN 原始 PDF/正式图纸/双方确认版规格书为准。"],
], { headerRows: 1, headerFill: colors.green });
sources.getRange("A15:H15").merge();
sources.getRange("A16:H18").merge();
finishSheet(sources, [280, 380, 280, 80, 80, 80, 80, 80]);

for (const sheetName of ["01_方案概览", "02_三轴规格", "03_运动时间评估", "04_旋转平台DMTB2", "05_Z轴楔形转换", "06_条件与注意事项", "07_资料来源"]) {
  const sheet = workbook.worksheets.getItem(sheetName);
  sheet.getUsedRange().format.wrapText = true;
}

await fs.mkdir(outputDir, { recursive: true });

const checks = await workbook.inspect({
  kind: "match",
  searchTerm: "#REF!|#DIV/0!|#VALUE!|#NAME\\?|#N/A",
  options: { useRegex: true, maxResults: 100 },
  summary: "final formula error scan",
});
console.log(checks.ndjson);

for (const sheetName of ["01_方案概览", "02_三轴规格", "03_运动时间评估", "04_旋转平台DMTB2", "05_Z轴楔形转换", "06_条件与注意事项", "07_资料来源"]) {
  const preview = await workbook.render({
    sheetName,
    autoCrop: "all",
    scale: 1,
    format: "png",
  });
  await fs.writeFile(`${outputDir}/${sheetName}.png`, new Uint8Array(await preview.arrayBuffer()));
}

const xlsx = await SpreadsheetFile.exportXlsx(workbook);
await xlsx.save(outputPath);
console.log(outputPath);
