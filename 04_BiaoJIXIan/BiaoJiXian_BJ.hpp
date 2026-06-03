#ifndef BIAOJIXIAN_BJ_H_INCLUDED
#define BIAOJIXIAN_BJ_H_INCLUDED

#include <string>
#include <vector>

#include <uf_defs.h>
#include <uf_object_types.h>
#include <uf_ui_types.h>

#include <NXOpen/BlockStyler_BlockDialog.hxx>
#include <NXOpen/BlockStyler_BodyCollector.hxx>
#include <NXOpen/BlockStyler_DoubleBlock.hxx>
#include <NXOpen/BlockStyler_Enumeration.hxx>
#include <NXOpen/BlockStyler_Group.hxx>
#include <NXOpen/BlockStyler_IntegerBlock.hxx>
#include <NXOpen/BlockStyler_LayerBlock.hxx>
#include <NXOpen/BlockStyler_LineFont.hxx>
#include <NXOpen/BlockStyler_ObjectColorPicker.hxx>
#include <NXOpen/BlockStyler_PropertyList.hxx>
#include <NXOpen/BlockStyler_SelectObject.hxx>
#include <NXOpen/BlockStyler_UIBlock.hxx>
#include <NXOpen/Body.hxx>
#include <NXOpen/Callback.hxx>
#include <NXOpen/Curve.hxx>
#include <NXOpen/Edge.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Selection.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/UI.hxx>

using namespace NXOpen;
using namespace NXOpen::BlockStyler;

// 主对话框类。
// 这个类负责：
// 1. 加载 Block Styler 对话框。
// 2. 读取对话框参数。
// 3. 根据“轮廓 / 底面 / 选边”三种方式执行标记。
// 4. 处理曲线标记、浅槽标记、显示设置等逻辑。
class DllExport BiaoJiXian_BJ
{
public:
    // NX 全局会话对象。插件运行期间共用这一份 Session。
    static Session* theSession;

    // NX 全局 UI 对象。用于弹消息框、访问选择器、调用 Block Styler 等。
    static UI* theUI;

    // 构造函数：创建主对话框对象，解析 dlx 文件路径，并绑定基础回调。
    BiaoJiXian_BJ();

    // 析构函数：释放主对话框对象等资源。
    ~BiaoJiXian_BJ();

    // 打开主对话框。
    NXOpen::BlockStyler::BlockDialog::DialogResponse Launch();

    // Block Styler 标准回调：
    // initialize_cb   对话框初始化，只在打开时执行一次。
    // dialogShown_cb  对话框真正显示出来后执行。
    // apply_cb        点击“应用”时执行主业务。
    // ok_cb           点击“确定”时执行主业务。
    // update_cb       任意控件值变化时执行，用来联动界面。
    void initialize_cb();
    void dialogShown_cb();
    int apply_cb();
    int ok_cb();
    int update_cb(NXOpen::BlockStyler::UIBlock* block);

    // 读取某个控件的属性列表。
    PropertyList* GetBlockProperties(const char* blockID);

private:
    // 刷新界面显示状态。
    // 例如：
    // - 轮廓 / 底面模式时显示“选择面”。
    // - 选边模式时显示“选择边”。
    // - 曲线标记时显示图层设置。
    // - 浅槽标记时只保留颜色和线型设置。
    void RefreshUiState();

    // 配置选择过滤器。
    void ConfigureSelectionFilters();
    void ConfigureFaceFilter() const;
    void ConfigureEdgeFilter() const;

    // 对“选择面”容器做二次清洗，只保留实体上的平面。
    void NormalizeFaceSelection() const;

    // 读取对话框控件当前值的小函数。
    int GetEnumerationValue(NXOpen::BlockStyler::Enumeration* block) const;
    double GetDoubleValue(NXOpen::BlockStyler::DoubleBlock* block) const;
    int GetIntegerValue(NXOpen::BlockStyler::IntegerBlock* block) const;

    // 控件显示/隐藏、启用/禁用辅助函数。
    void SetShow(NXOpen::BlockStyler::UIBlock* block, bool show) const;
    void SetEnable(NXOpen::BlockStyler::UIBlock* block, bool enable) const;

    // 从选择控件里读取最终结果。
    std::vector<NXOpen::Face*> GetSelectedFaces() const;
    std::vector<NXOpen::Edge*> GetSelectedEdges() const;

    // 消息框封装。
    void ShowInfo(const std::string& message) const;
    void ShowError(const std::string& message) const;
    bool AskContinue(const std::string& message) const;

    // 主对话框 dlx 文件名。
    const char* theDlxFileName;

    // 解析后的 dlx 完整路径。
    std::string m_dlxPath;

    // 主对话框对象。
    NXOpen::BlockStyler::BlockDialog* theDialog;

    // 主输入区分组。
    NXOpen::BlockStyler::Group* InputGroup;

    // 选择面控件，用于“轮廓 / 底面”模式。
    NXOpen::BlockStyler::SelectObject* SourceBody;

    // 选择边控件，用于“选边”模式。
    NXOpen::BlockStyler::SelectObject* SourceEdges;

    // 创建方式枚举：轮廓 / 底面 / 选边。
    NXOpen::BlockStyler::Enumeration* MarkLineMode;

    // 输出方式分组。
    NXOpen::BlockStyler::Group* OutputRuleGroup;

    // 输出方式枚举：曲线标记 / 浅槽标记。
    NXOpen::BlockStyler::Enumeration* MarkOutputMode;

    // 浅槽参数：槽深、槽宽偏置。
    NXOpen::BlockStyler::DoubleBlock* GrooveDepth;
    NXOpen::BlockStyler::DoubleBlock* GrooveOffset;

    // 分段规则分组。
    NXOpen::BlockStyler::Group* SegmentRuleGroup;

    // 分段参数：
    // SegmentLength    每段标记线长度。
    // MaxMarkSpacing   标记线之间允许的最大间隔。
    NXOpen::BlockStyler::DoubleBlock* SegmentLength;
    NXOpen::BlockStyler::IntegerBlock* MaxMarkSpacing;

    // 封闭曲线规则：删除最短边 / 打断最长边中间（缺口 2mm）。
    NXOpen::BlockStyler::Group* ClosedRuleGroup;
    NXOpen::BlockStyler::Enumeration* ClosedCurveRule;

    // 显示规则分组。
    NXOpen::BlockStyler::Group* DisplayRuleGroup;

    // 原生 LayerBlock，作为图层控件承载体保留。
    NXOpen::BlockStyler::LayerBlock* CurveLayerMode;

    // 自定义图层方式枚举：跟随相贴大体 / 指定图层。
    NXOpen::BlockStyler::Enumeration* CurveLayerModeOption;

    // 指定图层号输入框。
    NXOpen::BlockStyler::IntegerBlock* CustomCurveLayer;

    // 曲线颜色选择器。
    NXOpen::BlockStyler::ObjectColorPicker* CurveColorPicker;

    // 曲线线型选择器。
    NXOpen::BlockStyler::LineFont* CurveLineFont;
};

#endif
