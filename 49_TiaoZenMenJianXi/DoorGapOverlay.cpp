#include "DoorGapOverlay.hpp"

#include <NXOpen/BasePart.hxx>
#include <NXOpen/ModelingView.hxx>
#include <NXOpen/ModelingViewCollection.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Session.hxx>
#include <uf_ui.h>
#include <uf_view.h>

#include <algorithm>
#include <cmath>
#include <cwchar>
#include <cwctype>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace {
constexpr wchar_t kClassName[] = L"ZhDoorGapEditorWindow";
constexpr int kWidth = 146;
constexpr int kHeight = 31;

struct ViewportSearch {
    POINT cursor{};
    double aspect = 1;
    HWND best = nullptr;
    double bestScore = std::numeric_limits<double>::infinity();
};

BOOL CALLBACK ConsiderViewport(HWND window, LPARAM data) {
    auto& search = *reinterpret_cast<ViewportSearch*>(data);
    if (!IsWindowVisible(window)) return TRUE;
    DWORD process = 0;
    GetWindowThreadProcessId(window, &process);
    if (process != GetCurrentProcessId()) return TRUE;
    RECT client{}, screen{};
    if (!GetClientRect(window, &client) || !GetWindowRect(window, &screen)) return TRUE;
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    if (width < 400 || height < 300) return TRUE;
    const double ratio = static_cast<double>(width) / height;
    double score = std::abs(std::log(ratio / search.aspect));
    if (PtInRect(&screen, search.cursor)) score -= 0.45;
    score -= (std::min)(0.2, static_cast<double>(width * height) / 12000000.0);
    if (score < search.bestScore) {
        search.best = window;
        search.bestScore = score;
    }
    return TRUE;
}
}

DoorGapOverlay::DoorGapOverlay(NXOpen::Session* session) : session_(session) {
    owner_ = static_cast<HWND>(UF_UI_get_default_parent());
    if (!owner_ || !IsWindow(owner_)) throw std::runtime_error("无法定位 NX 主窗口。");
    module_ = reinterpret_cast<HINSTANCE>(&__ImageBase);
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = &DoorGapOverlay::WindowProc;
    windowClass.hInstance = module_;
    windowClass.lpszClassName = kClassName;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    if (!RegisterClassW(&windowClass)) {
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            throw std::runtime_error("无法注册门间隙输入窗口。");
    } else registered_ = true;

    const wchar_t* labels[4]{L"左侧间隙", L"右侧间隙", L"下侧间隙", L"上侧间隙"};
    try {
        for (int i = 0; i < 4; ++i) {
            windows_[i] = CreateWindowExW(WS_EX_TOOLWINDOW, kClassName, L"",
                WS_POPUP | WS_BORDER | WS_CLIPCHILDREN, 0, 0, kWidth, kHeight,
                owner_, nullptr, windowClass.hInstance, this);
            if (!windows_[i]) throw std::runtime_error("无法创建门间隙输入窗口。");
            HWND label = CreateWindowExW(0, L"STATIC", labels[i], WS_CHILD | WS_VISIBLE,
                5, 7, 59, 20, windows_[i], nullptr, windowClass.hInstance, nullptr);
            editors_[i] = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"0.00",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | ES_RIGHT,
                67, 4, 72, 21, windows_[i], reinterpret_cast<HMENU>(static_cast<INT_PTR>(i + 1)),
                windowClass.hInstance, nullptr);
            if (!label || !editors_[i]) throw std::runtime_error("无法创建门间隙输入控件。");
            const auto font = reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT));
            SendMessageW(label, WM_SETFONT, font, TRUE);
            SendMessageW(editors_[i], WM_SETFONT, font, TRUE);
            SendMessageW(editors_[i], EM_SETLIMITTEXT, 30, 0);
        }
        if (!SetTimer(windows_[0], 1, 90, nullptr))
            throw std::runtime_error("无法启动门间隙定位计时器。");
    } catch (...) {
        for (auto& window : windows_) if (window) { DestroyWindow(window); window = nullptr; }
        if (registered_) UnregisterClassW(kClassName, module_);
        throw;
    }
}

DoorGapOverlay::~DoorGapOverlay() {
    Hide();
    if (windows_[0]) KillTimer(windows_[0], 1);
    for (auto& window : windows_) if (window) { DestroyWindow(window); window = nullptr; }
    UnregisterClassW(kClassName, module_);
}

