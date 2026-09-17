// Independent countersunk-head library; shared, tested native construction helpers.
#define wmain SocketLibraryUnusedMain
#include "GenerateGbSocketLibrary.cpp"
#undef wmain

struct FlatSize { int d; double p,dk,s,t,r; int b,min,max,full; };
// GB/T 70.3-2023 / adopted ISO 10642:2019 tables 1-2.
// Actual maximum head diameter, nominal socket key, minimum penetration depth.
const FlatSize flatSizes[]={
    {3,.5,5.81,2,1.10,.1,18,8,30,25},
    {4,.7,7.96,2.5,1.40,.2,20,10,40,25},
    {5,.8,10.07,3,1.75,.2,22,12,50,30},
    {6,1,12.16,4,2.20,.25,24,12,60,35},
    {8,1.25,16.43,5,2.90,.4,28,16,80,45},
    {10,1.5,20.69,6,3.50,.4,32,20,100,50},
    {12,1.75,24.81,8,4.30,.6,36,25,100,60}
};

tag_t BuildFlatScrew(const fs::path& path,const FlatSize& size,int length)
{
    tag_t part=NULL_TAG; CheckPreviewUf(UF_PART_new(ToAnsi(path.wstring()).c_str(),1,&part));
    const auto exp=[&](const char* name,double value){ScrewExpression(std::string(name)+"="+ToAnsi(ParameterNumber(value)));};
    exp("D",size.d); exp("L",length); exp("DK",size.dk); exp("S",size.s);
    exp("T",size.t); exp("P",size.p); exp("R",size.r);
    ScrewExpression("F=0.07*D"); ScrewExpression("H=(DK-D)/2"); ScrewExpression("K=F+H");
    ScrewExpression("B=if (L<="+std::to_string(size.full)+") (L-K-R) else ("+std::to_string(size.b)+")");
    ScrewExpression("C=P/2");
    const double rim=.07*size.d,k=rim+(size.dk-size.d)/2;
    double origin[3]{},down[3]{0,0,-1},coneOrigin[3]{0,0,-rim};
    char rimHeight[]="F",diameter[]="DK",coneHeight[]="H",shaftDiameter[]="D",shaftLength[]="L";
    char* diameters[2]{diameter,shaftDiameter};
    tag_t feature=NULL_TAG,body=NULL_TAG;
    CheckPreviewUf(UF_MODL_create_cyl1(UF_NULLSIGN,origin,rimHeight,diameter,down,&feature));
    CheckPreviewUf(UF_MODL_create_cone1(UF_POSITIVE,coneOrigin,coneHeight,diameters,down,&feature));
    CheckPreviewUf(UF_MODL_create_cyl1(UF_POSITIVE,origin,shaftLength,shaftDiameter,down,&feature));
    CheckPreviewUf(UF_MODL_ask_feat_body(feature,&body));
    UF_OBJ_set_name(body,"GB_T_70_3_COUNTERSUNK_HEAD_SCREW");
    const double corner=(size.s+(size.d==12?.025:.02))/std::sqrt(3.0);
    uf_list_p_t curves=nullptr,features=nullptr; CheckPreviewUf(UF_MODL_create_list(&curves));
    for(int i=0;i<6;++i)
    {
        UF_CURVE_line_t line{}; const double a=i*std::acos(-1.0)/3,b=(i+1)*std::acos(-1.0)/3;
        line.start_point[0]=corner*std::cos(a);line.start_point[1]=corner*std::sin(a);line.start_point[2]=.1;
        line.end_point[0]=corner*std::cos(b);line.end_point[1]=corner*std::sin(b);line.end_point[2]=.1;
        tag_t curve=NULL_TAG;CheckPreviewUf(UF_CURVE_create_line(&line,&curve));
        CheckPreviewUf(UF_MODL_put_list_item(curves,curve)); UF_OBJ_set_blank_status(curve,UF_OBJ_BLANKED);
    }
    char zero[]="0",depth[]="T+0.1";char* limits[2]{zero,depth};
    CheckPreviewUf(UF_MODL_create_extruded(curves,zero,limits,origin,down,UF_NEGATIVE,&features));
    UF_MODL_delete_list(&curves);UF_MODL_delete_list(&features);
    ScrewChamfer(body,-length,size.d/2.0,"C"); ScrewChamfer(body,-k,size.d/2.0,"R",true);
    CheckPreviewUf(UF_MODL_update());
    double box[6]{};CheckPreviewUf(UF_MODL_ask_bounding_box(body,box));
    if(std::abs(box[2]+length)>1e-4 || std::abs(box[5])>1e-4 || std::abs(box[3]-size.dk/2)>1e-4)
        throw std::runtime_error("Flat screw envelope mismatch");
    auto meshPath=path;meshPath.replace_extension(L".stl");auto meshName=ToAnsi(meshPath.wstring());
    char header[]="GB/T 70.3 native NX preview";void* mesh=nullptr;
    CheckPreviewUf(UF_STD_open_binary_stl_file(meshName.data(),false,header,&mesh));
    int errors=0;UF_STD_stl_error_p_t details=nullptr;
    const int code=UF_STD_put_solid_in_stl_file(mesh,NULL_TAG,body,0,size.d/3.0,.015,&errors,&details);
    bool bad=false;for(int i=0;i<errors;++i)if(details[i].error_code!=UF_STD_STL_NEGSPACE)bad=true;
    UF_free(details);CheckPreviewUf(UF_STD_close_stl_file(mesh));CheckPreviewUf(code);
    if(bad)throw std::runtime_error("Flat screw mesh errors");
    ScrewThread(body,length); CheckPreviewUf(UF_MODL_update());CheckPreviewUf(UF_PART_save());return part;
}

