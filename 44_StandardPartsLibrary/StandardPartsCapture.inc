// Included inside StandardPartsLibrary.cpp's private namespace.
// The capture window runs only after the library Block Styler dialog is disposed.

constexpr std::array<double, 6> CaptureAbsoluteDirections{1,0,0,0,1,0};

std::array<double, 9> CaptureFrameAxes(const std::array<double, 6>& directions)
{
    for (double value : directions)
        if (!std::isfinite(value)) throw std::runtime_error("基准方位坐标轴必须为有限数值。");
    std::array<double, 3> x{directions[0], directions[1], directions[2]};
    std::array<double, 3> y{directions[3], directions[4], directions[5]};
    const auto normalize = [](std::array<double, 3>& axis)
    {
        const double length = std::hypot(axis[0], axis[1], axis[2]);
        if (!std::isfinite(length) || length <= 1.0e-9)
            throw std::runtime_error("基准方位坐标轴无效，X/Y 轴不能为零或平行。");
        for (double& value : axis) value /= length;
    };
    normalize(x);
    normalize(y);
    std::array<double, 3> z{x[1]*y[2]-x[2]*y[1], x[2]*y[0]-x[0]*y[2], x[0]*y[1]-x[1]*y[0]};
    normalize(z);
    y = {z[1]*x[2]-z[2]*x[1], z[2]*x[0]-z[0]*x[2], z[0]*x[1]-z[1]*x[0]};
    return {x[0],x[1],x[2],y[0],y[1],y[2],z[0],z[1],z[2]};
}

bool ValidateCaptureBodies(const std::vector<tag_t>& bodies, tag_t workPart, std::wstring& error)
{
    if (workPart == NULL_TAG || bodies.empty()) { error = L"请至少选择一个当前工作部件的实体。"; return false; }
    for (tag_t body : bodies)
    {
        int type = 0, subtype = 0;
        tag_t owner = NULL_TAG;
        if (body == NULL_TAG || UF_OBJ_ask_type_and_subtype(body, &type, &subtype) != 0 ||
            type != UF_solid_type || subtype != UF_solid_body_subtype ||
            UF_OBJ_ask_owning_part(body, &owner) != 0 || owner != workPart || UF_ASSEM_is_occurrence(body))
        {
            error = L"只能选择当前工作部件的实体，不能选择面、组件或装配发生体。";
            return false;
        }
    }
    return true;
}

void ExportCaptureBodies(const fs::path& target, const std::vector<tag_t>& selection,
    const std::array<double, 3>& basePoint,
    const std::array<double, 6>& baseDirections = CaptureAbsoluteDirections)
{
    std::wstring error;
    if (!ValidateCaptureBodies(selection, UF_ASSEM_ask_work_part(), error))
        throw std::runtime_error(ToAnsi(error));
    if (fs::exists(target)) throw std::runtime_error("入库目标文件已存在，未覆盖。");
    for (double value : basePoint)
        if (!std::isfinite(value)) throw std::runtime_error("插入点坐标必须为有限数值。");
    const auto axes = CaptureFrameAxes(baseDirections);
    auto bodies = selection;
    std::sort(bodies.begin(), bodies.end());
    bodies.erase(std::unique(bodies.begin(), bodies.end()), bodies.end());
    {
    struct WcsScope
    {
        tag_t original = NULL_TAG, temporary = NULL_TAG, matrix = NULL_TAG;
        ~WcsScope()
        {
            if (original != NULL_TAG && UF_CSYS_set_wcs(original) != 0)
                UF_print_syslog("[StandardPartsLibrary] Capture WCS restore failed\n", false);
            if (temporary != NULL_TAG) UF_OBJ_delete_object(temporary);
            if (matrix != NULL_TAG) UF_OBJ_delete_object(matrix);
        }
    } wcs;
    CheckPreviewUf(UF_CSYS_ask_wcs(&wcs.original));
    const double identity[9] = {1,0,0,0,1,0,0,0,1};
    CheckPreviewUf(UF_CSYS_create_matrix(identity, &wcs.matrix));
    const double absoluteOrigin[3] = {0,0,0};
    CheckPreviewUf(UF_CSYS_create_temp_csys(absoluteOrigin, wcs.matrix, &wcs.temporary));
    CheckPreviewUf(UF_CSYS_set_wcs(wcs.temporary));
    UF_PART_export_options_t options{};
    options.new_part = true;
    options.params_mode = UF_PART_remove_params;
    options.expression_mode = UF_PART_copy_exp_shallowly;
    // Export using absolute axes, then normalize the new file explicitly.
    // A new NX export can inherit source WCS, so offset WCS alone is insufficient.
    const int code = UF_PART_export_with_options(ToAnsi(target.wstring()).c_str(),
        static_cast<int>(bodies.size()), bodies.data(), &options);
    CheckPreviewUf(UF_CSYS_set_wcs(wcs.original));
    CheckPreviewUf(code);
    }
    struct ExportPartScope
    {
        tag_t part = NULL_TAG;
        UF_ASSEM_work_part_context_p_t previous = nullptr;
        ~ExportPartScope()
        {
            if (previous != nullptr && UF_ASSEM_restore_work_part_context_quietly(&previous) != 0)
            {
                UF_print_syslog("[StandardPartsLibrary] Capture work context restore failed\n", false);
                return; // Never close a part while restoration of its context failed.
            }
            if (part != NULL_TAG) UF_PART_close(part,0,2);
        }
    } exported;
    UF_PART_load_status_t status{};
    const int openCode = UF_PART_open_quiet(ToAnsi(target.wstring()).c_str(), &exported.part, &status);
    UF_PART_free_load_status(&status);
    CheckPreviewUf(openCode);
    CheckPreviewUf(UF_ASSEM_set_work_part_context_quietly(exported.part, &exported.previous));
    std::vector<tag_t> copiedBodies;
    tag_t body = NULL_TAG;
    for (;;)
    {
        CheckPreviewUf(UF_OBJ_cycle_objs_in_part(exported.part, UF_solid_type, &body));
        if (body == NULL_TAG) break;
        int type = 0, subtype = 0;
        CheckPreviewUf(UF_OBJ_ask_type_and_subtype(body, &type, &subtype));
        if (subtype == UF_solid_body_subtype) copiedBodies.push_back(body);
    }
    if (copiedBodies.size() != bodies.size()) throw std::runtime_error("导出实体数量不一致，已停止入库。");
    // Store copied geometry in the chosen insertion frame: local = R^-1 * (p - origin).
    // Each local coordinate is the dot product with its chosen axis.
    double translation[3]{};
    for (int axis = 0; axis < 3; ++axis)
        for (int component = 0; component < 3; ++component)
            translation[axis] -= axes[axis*3+component] * basePoint[component];
    double transform[16]{};
    UF_MTX4_initialize(1.0, translation, axes.data(), transform);
    CheckPreviewUf(UF_MODL_transform_entities(static_cast<int>(copiedBodies.size()), copiedBodies.data(), transform));
    CheckPreviewUf(UF_PART_save());
    CheckPreviewUf(UF_ASSEM_restore_work_part_context_quietly(&exported.previous));
}

