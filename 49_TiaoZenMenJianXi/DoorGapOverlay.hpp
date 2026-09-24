#pragma once

#include <NXOpen/ugmath.hxx>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#ifdef CreateDialog
#undef CreateDialog
#endif
#include <array>
#include <string>

namespace NXOpen { class Session; }

// Four small model-anchored editors.  NX's dimension focus handles expose
// only one key-in at a time, so the always-visible editors live in owned
// windows above the graphics viewport.
class DoorGapOverlay final {
public:
    explicit DoorGapOverlay(NXOpen::Session* session);
    ~DoorGapOverlay();
    DoorGapOverlay(const DoorGapOverlay&) = delete;
    DoorGapOverlay& operator=(const DoorGapOverlay&) = delete;

    void Show(const std::array<NXOpen::Point3d,4>& anchors,
              const std::array<NXOpen::Vector3d,4>& outwards,
              const std::array<bool,4>& available,
              const std::array<double,4>& values);
    void Hide() noexcept;
    bool Read(int index, double& value, std::string& error) const;
    void Reposition() noexcept;

private:
    static LRESULT CALLBACK WindowProc(HWND window, UINT message,
                                        WPARAM wparam, LPARAM lparam);
    HWND FindViewport(double aspect) const noexcept;

    NXOpen::Session* session_ = nullptr;
    HINSTANCE module_ = nullptr;
    bool registered_ = false;
    HWND owner_ = nullptr;
    HWND viewport_ = nullptr;
    std::array<HWND,4> windows_{};
    std::array<HWND,4> editors_{};
    std::array<NXOpen::Point3d,4> anchors_{};
    std::array<NXOpen::Vector3d,4> outwards_{};
    std::array<bool,4> available_{};
    bool shown_ = false;
};
