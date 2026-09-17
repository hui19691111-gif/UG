void PostLibraryDialogCancel(AppState* state);

// NX resolves loaded parts by basename. Never use a same-named part from
// another directory as a parameter template, or edit/close that user part.
tag_t LoadedParameterPart(const fs::path& path)
{
    const auto part=UF_PART_ask_part_tag(ToAnsi(path.wstring()).c_str());
    if(part==NULL_TAG)return part;
    char actual[MAX_FSPEC_BUFSIZE]{};CheckPreviewUf(UF_PART_ask_part_name(part,actual));
    std::error_code ec;
    if(!fs::equivalent(path,fs::path(FromAnsi(actual)),ec)||ec)
        throw std::runtime_error(ToAnsi(L"NX 已加载其他目录中的同名部件，请为模板文件使用唯一名称："+path.filename().wstring()));
    return part;
}

fs::path ParameterVariantPath(const fs::path& directory,const std::wstring& specification)
{
    return directory/(directory.filename().wstring()+L"_"+specification+L".prt");
}

std::vector<std::pair<std::wstring, tag_t>> PartNumericExpressions(tag_t part)
{
    int count = 0;
    tag_t* tags = nullptr;
    CheckPreviewUf(UF_MODL_ask_exps_of_part(part, &count, &tags));
    struct TagsScope { tag_t* tags; ~TagsScope() { UF_free(tags); } } scope{tags};
    std::vector<std::pair<std::wstring, tag_t>> result;
    for (int i = 0; i < count; ++i)
    {
        char* text = nullptr;
        CheckPreviewUf(UF_MODL_ask_exp_tag_string(tags[i], &text));
        const std::string expression = text ? text : "";
        UF_free(text);
        result.emplace_back(Trim(FromAnsi(expression.substr(0, expression.find('=')))), tags[i]);
    }
    return result;
}

void SelectLibrarySpecification(AppState* state, std::size_t index, const std::wstring& groupName = {})
{
    if (index >= state->items.size() || !InCurrentLibrary(state, state->items[index])) return;
    state->selectedCategory = state->items[index].category;
    SetText(state->window, ID_SEARCH, L"");
    RefreshCategories(state);
    RefreshList(state);
    for (std::size_t row = 0; row < state->visible.size(); ++row)
        if (state->items[state->visible[row]].name == state->items[index].name)
        {
            SelectContextRow(state, static_cast<int>(row));
            RefreshSpecifications(state, index, groupName);
            UpdatePreview(state);
            break;
        }
}

#include "StandardPartsParameterDefinition.inc"

