// NX owns this dialog's message loop and selection lifecycle. A private
// GetMessage loop inside ufusr leaves NX waiting and trips Work In Progress.
std::string DialogFilePath();

// A snapshot belongs to one pending capture. Failed recaptures retain the old
// image; Apply consumes it and Cancel/destruction removes temporary files only.
class CapturePreviewImage
{
public:
    ~CapturePreviewImage()
    {
        Clear();
        std::error_code ec;
        if (!directory_.empty()) fs::remove(directory_, ec);
    }

    const fs::path& Path() const { return image_; }

    void Clear() noexcept
    {
        std::error_code ec;
        if (!image_.empty()) fs::remove(image_, ec);
        image_.clear();
    }

    void Capture(const std::vector<tag_t>& selected)
    {
        auto* displayPart = NXOpen::Session::GetSession()->Parts()->Display();
        if (!displayPart) throw std::runtime_error("请先打开模型后再抓取图片。");
        if (directory_.empty())
        {
            GUID guid{};
            wchar_t name[40]{};
            if (FAILED(CoCreateGuid(&guid)) || StringFromGUID2(guid, name, 40) == 0)
                throw std::runtime_error("无法创建抓图临时目录。");
            const auto candidate = fs::temp_directory_path() / (std::wstring(L"ZhihuiCapture_") + name);
            if (!fs::create_directory(candidate)) throw std::runtime_error("抓图临时目录已存在。");
            directory_ = candidate;
        }
        const auto candidate = directory_ / (L"view_" + std::to_wstring(++sequence_) + L".png");
        try
        {
            // Keep the selection lists intact, but leave their display in normal
            // colors. Re-highlighting on scope exit made the captured view appear
            // selected again and could contaminate a subsequent capture.
            std::vector<tag_t> highlights = selected;
            struct GlobalSelection
            {
                tag_t* objects = nullptr;
                ~GlobalSelection() { if (objects) UF_free(objects); }
            } globalSelection;
            int globalCount = 0;
            CheckPreviewUf(UF_UI_ask_global_sel_object_list(&globalCount, &globalSelection.objects));
            for (int i = 0; i < globalCount; ++i)
                highlights.push_back(globalSelection.objects[i]);
            std::sort(highlights.begin(), highlights.end());
            highlights.erase(std::unique(highlights.begin(), highlights.end()), highlights.end());
            const auto clearHighlights = [&]()
            {
                for (tag_t object : highlights)
                    if (object != NULL_TAG) CheckPreviewUf(UF_DISP_set_highlight(object, 0));
                CheckPreviewUf(UF_DISP_regenerate_display());
            };
            clearHighlights();
            struct BuilderScope
            {
                NXOpen::Gateway::ImageExportBuilder* value = nullptr;
                ~BuilderScope() { try { if (value) value->Destroy(); } catch (...) {} }
            } builder;
            builder.value = displayPart->Views()->CreateImageExportBuilder();
            using Export = NXOpen::Gateway::ImageExportBuilder;
            builder.value->SetRegionMode(false);
            builder.value->SetFileFormat(Export::FileFormatsPng);
            builder.value->SetBackgroundOption(Export::BackgroundOptionsCustomColor);
            const std::vector<double> white{1.0, 1.0, 1.0};
            builder.value->SetCustomBackgroundColor(white);
            builder.value->SetFileName(ToAnsi(candidate.wstring()).c_str());
            builder.value->Commit();
            clearHighlights();
            if (!fs::is_regular_file(candidate) || fs::file_size(candidate) == 0)
                throw std::runtime_error("未生成有效图片，请重新抓取。");
            Clear();
            image_ = candidate;
            UF_print_syslog("[StandardPartsLibrary] Capture preview image ready\n", false);
        }
        catch (...)
        {
            std::error_code ec;
            fs::remove(candidate, ec);
            throw;
        }
    }

private:
    fs::path directory_, image_;
    unsigned sequence_ = 0;
};

