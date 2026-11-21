#pragma once

namespace zhihui::gcbz
{
// 非模态启动 WPF 悬浮公差标注面板并通过 NX 主线程定时器处理交互请求；
// 若 WPF 程序不存在返回 false 以回退原生对话框
bool RunWpfToleranceSession();

// 查询非模态 WPF 会话是否仍在运行（供 ufusr_ask_unload 返回 UF_UNLOAD_SEL_DIALOG）
bool IsWpfToleranceSessionActive();

// 卸载前清理定时器与句柄（供 ufusr_cleanup 调用）
void StopWpfToleranceSession();
}