// Save a new parametric part; the template and existing specifications are immutable.
std::size_t SaveParameterVariant(AppState* state, std::size_t index,
    const std::vector<LibraryParameter>& schema, const std::vector<std::wstring>& inputs,
    const std::wstring& specification, bool validateOnly = false)
{
    if (index >= state->items.size() || state->libraryFilter != 2 ||
        !InCurrentLibrary(state, state->items[index]) || schema.empty() || inputs.size() != schema.size())
        throw std::runtime_error(ToAnsi(L"请在本地有参选择有效图档。"));
    if (specification.empty() || specification != SanitizeFileName(specification) ||
        specification.find_first_of(L"\t\r\n") != std::wstring::npos)
        throw std::runtime_error(ToAnsi(L"请输入有效的新规格名称。"));
    std::wstring error;
    const auto item = state->items[index];
    const auto source = ResolvedModel(state, item, error);
    if (source.empty()) throw std::runtime_error(ToAnsi(error));
    const auto original = SpecificationValues(source, item.specification, schema);
    ValidateParameterInputs(schema, inputs, original);
    for (const auto& existing : state->items)
        if (existing.parameterized && existing.name == item.name && existing.category == item.category &&
            existing.specification == specification)
            throw std::runtime_error(ToAnsi(L"同名规格已存在，请从规格列表选择或使用新名称。"));
    const auto target = ParameterVariantPath(source.parent_path(),specification);
    if (fs::exists(target) || !FindSidecarPreview(target).empty())
        throw std::runtime_error(ToAnsi(L"目标图档或配图已经存在，未覆盖。"));
    LoadedParameterPart(target);
    const auto tablePath = source.parent_path() / L"parameters.tsv";
    std::wstring oldTable;
    if (!ReadUtf16File(tablePath, oldTable)) throw std::runtime_error(ToAnsi(L"无法读取规格参数表。"));
    auto rows = ReadParameterRows(tablePath);
    if (validateOnly) return index;
    const auto countBefore = state->items.size();
    const auto temporary = fs::path(tablePath.wstring() + L".tmp");
    bool created = false, tableChanged = false;
    try
    {
        fs::copy_file(source, target);
        created = true;
        auto values = inputs;
        {
            struct PartScope
            {
                tag_t part = NULL_TAG;
                ~PartScope()
                {
                    if (part != NULL_TAG && UF_PART_close(part, 0, 1) != 0)
                        UF_print_syslog("[StandardPartsLibrary] Parameter part close failed\n", false);
                }
            } part;
            UF_PART_load_status_t status{};
            const int code = UF_PART_open_quiet(ToAnsi(target.wstring()).c_str(), &part.part, &status);
            UF_PART_free_load_status(&status);
            CheckPreviewUf(code);
            // Edit/save only the new part without switching the user's work part.
            const auto expressions = PartNumericExpressions(part.part);
            for (std::size_t i = 0; i < schema.size(); ++i)
            {
                const auto found = std::find_if(expressions.begin(), expressions.end(),
                    [&](const auto& expression) { return expression.first == schema[i].expression; });
                if (found == expressions.end()) throw std::runtime_error(ToAnsi(L"PRT 缺少参数表指定的 NX 表达式。"));
                if (schema[i].mode == L"number" || schema[i].mode == L"list")
                {
                    auto* expression = dynamic_cast<NXOpen::Expression*>(NXOpen::NXObjectManager::Get(found->second));
                    if (!expression) throw std::runtime_error(ToAnsi(L"无法读取 NX 数值表达式。"));
                    expression->SetFormula(ToAnsi(inputs[i]).c_str());
                }
            }
            CheckPreviewUf(UF_MODL_update());
            for (std::size_t i = 0; i < schema.size(); ++i)
            {
                const auto found = std::find_if(expressions.begin(), expressions.end(),
                    [&](const auto& expression) { return expression.first == schema[i].expression; });
                auto* expression = dynamic_cast<NXOpen::Expression*>(NXOpen::NXObjectManager::Get(found->second));
                if (!expression) throw std::runtime_error(ToAnsi(L"无法读取更新后的参数。"));
                const double value = expression->GetValueUsingUnits(NXOpen::Expression::UnitsOptionExpression);
                values[i] = ParameterNumber(value);
            }
            auto* targetPart = dynamic_cast<NXOpen::Part*>(NXOpen::NXObjectManager::Get(part.part));
            if (!targetPart) throw std::runtime_error(ToAnsi(L"无法保存新规格部件。"));
            std::unique_ptr<NXOpen::PartSaveStatus> saved(targetPart->Save(
                NXOpen::BasePart::SaveComponentsFalse, NXOpen::BasePart::CloseAfterSaveFalse));
            if (saved && (saved->NumberUnsavedParts() != 0 || saved->NumberUnsavedObjects() != 0))
                throw std::runtime_error(ToAnsi(L"NX 未能完整保存新规格，请检查模型参数。"));
        }
        std::vector<std::wstring> newRow(rows.front().size());
        newRow[0] = specification;
        for (std::size_t i = 0; i < schema.size(); ++i)
        {
            const auto column = std::find(rows.front().begin(), rows.front().end(), schema[i].expression);
            if (column == rows.front().end()) throw std::runtime_error(ToAnsi(L"规格表缺少表达式列。"));
            newRow[static_cast<std::size_t>(column - rows.front().begin())] = values[i];
        }
        std::wstring updated = oldTable;
        if (!updated.empty() && updated.back() != L'\n') updated += L"\r\n";
        for (std::size_t i = 0; i < newRow.size(); ++i)
            updated += (i ? L"\t" : L"") + newRow[i];
        updated += L"\r\n";
        const auto backup = LibraryRoot(state) / L"backup" / (L"parameters_" + std::to_wstring(GetTickCount64()));
        fs::create_directories(backup);
        fs::copy_file(tablePath, backup / L"parameters.tsv");
        if (!WriteUtf16File(temporary, updated) || !MoveFileExW(temporary.c_str(), tablePath.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error(ToAnsi(L"保存参数表失败。"));
        tableChanged = true;
        state->items.push_back({L"param_" + std::to_wstring(GetTickCount64()), item.name, item.category,
            fs::relative(target, LibraryRoot(state)).wstring(), specification, true});
        if (!SaveIndex(state, error)) throw std::runtime_error(ToAnsi(error));
        return state->items.size() - 1;
    }
    catch (...)
    {
        state->items.resize(countBefore);
        if (tableChanged && !WriteUtf16File(tablePath, oldTable))
            UF_print_syslog("[StandardPartsLibrary] Parameter table rollback failed; recover backup\n", false);
        std::error_code ec;
        if (created) fs::remove(target, ec);
        fs::remove(temporary, ec);
        throw;
    }
}

// Parameter choice during insertion is generic: no screw-specific D/L naming.
// An existing match uses its original PRT without saving or clearing NX undo marks.
std::size_t ResolveInsertionParameters(AppState* state, std::size_t index,
    const std::vector<LibraryParameter>& schema, const std::vector<std::wstring>& inputs,
    bool validateOnly = false)
{
    if (state->libraryFilter != 2 || index >= state->items.size() || !state->items[index].parameterized)
        throw std::runtime_error(ToAnsi(L"请先选择本地有参标准件。"));
    const auto item = state->items[index];
    std::wstring error;
    const auto source = ResolvedModel(state, item, error);
    if (source.empty()) throw std::runtime_error(ToAnsi(error));
    const auto currentSchema = ReadParameterSchema(source);
    if (currentSchema.size() != schema.size())
        throw std::runtime_error(ToAnsi(L"参数定义已变化，请重新打开选择参数。"));
    for (std::size_t i = 0; i < schema.size(); ++i)
        if (currentSchema[i].expression != schema[i].expression || currentSchema[i].mode != schema[i].mode ||
            currentSchema[i].choices != schema[i].choices)
            throw std::runtime_error(ToAnsi(L"参数定义已变化，请重新打开选择参数。"));
    ValidateParameterInputs(schema, inputs, SpecificationValues(source, item.specification, schema));
    const auto matches = [&](std::size_t candidate)
    {
        const auto& existing = state->items[candidate];
        if (!existing.parameterized || existing.name != item.name || existing.category != item.category) return false;
        const auto model = ResolvedModel(state, existing, error);
        if (model.empty() || model.parent_path() != source.parent_path()) return false;
        const auto values = SpecificationValues(model, existing.specification, schema);
        for (std::size_t i = 0; i < schema.size(); ++i)
            if (schema[i].mode != L"derived" && ParseParameterNumber(values[i]) != ParseParameterNumber(inputs[i])) return false;
        return true;
    };
    if (matches(index)) return index;
    for (std::size_t candidate = 0; candidate < state->items.size(); ++candidate)
        if (candidate != index && matches(candidate)) return candidate;

    std::wstring signature, label = L"参数";
    for (std::size_t i = 0; i < schema.size(); ++i)
    {
        if (schema[i].mode == L"derived") continue;
        const auto value = ParameterNumber(ParseParameterNumber(inputs[i]));
        signature += schema[i].expression + L"=" + value + L";";
        if (schema[i].mode == L"number" || schema[i].mode == L"list") label += L"_" + schema[i].expression + value;
    }
    std::uint64_t hash = 14695981039346656037ULL;
    for (wchar_t ch : signature) { hash ^= static_cast<std::uint16_t>(ch); hash *= 1099511628211ULL; }
    std::wostringstream suffix; suffix << std::hex << hash;
    label = SanitizeFileName(label.substr(0, 48)) + L"_" + suffix.str();
    auto name = label;
    for (int ordinal = 2;; ++ordinal)
    {
        const auto target = ParameterVariantPath(source.parent_path(), name);
        const bool used = std::any_of(state->items.begin(),state->items.end(),[&](const auto& other)
            { return other.parameterized && other.name == item.name && other.category == item.category && other.specification == name; });
        if (!used && !fs::exists(target) && FindSidecarPreview(target).empty()) break;
        name = label + L"_" + std::to_wstring(ordinal);
    }
    const auto saved = SaveParameterVariant(state,index,schema,inputs,name,validateOnly);
    return validateOnly ? SIZE_MAX : saved;
}

void SelectMainParameterGroup(AppState* state, const std::wstring& name)
{
    const auto index = SelectedIndex(state);
    const auto model = ParameterGroupModel(state, index);
    const auto groups = ReadParameterGroups(model);
    const auto found = std::find_if(groups.begin(), groups.end(), [&](const auto& group) { return group.name == name; });
    if (found == groups.end()) throw std::runtime_error(ToAnsi(L"参数组已经变化，请重新选择。"));
    const auto schema = ReadParameterSchema(model);
    const auto inputs = ParameterGroupValues(*found, schema,
        SpecificationValues(model, state->items[index].specification, schema));
    const auto match = ResolveInsertionParameters(state, index, schema, inputs, true);
    if (match != SIZE_MAX)
        SelectLibrarySpecification(state, match, name);
    else if (state->embedded)
    {
        auto request = std::make_shared<ParameterSaveRequest>();
        request->root = LibraryRoot(state);
        request->itemId = state->items[index].id;
        request->groupName = name;
        request->schema = schema;
        request->inputs = inputs;
        request->forInsertion = true;
        if (state->prepareParameterSave) state->prepareParameterSave(state->orientationContext, *request);
        state->requestedParameterSave = request;
        // Saving an NX part clears modal undo marks. Use the existing deferred
        // save/reopen path, retaining the chosen group and placement settings.
        try { PostLibraryDialogCancel(state); }
        catch (...) { state->requestedParameterSave.reset(); throw; }
        return;
    }
    else SelectLibrarySpecification(state, ResolveInsertionParameters(state, index, schema, inputs), name);
    SetStatus(state, L"已选择参数组：" + name);
}

void HandleSpecificationSelection(AppState* state) noexcept
{
    const auto previous = SelectedIndex(state);
    try
    {
        const auto group = SelectedParameterGroup(state);
        if (g_parameterWindow && IsWindow(g_parameterWindow)) DestroyWindow(g_parameterWindow);
        if (group.empty())
        {
            UpdatePreview(state);
        }
        else SelectMainParameterGroup(state, group);
        return;
    }
    catch (const NXOpen::NXException& ex)
    { UF_print_syslog(const_cast<char*>(ex.Message()), false); MessageBoxW(state->window, FromAnsi(ex.Message()).c_str(), L"选择参数组", MB_OK | MB_ICONERROR); }
    catch (const std::exception& ex)
    { UF_print_syslog(const_cast<char*>(ex.what()), false); MessageBoxW(state->window, FromAnsi(ex.what()).c_str(), L"选择参数组", MB_OK | MB_ICONERROR); }
    catch (...) { UF_print_syslog("[StandardPartsLibrary] Group selection failed\n", false); }
    try { RefreshSpecifications(state, previous); UpdatePreview(state); }
    catch (...) { UF_print_syslog("[StandardPartsLibrary] Group selection rollback failed\n", false); }
}

struct ParameterWindowState
{
    AppState* app = nullptr;
    std::size_t index = SIZE_MAX;
    std::wstring itemId;
    fs::path root;
    std::vector<LibraryParameter> schema;
    std::vector<std::wstring> values;
    std::vector<ParameterGroup> groups;
    int selectedGroup = -1;
    bool forInsertion = false;
};

constexpr int ParameterGroupList = 3110, ParameterGroupName = 3111,
    ParameterGroupSave = 3112, ParameterGroupDelete = 3113;

void CheckParameterWindowItem(const ParameterWindowState* context)
{
    if (LibraryRoot(context->app) != context->root || context->app->libraryFilter != 2 ||
        context->index >= context->app->items.size() ||
        context->app->items[context->index].id != context->itemId)
        throw std::runtime_error(ToAnsi(L"图档列表已经变化，请关闭参数窗口后重新选择图档。"));
}

std::vector<std::wstring> ParameterWindowInputs(HWND window, const ParameterWindowState* context)
{
    std::vector<std::wstring> values;
    for (std::size_t i = 0; i < context->values.size(); ++i)
        values.push_back(Trim(GetText(window, 3000 + static_cast<int>(i))));
    return values;
}

void SetParameterWindowInputs(HWND window, const ParameterWindowState* context, const std::vector<std::wstring>& values)
{
    for (std::size_t i = 0; i < context->schema.size(); ++i)
    {
        const auto control = GetDlgItem(window, 3000 + static_cast<int>(i));
        if (context->schema[i].mode == L"list")
        {
            const auto& choices = context->schema[i].choices;
            for (std::size_t j = 0; j < choices.size(); ++j)
                if (ParseParameterNumber(choices[j]) == ParseParameterNumber(values[i]))
                    SendMessageW(control, CB_SETCURSEL, j, 0);
        }
        else SetWindowTextW(control, values[i].c_str());
    }
}

void RefreshParameterGroupList(HWND window, ParameterWindowState* context, const std::wstring& name = {})
{
    const auto combo = GetDlgItem(window, ParameterGroupList);
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"当前规格尺寸"));
    context->selectedGroup = -1;
    for (std::size_t i = 0; i < context->groups.size(); ++i)
    {
        SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(context->groups[i].name.c_str()));
        if (context->groups[i].name == name) context->selectedGroup = static_cast<int>(i);
    }
    SendMessageW(combo, CB_SETCURSEL, context->selectedGroup + 1, 0);
    SetText(window, ParameterGroupName, name);
    EnableWindow(GetDlgItem(window, ParameterGroupDelete), context->selectedGroup >= 0);
}

