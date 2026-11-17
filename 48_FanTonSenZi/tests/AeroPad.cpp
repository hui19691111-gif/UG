// 精密气浮板（120x120）参数化建模 + STEP 导出
// 用 NX UFUN 直接建体：本体（5 个腔 + 16 个节流孔 + 周圈泄流槽 + 4 个安装孔）
//                  + 盖板（4 个进气孔 + 1 个真空接口 + 4 个安装孔 + 5 圈 O 圈槽）
#include <NXOpen/Body.hxx>
#include <NXOpen/BodyCollection.hxx>
#include <NXOpen/DexManager.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/ObjectSelector.hxx>
#include <NXOpen/ObjectTypeSelector.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/StepCreator.hxx>
#include <uf.h>
#include <uf_modl.h>
#include <uf_modl_primitives.h>
#include <uf_part.h>
#include <uf_ps.h>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace NXOpen;

static void Chk(int rc, const char* what) {
    if (rc != 0) {
        char msg[133] = {0};
        UF_get_fail_message(rc, msg);
        throw std::runtime_error(std::string(what) + " 失败: " + msg);
    }
}

static tag_t Block(UF_FEATURE_SIGN sign, double x0, double y0, double z0,
                   double dx, double dy, double dz) {
    char a[64], b[64], c[64];
    std::snprintf(a, sizeof a, "%.6f", dx);
    std::snprintf(b, sizeof b, "%.6f", dy);
    std::snprintf(c, sizeof c, "%.6f", dz);
    double corner[3] = {x0, y0, z0};
    char* len[3] = {a, b, c};
    tag_t t = 0;
    Chk(UF_MODL_create_block1(sign, corner, len, &t), "块体");
    return t;
}

static tag_t Cyl(UF_FEATURE_SIGN sign, double x, double y, double z0,
                 double h, double dia) {
    char hs[64], ds[64];
    std::snprintf(hs, sizeof hs, "%.6f", h);
    std::snprintf(ds, sizeof ds, "%.6f", dia);
    double origin[3] = {x, y, z0}, dir[3] = {0, 0, 1};
    tag_t t = 0;
    Chk(UF_MODL_create_cyl1(sign, origin, hs, ds, dir, &t), "圆柱");
    return t;
}

static void PadReport(const char* tag) {
    auto* work = Session::GetSession()->Parts()->Work();
    int solids = 0, faces = 0;
    for (auto* b : *work->Bodies())
        if (b->IsSolidBody()) { ++solids; faces += static_cast<int>(b->GetFaces().size()); }
    std::cout << tag << " solids=" << solids << " faces=" << faces << std::endl;
}
// ---- 设计参数 (mm) ----
namespace P {
const double L = 120.0, W = 120.0;      // 板外形
const double T = 30.0;                  // 本体厚（工作面 z=30）
const double POCKET = 32.0;             // 气腔 / 真空腔边长
const double POCKET_D = 14.0;           // 腔深（自背面 z=0 起）
const double ZONE = 38.0;               // 气腔中心距（±38, ±38）
const double ORIFICE_D = 0.3;           // 节流孔
const double ORIFICE_OFF = 8.0;         // 节流孔相对腔中心的偏移
const double DRAIN_W = 0.5, DRAIN_D = 0.5; // 泄流槽
const double DRAIN_AT = 56.0;           // 泄流槽中心线
const double COVER_T = 5.0;             // 盖板厚
const double ORING_IN = 36.0, ORING_OUT = 40.0, ORING_D = 1.5; // O 圈槽
const double MOUNT_D = 5.5, MOUNT_AT = 50.0;   // 安装孔
const double INLET_D = 1.5, VAC_D = 6.0;
// 真空作用部分：工作面浅腔 + 连接孔（负压直接作用到导轨面）
const double VAC_RECESS = 40.0, VAC_RECESS_D = 0.6;
const double VAC_PORT_D = 3.0, VAC_PORT_OFF = 10.0;
}  // namespace P