int wmain(int argc,wchar_t** argv)
{
    if(argc!=2 || fs::exists(argv[1])){std::cerr<<"Supply new library root\n";return 2;}
    bool initialized=false;
    try
    {
        CheckPreviewUf(UF_initialize());initialized=true;
        AppState state;state.libraryRootOverride=fs::absolute(argv[1]);state.libraryFilter=2;
        std::wstring error;if(!EnsureLibraryFolders(&state,error))throw std::runtime_error(ToAnsi(error));
        for(const auto& size:flatSizes)
        {
            const auto family=L"M"+std::to_wstring(size.d)+L" 平头内六角螺钉";
            const auto directory=state.libraryRootOverride/L"LibParam"/L"国标螺钉"/family;fs::create_directories(directory);
            std::wstring choices;for(int l:screwLengths)if(l>=size.min&&l<=size.max)choices+=(choices.empty()?L"":L";")+std::to_wstring(l);
            const auto schema=L"expression\tlabel\tmode\tchoices\r\nD\t公称直径 D\tfixed\t\r\nL\t含头总长 L\tlist\t"+choices+
                L"\r\nDK\t头部直径 DK\tfixed\t\r\nK\t头部高度 K\tderived\t\r\nS\t扳手对边 S\tfixed\t\r\nT\t内六角深度 T\tfixed\t\r\nP\t螺距 P\tfixed\t\r\nB\t螺纹长度 B\tderived\t\r\n";
            if(!WriteUtf16File(directory/L"parameter-schema.tsv",schema))throw std::runtime_error("Save schema failed");
            std::wstring table=L"SPE\tD\tL\tDK\tK\tS\tT\tP\tB\r\n";
            for(int l:screwLengths)
            {
                if(l<size.min||l>size.max)continue;
                const auto spec=L"M"+std::to_wstring(size.d)+L"x"+std::to_wstring(l);
                const auto model=directory/(spec+L".prt");auto part=BuildFlatScrew(model,size,l);CheckPreviewUf(UF_PART_close(part,0,2));
                const double k=.07*size.d+(size.dk-size.d)/2;
                table+=spec;for(double v:{double(size.d),double(l),size.dk,k,size.s,size.t,size.p,l<=size.full?l-k-size.r:double(size.b)})table+=L"\t"+ParameterNumber(v);
                table+=L"\r\n";state.items.push_back({L"gbt70_3_"+spec,family,L"国标螺钉",fs::relative(model,state.libraryRootOverride).wstring(),spec,true});
                std::cout<<"PASS "<<ToAnsi(spec)<<": countersunk solid, total length, socket, thread, envelope\n"<<std::flush;
            }
            if(!WriteUtf16File(directory/L"parameters.tsv",table))throw std::runtime_error("Save table failed");
        }
        if(!SaveIndex(&state,error))throw std::runtime_error(ToAnsi(error));
        std::cout<<"PASS GB/T 70.3: "<<state.items.size()<<" specifications\n";UF_terminate();return 0;
    }
    catch(const NXOpen::NXException& ex){std::cerr<<"FAIL NX: "<<ex.Message()<<"\n";}
    catch(const std::exception& ex){std::cerr<<"FAIL: "<<ex.what()<<"\n";}
    if(initialized)UF_terminate();return 1;
}
