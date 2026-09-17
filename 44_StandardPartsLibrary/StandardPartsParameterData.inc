// Display labels, NX expression bindings and input modes are separate from
// specification values, following the local Yanxiu sample's table structure.
struct LibraryParameter
{
    std::wstring expression, label, mode;
    std::vector<std::wstring> choices;
};

struct ParameterSaveRequest
{
    fs::path root;
    std::wstring itemId, specification, savedItemId, groupName;
    std::vector<LibraryParameter> schema;
    std::vector<std::wstring> inputs;
    bool forInsertion = false;
    struct Placement
    {
        bool captured = false, hasPlacement = false, assembly = true, autoTrim = false;
        tag_t workPart = NULL_TAG;
        int mode = 1, pattern = 0;
        double origin[3]{}, matrix[9]{};
        bool axisLocked[3]{};
        std::vector<tag_t> selection, trim;
        std::wstring layer, spacingX, spacingY, countX, countY;
    } placement;
};

std::vector<std::vector<std::wstring>> ReadParameterRows(const fs::path& path)
{
    std::wstring text;
    if (!ReadUtf16File(path, text)) return {};
    std::vector<std::vector<std::wstring>> result;
    std::wistringstream stream(text);
    std::wstring line;
    while (std::getline(stream, line))
    {
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        if (!line.empty() && line.front() != L'#') result.push_back(SplitTabs(line));
    }
    return result;
}