struct CaptureMetadata
{
    std::wstring name, category, specification;
};

bool CaptureBodiesToLibrary(AppState* state, const std::vector<tag_t>& bodies,
    const std::array<double, 3>& basePoint, fs::path& savedPath, std::wstring& error,
    const CaptureMetadata* metadata = nullptr, const fs::path& capturedImage = {},
    const std::array<double, 6>& baseDirections = CaptureAbsoluteDirections)
{
    const auto name = Trim(metadata ? metadata->name : GetText(state->window, ID_NAME));
    auto category = Trim(metadata ? metadata->category : GetText(state->window, ID_EDIT_CATEGORY));
    auto specification = Trim(metadata ? metadata->specification : GetText(state->window, ID_EDIT_SPEC));
    if (name.empty()) { error = L"请输入标准件名称。"; return false; }
    if (category.empty()) category = L"用户自定义";
    if (specification.empty()) specification = L"默认";
    for (const auto& value : {name, category, specification})
        if (value.find_first_of(L"\t\r\n") != std::wstring::npos)
        { error = L"名称、分类和规格不能包含制表符或换行。"; return false; }
    if (!ValidateCaptureBodies(bodies, UF_ASSEM_ask_work_part(), error)) return false;
    for (double value : basePoint)
        if (!std::isfinite(value)) { error = L"插入点坐标无效。"; return false; }
    try { CaptureFrameAxes(baseDirections); }
    catch (const std::exception& ex) { error = FromAnsi(ex.what()); return false; }
    if (!capturedImage.empty() && (!fs::is_regular_file(capturedImage) || fs::file_size(capturedImage) == 0))
    { error = L"抓取的图片已失效，请重新抓取或清除图片。"; return false; }
    if (!EnsureLibraryFolders(state, error)) return false;
    const fs::path root = fs::weakly_canonical(LibraryRoot(state));
    const fs::path directory = fs::weakly_canonical(root / L"Lib" /
        SanitizeFileName(category) / SanitizeFileName(name));
    if (Lower(directory.wstring()).rfind(Lower(root.wstring() + L"\\"), 0) != 0)
    { error = L"入库目录不在标准件库内。"; return false; }
    fs::create_directories(directory);
    fs::path target = directory / (SanitizeFileName(specification) + L".prt");
    unsigned suffix = 1;
    // An orphaned sidecar must never be overwritten or attached to a new model.
    while (fs::exists(target) || !FindSidecarPreview(target).empty())
        target = directory / (SanitizeFileName(specification) + L"_" + std::to_wstring(suffix++) + L".prt");
    fs::path imageTarget = target;
    imageTarget.replace_extension(L".png");
    bool imageCreated = false;
    const auto originalCount = state->items.size();
    try
    {
        ExportCaptureBodies(target, bodies, basePoint, baseDirections);
        if (!capturedImage.empty())
        {
            // Reserve ownership before copying so even a partial-copy failure
            // can roll back our file without touching an existing sidecar.
            const HANDLE output = CreateFileW(imageTarget.c_str(), GENERIC_WRITE, 0,
                nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (output == INVALID_HANDLE_VALUE) throw std::runtime_error("预览图无法创建，已停止本次入库。");
            CloseHandle(output);
            imageCreated = true;
            fs::copy_file(capturedImage, imageTarget, fs::copy_options::overwrite_existing);
        }
        state->items.push_back({L"capture_" + std::to_wstring(GetTickCount64()), name, category,
            fs::relative(target, root).wstring(), specification, false});
        if (!SaveIndex(state, error)) throw std::runtime_error(ToAnsi(error));
        savedPath = target;
        return true;
    }
    catch (const std::exception& ex)
    {
        state->items.resize(originalCount);
        error = FromAnsi(ex.what());
        // Only the new, collision-checked file from this operation is removed.
        std::error_code ec;
        fs::remove(target, ec);
        if (ec) error += L"\r\n未完成文件无法清理：" + target.wstring();
        if (imageCreated)
        {
            fs::remove(imageTarget, ec);
            if (ec) error += L"\r\n未完成图片无法清理：" + imageTarget.wstring();
        }
        return false;
    }
}

#include "StandardPartsCaptureDialog.inc"