class BodyCaptureDialog
{
public:
    BodyCaptureDialog(const fs::path& root, const std::wstring& category)
        : initialCategory_(category), workPart_(UF_ASSEM_ask_work_part())
    {
        if (workPart_ == NULL_TAG) throw std::runtime_error("请先打开需要入库的工作部件。");
        state_.libraryRootOverride = root;
        const fs::path path = fs::path(DialogFilePath()).parent_path() / "StandardPartsCapture.dlx";
        dialog_.reset(NXOpen::UI::GetUI()->CreateDialog(ToAnsi(path.wstring()).c_str()));
        dialog_->AddInitializeHandler(NXOpen::make_callback(this, &BodyCaptureDialog::Initialize));
        dialog_->AddDialogShownHandler(NXOpen::make_callback(this, &BodyCaptureDialog::DialogShown));
        dialog_->AddUpdateHandler(NXOpen::make_callback(this, &BodyCaptureDialog::Update));
        dialog_->AddEnableOKButtonHandler(NXOpen::make_callback(this, &BodyCaptureDialog::EnableOK));
        dialog_->AddApplyHandler(NXOpen::make_callback(this, &BodyCaptureDialog::Apply));
        dialog_->AddOkHandler(NXOpen::make_callback(this, &BodyCaptureDialog::Ok));
        dialog_->AddCancelHandler(NXOpen::make_callback(this, &BodyCaptureDialog::Cancel));
        dialog_->AddCloseHandler(NXOpen::make_callback(this, &BodyCaptureDialog::Cancel));
    }

    void Launch() { dialog_->Launch(); }

private:
    std::unique_ptr<NXOpen::BlockStyler::BlockDialog> dialog_;
    AppState state_;
    std::wstring initialCategory_;
    tag_t workPart_ = NULL_TAG;
    NXOpen::BlockStyler::StringBlock *name_ = nullptr, *category_ = nullptr, *spec_ = nullptr;
    NXOpen::BlockStyler::SelectObject* selection_ = nullptr;
    NXOpen::BlockStyler::SpecifyPoint* point_ = nullptr;
    NXOpen::BlockStyler::SpecifyOrientation* orientation_ = nullptr;
    NXOpen::BlockStyler::DoubleBlock* coordinates_[3]{};
    NXOpen::BlockStyler::UIBlock* status_ = nullptr;
    NXOpen::BlockStyler::UIBlock *captureImage_ = nullptr, *clearImage_ = nullptr, *imageStatus_ = nullptr;
    CapturePreviewImage preview_;
    std::vector<tag_t> bodies_;
    bool initialized_ = false, shown_ = false, busy_ = false, closing_ = false, savedAny_ = false;

    struct BusyScope
    {
        bool& busy;
        bool previous;
        explicit BusyScope(bool& value) : busy(value), previous(value) { busy = true; }
        ~BusyScope() { busy = previous; }
    };

    template<class T> T* Find(const char* id)
    {
        auto* block = dynamic_cast<T*>(dialog_->TopBlock()->FindBlock(id));
        if (!block) throw std::runtime_error(std::string("入库对话框缺少控件：") + id);
        return block;
    }

    static void Label(NXOpen::BlockStyler::UIBlock* block, const std::wstring& text)
    {
        std::unique_ptr<NXOpen::BlockStyler::PropertyList> properties(block->GetProperties());
        properties->SetString("Label", ToAnsi(text).c_str());
    }

    void Error(const char* text) noexcept
    {
        try
        {
            std::string message = std::string("[StandardPartsLibrary] Capture: ") + text + "\n";
            UF_print_syslog(message.data(), false);
            NXOpen::UI::GetUI()->NXMessageBox()->Show("选择体加入标准件库",
                NXOpen::NXMessageBox::DialogTypeError, text);
        }
        catch (...) {}
    }

    void SyncBasePoint(const NXOpen::Point3d& point, bool updatePointBlock)
    {
        // Caller holds busy_ while synchronizing native blocks.
        coordinates_[0]->SetValue(point.X);
        coordinates_[1]->SetValue(point.Y);
        coordinates_[2]->SetValue(point.Z);
        if (updatePointBlock) point_->SetPoint(point);
        orientation_->SetOrigin(point);
        orientation_->SetOriginSpecified(true);
    }

    std::array<double, 6> BaseDirections()
    {
        const auto x = orientation_->XAxis();
        const auto y = orientation_->YAxis();
        return {x.X,x.Y,x.Z,y.X,y.Y,y.Z};
    }

