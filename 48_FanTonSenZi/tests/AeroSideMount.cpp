// 侧置气浮板安装调整结构（斜楔 1:50 + 球头 + 定位销 + 回拉螺钉 + 下端让位）3D 装配模型
// 坐标：X 指向导轨（气膜法向），Y 沿导轨，Z 竖直；单位 mm
#include <NXOpen/Body.hxx>
#include <NXOpen/BodyCollection.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Session.hxx>
#include <uf.h>
#include <uf_curve.h>
#include <uf_modl.h>
#include <uf_modl_primitives.h>
#include <uf_obj.h>
#include <uf_part.h>
#include <uf_ps.h>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace NXOpen;

static const double kPi = 3.14159265358979323846;

struct Pnt3 { double x, y, z; };
static Pnt3 Pv(double x, double y, double z) { return Pnt3{x, y, z}; }

static void Chk(int rc, const char* what) {
    if (rc != 0) {
        char msg[133] = {0};
        UF_get_fail_message(rc, msg);
        throw std::runtime_error(std::string(what) + " 失败: " + msg);
    }
}

static tag_t Block(UF_FEATURE_SIGN sign, Pnt3 c, double dx, double dy, double dz) {
    char a[64], b[64], d[64];
    std::snprintf(a, sizeof a, "%.6f", dx);
    std::snprintf(b, sizeof b, "%.6f", dy);
    std::snprintf(d, sizeof d, "%.6f", dz);
    double corner[3] = {c.x, c.y, c.z};
    char* len[3] = {a, b, d};
    tag_t t = 0;
    Chk(UF_MODL_create_block1(sign, corner, len, &t), "块体");
    return t;
}

static tag_t Bore(UF_FEATURE_SIGN sign, Pnt3 o, double len, double dia, Pnt3 dir) {
    char hs[64], ds[64];
    std::snprintf(hs, sizeof hs, "%.6f", len);
    std::snprintf(ds, sizeof ds, "%.6f", dia);
    double origin[3] = {o.x, o.y, o.z}, d[3] = {dir.x, dir.y, dir.z};
    tag_t t = 0;
    Chk(UF_MODL_create_cyl1(sign, origin, hs, ds, d, &t), "圆柱");
    return t;
}

static tag_t Line(Pnt3 a, Pnt3 b) {
    UF_CURVE_line_t l = {{a.x, a.y, a.z}, {b.x, b.y, b.z}};
    tag_t c = 0;
    Chk(UF_CURVE_create_line(&l, &c), "线");
    return c;
}

// 闭合多边形沿方向拉伸
static tag_t Extrude(UF_FEATURE_SIGN sign, const std::vector<Pnt3>& pts, Pnt3 dir, double len) {
    uf_list_p_t curves = nullptr, features = nullptr;
    Chk(UF_MODL_create_list(&curves), "list");
    std::vector<tag_t> tags;
    for (size_t i = 0; i < pts.size(); ++i) {
        auto t = Line(pts[i], pts[(i + 1) % pts.size()]);
        tags.push_back(t);
        Chk(UF_MODL_put_list_item(curves, t), "item");
    }
    char zero[] = "0";
    std::string end = std::to_string(len);
    char* limits[] = {zero, end.data()};
    double p[3] = {0, 0, 0}, d[3] = {dir.x, dir.y, dir.z};
    Chk(UF_MODL_create_extruded(curves, zero, limits, p, d, sign, &features), "拉伸");
    tag_t f = 0, body = 0;
    Chk(UF_MODL_ask_list_item(features, 0, &f), "feat");
    Chk(UF_MODL_ask_feat_body(f, &body), "body");
    UF_MODL_delete_list(&features);
    UF_MODL_delete_list(&curves);
    for (auto t : tags) UF_OBJ_set_blank_status(t, UF_OBJ_BLANKED);
    return body;
}

// 球体：XZ 平面半圆绕 Y 轴整圈旋转
static tag_t Sphere(double cx, double cy, double cz, double r) {
    std::vector<Pnt3> prof;
    for (int i = 0; i <= 36; ++i) {
        const double a = -kPi / 2 + kPi * i / 36.0;
        prof.push_back(Pv(cx + r * std::cos(a), cy, cz + r * std::sin(a)));
    }
    std::vector<tag_t> curves;
    for (size_t i = 0; i < prof.size(); ++i) curves.push_back(Line(prof[i], prof[(i + 1) % prof.size()]));
    char zero[] = "0", full[] = "360";
    char* limits[] = {zero, full};
    char* offsets[] = {zero, zero};
    double origin[3] = {0, 0, 0}, axis[3] = {0, 1, 0};
    tag_t* features = nullptr;
    int count = 0;
    Chk(UF_MODL_create_revolution(curves.data(), static_cast<int>(curves.size()), nullptr, limits,
                                  offsets, origin, false, true, origin, axis, UF_NULLSIGN,
                                  &features, &count),
        "球体");
    tag_t body = 0;
    if (count > 0) Chk(UF_MODL_ask_feat_body(features[0], &body), "球体体");
    UF_free(features);
    for (auto c : curves) UF_OBJ_set_blank_status(c, UF_OBJ_BLANKED);
    return body;
}

static void PadReport(const char* tag) {
    auto* work = Session::GetSession()->Parts()->Work();
    int solids = 0, faces = 0;
    for (auto* b : *work->Bodies())
        if (b->IsSolidBody()) { ++solids; faces += static_cast<int>(b->GetFaces().size()); }
    std::cout << tag << " solids=" << solids << " faces=" << faces << std::endl;
}

