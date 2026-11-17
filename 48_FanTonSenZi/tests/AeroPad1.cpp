// 一体式气浮板 120x120（无盖板）：深孔交叉供气 + 工作面均压槽 + 中央真空浅腔
// 内部气路全部靠侧面深孔，工艺口用 M10 堵头/快插接头封（模型里做成 φ10 沉孔）
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

// 任意方向圆柱（钻孔）：origin 为起点，dir 为钻入方向
static tag_t Bore(UF_FEATURE_SIGN sign, double x, double y, double z, double len, double dia,
                  double dx, double dy, double dz) {
    char hs[64], ds[64];
    std::snprintf(hs, sizeof hs, "%.6f", len);
    std::snprintf(ds, sizeof ds, "%.6f", dia);
    double origin[3] = {x, y, z}, dir[3] = {dx, dy, dz};
    tag_t t = 0;
    Chk(UF_MODL_create_cyl1(sign, origin, hs, ds, dir, &t), "钻孔");
    return t;
}

static void PadReport(const char* tag) {
    auto* work = Session::GetSession()->Parts()->Work();
    int solids = 0, faces = 0;
    for (auto* b : *work->Bodies())
        if (b->IsSolidBody()) { ++solids; faces += static_cast<int>(b->GetFaces().size()); }
    std::cout << tag << " solids=" << solids << " faces=" << faces << std::endl;
}

namespace P {
const double L = 120.0, W = 120.0, T = 30.0;   // 一体式：整块 30 厚，无盖板
const double ZONE = 38.0;                       // 4 个气腔中心 ±38, ±38
const double AIR_BORE_D = 8.0;                  // 供气深孔
const double AIR_BORE_Z = 20.0;                 // 深孔所在高度（距工作面 10）
const double AIR_BORE_IN = 26.0;                // 深孔内端 x 坐标（盲孔）
const double FEED_D = 0.2;                      // 供气节流孔（深孔 → 工作面）
const double FEED_STEP = 10.0;                  // 同一深孔上相邻节流孔间距
const int FEED_PER_ZONE = 3;                    // 每腔节流孔数（孔多孔小：均匀 + 冗余）
const double GROOVE_W = 1.5, GROOVE_D = 0.6;    // 工作面均压槽
const double GROOVE = 26.0;                     // 均压槽外框
const double VAC_BORE_D = 6.0, VAC_BORE_END = -20.0;   // 真空深孔（沿 -Y）
const double VAC_PORT_D = 3.0, VAC_PORT_OFF = 10.0;    // 真空连接孔
const double VAC_REC = 40.0, VAC_REC_D = 0.6;          // 工作面真空浅腔
const double DRAIN_W = 0.5, DRAIN_D = 0.5, DRAIN_AT = 56.0;
const double MOUNT_D = 5.5, MOUNT_AT = 50.0;
const double PLUG_D = 10.0, PLUG_L = 14.0;      // M10 堵头沉孔
}  // namespace P