void CompleteParameterSave(ParameterSaveRequest& request) noexcept
{
    try
    {
        AppState state;
        state.libraryRootOverride = request.root;
        state.libraryFilter = 2;
        LoadIndex(&state);
        const auto item = std::find_if(state.items.begin(), state.items.end(),
            [&](const auto& current) { return current.id == request.itemId; });
        if (item == state.items.end()) throw std::runtime_error(ToAnsi(L"原图档已经变化，请重新选择。"));
        const auto saved = request.forInsertion
            ? ResolveInsertionParameters(&state, item - state.items.begin(), request.schema, request.inputs)
            : SaveParameterVariant(&state, item - state.items.begin(), request.schema, request.inputs, request.specification);
        request.savedItemId = state.items[saved].id;
    }
    catch (const NXOpen::NXException& ex)
    { UF_print_syslog(const_cast<char*>(ex.Message()),false); MessageBoxW(GetForegroundWindow(),FromAnsi(ex.Message()).c_str(),L"另存规格",MB_OK|MB_ICONERROR); }
    catch (const std::exception& ex)
    { MessageBoxW(GetForegroundWindow(),FromAnsi(ex.what()).c_str(),L"另存规格",MB_OK|MB_ICONERROR); }
    catch (...) { UF_print_syslog("[StandardPartsLibrary] Deferred parameter save failed\n",false); }
}