LRESULT CALLBACK DoorGapOverlay::WindowProc(HWND window, UINT message,
                                             WPARAM wparam, LPARAM lparam) {
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        SetWindowLongPtrW(window, GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    }
    auto* overlay = reinterpret_cast<DoorGapOverlay*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_TIMER && wparam == 1 && overlay) {
        overlay->Reposition();
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

HWND DoorGapOverlay::FindViewport(double aspect) const noexcept {
    HWND root = GetAncestor(owner_, GA_ROOT);
    if (!root) root = owner_;
    ViewportSearch search;
    search.aspect = aspect > 0 ? aspect : 1;
    GetCursorPos(&search.cursor);
    EnumChildWindows(root, &ConsiderViewport, reinterpret_cast<LPARAM>(&search));
    return search.best;
}

void DoorGapOverlay::Show(const std::array<NXOpen::Point3d,4>& anchors,
                          const std::array<NXOpen::Vector3d,4>& outwards,
                          const std::array<bool,4>& available,
                          const std::array<double,4>& values) {
    anchors_ = anchors;
    outwards_ = outwards;
    available_ = available;
    shown_ = true;
    for (int i = 0; i < 4; ++i) {
        std::wostringstream stream;
        if (available[i]) stream << std::fixed << std::setprecision(2) << values[i];
        else stream << L"--";
        SetWindowTextW(editors_[i], stream.str().c_str());
        EnableWindow(editors_[i], available[i] ? TRUE : FALSE);
    }
    Reposition();
}

void DoorGapOverlay::Hide() noexcept {
    shown_ = false;
    for (auto* window : windows_) if (window && IsWindow(window)) ShowWindow(window, SW_HIDE);
}

bool DoorGapOverlay::Read(int index, double& value, std::string& error) const {
    if (index < 0 || index >= 4 || !editors_[index]) return false;
    wchar_t content[64]{};
    GetWindowTextW(editors_[index], content, 64);
    wchar_t* end = nullptr;
    const double parsed = std::wcstod(content, &end);
    while (end && iswspace(*end)) ++end;
    if (end == content || !end || *end || !std::isfinite(parsed) || parsed < 0 || parsed > 100) {
        static const char* names[4]{"左", "右", "下", "上"};
        error = std::string(names[index]) + "侧目标间隙须在 0～100 mm。";
        return false;
    }
    value = parsed;
    return true;
}

void DoorGapOverlay::Reposition() noexcept {
    try {
        if (!shown_ || !session_) return;
        HWND foreground = GetForegroundWindow();
        DWORD process = 0;
        if (foreground) GetWindowThreadProcessId(foreground, &process);
        if (process != GetCurrentProcessId()) {
            for (auto* window : windows_) if (window) ShowWindow(window, SW_HIDE);
            return;
        }
        auto* part = session_->Parts()->Display();
        auto* view = part ? part->ModelingViews()->WorkView() : nullptr;
        if (!view) return;
        double clip[4]{};
        if (UF_VIEW_ask_current_xy_clip(view->Tag(), clip) != 0 ||
            clip[1] - clip[0] <= 0 || clip[3] - clip[2] <= 0) return;
        const double aspect = (clip[1] - clip[0]) / (clip[3] - clip[2]);
        if (!viewport_ || !IsWindow(viewport_) || !IsWindowVisible(viewport_))
            viewport_ = FindViewport(aspect);
        if (!viewport_) return;
        RECT client{};
        if (!GetClientRect(viewport_, &client)) return;
        const int width = client.right - client.left;
        const int height = client.bottom - client.top;
        if (width < 400 || height < 300) { viewport_ = nullptr; return; }
        const auto matrix = view->Matrix();
        const auto origin = view->Origin();
        for (int i = 0; i < 4; ++i) {
            const auto& p = anchors_[i];
            const double vx = matrix.Xx*p.X + matrix.Xy*p.Y + matrix.Xz*p.Z + origin.X;
            const double vy = matrix.Yx*p.X + matrix.Yy*p.Y + matrix.Yz*p.Z + origin.Y;
            POINT pixel{
                static_cast<LONG>(std::lround((vx - clip[0]) * width / (clip[1] - clip[0]))),
                static_cast<LONG>(std::lround((clip[3] - vy) * height / (clip[3] - clip[2])))};
            if (pixel.x < 0 || pixel.x > width || pixel.y < 0 || pixel.y > height) {
                ShowWindow(windows_[i], SW_HIDE);
                continue;
            }
            ClientToScreen(viewport_, &pixel);
            const auto& outward = outwards_[i];
            double dx = matrix.Xx*outward.X + matrix.Xy*outward.Y + matrix.Xz*outward.Z;
            double dy = -(matrix.Yx*outward.X + matrix.Yy*outward.Y + matrix.Yz*outward.Z);
            const double length = std::hypot(dx, dy);
            if (length > 1.0e-8) { dx /= length; dy /= length; }
            const double offset = std::abs(dx)*kWidth/2.0 + std::abs(dy)*kHeight/2.0 + 8;
            const int x = static_cast<int>(std::lround(pixel.x + dx*offset - kWidth/2.0));
            const int y = static_cast<int>(std::lround(pixel.y + dy*offset - kHeight/2.0));
            SetWindowPos(windows_[i], nullptr, x, y, kWidth, kHeight,
                SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW);
        }
    } catch (...) {
        for (auto* window : windows_) if (window) ShowWindow(window, SW_HIDE);
    }
}