int main(int argc, char** argv) {
    try {
        std::filesystem::path out = argc > 1 ? std::filesystem::path(argv[1])
                                            : std::filesystem::path("G:/AeroPad1");
        std::filesystem::create_directories(out);
        Chk(UF_initialize(), "UF_initialize");

        auto prtFile = out / "AeroPad1_120x120.prt";
        auto stpFile = out / "AeroPad1_120x120.stp";
        std::filesystem::remove(prtFile);

        tag_t part = 0;
        Chk(UF_PART_new(prtFile.string().c_str(), METRIC, &part), "新建部件");
        std::cout << "PART " << prtFile.string() << std::endl;

        // ================= 一体本体 =================
        tag_t body = Block(UF_NULLSIGN, -P::L / 2, -P::W / 2, 0, P::L, P::W, P::T);

        // 4 条供气深孔（每个气腔一条，从侧面钻到腔中心并穿过一点）
        std::vector<std::pair<double, double>> zones = {
            {P::ZONE, P::ZONE}, {P::ZONE, -P::ZONE},
            {-P::ZONE, P::ZONE}, {-P::ZONE, -P::ZONE}};
        for (size_t i = 0; i < zones.size(); ++i) {
            const double cx = zones[i].first, cy = zones[i].second;
            const double mouth = (cx > 0 ? P::L / 2 : -P::L / 2);
            const double len = std::abs(mouth - (cx > 0 ? P::AIR_BORE_IN : -P::AIR_BORE_IN));
            Bore(UF_NEGATIVE, mouth, cy, P::AIR_BORE_Z, len, P::AIR_BORE_D,
                 cx > 0 ? -1 : 1, 0, 0);
            // M10 堵头/接头沉孔
            Bore(UF_NEGATIVE, mouth, cy, P::AIR_BORE_Z, P::PLUG_L, P::PLUG_D,
                 cx > 0 ? -1 : 1, 0, 0);
            // 供气节流孔：深孔 → 工作面（3 个，沿深孔轴线，间距 10）
            for (int k = -1; k <= 1; ++k)
                Bore(UF_NEGATIVE, cx + k * P::FEED_STEP, cy, P::AIR_BORE_Z,
                     P::T - P::AIR_BORE_Z, P::FEED_D, 0, 0, 1);
            // 工作面均压槽（外框 26，宽 1.5，深 0.6）
            const double o = P::GROOVE / 2, w = P::GROOVE_W, z = P::T - P::GROOVE_D;
            Block(UF_NEGATIVE, cx - o, cy + o - w, z, 2 * o, w, P::GROOVE_D);
            Block(UF_NEGATIVE, cx - o, cy - o, z, 2 * o, w, P::GROOVE_D);
            Block(UF_NEGATIVE, cx + o - w, cy - o, z, w, 2 * o, P::GROOVE_D);
            Block(UF_NEGATIVE, cx - o, cy - o, z, w, 2 * o, P::GROOVE_D);
        }

        // 真空深孔（沿 -Y，从 y=60 钻到 y=-20，覆盖 ±10 区域内）
        {
            const double len = P::W / 2 - P::VAC_BORE_END;
            Bore(UF_NEGATIVE, 0, P::W / 2, P::AIR_BORE_Z, len, P::VAC_BORE_D, 0, -1, 0);
            Bore(UF_NEGATIVE, 0, P::W / 2, P::AIR_BORE_Z, P::PLUG_L, P::PLUG_D, 0, -1, 0);
            for (int sy : {-1, 1})   // φ3 连接孔 → 工作面浅腔
                Bore(UF_NEGATIVE, 0, sy * P::VAC_PORT_OFF, P::AIR_BORE_Z,
                     P::T - P::VAC_REC_D - P::AIR_BORE_Z, P::VAC_PORT_D, 0, 0, 1);
            Block(UF_NEGATIVE, -P::VAC_REC / 2, -P::VAC_REC / 2, P::T - P::VAC_REC_D,
                  P::VAC_REC, P::VAC_REC, P::VAC_REC_D);
        }

        // 工作面周圈泄流槽
        {
            const double dz = P::T - P::DRAIN_D, dw = P::DRAIN_W;
            Block(UF_NEGATIVE, -P::DRAIN_AT, P::DRAIN_AT - dw / 2, dz, 2 * P::DRAIN_AT, dw,
                  P::DRAIN_D);
            Block(UF_NEGATIVE, -P::DRAIN_AT, -P::DRAIN_AT - dw / 2, dz, 2 * P::DRAIN_AT, dw,
                  P::DRAIN_D);
            Block(UF_NEGATIVE, P::DRAIN_AT - dw / 2, -P::DRAIN_AT, dz, dw, 2 * P::DRAIN_AT,
                  P::DRAIN_D);
            Block(UF_NEGATIVE, -P::DRAIN_AT - dw / 2, -P::DRAIN_AT, dz, dw, 2 * P::DRAIN_AT,
                  P::DRAIN_D);
        }

        // 4 个安装通孔（±50, ±50，避开深孔与泄流槽）
        for (int sx : {-1, 1})
            for (int sy : {-1, 1})
                Bore(UF_NEGATIVE, sx * P::MOUNT_AT, sy * P::MOUNT_AT, 0, P::T, P::MOUNT_D,
                     0, 0, 1);

        Chk(UF_MODL_update(), "update");
        PadReport("BODY");

        Chk(UF_PART_save(), "保存部件");

        // STEP（批处理下拿不到许可时返回空，下面用 Parasolid 兜底）
        Session* session = Session::GetSession();
        if (session->Parts()->Work() == nullptr) throw std::runtime_error("work part missing");
        Chk(UF_PART_set_display_part(part), "设置显示部件");
        StepCreator* step = session->DexManager()->CreateStepCreator();
        try {
            step->SetExportFrom(NXOpen::StepCreator::ExportFromOptionDisplayPart);
            step->SetExportDestination(NXOpen::BaseCreator::ExportDestinationOptionNativeFileSystem);
            step->ExportSelectionBlock()->SetSelectionScope(NXOpen::ObjectSelector::ScopeEntirePart);
            step->ObjectTypes()->SetSolids(true);
            step->ObjectTypes()->SetSurfaces(true);
            step->SetInputFile(prtFile.string().c_str());
            step->SetOutputFile(stpFile.string().c_str());
            step->SetExportAs(NXOpen::StepCreator::ExportAsOptionAp214);
            NXOpen::NXObject* res = step->Commit();
            std::cout << "COMMIT " << (res ? "ok" : "null") << std::endl;
        } catch (const NXException& e) {
            std::cerr << "STEP exception: " << e.Message() << std::endl;
        }
        delete step;

        auto xtFile = out / "AeroPad1_120x120.x_t";
        std::filesystem::remove(xtFile);
        {
            uf_list_p_t list = nullptr;
            Chk(UF_MODL_create_list(&list), "list");
            for (auto* b : *session->Parts()->Work()->Bodies())
                if (b->IsSolidBody()) Chk(UF_MODL_put_list_item(list, b->Tag()), "list item");
            Chk(UF_PS_export_data(list, const_cast<char*>(xtFile.string().c_str())),
                "parasolid export");
            UF_MODL_delete_list(&list);
            std::cout << "XT " << xtFile.string() << " "
                      << std::filesystem::file_size(xtFile) << " bytes" << std::endl;
        }
        std::cout << "PASS one-piece pad: 120x120x30, 4 air bores + grooves, 1 vacuum bore, "
                     "no cover plate" << std::endl;
        Chk(UF_terminate(), "UF_terminate");
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ERROR " << e.what() << std::endl;
        return 1;
    }
}