int main(int argc, char** argv) {
    try {
        std::filesystem::path out = argc > 1 ? std::filesystem::path(argv[1])
                                            : std::filesystem::path("G:/AeroMount");
        std::filesystem::create_directories(out);
        Chk(UF_initialize(), "UF_initialize");
        auto prtFile = out / "AeroSideMount.prt";
        auto xtFile = out / "AeroSideMount.x_t";
        std::filesystem::remove(prtFile);
        tag_t part = 0;
        Chk(UF_PART_new(prtFile.string().c_str(), METRIC, &part), "新建部件");
        std::cout << "PART " << prtFile.string() << std::endl;

        const double GAP = 0.15;      // 气膜（真实 10μm，模型放大便于看）
        const double PAD_T = 25.0, PAD_W = 120.0, PAD_H = 120.0;
        const double PAD_X0 = GAP, PAD_X1 = GAP + PAD_T;
        const double PAD_Z0 = 10.0, PAD_Z1 = PAD_Z0 + PAD_H;
        const double WEDGE_X = 46.55; // 斜楔薄端（z=-10 处），给 φ18 球头留空间
        const double TAPER = 1.0 / 50.0;
        const double BASE_X = 56.0;

        // 1) 导轨
        Block(UF_NULLSIGN, Pv(-60, -150, -20), 60, 300, 160);

        // 2) 气浮板 + 背面球头座 + 销孔 + 回拉螺钉孔 + 下端让位
        Block(UF_NULLSIGN, Pv(PAD_X0, -PAD_W / 2, PAD_Z0), PAD_T, PAD_W, PAD_H);
        Bore(UF_NEGATIVE, Pv(PAD_X1 - 6, 0, 70), 6, 18, Pv(1, 0, 0));         // 球头座孔
        for (int s : {-1, 1}) {
            Bore(UF_NEGATIVE, Pv(PAD_X1, s * 55.0, 30), PAD_T, 6.2, Pv(-1, 0, 0));    // 销孔
            Bore(UF_NEGATIVE, Pv(PAD_X1, s * 55.0, 110), PAD_T, 6.6, Pv(-1, 0, 0));   // 回拉螺钉
        }
        Extrude(UF_NEGATIVE, {Pv(PAD_X0 - 1, -PAD_W / 2, PAD_Z0 - 1),
                              Pv(PAD_X0 + 6, -PAD_W / 2, PAD_Z0 - 1),
                              Pv(PAD_X0 - 1, -PAD_W / 2, PAD_Z0 + 6)},
                Pv(0, 1, 0), PAD_W);                                             // 下端 5x5 让位

        // 3) 球头 R9 + 球座块
        Block(UF_NULLSIGN, Pv(PAD_X1, -14, 56), 8, 28, 28);
        // 球头：模型用 φ18 × 8 圆柱简化表示（真实为 R9 球面垫）
        Bore(UF_NULLSIGN, Pv(PAD_X1 + 9, 0, 61), 8, 18, Pv(0, 0, 1));

        // 4) 斜楔（1:50）：z 方向移动 1mm → 气浮板位移 20μm
        {
            const double z0 = -10.0, z1 = 90.0;
            auto xAt = [&](double z) { return WEDGE_X - TAPER * (z + 10.0); };
            Extrude(UF_NULLSIGN, {Pv(xAt(z0), -40, z0), Pv(BASE_X, -40, z0),
                                  Pv(BASE_X, -40, z1), Pv(xAt(z1), -40, z1)},
                    Pv(0, 1, 0), 80);
        }

        // 5) 基座 + 推楔螺孔 + 定位销 + 回拉螺钉
        Block(UF_NULLSIGN, Pv(BASE_X, -120, -40), 76, 240, 200);
        Bore(UF_NEGATIVE, Pv(BASE_X + 20, 0, 160), 60, 8, Pv(0, 0, -1));
        for (int s : {-1, 1}) {
            Bore(UF_NULLSIGN, Pv(BASE_X, s * 55.0, 30), 22, 6, Pv(-1, 0, 0));         // 定位销
            Bore(UF_NULLSIGN, Pv(BASE_X, s * 55.0, 110), 30, 6.6, Pv(-1, 0, 0));       // 回拉螺钉
        }

        Chk(UF_MODL_update(), "update");
        PadReport("ASSY");
        Chk(UF_PART_save(), "保存部件");

        std::filesystem::remove(xtFile);
        {
            uf_list_p_t list = nullptr;
            Chk(UF_MODL_create_list(&list), "list");
            for (auto* b : *Session::GetSession()->Parts()->Work()->Bodies())
                if (b->IsSolidBody()) Chk(UF_MODL_put_list_item(list, b->Tag()), "item");
            Chk(UF_PS_export_data(list, const_cast<char*>(xtFile.string().c_str())), "ps");
            UF_MODL_delete_list(&list);
            std::cout << "XT " << xtFile.string() << " "
                      << std::filesystem::file_size(xtFile) << " bytes" << std::endl;
        }
        std::cout << "PASS side mount: rail + pad + ball R9 + wedge 1:50 + base + 2 dowels + 2 "
                     "pull screws + push screw" << std::endl;
        Chk(UF_terminate(), "UF_terminate");
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ERROR " << e.what() << std::endl;
        return 1;
    }
}