LRESULT CALLBACK ParameterWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* context = reinterpret_cast<ParameterWindowState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE)
    {
        context = static_cast<ParameterWindowState*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(context));
    }
    try
    {
        if (message == WM_CREATE && context)
        {
            const auto add = [&](const wchar_t* cls, const std::wstring& text, DWORD style,
                int x, int y, int width, int height, int id)
            {
                const auto control = CreateWindowExW(wcscmp(cls, L"EDIT") == 0 ? WS_EX_CLIENTEDGE : 0,
                    cls, text.c_str(), WS_CHILD | WS_VISIBLE | style, x,y,width,height,window,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
                SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(context->app->font), TRUE);
                return control;
            };
            const auto& item = context->app->items[context->index];
            add(L"STATIC", item.name + L" / " + item.specification, SS_LEFT,16,12,580,24,0);
            add(L"STATIC", L"参数组", 0, 16, 48, 136, 24, 0);
            add(WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_VSCROLL, 164, 44, 414, 230, ParameterGroupList);
            add(L"STATIC", L"组名称", 0, 16, 86, 136, 24, 0);
            const auto groupName = add(L"EDIT", L"", ES_AUTOHSCROLL, 164, 82, 164, 26, ParameterGroupName);
            SendMessageW(groupName, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"例如：A组、B组"));
            add(L"BUTTON", L"保存参数组", BS_PUSHBUTTON, 340, 80, 114, 30, ParameterGroupSave);
            add(L"BUTTON", L"删除选中组", BS_PUSHBUTTON, 466, 80, 112, 30, ParameterGroupDelete);
            add(L"STATIC", L"切换组带入整组尺寸；同名保存更新该组，改名保存为新组。", 0, 16, 116, 568, 24, 0);
            RefreshParameterGroupList(window, context, SelectedParameterGroup(context->app));
            const int labelWidth = context->forInsertion ? 244 : 140;
            add(L"STATIC", L"参数名称",0,16,148,labelWidth,22,0);
            if (!context->forInsertion) add(L"STATIC", L"NX 表达式",0,164,148,100,22,0);
            add(L"STATIC", context->forInsertion ? L"参数值" : L"值（表达式单位）",0,278,148,148,22,0);
            add(L"STATIC", L"输入类型",0,452,148,130,22,0);
            for (std::size_t i = 0; i < context->schema.size(); ++i)
            {
                const auto& parameter = context->schema[i];
                const int y = 176 + static_cast<int>(i)*32;
                add(L"STATIC",parameter.label,0,16,y+4,labelWidth,24,0);
                if (!context->forInsertion) add(L"STATIC",parameter.expression,0,164,y+4,100,24,0);
                if (parameter.mode == L"list")
                {
                    const HWND input = add(WC_COMBOBOXW,L"",CBS_DROPDOWNLIST | WS_VSCROLL,278,y,148,220,3000+static_cast<int>(i));
                    for (std::size_t choice = 0; choice < parameter.choices.size(); ++choice)
                    {
                        SendMessageW(input,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(parameter.choices[choice].c_str()));
                        if (ParseParameterNumber(parameter.choices[choice]) == ParseParameterNumber(context->values[i]))
                            SendMessageW(input,CB_SETCURSEL,choice,0);
                    }
                }
                else add(L"EDIT",context->values[i],ES_AUTOHSCROLL |
                    (parameter.mode == L"number" ? 0 : ES_READONLY),278,y,148,25,3000+static_cast<int>(i));
                add(L"STATIC",parameter.mode == L"derived" ? L"生成时联动" :
                    parameter.mode == L"fixed" ? L"固定尺寸" : L"可修改",0,452,y+4,130,24,0);
            }
            const int bottom = 182 + static_cast<int>(context->schema.size())*32;
            if (context->forInsertion)
            {
                add(L"STATIC",L"按尺寸匹配已有规格；没有匹配项时自动生成并加入规格列表。",0,16,bottom+4,580,25,0);
                add(L"STATIC",L"选用后继续指定插入点和方位；原模板保留。",0,16,bottom+38,580,24,0);
            }
            else
            {
                add(L"STATIC",L"新规格名称",0,16,bottom+4,120,24,0);
                add(L"EDIT",L"",ES_AUTOHSCROLL,164,bottom,262,25,3100);
                add(L"STATIC",L"另存时更新 NX 表达式并生成新图档，原规格保留。",0,16,bottom+38,580,24,0);
            }
            add(L"BUTTON",context->forInsertion ? L"选用此参数" : L"另存规格",BS_DEFPUSHBUTTON,354,bottom+76,108,32,IDOK);
            add(L"BUTTON",L"取消",BS_PUSHBUTTON,476,bottom+76,92,32,IDCANCEL);
            return 0;
        }
        if (message == WM_COMMAND && context)
        {
            if (LOWORD(wParam) == IDCANCEL) { DestroyWindow(window); return 0; }
            if (LOWORD(wParam) == ParameterGroupList && HIWORD(wParam) == CBN_SELCHANGE)
            {
                CheckParameterWindowItem(context);
                const int selected = static_cast<int>(SendMessageW(GetDlgItem(window, ParameterGroupList), CB_GETCURSEL, 0, 0)) - 1;
                try
                {
                    const auto model = ParameterGroupModel(context->app, context->index);
                    CheckCurrentParameterSchema(model, context->schema);
                    const auto values = selected < 0 ? context->values :
                        ParameterGroupValues(context->groups.at(selected), context->schema, context->values);
                    SetParameterWindowInputs(window, context, values);
                    context->selectedGroup = selected;
                    SetText(window, ParameterGroupName, selected < 0 ? L"" : context->groups[selected].name);
                    EnableWindow(GetDlgItem(window, ParameterGroupDelete), selected >= 0);
                }
                catch (...)
                {
                    SendMessageW(GetDlgItem(window, ParameterGroupList), CB_SETCURSEL, context->selectedGroup + 1, 0);
                    throw;
                }
                return 0;
            }
            if (LOWORD(wParam) == ParameterGroupSave)
            {
                CheckParameterWindowItem(context);
                const auto name = Trim(GetText(window, ParameterGroupName));
                SaveParameterGroup(context->app, context->index, name, context->schema, ParameterWindowInputs(window, context));
                context->groups = ReadParameterGroups(ParameterGroupModel(context->app, context->index));
                RefreshParameterGroupList(window, context, name);
                RefreshSpecifications(context->app);
                SetStatus(context->app, L"已保存参数组：" + name + L"，插入时可整组选用。");
                return 0;
            }
            if (LOWORD(wParam) == ParameterGroupDelete && context->selectedGroup >= 0)
            {
                CheckParameterWindowItem(context);
                const auto name = context->groups.at(context->selectedGroup).name;
                if (MessageBoxW(window, (L"删除参数组“" + name + L"”？已有规格和 PRT 保留。").c_str(),
                    L"删除参数组", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) return 0;
                DeleteParameterGroup(context->app, context->index, name);
                context->groups = ReadParameterGroups(ParameterGroupModel(context->app, context->index));
                RefreshParameterGroupList(window, context);
                SetParameterWindowInputs(window, context, context->values);
                RefreshSpecifications(context->app);
                SetStatus(context->app, L"已删除参数组：" + name);
                return 0;
            }
            if (LOWORD(wParam) == IDOK)
            {
                CheckParameterWindowItem(context);
                const auto values = ParameterWindowInputs(window, context);
                if (context->forInsertion)
                {
                    const auto match = ResolveInsertionParameters(context->app,context->index,context->schema,values,true);
                    if (match != SIZE_MAX)
                    {
                        SelectLibrarySpecification(context->app,match);
                        SetStatus(context->app,L"已按参数选用规格："+context->app->items[match].specification);
                        DestroyWindow(window);
                        return 0;
                    }
                }
                auto name = Trim(GetText(window,3100));
                if (name.empty() && !context->forInsertion)
                {
                    const auto d = std::find_if(context->schema.begin(),context->schema.end(),[](const auto& p){return p.expression==L"D";});
                    const auto l = std::find_if(context->schema.begin(),context->schema.end(),[](const auto& p){return p.expression==L"L";});
                    if (d != context->schema.end() && l != context->schema.end())
                        name = L"M" + values[d-context->schema.begin()] + L"x" + values[l-context->schema.begin()];
                }
                if (context->app->embedded)
                {
                    if (!context->forInsertion) SaveParameterVariant(context->app,context->index,context->schema,values,name,true);
                    auto request = std::make_shared<ParameterSaveRequest>();
                    request->root = context->root;
                    request->itemId = context->itemId;
                    request->specification = name;
                    request->schema = context->schema;
                    request->inputs = values;
                    request->forInsertion = context->forInsertion;
                    if (context->selectedGroup >= 0)
                        request->groupName = context->groups.at(context->selectedGroup).name;
                    if (context->app->prepareParameterSave)
                        context->app->prepareParameterSave(context->app->orientationContext,*request);
                    context->app->requestedParameterSave = request;
                    // NX part save clears undo marks. End the current modal
                    // dialog before saving, then reopen at the saved specification.
                    try { PostLibraryDialogCancel(context->app); }
                    catch (...) { context->app->requestedParameterSave.reset(); throw; }
                }
                else
                {
                    const auto saved = context->forInsertion
                        ? ResolveInsertionParameters(context->app,context->index,context->schema,values)
                        : SaveParameterVariant(context->app,context->index,context->schema,values,name);
                    SelectLibrarySpecification(context->app,saved);
                }
                DestroyWindow(window);
                return 0;
            }
        }
        if (message == WM_CLOSE) { DestroyWindow(window); return 0; }
        if (message == WM_NCDESTROY)
        {
            if (g_parameterWindow == window) g_parameterWindow = nullptr;
            SetWindowLongPtrW(window,GWLP_USERDATA,0); delete context;
        }
    }
    catch (const NXOpen::NXException& ex)
    { UF_print_syslog(const_cast<char*>(ex.Message()),false); MessageBoxW(window,FromAnsi(ex.Message()).c_str(),L"参数管理",MB_OK|MB_ICONERROR); }
    catch (const std::exception& ex)
    { UF_print_syslog(const_cast<char*>(ex.what()),false); MessageBoxW(window,FromAnsi(ex.what()).c_str(),L"参数管理",MB_OK|MB_ICONERROR); }
    catch (...) { UF_print_syslog("[StandardPartsLibrary] Parameter callback failed\n",false); }
    return DefWindowProcW(window,message,wParam,lParam);
}