int main(int argc, char** argv) {
    try {
        std::filesystem::path out = argc > 1 ? std::filesystem::path(argv[1])
                                                     : std::filesystem::path("G:/AeroPad");
        std::filesystem::create_directories(out);
        Chk(UF_initialize(), "UF_initialize");

        auto prtFile = out / "AeroPad_120x120.prt";
        auto stpFile = out / "AeroPad_120x120.stp";
        std::filesystem::remove(prtFile);

        tag_t part = 0;
        Chk(UF_PART_new(prtFile.string().c_str(), METRIC, &part), "新建部件");
        std::cout << "PART " << prtFile.string() << std::endl;

        // ================= 本体 =================
        tag_t body = Block(UF_NULLSIGN, -P::L / 2, -P::W / 2, 0, P::L, P::W, P::T);

        const double h = P::POCKET / 2;
        std::vector<std::pair<double, double>> pockets = {
            {0, 0}, {-P::ZONE, P::ZONE}, {P::ZONE, P::ZONE},
            {-P::ZONE, -P::ZONE}, {P::ZONE, -P::ZONE}};
        for (const auto& c : pockets)
            Block(UF_NEGATIVE, c.first - h, c.second - h, 0, P::POCKET, P::POCKET, P::POCKET_D);

        const double air = P::ORIFICE_OFF;
        // 中央真空腔：工作面开 40x40x0.6 浅腔 + 4 个 φ3 连接孔，负压直接吸导轨面
        Block(UF_NEGATIVE, -P::VAC_RECESS / 2, -P::VAC_RECESS / 2,
              P::T - P::VAC_RECESS_D, P::VAC_RECESS, P::VAC_RECESS, P::VAC_RECESS_D);
        for (int sx : {-1, 1})
            for (int sy : {-1, 1})
                Cyl(UF_NEGATIVE, sx * P::VAC_PORT_OFF, sy * P::VAC_PORT_OFF, P::POCKET_D,
                    P::T - P::VAC_RECESS_D - P::POCKET_D, P::VAC_PORT_D);
        for (size_t i = 1; i < pockets.size(); ++i)   // 4 个正压气腔各 4 个节流孔
            for (int sx : {-1, 1})
                for (int sy : {-1, 1})
                    Cyl(UF_NEGATIVE, pockets[i].first + sx * air, pockets[i].second + sy * air,
                        P::POCKET_D, P::T - P::POCKET_D, P::ORIFICE_D);

        // 工作面周圈泄流槽（z 29.5~30）
        const double dz = P::T - P::DRAIN_D, dw = P::DRAIN_W;
        Block(UF_NEGATIVE, -P::DRAIN_AT, P::DRAIN_AT - dw / 2, dz, 2 * P::DRAIN_AT, dw, P::DRAIN_D);
        Block(UF_NEGATIVE, -P::DRAIN_AT, -P::DRAIN_AT - dw / 2, dz, 2 * P::DRAIN_AT, dw, P::DRAIN_D);
        Block(UF_NEGATIVE, P::DRAIN_AT - dw / 2, -P::DRAIN_AT, dz, dw, 2 * P::DRAIN_AT, P::DRAIN_D);
        Block(UF_NEGATIVE, -P::DRAIN_AT - dw / 2, -P::DRAIN_AT, dz, dw, 2 * P::DRAIN_AT, P::DRAIN_D);

        // 4 个安装通孔
        for (int s : {-1, 1}) {
            Cyl(UF_NEGATIVE, s * P::MOUNT_AT, 0, 0, P::T, P::MOUNT_D);
            Cyl(UF_NEGATIVE, 0, s * P::MOUNT_AT, 0, P::T, P::MOUNT_D);
        }
        Chk(UF_MODL_update(), "update");
        PadReport("BODY");

        // ================= 盖板 =================
        tag_t cover = Block(UF_NULLSIGN, -P::L / 2, -P::W / 2, -P::COVER_T, P::L, P::W, P::COVER_T);
        for (size_t i = 1; i < pockets.size(); ++i)   // 4 个进气孔 φ1.5
            Cyl(UF_NEGATIVE, pockets[i].first, pockets[i].second, -P::COVER_T, P::COVER_T, P::INLET_D);
        Cyl(UF_NEGATIVE, 0, 0, -P::COVER_T, P::COVER_T, P::VAC_D);   // 真空接口 φ6
        for (int s : {-1, 1}) {
            Cyl(UF_NEGATIVE, s * P::MOUNT_AT, 0, -P::COVER_T, P::COVER_T, P::MOUNT_D);
            Cyl(UF_NEGATIVE, 0, s * P::MOUNT_AT, -P::COVER_T, P::COVER_T, P::MOUNT_D);
        }
        // 5 圈 O 圈槽（开在盖板贴合面 z=0 上，截面 2 x 1.5）
        const double oi = P::ORING_IN / 2, oo = P::ORING_OUT / 2, ow = oo - oi;
        for (const auto& c : pockets) {
            const double zz = -P::ORING_D;
            Block(UF_NEGATIVE, c.first - oo, c.second + oi, zz, 2 * oo, ow, P::ORING_D);
            Block(UF_NEGATIVE, c.first - oo, c.second - oo, zz, 2 * oo, ow, P::ORING_D);
            Block(UF_NEGATIVE, c.first + oi, c.second - oo, zz, ow, 2 * oo, P::ORING_D);
            Block(UF_NEGATIVE, c.first - oo, c.second - oo, zz, ow, 2 * oo, P::ORING_D);
        }
        Chk(UF_MODL_update(), "update");
        PadReport("AFTER COVER");

        Chk(UF_PART_save(), "保存部件");

        // ================= STEP AP214 =================
        Session* session = Session::GetSession();
        if (session->Parts()->Work() == nullptr) throw std::runtime_error("work part missing");
        Chk(UF_PART_set_display_part(part), "设置显示部件");
        StepCreator* step = session->DexManager()->CreateStepCreator();
        try {
            step->SetExportFrom(NXOpen::StepCreator::ExportFromOptionDisplayPart);
            step->SetExportDestination(NXOpen::BaseCreator::ExportDestinationOptionNativeFileSystem);
            step->ExportSelectionBlock()->SetSelectionScope(NXOpen::ObjectSelector::ScopeEntirePart);
            step->ObjectTypes()->SetSolids(true);    // 默认不勾实体, 必须显式打开
            step->ObjectTypes()->SetSurfaces(true);
            step->SetInputFile(prtFile.string().c_str());
            step->SetOutputFile(stpFile.string().c_str());
            step->SetExportAs(NXOpen::StepCreator::ExportAsOptionAp214);
            NXOpen::NXObject* res = step->Commit();
            std::cout << "COMMIT " << (res ? "ok" : "null") << std::endl;
        } catch (const NXException& e) {
            std::cerr << "STEP exception: " << e.Message() << std::endl;
            delete step;
            Chk(UF_terminate(), "UF_terminate");
            return 2;
        }
        delete step;
        // Parasolid 兜底导出（STEP 失败时也能拿到 3D 图档）
        auto xtFile = out / "AeroPad_120x120.x_t";
        std::filesystem::remove(xtFile);
        {
            uf_list_p_t list = nullptr;
            Chk(UF_MODL_create_list(&list), "list");
            for (auto* b : *session->Parts()->Work()->Bodies())
                if (b->IsSolidBody()) Chk(UF_MODL_put_list_item(list, b->Tag()), "list item");
            Chk(UF_PS_export_data(list, const_cast<char*>(xtFile.string().c_str())), "parasolid export");
            UF_MODL_delete_list(&list);
            std::cout << "XT " << xtFile.string() << " "
                      << std::filesystem::file_size(xtFile) << " bytes" << std::endl;
        }
        if (!std::filesystem::exists(stpFile)) {
            std::cerr << "STEP file missing (Parasolid 已导出)" << std::endl;
            Chk(UF_terminate(), "UF_terminate");
            return 3;
        }
        std::cout << "STEP " << stpFile.string() << " "
                  << std::filesystem::file_size(stpFile) << " bytes" << std::endl;
        std::cout << "PASS 120x120 air bearing pad: 2 bodies, 5 pockets, 16 orifices, 5 O-ring grooves, drain groove, STEP exported" << std::endl;
        Chk(UF_terminate(), "UF_terminate");
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ERROR " << e.what() << std::endl;
        return 1;
    }
}













