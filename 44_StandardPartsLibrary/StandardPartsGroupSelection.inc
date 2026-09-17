// The main combo selects named groups; its item data always identifies the
// actual resolved PRT, including while a new group is being validated.
std::wstring SelectedParameterGroup(AppState* state)
{
    const auto row = SendDlgItemMessageW(state->window, ID_SPEC, CB_GETCURSEL, 0, 0);
    return row >= 0 && static_cast<std::size_t>(row) < state->specificationGroups.size()
        ? state->specificationGroups[row] : L"";
}

bool GroupMatchesValues(const ParameterGroup& group, const std::vector<LibraryParameter>& schema,
    const std::vector<std::wstring>& original)
{
    try
    {
        const auto values = ParameterGroupValues(group, schema, original);
        for (std::size_t i = 0; i < schema.size(); ++i)
            if ((schema[i].mode == L"number" || schema[i].mode == L"list") &&
                ParseParameterNumber(values[i]) != ParseParameterNumber(original[i])) return false;
        return true;
    }
    catch (const std::exception&) { return false; } // A stale group remains available for repair.
}

void RefreshSpecifications(AppState* state, std::size_t preferred, const std::wstring& groupName)
{
    const HWND combo = GetDlgItem(state->window, ID_SPEC);
    if (!combo) return;
    const auto oldIndex = preferred == SIZE_MAX ? SelectedIndex(state) : preferred;
    const auto oldGroup = groupName.empty() ? SelectedParameterGroup(state) : groupName;
    const auto oldText = GetText(state->window, ID_SPEC);
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    state->specificationGroups.clear();
    const auto family = SelectedFamilyIndex(state);
    if (family >= state->items.size()) return;
    const auto sameFamily = [&](std::size_t index) {
        return index < state->items.size() && state->items[index].category == state->items[family].category &&
            state->items[index].name == state->items[family].name &&
            state->items[index].parameterized == state->items[family].parameterized;
    };
    const auto current = sameFamily(oldIndex) ? oldIndex : family;
    const auto add = [&](const std::wstring& label, std::size_t index, const std::wstring& group) {
        const auto row = SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
        if (row == CB_ERR || row == CB_ERRSPACE) return;
        SendMessageW(combo, CB_SETITEMDATA, row, static_cast<LPARAM>(index));
        state->specificationGroups.push_back(group);
    };
    if (state->items[current].parameterized)
    {
        try
        {
            std::wstring error;
            const auto model = ResolvedModel(state, state->items[current], error);
            const auto groups = model.empty() ? std::vector<ParameterGroup>{} : ReadParameterGroups(model);
            if (!groups.empty())
            {
                const auto schema = ReadParameterSchema(model);
                const auto values = SpecificationValues(model, state->items[current].specification, schema);
                int selected = -1;
                for (const auto& group : groups)
                {
                    const auto row = static_cast<int>(state->specificationGroups.size());
                    add(group.name, current, group.name);
                    if (!schema.empty() && GroupMatchesValues(group, schema, values) &&
                        (selected < 0 || group.name == oldGroup)) selected = row;
                }
                if (selected < 0)
                {
                    selected = static_cast<int>(state->specificationGroups.size());
                    add(L"当前尺寸（自定义）", current, L"");
                }
                SendMessageW(combo, CB_SETCURSEL, selected, 0);
                return;
            }
        }
        catch (const std::exception& ex)
        {
            UF_print_syslog(const_cast<char*>(ex.what()), false);
            SetStatus(state, L"参数组读取失败，请在选择参数中检查参数定义。");
        }
    }
    int selected = 0;
    for (std::size_t index = 0; index < state->items.size(); ++index)
    {
        if (!sameFamily(index)) continue;
        const auto row = static_cast<int>(state->specificationGroups.size());
        add(state->items[index].specification, index, L"");
        if (index == current || (preferred == SIZE_MAX && !sameFamily(oldIndex) &&
            state->items[index].specification == oldText)) selected = row;
    }
    SendMessageW(combo, CB_SETCURSEL, selected, 0);
}