void ShowParameterManager(AppState* state, bool forInsertion)
{
    try
    {
        const auto index = SelectedIndex(state);
        if (state->libraryFilter != 2 || index >= state->items.size()) throw std::runtime_error(ToAnsi(L"请先选择本地有参图档。"));
        if (g_parameterWindow && IsWindow(g_parameterWindow))
        {
            wchar_t className[128]{};
            GetClassNameW(g_parameterWindow, className, static_cast<int>(std::size(className)));
            const auto* opened = wcscmp(className, L"ZhihuiStandardPartsParameters") == 0
                ? reinterpret_cast<ParameterWindowState*>(GetWindowLongPtrW(g_parameterWindow,GWLP_USERDATA)) : nullptr;
            if (opened && opened->forInsertion == forInsertion && opened->itemId == state->items[index].id && opened->root == LibraryRoot(state))
            { ShowWindow(g_parameterWindow,SW_RESTORE); SetForegroundWindow(g_parameterWindow); return; }
            DestroyWindow(g_parameterWindow);
        }
        std::wstring error;
        const auto model = ResolvedModel(state,state->items[index],error);
        if (model.empty()) throw std::runtime_error(ToAnsi(error));
        auto context = std::make_unique<ParameterWindowState>();
        context->app = state; context->index = index;
        context->forInsertion = forInsertion;
        context->itemId = state->items[index].id; context->root = LibraryRoot(state);
        context->schema = ReadParameterSchema(model);
        if (context->schema.empty())
        {
            const auto& item=state->items[index];
            ShowParameterDefinition(state,model,index,item.category,item.name,item.specification);
            return;
        }
        context->values = SpecificationValues(model,state->items[index].specification,context->schema);
        context->groups = ReadParameterGroups(model);
        HMODULE module = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&ShowParameterManager),&module);
        WNDCLASSEXW cls{}; cls.cbSize=sizeof(cls); cls.lpfnWndProc=ParameterWindowProc; cls.hInstance=module;
        cls.hCursor=LoadCursorW(nullptr,IDC_ARROW); cls.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_BTNFACE+1);
        cls.lpszClassName=L"ZhihuiStandardPartsParameters";
        if (!RegisterOwnedWindowClass(cls, state->window)) return;
        RECT rect{}; GetWindowRect(state->window,&rect);
        const int height=332+static_cast<int>(context->schema.size())*32;
        auto* raw = context.release(); // WM_NCDESTROY owns the context from WM_NCCREATE onward.
        const auto window=CreateWindowExW(WS_EX_DLGMODALFRAME,cls.lpszClassName,forInsertion ? L"插入标准件 - 选择参数" : L"本地有参 - 管理参数",
            WS_CAPTION|WS_SYSMENU|WS_POPUP,rect.left+40,rect.top+80,610,height,
            !forInsertion && g_managerWindow ? g_managerWindow : state->window,nullptr,module,raw);
        if (window) { g_parameterWindow = window; ShowWindow(window,SW_SHOW); }
    }
    catch(const NXOpen::NXException& ex) { UF_print_syslog(const_cast<char*>(ex.Message()),false); MessageBoxW(state->window,FromAnsi(ex.Message()).c_str(),L"参数管理",MB_OK|MB_ICONERROR); }
    catch(const std::exception& ex) { MessageBoxW(state->window,FromAnsi(ex.what()).c_str(),L"参数管理",MB_OK|MB_ICONERROR); }
    catch(...) { UF_print_syslog("[StandardPartsLibrary] Parameter manager failed\n",false); }
}