std::vector<LibraryParameter> ReadParameterSchema(const fs::path& model)
{
    const auto rows = ReadParameterRows(model.parent_path() / L"parameter-schema.tsv");
    std::vector<LibraryParameter> result;
    std::set<std::wstring> names;
    for (std::size_t i = 1; i < rows.size(); ++i)
    {
        const auto& row = rows[i];
        if (row.size() < 3 || row[0].empty() || result.size() >= 12 ||
            !names.insert(row[0]).second ||
            row[0].find_first_not_of(L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_") != std::wstring::npos ||
            (row[0][0] >= L'0' && row[0][0] <= L'9') ||
            (row[2] != L"fixed" && row[2] != L"derived" && row[2] != L"number" && row[2] != L"list"))
            throw std::runtime_error(ToAnsi(L"参数定义无效，请检查表达式名、输入类型和重复项。"));
        LibraryParameter parameter{row[0], row[1], row[2], {}};
        if (row.size() >= 4)
        {
            std::wistringstream choices(row[3]);
            std::wstring choice;
            while (std::getline(choices, choice, L';'))
                if (!Trim(choice).empty()) parameter.choices.push_back(Trim(choice));
        }
        if (parameter.mode == L"list" && parameter.choices.empty())
            throw std::runtime_error(ToAnsi(L"列表参数缺少可选值。"));
        result.push_back(std::move(parameter));
    }
    return result;
}

std::wstring ParameterNumber(double value)
{
    std::wostringstream text;
    text.precision(12);
    text << value;
    return text.str();
}

double ParseParameterNumber(const std::wstring& text)
{
    const auto trimmed = Trim(text);
    wchar_t* end = nullptr;
    const double value = wcstod(trimmed.c_str(), &end);
    if (trimmed.empty() || end == trimmed.c_str() || *end != L'\0' || !std::isfinite(value))
        throw std::runtime_error(ToAnsi(L"参数值必须是有限数字。"));
    return value;
}

void ValidateParameterInputs(const std::vector<LibraryParameter>& schema,
    const std::vector<std::wstring>& inputs, const std::vector<std::wstring>& original)
{
    if (schema.empty() || inputs.size() != schema.size() || original.size() != schema.size())
        throw std::runtime_error(ToAnsi(L"参数数量与定义不一致，请重新选择图档。"));
    for (std::size_t i = 0; i < schema.size(); ++i)
    {
        const double value = ParseParameterNumber(inputs[i]);
        if ((schema[i].mode == L"fixed" || schema[i].mode == L"derived") && value != ParseParameterNumber(original[i]))
            throw std::runtime_error(ToAnsi(L"固定尺寸和公式尺寸不能直接修改。"));
        if (schema[i].mode == L"list" && std::none_of(schema[i].choices.begin(), schema[i].choices.end(),
            [&](const std::wstring& choice) { return ParseParameterNumber(choice) == value; }))
            throw std::runtime_error(ToAnsi(L"参数不在当前规格的可选尺寸范围内。"));
    }
}

std::vector<std::wstring> SpecificationValues(const fs::path& model, const std::wstring& specification,
    const std::vector<LibraryParameter>& schema)
{
    const auto rows = ReadParameterRows(model.parent_path() / L"parameters.tsv");
    if (!rows.empty())
        for (std::size_t row = 1; row < rows.size(); ++row)
            if (!rows[row].empty() && rows[row][0] == specification)
            {
                std::vector<std::wstring> values;
                for (const auto& parameter : schema)
                {
                    const auto field = std::find(rows[0].begin(), rows[0].end(), parameter.expression);
                    const auto index = static_cast<std::size_t>(field - rows[0].begin());
                    if (field == rows[0].end() || index >= rows[row].size())
                        throw std::runtime_error(ToAnsi(L"当前规格缺少参数值。"));
                    ParseParameterNumber(rows[row][index]);
                    values.push_back(rows[row][index]);
                }
                return values;
            }
    throw std::runtime_error(ToAnsi(L"参数表中没有当前规格。"));
}

// Named dimension sets belong to a family, independently of generated PRTs.
// Bind by expression name so renaming labels or reordering rows is harmless.
struct ParameterGroup
{
    std::wstring name;
    std::map<std::wstring, std::wstring> inputs;
};

bool ValidParameterGroupName(const std::wstring& name)
{
    return !name.empty() && name.size() <= 80 && name == Trim(name) &&
        name.find_first_of(L"\t\r\n") == std::wstring::npos;
}

std::vector<ParameterGroup> ReadParameterGroups(const fs::path& model)
{
    const auto path = model.parent_path() / L"parameter-groups.tsv";
    if (!fs::exists(path)) return {};
    const auto rows = ReadParameterRows(path);
    if (rows.empty() || rows.front() != std::vector<std::wstring>{L"group", L"expression", L"value"})
        throw std::runtime_error(ToAnsi(L"参数组文件无法读取，请检查 parameter-groups.tsv。"));
    std::vector<ParameterGroup> groups;
    for (std::size_t i = 1; i < rows.size(); ++i)
    {
        const auto& row = rows[i];
        if (row.size() != 3 || !ValidParameterGroupName(row[0]) || row[1].empty() ||
            row[1].find_first_not_of(L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_") != std::wstring::npos)
            throw std::runtime_error(ToAnsi(L"参数组名称或表达式无效。"));
        ParseParameterNumber(row[2]);
        auto found = std::find_if(groups.begin(), groups.end(), [&](const auto& group) { return group.name == row[0]; });
        if (found == groups.end())
        {
            if (groups.size() >= 200) throw std::runtime_error(ToAnsi(L"一个零件族最多保存 200 个参数组。"));
            groups.push_back({row[0], {}});
            found = groups.end() - 1;
        }
        if (found->inputs.size() >= 12 || !found->inputs.emplace(row[1], row[2]).second)
            throw std::runtime_error(ToAnsi(L"参数组存在重复表达式或参数数量超过 12。"));
    }
    return groups;
}

std::vector<std::wstring> ParameterGroupValues(const ParameterGroup& group,
    const std::vector<LibraryParameter>& schema, const std::vector<std::wstring>& original)
{
    auto values = original;
    if (values.size() != schema.size()) throw std::runtime_error(ToAnsi(L"参数定义已变化，请重新打开窗口。"));
    for (std::size_t i = 0; i < schema.size(); ++i)
    {
        if (schema[i].mode != L"number" && schema[i].mode != L"list") continue;
        const auto found = group.inputs.find(schema[i].expression);
        if (found == group.inputs.end())
            throw std::runtime_error(ToAnsi(L"参数组缺少新定义的尺寸：" + schema[i].label + L"。请使用当前规格填写尺寸，再保存同名参数组。"));
        values[i] = found->second;
    }
    // Formula/fixed values always come from the current PRT, never from a preset.
    ValidateParameterInputs(schema, values, original);
    return values;
}

void WriteParameterGroups(AppState* state, const fs::path& model, const std::vector<ParameterGroup>& groups)
{
    if (groups.size() > 200) throw std::runtime_error(ToAnsi(L"一个零件族最多保存 200 个参数组。"));
    std::set<std::wstring> names;
    std::wstring text = L"group\texpression\tvalue\r\n";
    for (const auto& group : groups)
    {
        if (!ValidParameterGroupName(group.name) || !names.insert(group.name).second || group.inputs.empty() || group.inputs.size() > 12)
            throw std::runtime_error(ToAnsi(L"参数组名称重复、为空或参数数量无效。"));
        for (const auto& input : group.inputs)
        {
            if (input.first.empty() || input.first.find_first_not_of(L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_") != std::wstring::npos)
                throw std::runtime_error(ToAnsi(L"参数组表达式无效。"));
            text += group.name + L"\t" + input.first + L"\t" + ParameterNumber(ParseParameterNumber(input.second)) + L"\r\n";
        }
    }
    const auto path = model.parent_path() / L"parameter-groups.tsv";
    const auto temporary = fs::path(path.wstring() + L".tmp");
    try
    {
        if (!WriteUtf16File(temporary, text)) throw std::runtime_error(ToAnsi(L"写入参数组失败。"));
        if (fs::exists(path))
        {
            static unsigned sequence = 0;
            const auto backup = LibraryRoot(state) / L"backup" /
                (L"parameter_groups_" + std::to_wstring(GetTickCount64()) + L"_" + std::to_wstring(++sequence));
            fs::create_directories(backup);
            fs::copy_file(path, backup / path.filename());
        }
        if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error(ToAnsi(L"更新参数组失败。"));
    }
    catch (...)
    {
        std::error_code ec;
        fs::remove(temporary, ec);
        throw;
    }
}

fs::path ParameterGroupModel(AppState* state, std::size_t index)
{
    if (state->libraryFilter != 2 || index >= state->items.size() || !InCurrentLibrary(state, state->items[index]))
        throw std::runtime_error(ToAnsi(L"请选择本地有参标准件。"));
    std::wstring error;
    const auto model = ResolvedModel(state, state->items[index], error);
    if (model.empty()) throw std::runtime_error(ToAnsi(error));
    return model;
}

void CheckCurrentParameterSchema(const fs::path& model, const std::vector<LibraryParameter>& schema)
{
    const auto current = ReadParameterSchema(model);
    if (current.size() != schema.size()) throw std::runtime_error(ToAnsi(L"参数定义已变化，请重新打开参数窗口。"));
    for (std::size_t i = 0; i < schema.size(); ++i)
        if (current[i].expression != schema[i].expression || current[i].mode != schema[i].mode || current[i].choices != schema[i].choices)
            throw std::runtime_error(ToAnsi(L"参数定义已变化，请重新打开参数窗口。"));
}

void SaveParameterGroup(AppState* state, std::size_t index, const std::wstring& name,
    const std::vector<LibraryParameter>& schema, const std::vector<std::wstring>& values)
{
    const auto model = ParameterGroupModel(state, index);
    CheckCurrentParameterSchema(model, schema);
    ValidateParameterInputs(schema, values, SpecificationValues(model, state->items[index].specification, schema));
    ParameterGroup group{name, {}};
    for (std::size_t i = 0; i < schema.size(); ++i)
        if (schema[i].mode == L"number" || schema[i].mode == L"list")
            group.inputs.emplace(schema[i].expression, values[i]);
    if (group.inputs.empty()) throw std::runtime_error(ToAnsi(L"没有可修改的尺寸，无需保存参数组。"));
    auto groups = ReadParameterGroups(model);
    const auto found = std::find_if(groups.begin(), groups.end(), [&](const auto& existing) { return existing.name == name; });
    if (found == groups.end()) groups.push_back(std::move(group));
    else *found = std::move(group);
    WriteParameterGroups(state, model, groups);
}

void DeleteParameterGroup(AppState* state, std::size_t index, const std::wstring& name)
{
    const auto model = ParameterGroupModel(state, index);
    auto groups = ReadParameterGroups(model);
    const auto found = std::find_if(groups.begin(), groups.end(), [&](const auto& group) { return group.name == name; });
    if (found == groups.end()) throw std::runtime_error(ToAnsi(L"参数组已变化，请重新打开窗口。"));
    groups.erase(found);
    WriteParameterGroups(state, model, groups);
}