    void Initialize() noexcept
    {
        shown_ = false;
        initialized_ = false;
        BusyScope guard(busy_);
        try
        {
            name_ = Find<NXOpen::BlockStyler::StringBlock>("captureName");
            category_ = Find<NXOpen::BlockStyler::StringBlock>("captureCategory");
            spec_ = Find<NXOpen::BlockStyler::StringBlock>("captureSpec");
            selection_ = Find<NXOpen::BlockStyler::SelectObject>("captureBodies");
            point_ = Find<NXOpen::BlockStyler::SpecifyPoint>("capturePoint");
            orientation_ = Find<NXOpen::BlockStyler::SpecifyOrientation>("captureOrientation");
            const char* ids[] = {"captureX", "captureY", "captureZ"};
            for (int axis = 0; axis < 3; ++axis)
            {
                coordinates_[axis] = Find<NXOpen::BlockStyler::DoubleBlock>(ids[axis]);
                coordinates_[axis]->SetValue(0.0);
            }
            status_ = Find<NXOpen::BlockStyler::UIBlock>("captureStatus");
            captureImage_ = Find<NXOpen::BlockStyler::UIBlock>("captureImage");
            clearImage_ = Find<NXOpen::BlockStyler::UIBlock>("clearCaptureImage");
            imageStatus_ = Find<NXOpen::BlockStyler::UIBlock>("captureImageStatus");
            auto* rootLabel = Find<NXOpen::BlockStyler::UIBlock>("captureRoot");
            Label(rootLabel, L"库目录：" + state_.libraryRootOverride.wstring());
            Label(status_, L"确定入库并关闭；应用入库后可继续选择。只复制实体，不保存来源模型。");
            name_->SetValue("");
            category_->SetValue(ToAnsi(initialCategory_.empty() ? L"用户自定义" : initialCategory_).c_str());
            spec_->SetValue(ToAnsi(L"默认").c_str());
            point_->SetPoint(NXOpen::Point3d(0, 0, 0));
            orientation_->SetOrigin(NXOpen::Point3d(0, 0, 0));
            orientation_->SetXAxis(NXOpen::Vector3d(1, 0, 0));
            orientation_->SetYAxis(NXOpen::Vector3d(0, 1, 0));
            orientation_->SetWCSCoordinates(false);
            std::vector<NXOpen::Selection::MaskTriple> masks;
            masks.emplace_back(UF_solid_type, UF_solid_body_subtype, UF_UI_SEL_FEATURE_BODY);
            selection_->SetSelectionFilter(NXOpen::Selection::SelectionActionClearAndEnableSpecific, masks);
            LoadIndex(&state_);
            initialized_ = true;
        }
        catch (const NXOpen::NXException& ex) { Error(ex.Message()); }
        catch (const std::exception& ex) { Error(ex.what()); }
        catch (...) { Error("初始化入库对话框失败。"); }
    }

    void DialogShown() noexcept
    {
        if (!initialized_) return;
        BusyScope guard(busy_);
        try
        {
            shown_ = true;
            clearImage_->SetEnable(!preview_.Path().empty());
            name_->Focus();
            UF_print_syslog("[StandardPartsLibrary] Native capture dialog shown\n", false);
        }
        catch (const NXOpen::NXException& ex) { Error(ex.Message()); }
        catch (const std::exception& ex) { Error(ex.what()); }
        catch (...) { Error("显示入库对话框失败。"); }
    }