fs::path ReplaceManagedPreview(AppState* state, std::size_t index, const fs::path& image)
{
    if (index >= state->items.size() || !InCurrentLibrary(state, state->items[index]))
        throw std::runtime_error(ToAnsi(L"请先选中当前库内的图档。"));
    std::wstring error;
    auto preview = ResolvedModel(state, state->items[index], error);
    if (preview.empty()) throw std::runtime_error(ToAnsi(error));
    preview.replace_extension(L".png");
    const auto temporary = fs::path(preview.wstring() + L".tmp");
    try
    {
        WritePreviewPng(image, temporary);
        if (fs::exists(preview))
        {
            static unsigned sequence = 0;
            const auto backup = LibraryRoot(state) / L"backup" /
                (L"preview_" + std::to_wstring(GetTickCount64()) + L"_" + std::to_wstring(++sequence));
            fs::create_directories(backup);
            fs::copy_file(preview, backup / preview.filename());
        }
        if (!MoveFileExW(temporary.c_str(), preview.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error(ToAnsi(L"保存当前图档图片失败。"));
    }
    catch (...)
    {
        std::error_code ec;
        fs::remove(temporary, ec);
        throw;
    }
    UpdatePreview(state);
    SetStatus(state, L"已更新当前规格预览图：" + state->items[index].specification);
    return preview;
}

void BrowseManagedPreview(AppState* state, HWND owner)
{
    const auto index = SelectedIndex(state);
    if (index >= state->items.size()) throw std::runtime_error(ToAnsi(L"请先选择标准件及规格。"));
    wchar_t file[32768]{};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrTitle = L"更换当前规格的预览图片";
    dialog.lpstrFilter = L"图片（PNG / JPG / BMP）\0*.png;*.jpg;*.jpeg;*.bmp\0\0";
    dialog.lpstrFile = file;
    dialog.nMaxFile = static_cast<DWORD>(std::size(file));
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&dialog)) ReplaceManagedPreview(state, index, file);
}

