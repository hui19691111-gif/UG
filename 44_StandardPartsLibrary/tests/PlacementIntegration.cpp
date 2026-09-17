// Tests the actual insertion implementation in a separate NX external process.
// Does not load the deployed plugin, drive NX dialogs, or touch user models.
#include "../StandardPartsLibrary.cpp"
#include <iostream>
#include <stdexcept>

static void Check(int code)
{
    if (code != 0) throw std::runtime_error(ToAnsi(UfError(code)));
}

static void Require(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

static std::vector<tag_t> Objects(tag_t part, int type, int subtype)
{
    std::vector<tag_t> objects;
    tag_t object = NULL_TAG;
    do
    {
        Check(UF_OBJ_cycle_objs_in_part(part, type, &object));
        if (object == NULL_TAG) break;
        int actualType = 0, actualSubtype = 0;
        Check(UF_OBJ_ask_type_and_subtype(object, &actualType, &actualSubtype));
        if (subtype < 0 || subtype == actualSubtype) objects.push_back(object);
    } while (object != NULL_TAG);
    return objects;
}

static void TestLibraryContextDelete(const fs::path& root, HWND controls)
{
    const fs::path library = root / "delete_test";
    fs::create_directories(library / "Lib");
    SetText(controls, ID_ROOT, library.wstring());
    INITCOMMONCONTROLSEX common{sizeof(common), ICC_LISTVIEW_CLASSES};
    Require(InitCommonControlsEx(&common) != FALSE, "Initialize test list controls");
    AppState state;
    state.window = controls;
    state.list = CreateWindowExW(0, WC_LISTVIEWW, L"", WS_CHILD | LVS_REPORT | LVS_SINGLESEL,
        0,0,100,100,controls,reinterpret_cast<HMENU>(ID_LIST),nullptr,nullptr);
    const HWND combo = CreateWindowExW(0, WC_COMBOBOXW, L"", WS_CHILD | CBS_DROPDOWNLIST,
        0,0,100,100,controls,reinterpret_cast<HMENU>(ID_SPEC),nullptr,nullptr);
    Require(state.list != nullptr && combo != nullptr, "Create own test list/specification controls");
    state.items = {{L"a1",L"A",L"test",L"Lib/a1.prt",L"M6",false},
                   {L"a2",L"A",L"test",L"Lib/a2.prt",L"M8",false},
                   {L"b",L"B",L"test",L"Lib/b.prt",L"default",false}};
    for (const auto& item : state.items) fs::copy_file(root / "sample.prt", library / item.relativePath);
    state.visible = {2,0}; // Filtered/sorted rows do not equal model indices.
    for (int rowIndex = 0; rowIndex < 2; ++rowIndex)
    {
        LVITEMW row{};
        row.mask = LVIF_TEXT;
        row.iItem = rowIndex;
        row.pszText = const_cast<wchar_t*>(rowIndex == 0 ? L"B" : L"A");
        ListView_InsertItem(state.list, &row);
    }
    Require(SelectContextRow(&state, 0) == 2, "Right click must target clicked filtered row");
    Require(SelectContextRow(&state, 1) == 0, "Right click must replace stale family selection");
    SendMessageW(combo, CB_SETCURSEL, 1, 0);
    Require(SelectContextRow(&state, 1) == 1, "Right click selected family must preserve current specification");
    Require(SelectContextRow(&state, -1) == SIZE_MAX && SelectedIndex(&state) == 1,
        "Empty space must not offer delete for prior selection");
    Require(SelectContextRow(&state, 42) == SIZE_MAX, "Invalid row must be rejected");
    Require(WriteUtf16File(library / "Lib/a2.png", L"temporary preview fixture") &&
        WriteUtf16File(library / "Lib/a2.jpg", L"temporary preview fixture"), "Create own sidecar fixtures");
    std::wstring error;
    Require(SaveIndex(&state, error), "Save test library");
    fs::path backup;
    Require(RemoveLibraryItem(&state, 1, backup, error), "Delete selected specification");
    Require(state.items.size() == 2 && !fs::exists(library / "Lib/a2.prt") &&
        !fs::exists(library / "Lib/a2.png") && !fs::exists(library / "Lib/a2.jpg") &&
        fs::exists(library / "Lib/a1.prt") && fs::exists(library / "Lib/b.prt"),
        "Delete only selected model and its sidecars");
    Require(fs::exists(backup / "a2.prt") && fs::exists(backup / "a2.png") &&
        fs::exists(backup / "a2.jpg") && fs::exists(backup / "restore.tsv"), "Recoverable backup required");
    LoadIndex(&state);
    Require(state.items.size() == 2, "Folder rescan must not resurrect deleted specification");
    state.items.push_back({L"shared",L"shared",L"test",L"Lib/a1.prt",L"shared",false});
    Require(RemoveLibraryItem(&state, 0, backup, error) && backup.empty() &&
        fs::exists(library / "Lib/a1.prt"), "Shared model must not be moved");
    const auto originalItems = state.items;
    state.items.push_back({L"outside",L"outside",L"test",L"../sample.prt",L"default",false});
    Require(!RemoveLibraryItem(&state, state.items.size() - 1, backup, error) &&
        fs::exists(root / "sample.prt"), "Outside-library deletion must be rejected");
    state.items = originalItems;
    const HANDLE lock = CreateFileW((library / "library.tsv.tmp").c_str(), GENERIC_READ | GENERIC_WRITE,
        0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    Require(lock != INVALID_HANDLE_VALUE, "Lock own temporary index for rollback test");
    const bool deleted = RemoveLibraryItem(&state, 0, backup, error);
    CloseHandle(lock);
    Require(!deleted && fs::exists(library / "Lib/b.prt") && state.items.size() == originalItems.size(),
        "Index failure must restore moved model and in-memory entry");
    std::cout << "PASS context deletion: clicked row, retained spec, blank space, recoverable sidecars, rescan, shared model, path guard, rollback\n";
}

static void TestBodyCapture(const fs::path& root, HWND controls)
{

    tag_t source = NULL_TAG;
    Check(UF_PART_new((root / "capture_source.prt").string().c_str(), 1, &source));
    double origin[3] = {20,40,3};
    char x[] = "10", y[] = "8", z[] = "6";
    char* lengths[] = {x,y,z};
    tag_t feature = NULL_TAG, selected = NULL_TAG;
    Check(UF_MODL_create_block1(UF_NULLSIGN, origin, lengths, &feature));
    Check(UF_MODL_ask_feat_body(feature, &selected));
    origin[0] = 100;
    Check(UF_MODL_create_block1(UF_NULLSIGN, origin, lengths, &feature));
    double before[6]{};
    Check(UF_MODL_ask_bounding_box(selected, before));
    tag_t oldWcs = NULL_TAG, rotatedMatrix = NULL_TAG, rotatedWcs = NULL_TAG;
    Check(UF_CSYS_ask_wcs(&oldWcs));
    const double rotation[9] = {0,1,0,-1,0,0,0,0,1};
    Check(UF_CSYS_create_matrix(rotation, &rotatedMatrix));
    Check(UF_CSYS_create_temp_csys(origin, rotatedMatrix, &rotatedWcs));
    Check(UF_CSYS_set_wcs(rotatedWcs));
    const fs::path library = root / "capture_library";
    SetText(controls, ID_ROOT, library.wstring());
    for (int id : {ID_NAME,ID_EDIT_CATEGORY,ID_EDIT_SPEC})
        CreateWindowExW(0,L"EDIT",L"",WS_CHILD,0,0,1,1,controls,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),nullptr,nullptr);
    SetText(controls,ID_NAME,L"偏心支架");
    SetText(controls,ID_EDIT_CATEGORY,L"用户自定义");
    SetText(controls,ID_EDIT_SPEC,L"默认");
    AppState state;
    state.window = controls;
    std::wstring error;
    fs::path saved;
    Require(!CaptureBodiesToLibrary(&state, {}, {22,42,4}, saved, error), "Empty capture must fail");
    // Native Block Styler supplies metadata directly, without Win32 edits.
    state.libraryRootOverride = library;
    const CaptureMetadata metadata{L"偏心支架", L"用户自定义", L"默认"};
    const unsigned char png[] = {
        0x89,0x50,0x4e,0x47,0x0d,0x0a,0x1a,0x0a,0,0,0,0x0d,0x49,0x48,0x44,0x52,
        0,0,0,2,0,0,0,2,8,2,0,0,0,0xfd,0xd4,0x9a,0x73,0,0,0,0x12,0x49,0x44,0x41,0x54,
        0x78,0x9c,0x63,0xf8,0xff,0xff,0xbf,0x40,0xc0,4,6,8,5,0,0x3c,0x2b,7,0xdb,
        0x3e,0x61,0x46,0xa8,0,0,0,0,0x49,0x45,0x4e,0x44,0xae,0x42,0x60,0x82};
    const auto snapshot = root / "snapshot.png";
    { std::ofstream file(snapshot, std::ios::binary); file.write(reinterpret_cast<const char*>(png), sizeof(png)); }
    auto readBytes = [](const fs::path& path) {
        std::ifstream file(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    };
    const auto orphan = library / L"Lib/用户自定义/偏心支架/默认.png";
    fs::create_directories(orphan.parent_path());
    fs::copy_file(snapshot, orphan);
    Require(CaptureBodiesToLibrary(&state, {selected,selected}, {22,42,4}, saved, error, &metadata, snapshot), ToAnsi(error).c_str());
    const auto image = FindSidecarPreview(saved);
    Require(saved.stem() == L"默认_1" && !image.empty() && image.stem() == saved.stem() &&
        readBytes(image) == readBytes(snapshot) && readBytes(orphan) == readBytes(snapshot),
        "Captured PNG must match saved model name and preserve an orphaned preview");
    Require(state.items.size() == 1 && !state.items[0].parameterized, "Body capture must add a static item");
    double after[6]{};
    Check(UF_MODL_ask_bounding_box(selected, after));
    for (int axis = 0; axis < 6; ++axis) Require(std::abs(before[axis]-after[axis]) < 1e-9, "Capture moved source geometry");
    tag_t restoredWcs = NULL_TAG;
    Check(UF_CSYS_ask_wcs(&restoredWcs));
    Require(restoredWcs == rotatedWcs && UF_ASSEM_ask_work_part() == source &&
        UF_PART_ask_display_part() == source, "Capture must restore WCS and retain work/display part");
    UF_PART_load_status_t load{};
    tag_t exported = NULL_TAG;
    const int code = UF_PART_open_quiet(ToAnsi(saved.wstring()).c_str(), &exported, &load);
    UF_PART_free_load_status(&load);
    Check(code);
    const auto bodies = Objects(exported,UF_solid_type,UF_solid_body_subtype);
    Require(bodies.size() == 1, "Only selected body should be exported; duplicates must be removed");
    double box[6]{};
    Check(UF_MODL_ask_bounding_box(bodies.front(),box));
    Require(std::abs(box[0]+2)<1e-6 && std::abs(box[1]+2)<1e-6 && std::abs(box[2]+1)<1e-6 &&
        std::abs(box[3]-8)<1e-6 && std::abs(box[4]-6)<1e-6 && std::abs(box[5]-5)<1e-6,
        "Selected insertion base must map to origin, independent of original rotated WCS");
    Check(UF_PART_close(exported,0,2));
    fs::path second;
    Require(CaptureBodiesToLibrary(&state,{selected},{22,42,4},second,error) && second != saved && fs::exists(saved),
        "Same name/specification must not overwrite existing standard");
    Require(FindSidecarPreview(second).empty(), "A capture without an image must not reuse a previous image");
    fs::path rejected;
    Require(!CaptureBodiesToLibrary(&state,{selected},{22,42,4},rejected,error,&metadata,root / "missing.png"),
        "Missing captured image must stop before exporting");
    const HANDLE imageLock = CreateFileW(snapshot.c_str(),GENERIC_READ,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    Require(imageLock != INVALID_HANDLE_VALUE,"Lock own snapshot to simulate copy failure");
    const bool copied = CaptureBodiesToLibrary(&state,{selected},{22,42,4},rejected,error,&metadata,snapshot);
    CloseHandle(imageLock);
    Require(!copied && state.items.size() == 2,"Image copy failure must roll back model and index entry");
    const HANDLE lock = CreateFileW((library / "library.tsv.tmp").c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    Require(lock != INVALID_HANDLE_VALUE,"Lock own capture index");
    fs::path failed;
    const bool added = CaptureBodiesToLibrary(&state,{selected},{22,42,4},failed,error,&metadata,snapshot);
    CloseHandle(lock);
    Require(!added && state.items.size() == 2,"Capture index failure must rollback entry");
    std::size_t partCount = 0, imageCount = 0;
    for (const auto& file : fs::recursive_directory_iterator(library / "Lib"))
    {
        if (IsPartFile(file.path())) ++partCount;
        if (file.path().extension() == L".png") ++imageCount;
    }
    Require(partCount == 2,"Capture index failure must remove only newly exported PRT");
    Require(imageCount == 2 && readBytes(image) == readBytes(snapshot),
        "Copy/index failures must remove new sidecars, preserving existing image and orphan");
    // A cyclic frame exercises all three axes and an off-origin base point.
    // Invalid directions must not create an index entry or any model file.
    for (const auto& invalid : std::vector<std::array<double,6>>{
        {0,0,0,0,1,0}, {1,0,0,2,0,0}, {NAN,0,0,0,1,0}})
        Require(!CaptureBodiesToLibrary(&state,{selected},{22,42,4},rejected,error,&metadata,{},invalid) &&
            state.items.size() == 2, "Invalid capture axes must fail before exporting");
    const std::array<double,6> captureDirections{0,2,0,0,0,3};
    fs::path oriented;
    Require(CaptureBodiesToLibrary(&state,{selected},{22,42,4},oriented,error,&metadata,snapshot,captureDirections),
        ToAnsi(error).c_str());
    const auto orientedItem = state.items.back();
    exported = NULL_TAG;
    const int orientedOpen = UF_PART_open_quiet(ToAnsi(oriented.wstring()).c_str(), &exported, &load);
    UF_PART_free_load_status(&load);
    Check(orientedOpen);
    const auto orientedBodies = Objects(exported,UF_solid_type,UF_solid_body_subtype);
    Require(orientedBodies.size() == 1, "Oriented capture must retain selected body count");
    Check(UF_MODL_ask_bounding_box(orientedBodies.front(),box));
    const double expectedLocal[6] = {-2,-1,-2,6,5,8};
    for (int axis=0; axis<6; ++axis)
        Require(std::abs(box[axis]-expectedLocal[axis])<1e-6,
            ("Capture frame bound " + std::to_string(axis) + ": expected " + std::to_string(expectedLocal[axis]) +
                ", got " + std::to_string(box[axis])).c_str());
    Check(UF_PART_close(exported,0,2));
    Check(UF_MODL_ask_bounding_box(selected,after));
    for (int axis=0; axis<6; ++axis)
        Require(std::abs(after[axis]-before[axis])<1e-9, "Oriented capture must not move source geometry");
    Check(UF_CSYS_ask_wcs(&restoredWcs));
    Require(restoredWcs == rotatedWcs && UF_ASSEM_ask_work_part() == source &&
        UF_PART_ask_display_part() == source, "Oriented capture must restore WCS/work/display context");
    Require(readBytes(FindSidecarPreview(oriented)) == readBytes(snapshot), "Oriented capture must retain its preview");
    Check(UF_CSYS_set_wcs(oldWcs));
    Check(UF_OBJ_delete_object(rotatedWcs));
    // Round trip through the production insertion function at a new placement.
    tag_t target = NULL_TAG;
    Check(UF_PART_new((root / "capture_insert_target.prt").string().c_str(),1,&target));
    state.hasPlacement = true;
    state.placementOrigin[0]=50; state.placementOrigin[1]=60; state.placementOrigin[2]=70;
    Require(InsertBodies(&state,state.items.front(),error),ToAnsi(error).c_str());
    const auto inserted = Objects(target,UF_solid_type,UF_solid_body_subtype);
    Require(inserted.size()==1,"Round-trip capture inserts only selected geometry");
    Check(UF_MODL_ask_bounding_box(inserted.front(),box));
    Require(std::abs(box[0]-48)<1e-6 && std::abs(box[1]-58)<1e-6 && std::abs(box[2]-69)<1e-6,
        "Insertion must align captured base point to placement");
    Check(UF_PART_new((root / "capture_oriented_insert_target.prt").string().c_str(),1,&target));
    state.placementOrigin[0]=22; state.placementOrigin[1]=42; state.placementOrigin[2]=4;
    const double placementAxes[9] = {0,1,0,0,0,1,1,0,0};
    std::copy(placementAxes,placementAxes+9,state.placementMatrix);
    Require(InsertBodies(&state,orientedItem,error),ToAnsi(error).c_str());
    const auto orientedInserted = Objects(target,UF_solid_type,UF_solid_body_subtype);
    Require(orientedInserted.size()==1,"Oriented round trip must insert exactly one body");
    Check(UF_MODL_ask_bounding_box(orientedInserted.front(),box));
    for (int axis=0; axis<6; ++axis)
        Require(std::abs(box[axis]-before[axis])<1e-6,"Chosen insertion frame must reproduce original geometry pose");
    std::cout << "PASS capture: selected bodies only, dedup, base-to-origin, WCS/source preserved, no overwrite, rollback, insertion round trip\n";
    std::cout << "PASS capture image: PNG round trip, matching filename, no stale reuse, orphan preserved, missing/copy/index failure rollback\n";
    std::cout << "PASS capture orientation: all axes normalized, invalid frames rejected, base-frame round trip, source/WCS preserved\n";
}

static void TestAxisPatternCounts(const fs::path& root)
{
    const HWND controls = CreateWindowExW(0, L"STATIC", L"Pattern inputs", 0,
        0, 0, 1, 1, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    struct Scope { HWND window; ~Scope() { DestroyWindow(window); } } scope{controls};
    for (const auto& input : std::vector<std::pair<int, const wchar_t*>>{
        {ID_PATTERN_COUNT_X,L"3"},{ID_PATTERN_COUNT_Y,L"4"},
        {ID_PATTERN_SPACING_X,L"20"},{ID_PATTERN_SPACING_Y,L"10"}})
        CreateWindowExW(0,L"EDIT",input.second,WS_CHILD,0,0,1,1,controls,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(input.first)),nullptr,nullptr);
    AppState state; state.window = controls; state.libraryRootOverride = root;
    state.hasPlacement = true;
    const double origin[] = {50,60,7};
    const double rotated[] = {0,1,0,-1,0,0,0,0,1};
    state.patternMode = 1;
    auto points = PatternOrigins(&state, origin, rotated);
    Require(points.size()==3 && points[0]==std::array<double,3>{50,40,7} && points[2]==std::array<double,3>{50,80,7},
        "X count and spacing must follow local X after orientation");
    state.patternMode = 2;
    points = PatternOrigins(&state, origin, rotated);
    Require(points.size()==4 && points[0]==std::array<double,3>{65,60,7} && points[3]==std::array<double,3>{35,60,7},
        "Y count and spacing must follow local Y after orientation");
    SetText(controls,ID_PATTERN_COUNT_Y,L"1");
    Require(PatternOrigins(&state,origin,rotated)==std::vector<std::array<double,3>>{{50,60,7}},"Count one keeps the selected origin");
    SetText(controls,ID_PATTERN_COUNT_Y,L"999");
    Require(PatternOrigins(&state,origin,rotated).size()==50,"Axis count must keep the existing limit");
    SetText(controls,ID_PATTERN_COUNT_Y,L"4");
    state.patternMode=6;
    Require(PatternOrigins(&state,origin,rotated).size()==12,"Grid still uses X times Y");
    for(const auto& entry:std::vector<std::pair<int,std::size_t>>{{0,1},{3,2},{4,4},{5,1}})
    { state.patternMode=entry.first;Require(PatternOrigins(&state,origin,rotated).size()==entry.second,"Fixed patterns remain unchanged"); }

    const auto previousWork=UF_ASSEM_ask_work_part(),previousDisplay=UF_PART_ask_display_part();
    tag_t assembly=NULL_TAG,merged=NULL_TAG;
    Check(UF_PART_new((root/"axis_count_assembly.prt").string().c_str(),1,&assembly));
    std::copy(origin,origin+3,state.placementOrigin);
    state.patternMode=1;SetText(controls,ID_PATTERN_COUNT_X,L"5");
    LibraryItem item{L"axis",L"sample",L"test",L"sample.prt",L"default",false};std::wstring error;
    int previewUnits=0;
    const auto preview=LoadPlacementWireframe(root/"sample.prt",previewUnits);
    bool simplified=false;
    Require(BuildPlacedPreview(preview,PatternOrigins(&state,origin,state.placementMatrix),state.placementMatrix,1.0,simplified).size()==60,
        "X preview must contain five complete boxes");
    Require(InsertAssembly(&state,item,error),ToAnsi(error).c_str());
    const auto instances=Objects(assembly,UF_component_type,-1);
    Require(instances.size()==5,"X quantity five must insert five assembly components");
    std::vector<double> positions;
    for(const auto instance:instances)
    {
        char part[MAX_FSPEC_BUFSIZE]{},refset[UF_OBJ_NAME_BUFSIZE]{},name[UF_CFI_MAX_FILE_NAME_BUFSIZE]{};
        double placed[3]{},matrix[9]{},transform[4][4]{};
        Check(UF_ASSEM_ask_component_data(instance,part,refset,name,placed,matrix,transform));
        positions.push_back(placed[0]);
    }
    std::sort(positions.begin(),positions.end());
    Require(positions==std::vector<double>{10,30,50,70,90},"X inserted origins must match preview spacing");
    Check(UF_PART_save());
    Check(UF_PART_new((root/"axis_count_bodies.prt").string().c_str(),1,&merged));
    state.patternMode=2;
    Require(BuildPlacedPreview(preview,PatternOrigins(&state,origin,state.placementMatrix),state.placementMatrix,1.0,simplified).size()==48,
        "Y preview must contain four complete boxes");
    Require(InsertBodies(&state,item,error),ToAnsi(error).c_str());
    const auto bodies=Objects(merged,UF_solid_type,UF_solid_body_subtype);
    Require(bodies.size()==4,"Y quantity four must insert four bodies");
    positions.clear();
    for(const auto body:bodies){double box[6]{};Check(UF_MODL_ask_bounding_box(body,box));positions.push_back(box[1]);}
    std::sort(positions.begin(),positions.end());
    for(std::size_t i=0;i<4;++i)Require(std::abs(positions[i]-(45+10*i))<1e-4,"Y inserted origins must match preview spacing");
    Check(UF_PART_save());
    Check(UF_PART_set_display_part(previousDisplay));Check(UF_ASSEM_set_work_part(previousWork));
    Check(UF_PART_close(merged,0,1));Check(UF_PART_close(assembly,0,1));
    std::cout<<"PASS axis quantities: oriented X/Y, count one/limit, grid/fixed modes, five native components, four bodies, exact preview positions\n";
}

static void TestPreviewReplacement(const fs::path& root)
{
    const HRESULT com=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    struct ComScope{HRESULT status;~ComScope(){if(SUCCEEDED(status))CoUninitialize();}} comScope{com};
    const auto directory=root/L"preview_edit"/L"Lib";
    fs::create_directories(directory);
    AppState state;state.libraryRootOverride=directory.parent_path();
    state.items={{L"first",L"First",L"",L"Lib/first.prt",L"default",false},
                 {L"second",L"Second",L"",L"Lib/second.prt",L"default",false}};
    fs::copy_file(root/L"sample.prt",directory/L"first.prt");
    fs::copy_file(root/L"sample.prt",directory/L"second.prt");
    const auto bytes=[](const fs::path& file){std::ifstream in(file,std::ios::binary);return std::string(std::istreambuf_iterator<char>(in),{});};
    const auto modelBefore=bytes(directory/L"second.prt");
    const auto source=directory.parent_path()/L"source.bmp";
    const auto makeImage=[&](DWORD pixel)
    {
        BITMAPFILEHEADER file{};file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(BITMAPINFOHEADER);file.bfSize=file.bfOffBits+16;
        BITMAPINFOHEADER info{};info.biSize=sizeof(info);info.biWidth=2;info.biHeight=2;info.biPlanes=1;info.biBitCount=32;
        std::ofstream out(source,std::ios::binary);out.write(reinterpret_cast<const char*>(&file),sizeof(file));out.write(reinterpret_cast<const char*>(&info),sizeof(info));
        for(int i=0;i<4;++i)out.write(reinterpret_cast<const char*>(&pixel),sizeof(pixel));
    };
    makeImage(0x00ff0000);
    const auto target=ReplaceManagedPreview(&state,1,source);
    Require(target==directory/L"second.png" && !fs::exists(directory/L"first.png"),"Preview replacement targets current specification only");
    auto bitmap=ReadPreviewBitmap(target);BITMAP data{};GetObjectW(bitmap,sizeof(data),&data);
    Require(data.bmBits && static_cast<BYTE*>(data.bmBits)[2]==255,"Decode replacement preview directly");DeleteObject(bitmap);
    const auto firstImage=bytes(target);
    makeImage(0x000000ff);ReplaceManagedPreview(&state,1,source);
    bitmap=ReadPreviewBitmap(target);GetObjectW(bitmap,sizeof(data),&data);
    Require(data.bmBits && static_cast<BYTE*>(data.bmBits)[0]==255 && static_cast<BYTE*>(data.bmBits)[2]==0,"Same filename must immediately show changed pixels");DeleteObject(bitmap);
    const auto secondImage=bytes(target);
    Require(firstImage!=secondImage && bytes(directory/L"second.prt")==modelBefore,"Preview update preserves PRT");
    bool backedUp=false;
    for(const auto& file:fs::recursive_directory_iterator(directory.parent_path()/L"backup"))
        if(file.is_regular_file()&&file.path().filename()==L"second.png"&&bytes(file.path())==firstImage)backedUp=true;
    Require(backedUp,"Previous preview must be recoverable");
    WriteUtf16File(source,L"invalid image");bool failed=false;
    try{ReplaceManagedPreview(&state,1,source);}catch(...){failed=true;}
    Require(failed&&bytes(target)==secondImage,"Invalid image leaves existing preview intact");
    ReplaceManagedPreview(&state,1,target);
    Require(bytes(directory/L"second.prt")==modelBefore,"Choosing current image is safe");
    std::cout<<"PASS preview editing: BMP to PNG, selected specification, immediate uncached pixels, backup, invalid input rollback, unchanged PRT\n";
}

int wmain(int argc, wchar_t** argv)
{
    if (argc < 2 || fs::exists(argv[1]))
    {
        std::cerr << "Supply a new, nonexistent test output directory.\n";
        return 2;
    }
    bool initialized = false;
    HWND controls = nullptr;
    try
    {
        Require(!CanConfirmPlacement(false, false, true, false), "No confirm before dialog shown");
        Require(!CanConfirmPlacement(true, false, false, false), "No confirm before placement");
        Require(CanConfirmPlacement(true, false, true, false), "Selected point must enable confirm");
        Require(!CanConfirmPlacement(true, false, false, false), "Clearing first point must disable confirm");
        Require(CanConfirmPlacement(true, false, false, true), "OK remains available after Apply");
        Require(CanConfirmPlacement(true, false, true, true), "Next placement must enable confirm");
        Require(!CanConfirmPlacement(true, true, true, true), "No confirm while closing");
        std::cout << "PASS confirmation readiness: initial, selected, cleared, applied, next, closing\n";
        const fs::path root = fs::absolute(argv[1]);
        fs::create_directories(root);
        Check(UF_initialize());
        initialized = true;
        tag_t source = NULL_TAG;
        Check(UF_PART_new((root / "sample.prt").string().c_str(), 1, &source));
        double corner[3] = {0, 0, 0};
        char x[] = "10", y[] = "8", z[] = "6";
        char* lengths[] = {x, y, z};
        tag_t feature = NULL_TAG;
        Check(UF_MODL_create_block1(UF_NULLSIGN, corner, lengths, &feature));
        Check(UF_PART_save());
        Check(UF_PART_close(source, 0, 2));

        // These hidden controls supply this module's input values only.
        // They never operate any external application window.
        controls = CreateWindowExW(0, L"STATIC", L"Test inputs", 0,
            0, 0, 1, 1, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        Require(controls != nullptr, "Cannot create isolated test inputs");
        CreateWindowExW(0, L"EDIT", root.c_str(), WS_CHILD,
            0, 0, 1, 1, controls, reinterpret_cast<HMENU>(ID_ROOT), nullptr, nullptr);
        CreateWindowExW(0, L"EDIT", L"7", WS_CHILD,
            0, 0, 1, 1, controls, reinterpret_cast<HMENU>(ID_LAYER), nullptr, nullptr);
        AppState state;
        state.window = controls;
        state.hasPlacement = true;
        state.placementOrigin[0] = 20;
        state.placementOrigin[1] = 40;
        state.placementOrigin[2] = 3;
        LibraryItem item{L"test", L"sample", L"test", L"sample.prt", L"default", false};
        std::wstring error;

        tag_t assembly = NULL_TAG;
        Check(UF_PART_new((root / "assembly.prt").string().c_str(), 1, &assembly));
        int previewUnits = 0;
        const tag_t originalDisplay = UF_PART_ask_display_part();
        const auto wireframe = LoadPlacementWireframe(root / "sample.prt", previewUnits);
        Require(wireframe.size() == 12 && previewUnits == 1, "Block wireframe must contain 12 edges in mm");
        Require(UF_ASSEM_ask_work_part() == assembly && UF_PART_ask_display_part() == originalDisplay,
            "Preview must not switch work/display part");
        Require(UF_PART_is_loaded((root / "sample.prt").string().c_str()) == 0,
            "Preview must release a source it loaded");
        Require(Objects(assembly, UF_solid_type, UF_solid_body_subtype).empty() &&
            Objects(assembly, UF_component_type, -1).empty(), "Preview must not create model objects");
        const double rotation[9] = {0,1,0,-1,0,0,0,0,1};
        const auto rotated = TransformPreviewPoint({1,2,3}, {20,40,3}, rotation, 2);
        Require(rotated == std::array<double,3>{16,42,9}, "Preview rotation/translation/scale mismatch");
        Require(std::abs(UnitMillimeters(2) / UnitMillimeters(1) - 25.4) < 1e-9,
            "Preview inch to mm conversion mismatch");
        std::cout << "PASS preview: 12 edges, correct transform, no model objects, source released\n";
        Require(InsertAssembly(&state, item, error), ToAnsi(error).c_str());
        state.placementOrigin[0] = 60;
        Require(InsertAssembly(&state, item, error), ToAnsi(error).c_str());
        auto instances = Objects(assembly, UF_component_type, -1);
        Require(instances.size() == 2, "Expected two assembly instances");
        std::vector<double> placedX;
        for (tag_t instance : instances)
        {
            char partName[MAX_FSPEC_BUFSIZE]{}, refset[UF_OBJ_NAME_BUFSIZE]{};
            char instanceName[UF_CFI_MAX_FILE_NAME_BUFSIZE]{};
            double origin[3]{}, matrix[9]{}, transform[4][4]{};
            Check(UF_ASSEM_ask_component_data(instance, partName, refset,
                instanceName, origin, matrix, transform));
            Require(std::abs(origin[1] - 40) < 1e-6 && std::abs(origin[2] - 3) < 1e-6,
                "Incorrect assembly origin");
            Require(std::string(refset) != "MODEL", "Must not require MODEL reference set");
            placedX.push_back(origin[0]);
        }
        std::sort(placedX.begin(), placedX.end());
        Require(std::abs(placedX[0] - 20) < 1e-6 && std::abs(placedX[1] - 60) < 1e-6,
            "Assembly did not use selected X coordinates");
        Check(UF_PART_save());
        std::cout << "PASS assembly: two selected origins, entire-part reference set\n";

        tag_t merged = NULL_TAG;
        Check(UF_PART_new((root / "merged.prt").string().c_str(), 1, &merged));
        state.placementOrigin[0] = 20;
        Require(InsertBodies(&state, item, error), ToAnsi(error).c_str());
        state.placementOrigin[0] = 60;
        Require(InsertBodies(&state, item, error), ToAnsi(error).c_str());
        auto bodies = Objects(merged, UF_solid_type, UF_solid_body_subtype);
        Require(bodies.size() == 2, "Expected two imported bodies");
        placedX.clear();
        for (tag_t body : bodies)
        {
            double box[6]{};
            Check(UF_MODL_ask_bounding_box(body, box));
            Require(std::abs(box[1] - 40) < 1e-3 && std::abs(box[2] - 3) < 1e-3,
                "Incorrect imported body origin");
            placedX.push_back(box[0]);
        }
        std::sort(placedX.begin(), placedX.end());
        Require(std::abs(placedX[0] - 20) < 1e-3 && std::abs(placedX[1] - 60) < 1e-3,
            "Body import did not use selected X coordinates");
        Check(UF_PART_save());
        std::cout << "PASS multi-body: two selected origins, exactly two bodies\n";
        tag_t cylinderPart = NULL_TAG;
        const fs::path cylinderPath = root / "round_sample.prt";
        Check(UF_PART_new(cylinderPath.string().c_str(), 1, &cylinderPart));
        double axis[3] = {0,0,1};
        char height[] = "5", diameter[] = "4";
        Check(UF_MODL_create_cyl1(UF_NULLSIGN, corner, height, diameter, axis, &feature));
        Check(UF_PART_save());
        Check(UF_PART_close(cylinderPart, 0, 2));
        tag_t previewTarget = NULL_TAG;
        Check(UF_PART_new((root / "preview_target.prt").string().c_str(), 1, &previewTarget));
        const auto roundWireframe = LoadPlacementWireframe(cylinderPath, previewUnits);
        Require(roundWireframe.size() >= 64, "Cylinder preview must tessellate both circular edges");
        Require(Objects(previewTarget, UF_solid_type, UF_solid_body_subtype).empty(),
            "Curved preview must leave target empty");
        std::cout << "PASS curved preview: tessellated circles, no target geometry\n";
        const double identity[9] = {1,0,0,0,1,0,0,0,1};
        bool simplified = false;
        const auto exactPlacement = BuildPlacedPreview(roundWireframe, {{20,40,3}}, identity, 1, simplified);
        Require(!simplified && exactPlacement.size() == roundWireframe.size(),
            "Small previews must retain full geometry");
        std::vector<std::array<double,3>> origins;
        for (int i = 0; i < 100; ++i) origins.push_back({double(i * 10), 0, 0});
        auto bounded = BuildPlacedPreview(roundWireframe, origins, identity, 1, simplified);
        Require(simplified && bounded.size() == 1200, "Large pattern must show a box at every origin");
        const auto roundExtent = PreviewExtent(roundWireframe);
        auto extent = PreviewExtent(bounded);
        Require(std::abs(extent[0] - roundExtent[0]) < 1e-6 &&
            std::abs(extent[3] - (roundExtent[3] + 990)) < 1e-6, "Pattern bounds lost first/last origin");
        for (int i = 100; i < 1000; ++i) origins.push_back({double(i * 10), 0, 0});
        bounded = BuildPlacedPreview(roundWireframe, origins, identity, 1, simplified);
        Require(simplified && bounded.size() == 12, "Very large pattern must retain bounded overall preview");
        extent = PreviewExtent(bounded);
        Require(std::abs(extent[3] - (roundExtent[3] + 9990)) < 1e-6, "Overall preview lost last origin");
        std::vector<PreviewBox> manyBoxes(1000, PreviewBox{0,0,0,1,1,1});
        manyBoxes.back() = {500,0,0,501,1,1};
        const auto manyBodyPreview = PreviewBoxWireframe(manyBoxes);
        Require(manyBodyPreview.size() == 12 && PreviewExtent(manyBodyPreview)[3] == 501,
            "Many-body fallback must include the last body");
        std::cout << "PASS bounded preview: small model intact; 100/1000 placements; all body extents retained\n";
        // Optional real library models are opened read-only and never saved.
        for (int input = 2; input < argc; ++input)
        {
            const fs::path libraryModel = fs::absolute(argv[input]);
            const auto displayBefore = UF_PART_ask_display_part();
            const auto workBefore = UF_ASSEM_ask_work_part();
            const auto realWireframe = LoadPlacementWireframe(libraryModel, previewUnits, &simplified);
            Require(simplified && !realWireframe.empty() && realWireframe.size() <= kPreviewSegmentBudget,
                "Reported complex model must fall back to a nonempty bounded preview");
            Require(UF_PART_ask_display_part() == displayBefore && UF_ASSEM_ask_work_part() == workBefore,
                "Real source preview changed work/display part");
            Require(UF_PART_is_loaded(ToAnsi(libraryModel.wstring()).c_str()) == 0,
                "Real source was not released");
            Require(Objects(previewTarget, UF_solid_type, UF_solid_body_subtype).empty() &&
                Objects(previewTarget, UF_component_type, -1).empty(), "Real preview created model objects");
            std::cout << "PASS reported source " << input - 1 << ": " << realWireframe.size()
                << " simplified segments; no target objects; source released\n";
        }
        TestLibraryContextDelete(root, controls);
        const auto previousWorkPart = UF_ASSEM_ask_work_part();
        OpenLibraryPartForEdit(cylinderPath);
        const auto editPart = UF_PART_ask_part_tag(cylinderPath.string().c_str());
        Require(editPart != NULL_TAG && UF_ASSEM_ask_work_part() == editPart &&
            UF_PART_ask_display_part() == editPart, "Edit must activate requested source PRT");
        Require(UF_PART_ask_part_tag((root / "preview_target.prt").string().c_str()) == previousWorkPart,
            "Edit must keep previous work part loaded");
        const int partCount = UF_PART_ask_num_parts();
        OpenLibraryPartForEdit(cylinderPath);
        Require(UF_PART_ask_num_parts() == partCount && UF_ASSEM_ask_work_part() == editPart,
            "Editing already-loaded source must reuse it");
        std::cout << "PASS edit PRT: activate library source, preserve previous work part, reuse loaded source\n";
        TestBodyCapture(root, controls);
        TestAxisPatternCounts(root);
        TestPreviewReplacement(root);
        DestroyWindow(controls);
        UF_terminate();
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "FAIL: " << ex.what() << "\n";
        if (controls != nullptr) DestroyWindow(controls);
        if (initialized) UF_terminate();
        return 1;
    }
}