    int Update(NXOpen::BlockStyler::UIBlock* block) noexcept
    {
        if (!shown_ || busy_ || closing_) return 0;
        BusyScope guard(busy_);
        try
        {
            if (block == captureImage_)
            {
                if (UF_ASSEM_ask_work_part() != workPart_)
                    throw std::runtime_error("工作部件已改变，请关闭后重新入库。");
                std::vector<tag_t> highlights = bodies_;
                for (auto* object : point_->GetSelectedObjects())
                    if (object) highlights.push_back(object->Tag());
                struct OrientationVisibilityScope
                {
                    NXOpen::BlockStyler::SpecifyOrientation* block;
                    bool shown;
                    ~OrientationVisibilityScope()
                    {
                        try { block->SetShow(shown); }
                        catch (const NXOpen::NXException& ex)
                        {
                            char message[1024]{};
                            strncpy_s(message, ex.Message(), _TRUNCATE);
                            UF_print_syslog(message, false);
                        }
                        catch (...) { UF_print_syslog("[StandardPartsLibrary] Capture orientation visibility restore failed\n", false); }
                    }
                } visibility{orientation_, orientation_->Show()};
                // Hiding only handles leaves the triad arcs/labels and active
                // on-screen numeric editor in exported images. Hide the block.
                orientation_->SetShow(false);
                preview_.Capture(highlights);
                clearImage_->SetEnable(true);
                Label(imageStatus_, L"已抓取当前视图；入库时保存为同名 PNG，可再次抓取替换。");
            }
            else if (block == clearImage_)
            {
                preview_.Clear();
                clearImage_->SetEnable(false);
                Label(imageStatus_, L"已清除图片；本次仅保存实体，也可重新抓取。");
            }
            else if (block == selection_)
            {
                bodies_.clear();
                std::vector<tag_t> selected;
                for (auto* object : selection_->GetSelectedObjects())
                    if (object) selected.push_back(object->Tag());
                std::wstring error;
                if (!selected.empty() && !ValidateCaptureBodies(selected, workPart_, error))
                    throw std::runtime_error(ToAnsi(error));
                bodies_ = std::move(selected);
            }
            else if (block == point_)
            {
                SyncBasePoint(point_->Point(), false);
            }
            else if (block == orientation_)
            {
                CaptureFrameAxes(BaseDirections());
                SyncBasePoint(orientation_->Origin(), true);
            }
            else if (block == coordinates_[0] || block == coordinates_[1] || block == coordinates_[2])
            {
                SyncBasePoint(NXOpen::Point3d(coordinates_[0]->Value(), coordinates_[1]->Value(),
                    coordinates_[2]->Value()), true);
            }
            return 0;
        }
        catch (const NXOpen::NXException& ex) { Error(ex.Message()); }
        catch (const std::exception& ex) { Error(ex.what()); }
        catch (...) { Error("更新入库输入失败。"); }
        return 1;
    }

    bool EnableOK() noexcept
    {
        return initialized_ && shown_ && !busy_ && !closing_ && (!bodies_.empty() || savedAny_);
    }

    int Apply() noexcept
    {
        if (!initialized_ || !shown_ || busy_ || closing_) return 1;
        if (bodies_.empty() && savedAny_) return 0; // Do not repeat an already applied export.
        BusyScope guard(busy_);
        try
        {
            if (UF_ASSEM_ask_work_part() != workPart_)
                throw std::runtime_error("工作部件已改变，请关闭后重新入库。");
            const CaptureMetadata metadata{FromAnsi(name_->Value().GetLocaleText()),
                FromAnsi(category_->Value().GetLocaleText()), FromAnsi(spec_->Value().GetLocaleText())};
            std::array<double, 3> point{};
            for (int axis = 0; axis < 3; ++axis) point[axis] = coordinates_[axis]->Value();
            fs::path saved;
            std::wstring error;
            UF_print_syslog("[StandardPartsLibrary] Capture export begin\n", false);
            if (!CaptureBodiesToLibrary(&state_, bodies_, point, saved, error, &metadata, preview_.Path(), BaseDirections()))
                throw std::runtime_error(ToAnsi(error));
            bodies_.clear();
            savedAny_ = true;
            preview_.Clear();
            clearImage_->SetEnable(false);
            UF_print_syslog("[StandardPartsLibrary] Capture export and index committed\n", false);
            // Guard remains active while NX dispatches selection-clear updates.
            selection_->SetSelectedObjects({});
            Label(status_, L"已入库：" + saved.wstring());
            selection_->Focus();
            return 0;
        }
        catch (const NXOpen::NXException& ex) { Error(ex.Message()); }
        catch (const std::exception& ex) { Error(ex.what()); }
        catch (...) { Error("选择体入库失败。"); }
        return 1;
    }

    int Ok() noexcept
    {
        const int result = Apply();
        if (result == 0) closing_ = true;
        return result;
    }

    int Cancel() noexcept { closing_ = true; shown_ = false; return 0; }
};

void ShowBodyCapture(const fs::path& root, const std::wstring& category)
{
    BodyCaptureDialog dialog(root, category);
    dialog.Launch();
}