void CaptureManagedPreview(AppState* state, HWND owner)
{
    try
    {
        const auto index=SelectedIndex(state);
        if(index>=state->items.size()) throw std::runtime_error(ToAnsi(L"请先选中当前库内的图档。"));
        std::wstring error;
        auto preview=ResolvedModel(state,state->items[index],error);
        if(preview.empty()) throw std::runtime_error(ToAnsi(error));
        preview.replace_extension(L".png");
        if(fs::exists(preview) && MessageBoxW(owner,(L"替换当前图档的配图？\r\n"+preview.wstring()).c_str(),
            CurrentLibraryName(state),MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2)!=IDYES) return;
        CapturePreviewImage image;
        if (state->capturePreview) state->capturePreview(state->orientationContext, image);
        else image.Capture({});
        ReplaceManagedPreview(state, index, image.Path());
    }
    catch(const NXOpen::NXException& ex) { UF_print_syslog(const_cast<char*>(ex.Message()),false); MessageBoxW(owner,FromAnsi(ex.Message()).c_str(),L"抓取图片",MB_OK|MB_ICONERROR); }
    catch(const std::exception& ex) { MessageBoxW(owner,FromAnsi(ex.what()).c_str(),L"抓取图片",MB_OK|MB_ICONERROR); }
    catch(...) { UF_print_syslog("[StandardPartsLibrary] Managed preview failed\n",false); }
}
