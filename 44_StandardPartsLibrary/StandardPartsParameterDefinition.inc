// User-authored parameter templates. Only metadata and copied files are written;
// the user's work part is neither edited nor saved by this workflow.
struct ParameterExpressionInfo
{
    std::wstring name,formula,value;
    bool editable=false;
};

std::vector<ParameterExpressionInfo> ReadTemplateExpressions(const fs::path& path)
{
    if(!fs::exists(path) || !IsPartFile(path))throw std::runtime_error(ToAnsi(L"请选择已保存的有参 PRT。"));
    const auto name=ToAnsi(path.wstring());
    tag_t part=LoadedParameterPart(path);
    const bool loaded=part!=NULL_TAG;
    struct Scope {tag_t& part;bool owned;~Scope(){if(owned&&part!=NULL_TAG)UF_PART_close(part,0,1);}} scope{part,!loaded};
    if(loaded && part!=NULL_TAG && UF_PART_is_modified(part))
        throw std::runtime_error(ToAnsi(L"该部件有未保存修改，请先在 NX 中保存后再读取参数。"));
    if(part==NULL_TAG || UF_PART_is_loaded(name.c_str())!=1)
    {
        UF_PART_load_status_t status{};const int code=UF_PART_open_quiet(name.c_str(),&part,&status);
        UF_PART_free_load_status(&status);CheckPreviewUf(code);
    }
    std::vector<ParameterExpressionInfo> result;
    for(const auto& entry:PartNumericExpressions(part))
    {
        if(entry.first.empty() || entry.first.find_first_not_of(L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_")!=std::wstring::npos || iswdigit(entry.first[0]))continue;
        auto* exp=dynamic_cast<NXOpen::Expression*>(NXOpen::NXObjectManager::Get(entry.second));
        if(!exp)continue;
        const std::string type=exp->Type().GetText();
        if(type!="Number" && type!="Integer")continue;
        ParameterExpressionInfo info;
        info.name=entry.first;info.formula=FromAnsi(exp->GetFormula().GetText());
        info.value=ParameterNumber(exp->GetValueUsingUnits(NXOpen::Expression::UnitsOptionExpression));
        try{ParseParameterNumber(info.formula);info.editable=!exp->IsNoEdit()&&!exp->IsMeasurementExpression();}catch(const std::exception&){}
        result.push_back(std::move(info));
    }
    const auto generated=[](const std::wstring& n){return n.size()>1&&n[0]==L'p'&&std::all_of(n.begin()+1,n.end(),[](wchar_t c){return iswdigit(c)!=0;});};
    std::stable_sort(result.begin(),result.end(),[&](const auto& a,const auto& b){
        if(generated(a.name)!=generated(b.name))return !generated(a.name);
        return a.name<b.name;
    });
    if(result.empty())throw std::runtime_error(ToAnsi(L"该 PRT 没有可管理的数值表达式。请先在 NX 中用表达式驱动建模尺寸。"));
    return result;
}

std::wstring ParameterSchemaText(const std::vector<LibraryParameter>& schema)
{
    std::wstring text=L"expression\tlabel\tmode\tchoices\r\n";
    for(const auto& p:schema)
    {
        text+=p.expression+L"\t"+p.label+L"\t"+p.mode+L"\t";
        for(std::size_t j=0;j<p.choices.size();++j)text+=(j?L";":L"")+p.choices[j];
        text+=L"\r\n";
    }
    return text;
}

std::vector<std::wstring> ValidateTemplateSchema(const std::vector<LibraryParameter>& schema,
    const std::vector<ParameterExpressionInfo>& expressions)
{
    if(schema.empty()||schema.size()>12)throw std::runtime_error(ToAnsi(L"请选择 1 至 12 个需要显示的参数。"));
    std::set<std::wstring> names;std::vector<std::wstring> values;
    for(const auto& p:schema)
    {
        const auto found=std::find_if(expressions.begin(),expressions.end(),[&](const auto& e){return e.name==p.expression;});
        if(found==expressions.end()||!names.insert(p.expression).second||p.label.empty()||p.label.find_first_of(L"\t\r\n")!=std::wstring::npos)
            throw std::runtime_error(ToAnsi(L"表达式不存在、重复或中文名称无效。"));
        if(p.mode!=L"fixed"&&p.mode!=L"derived"&&p.mode!=L"number"&&p.mode!=L"list")throw std::runtime_error(ToAnsi(L"输入方式无效。"));
        if(!found->editable&&(p.mode==L"number"||p.mode==L"list"))
            throw std::runtime_error(ToAnsi(L"公式或锁定表达式只能设为联动或固定显示，避免破坏模型关系："+p.expression));
        if(p.mode==L"list")
        {
            if(p.choices.empty())throw std::runtime_error(ToAnsi(L"下拉选择需要填写可选数值，用分号分隔。"));
            bool contains=false;std::set<double> choices;
            for(const auto& choice:p.choices){const auto v=ParseParameterNumber(choice);if(!choices.insert(v).second)throw std::runtime_error(ToAnsi(L"可选值重复。"));if(v==ParseParameterNumber(found->value))contains=true;}
            if(!contains)throw std::runtime_error(ToAnsi(L"可选值必须包含当前模型中的数值："+p.expression+L"="+found->value));
        }
        values.push_back(found->value);
    }
    return values;
}

std::wstring ParameterValuesHeader(const std::vector<LibraryParameter>& schema)
{std::wstring s=L"SPE";for(const auto& p:schema)s+=L"\t"+p.expression;return s+L"\r\n";}
std::wstring ParameterValuesRow(const std::wstring& spec,const std::vector<std::wstring>& values)
{std::wstring s=spec;for(const auto& v:values)s+=L"\t"+v;return s+L"\r\n";}

std::size_t AddParameterTemplate(AppState* state,const fs::path& source,const std::wstring& category,
    const std::wstring& family,const std::wstring& spec,const std::vector<LibraryParameter>& schema)
{
    if(state->libraryFilter!=2)throw std::runtime_error(ToAnsi(L"请切换到本地有参。"));
    for(const auto& value:{category,family,spec})
        if(value.empty()||value!=SanitizeFileName(value)||value.find_first_of(L"\t\r\n")!=std::wstring::npos)
            throw std::runtime_error(ToAnsi(L"请填写有效的分类、零件族和首个规格名称。"));
    const auto definitions=ReadTemplateExpressions(source);
    ValidateTemplateSchema(schema,definitions);
    const auto directory=LibraryRoot(state)/L"LibParam"/category/family;
    if(fs::exists(directory))throw std::runtime_error(ToAnsi(L"此零件族目录已存在，请选择新的零件族名称；已有系列使用管理参数另存规格。"));
    for(const auto& item:state->items)if(item.parameterized&&item.category==category&&item.name==family)
        throw std::runtime_error(ToAnsi(L"此有参零件族已存在。"));
    const auto count=state->items.size();bool created=false;
    try
    {
        fs::create_directories(directory.parent_path());
        if(!fs::create_directory(directory))throw std::runtime_error(ToAnsi(L"无法创建零件族目录。"));
        created=true;const auto target=ParameterVariantPath(directory,spec);LoadedParameterPart(target);fs::copy_file(source,target);
        // Validate the actual saved copy, not an unsaved in-memory template.
        const auto values=ValidateTemplateSchema(schema,ReadTemplateExpressions(target));
        const auto preview=FindSidecarPreview(source);
        if(!preview.empty()){auto image=target;image.replace_extension(preview.extension());fs::copy_file(preview,image);}
        if(!WriteUtf16File(directory/L"parameter-schema.tsv",ParameterSchemaText(schema))||
            !WriteUtf16File(directory/L"parameters.tsv",ParameterValuesHeader(schema)+ParameterValuesRow(spec,values)))
            throw std::runtime_error(ToAnsi(L"保存参数定义或首个规格失败。"));
        state->items.push_back({L"template_"+std::to_wstring(GetTickCount64()),family,category,fs::relative(target,LibraryRoot(state)).wstring(),spec,true});
        std::wstring error;if(!SaveIndex(state,error))throw std::runtime_error(ToAnsi(error));
        return count;
    }
    catch(...){state->items.resize(count);if(created){std::error_code ec;fs::remove_all(directory,ec);}throw;}
}

void SaveTemplateDefinition(AppState* state,std::size_t index,const std::vector<LibraryParameter>& schema)
{
    if(index>=state->items.size()||state->libraryFilter!=2||!InCurrentLibrary(state,state->items[index]))throw std::runtime_error(ToAnsi(L"当前有参图档已变化。"));
    std::wstring error;const auto source=ResolvedModel(state,state->items[index],error);if(source.empty())throw std::runtime_error(ToAnsi(error));
    auto table=ParameterValuesHeader(schema);std::set<std::wstring> specs;
    for(const auto& item:state->items)
    {
        if(!item.parameterized||item.category!=state->items[index].category||item.name!=state->items[index].name)continue;
        const auto path=ResolvedModel(state,item,error);
        if(path.empty()||path.parent_path()!=source.parent_path()||!specs.insert(item.specification).second)
            throw std::runtime_error(ToAnsi(L"同一系列存在不同目录或重复规格，请整理后再定义参数。"));
        table+=ParameterValuesRow(item.specification,ValidateTemplateSchema(schema,ReadTemplateExpressions(path)));
    }
    const auto schemaPath=source.parent_path()/L"parameter-schema.tsv",tablePath=source.parent_path()/L"parameters.tsv";
    std::wstring oldSchema,oldTable;const bool hasSchema=ReadUtf16File(schemaPath,oldSchema),hasTable=ReadUtf16File(tablePath,oldTable);
    const auto backup=LibraryRoot(state)/L"backup"/(L"parameter_definition_"+std::to_wstring(GetTickCount64()));fs::create_directories(backup);
    if(hasSchema)fs::copy_file(schemaPath,backup/schemaPath.filename());if(hasTable)fs::copy_file(tablePath,backup/tablePath.filename());
    const auto schemaTemp=fs::path(schemaPath.wstring()+L".tmp"),tableTemp=fs::path(tablePath.wstring()+L".tmp");
    bool changed=false;
    try
    {
        if(!WriteUtf16File(schemaTemp,ParameterSchemaText(schema))||!WriteUtf16File(tableTemp,table))throw std::runtime_error(ToAnsi(L"写入参数文件失败。"));
        if(!MoveFileExW(schemaTemp.c_str(),schemaPath.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error(ToAnsi(L"更新参数定义失败。"));
        changed=true;
        if(!MoveFileExW(tableTemp.c_str(),tablePath.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error(ToAnsi(L"更新规格表失败。"));
    }
    catch(...)
    {
        std::error_code ec;if(changed){if(hasSchema)WriteUtf16File(schemaPath,oldSchema);else fs::remove(schemaPath,ec);}
        fs::remove(schemaTemp,ec);fs::remove(tableTemp,ec);throw;
    }
}

struct ParameterDefinitionState
{
    AppState* app=nullptr;fs::path root,source;std::size_t index=SIZE_MAX;std::wstring itemId,category,family,spec;
    std::vector<ParameterExpressionInfo> expressions;std::vector<LibraryParameter> schema;
};
constexpr int DefinitionExpression=4000,DefinitionLabel=4100,DefinitionMode=4200,DefinitionChoices=4300,DefinitionValue=4400;
const wchar_t* ParameterModes[]={L"number",L"list",L"fixed",L"derived"};

void RefreshDefinitionValue(HWND window,ParameterDefinitionState* state,int row,bool resetMode)
{
    const auto name=GetText(window,DefinitionExpression+row);
    const auto found=std::find_if(state->expressions.begin(),state->expressions.end(),[&](const auto& e){return e.name==name;});
    SetText(window,DefinitionValue+row,found==state->expressions.end()?L"":found->value+L"  ["+found->formula+L"]");
    if(resetMode)
    {
        SetText(window,DefinitionLabel+row,name);
        SendMessageW(GetDlgItem(window,DefinitionMode+row),CB_SETCURSEL,found!=state->expressions.end()&&found->editable?0:3,0);
    }
}

LRESULT CALLBACK ParameterDefinitionProc(HWND window,UINT message,WPARAM wParam,LPARAM lParam)
{
    auto* state=reinterpret_cast<ParameterDefinitionState*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if(message==WM_NCCREATE){state=static_cast<ParameterDefinitionState*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(state));}
    try
    {
        if(message==WM_CREATE&&state)
        {
            const auto add=[&](const wchar_t* cls,const std::wstring& text,DWORD style,int x,int y,int width,int height,int id){
                const auto c=CreateWindowExW(wcscmp(cls,L"EDIT")==0?WS_EX_CLIENTEDGE:0,cls,text.c_str(),WS_CHILD|WS_VISIBLE|WS_TABSTOP|style,x,y,width,height,window,reinterpret_cast<HMENU>(INT_PTR(id)),GetModuleHandleW(nullptr),nullptr);
                SendMessageW(c,WM_SETFONT,reinterpret_cast<WPARAM>(state->app->font),TRUE);return c;};
            add(L"STATIC",state->source.wstring(),SS_PATHELLIPSIS,16,12,870,24,0);
            add(L"STATIC",L"分类",0,16,48,44,22,0);add(L"EDIT",state->category,ES_AUTOHSCROLL,64,44,185,26,4500);
            add(L"STATIC",L"零件族",0,262,48,54,22,0);add(L"EDIT",state->family,ES_AUTOHSCROLL,320,44,250,26,4501);
            add(L"STATIC",L"首个规格",0,584,48,70,22,0);add(L"EDIT",state->spec,ES_AUTOHSCROLL,660,44,222,26,4502);
            if(state->index!=SIZE_MAX)for(int id:{4500,4501,4502})SendMessageW(GetDlgItem(window,id),EM_SETREADONLY,TRUE,0);
            add(L"STATIC",L"只添加需要在调用时显示的参数；未选择的行不显示。公式保留联动，数值沿用原单位。",0,16,82,875,24,0);
            add(L"STATIC",L"NX 表达式",0,16,114,130,22,0);add(L"STATIC",L"中文名称",0,157,114,140,22,0);
            add(L"STATIC",L"输入方式",0,310,114,105,22,0);add(L"STATIC",L"可选值（分号分隔）",0,426,114,216,22,0);add(L"STATIC",L"当前值 [模型公式]",0,650,114,233,22,0);
            for(int i=0;i<12;++i)
            {
                const int y=142+i*32;const auto expr=add(WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|WS_VSCROLL,16,y,130,240,DefinitionExpression+i);
                SendMessageW(expr,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L""));for(const auto& e:state->expressions)SendMessageW(expr,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(e.name.c_str()));
                add(L"EDIT",L"",ES_AUTOHSCROLL,157,y,140,25,DefinitionLabel+i);
                const auto mode=add(WC_COMBOBOXW,L"",CBS_DROPDOWNLIST,310,y,105,160,DefinitionMode+i);
                for(const auto* s:{L"数值输入",L"下拉选择",L"固定显示",L"公式联动"})SendMessageW(mode,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(s));
                SendMessageW(mode,CB_SETCURSEL,0,0);add(L"EDIT",L"",ES_AUTOHSCROLL,426,y,216,25,DefinitionChoices+i);
                add(L"STATIC",L"",SS_PATHELLIPSIS,650,y+4,233,24,DefinitionValue+i);
                if(std::size_t(i)<state->schema.size())
                {
                    const auto& p=state->schema[i];SendMessageW(expr,CB_SELECTSTRING,WPARAM(-1),reinterpret_cast<LPARAM>(p.expression.c_str()));
                    SetText(window,DefinitionLabel+i,p.label);for(int j=0;j<4;++j)if(p.mode==ParameterModes[j])SendMessageW(mode,CB_SETCURSEL,j,0);
                    std::wstring choices;for(const auto& c:p.choices)choices+=(choices.empty()?L"":L";")+c;SetText(window,DefinitionChoices+i,choices);RefreshDefinitionValue(window,state,i,false);
                }
                else SendMessageW(expr,CB_SETCURSEL,0,0);
            }
            add(L"STATIC",L"保存后可在“管理参数”中修改尺寸并另存规格；同名预览图随 PRT 复制，也可入库后抓图。",0,16,538,870,25,0);
            add(L"BUTTON",state->index==SIZE_MAX?L"创建有参零件族":L"保存参数定义",BS_DEFPUSHBUTTON,622,578,155,34,IDOK);
            add(L"BUTTON",L"取消",BS_PUSHBUTTON,792,578,90,34,IDCANCEL);return 0;
        }
        if(message==WM_COMMAND&&state)
        {
            const int id=LOWORD(wParam);
            if(id>=DefinitionExpression&&id<DefinitionExpression+12&&HIWORD(wParam)==CBN_SELCHANGE){RefreshDefinitionValue(window,state,id-DefinitionExpression,true);return 0;}
            if(id==IDCANCEL){DestroyWindow(window);return 0;}
            if(id==IDOK)
            {
                if(LibraryRoot(state->app)!=state->root||state->app->libraryFilter!=2)throw std::runtime_error(ToAnsi(L"库位置已变化，请重新打开参数定义。"));
                std::vector<LibraryParameter> schema;
                for(int i=0;i<12;++i)
                {
                    const auto expr=Trim(GetText(window,DefinitionExpression+i));if(expr.empty())continue;
                    const auto mode=SendMessageW(GetDlgItem(window,DefinitionMode+i),CB_GETCURSEL,0,0);if(mode<0||mode>3)throw std::runtime_error(ToAnsi(L"请选择输入方式。"));
                    LibraryParameter p{expr,Trim(GetText(window,DefinitionLabel+i)),ParameterModes[mode],{}};
                    std::wstring choices=GetText(window,DefinitionChoices+i);for(auto& c:choices)if(c==L'；'||c==L','||c==L'，')c=L';';
                    if(p.mode==L"list"){std::wistringstream s(choices);std::wstring choice;while(std::getline(s,choice,L';'))if(!Trim(choice).empty())p.choices.push_back(Trim(choice));}
                    schema.push_back(std::move(p));
                }
                std::size_t index=state->index;
                if(index==SIZE_MAX)index=AddParameterTemplate(state->app,state->source,Trim(GetText(window,4500)),Trim(GetText(window,4501)),Trim(GetText(window,4502)),schema);
                else
                {
                    if(index>=state->app->items.size()||state->app->items[index].id!=state->itemId)throw std::runtime_error(ToAnsi(L"图档已经变化，请重新选择。"));
                    SaveTemplateDefinition(state->app,index,schema);
                }
                SelectLibrarySpecification(state->app,index);SetStatus(state->app,L"参数定义已保存，可打开管理参数另存规格。");DestroyWindow(window);return 0;
            }
        }
        if(message==WM_CLOSE){DestroyWindow(window);return 0;}
        if(message==WM_NCDESTROY){if(g_parameterWindow==window)g_parameterWindow=nullptr;SetWindowLongPtrW(window,GWLP_USERDATA,0);delete state;}
    }
    catch(const NXOpen::NXException& ex){UF_print_syslog(const_cast<char*>(ex.Message()),false);MessageBoxW(window,FromAnsi(ex.Message()).c_str(),L"定义有参标准件",MB_OK|MB_ICONERROR);}
    catch(const std::exception& ex){UF_print_syslog(const_cast<char*>(ex.what()),false);MessageBoxW(window,FromAnsi(ex.what()).c_str(),L"定义有参标准件",MB_OK|MB_ICONERROR);}
    catch(...){UF_print_syslog("[StandardPartsLibrary] Parameter definition callback failed\n",false);}
    return DefWindowProcW(window,message,wParam,lParam);
}

void ShowParameterDefinition(AppState* app,const fs::path& source,std::size_t index,
    const std::wstring& category,const std::wstring& family,const std::wstring& spec)
{
    try
    {
        if (g_parameterWindow && IsWindow(g_parameterWindow))
        {
            wchar_t className[128]{};
            GetClassNameW(g_parameterWindow, className, static_cast<int>(std::size(className)));
            const auto* opened = wcscmp(className, L"ZhihuiStandardPartsParameterDefinition") == 0
                ? reinterpret_cast<ParameterDefinitionState*>(GetWindowLongPtrW(g_parameterWindow, GWLP_USERDATA)) : nullptr;
            if (opened && opened->root == LibraryRoot(app) && opened->source == source && opened->index == index)
            { ShowWindow(g_parameterWindow, SW_RESTORE); SetForegroundWindow(g_parameterWindow); return; }
            DestroyWindow(g_parameterWindow);
        }
        if(app->libraryFilter!=2)throw std::runtime_error(ToAnsi(L"请切换到本地有参。"));
        auto state=std::make_unique<ParameterDefinitionState>();state->app=app;state->root=LibraryRoot(app);state->source=source;state->index=index;
        state->category=category.empty()?L"用户自定义":category;state->family=family.empty()?source.stem().wstring():family;state->spec=spec.empty()?source.stem().wstring():spec;
        if(index!=SIZE_MAX){if(index>=app->items.size())throw std::runtime_error(ToAnsi(L"请先选择有参图档。"));state->itemId=app->items[index].id;}
        state->expressions=ReadTemplateExpressions(source);state->schema=ReadParameterSchema(source);
        // Available expressions are candidates, not published parameters.
        // A new definition stays empty until the user explicitly adds rows.
        HMODULE module=nullptr;GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&ShowParameterDefinition),&module);
        WNDCLASSEXW cls{};cls.cbSize=sizeof(cls);cls.lpfnWndProc=ParameterDefinitionProc;cls.hInstance=module;cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);cls.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_BTNFACE+1);cls.lpszClassName=L"ZhihuiStandardPartsParameterDefinition";
        if (!RegisterOwnedWindowClass(cls, app->window)) return;
        RECT rect{};GetWindowRect(app->window,&rect);auto* raw=state.release();
        const auto window=CreateWindowExW(WS_EX_DLGMODALFRAME,cls.lpszClassName,index==SIZE_MAX?L"新建有参标准件 - 定义参数":L"本地有参 - 定义参数",WS_CAPTION|WS_SYSMENU|WS_POPUP,rect.left+20,rect.top+40,918,662,g_managerWindow?g_managerWindow:app->window,nullptr,module,raw);
        if(window){g_parameterWindow=window;ShowWindow(window,SW_SHOW);}
    }
    catch(const NXOpen::NXException& ex){UF_print_syslog(const_cast<char*>(ex.Message()),false);MessageBoxW(app->window,FromAnsi(ex.Message()).c_str(),L"定义有参标准件",MB_OK|MB_ICONERROR);}
    catch(const std::exception& ex){MessageBoxW(app->window,FromAnsi(ex.what()).c_str(),L"定义有参标准件",MB_OK|MB_ICONERROR);}
    catch(...){UF_print_syslog("[StandardPartsLibrary] Open parameter definition failed\n",false);}
}
