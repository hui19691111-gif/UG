// Builds independent native NX parametric library parts, never user work parts.
#include "../StandardPartsLibrary.cpp"
#include <NXOpen/Expression.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <NXOpen/Features_ThreadBuilder.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/SelectDisplayableObject.hxx>
#include <NXOpen/Face.hxx>
#include <uf_std.h>
#include <iostream>

struct ScrewSize
{
    int d;
    double pitch, head, key, keyClearance, depth, radius;
    int b, minLength, maxLength, fullLength;
};

// GB/T 70.1-2008 table 1, printed pages 4-5. Smooth head, coarse pitch.
const ScrewSize screwSizes[] = {
    {3,.5,5.5,2.5,.02,1.3,.1,18,5,30,20},
    {4,.7,7,3,.02,2,.2,20,6,40,25},
    {5,.8,8.5,4,.02,2.5,.2,22,8,50,25},
    {6,1,10,5,.02,3,.25,24,10,60,30},
    {8,1.25,13,6,.02,4,.4,28,12,80,35},
    {10,1.5,16,8,.025,5,.4,32,16,100,40},
    {12,1.75,18,10,.025,6,.6,36,20,120,50}
};
const int screwLengths[] = {5,6,8,10,12,16,20,25,30,35,40,45,50,55,60,65,70,80,90,100,110,120};

void ScrewExpression(const std::string& expression)
{
    tag_t tag = NULL_TAG;
    CheckPreviewUf(UF_MODL_create_exp_tag(expression.c_str(), &tag));
}

std::vector<tag_t> ListTags(uf_list_p_t list)
{
    int count = 0;
    CheckPreviewUf(UF_MODL_ask_list_count(list,&count));
    std::vector<tag_t> result;
    for(int i=0;i<count;++i) {tag_t tag=NULL_TAG; CheckPreviewUf(UF_MODL_ask_list_item(list,i,&tag)); result.push_back(tag);}
    return result;
}

void ScrewThread(tag_t body, int length)
{
    uf_list_p_t faces=nullptr;
    CheckPreviewUf(UF_MODL_ask_body_faces(body,&faces));
    tag_t cylinder=NULL_TAG, start=NULL_TAG;
    for(tag_t face:ListTags(faces))
    {
        int type=0,normal=0;
        double point[3]{},direction[3]{},box[6]{},radius=0,radData=0;
        CheckPreviewUf(UF_MODL_ask_face_data(face,&type,point,direction,box,&radius,&radData,&normal));
        if(type==16 && box[2]<-.5*length) cylinder=face;
        if(type==22 && std::abs(box[2]+length)<1e-5 && std::abs(box[5]+length)<1e-5) start=face;
    }
    UF_MODL_delete_list(&faces);
    if(cylinder==NULL_TAG || start==NULL_TAG) throw std::runtime_error("Screw thread face not found");
    auto* part=NXOpen::Session::GetSession()->Parts()->Work();
    auto* builder=part->Features()->CreateThreadBuilder(nullptr);
    try
    {
        using Thread=NXOpen::Features::ThreadBuilder;
        builder->SetThreadType(Thread::TypeSymbolic);
        builder->CylindricalFace()->SetValue(dynamic_cast<NXOpen::Face*>(NXOpen::NXObjectManager::Get(cylinder)));
        builder->StartObject()->SetValue(dynamic_cast<NXOpen::Face*>(NXOpen::NXObjectManager::Get(start)));
        builder->SetReverseThreadDirection(true); // Start at the tip, thread toward the head (+Z).
        builder->SetThreadInput(Thread::InputManual);
        builder->SetMatchThreadSizeToCylinder(false);
        builder->SetThreadHandedness(Thread::HandednessRightHand);
        builder->SetThreadLimit(Thread::LimitOptionValue);
        builder->MajorDiameterExp()->SetRightHandSide("D");
        builder->MinorDiameterExp()->SetRightHandSide("D-1.226869*P");
        builder->ShaftDiameterExp()->SetRightHandSide("D");
        builder->PitchExp()->SetRightHandSide("P");
        builder->AngleExp()->SetRightHandSide("60");
        builder->ThreadLength()->SetRightHandSide("B");
        CheckPreviewUf(UF_MODL_update());
        builder->Commit();
        builder->Destroy();
    }
    catch(...) {builder->Destroy();throw;}
}

void ScrewChamfer(tag_t body, double z, double radius, const char* offset, bool seatBlend=false)
{
    uf_list_p_t edges=nullptr, chosen=nullptr;
    CheckPreviewUf(UF_MODL_ask_body_edges(body,&edges));
    CheckPreviewUf(UF_MODL_create_list(&chosen));
    for(tag_t edge:ListTags(edges))
    {
        UF_EVAL_p_t evaluator=nullptr;
        CheckPreviewUf(UF_EVAL_initialize(edge,&evaluator));
        logical arc=false;
        CheckPreviewUf(UF_EVAL_is_arc(evaluator,&arc));
        if(arc)
        {
            UF_EVAL_arc_t data{};
            CheckPreviewUf(UF_EVAL_ask_arc(evaluator,&data));
            if(std::abs(data.center[2]-z)<1e-5 && std::abs(data.radius-radius)<1e-5)
                CheckPreviewUf(UF_MODL_put_list_item(chosen,edge));
        }
        UF_EVAL_free(evaluator);
    }
    if(ListTags(chosen).empty()) throw std::runtime_error("Screw chamfer edge not found");
    tag_t feature=NULL_TAG;
    std::string width=offset;
    char zero[]="0";
    if(seatBlend) CheckPreviewUf(UF_MODL_create_blend(offset,chosen,0,0,0,0,&feature));
    else CheckPreviewUf(UF_MODL_create_chamfer(1,width.data(),zero,zero,chosen,&feature));
    UF_MODL_delete_list(&edges); UF_MODL_delete_list(&chosen);
}

tag_t BuildScrew(const fs::path& path, const ScrewSize& size, int length)
{
    tag_t part=NULL_TAG;
    CheckPreviewUf(UF_PART_new(ToAnsi(path.wstring()).c_str(),1,&part));
    const auto expression=[&](const char* name,double value){ScrewExpression(std::string(name)+"="+ToAnsi(ParameterNumber(value)));};
    expression("D",size.d); expression("L",length); expression("DK",size.head);
    expression("K",size.d); expression("S",size.key); expression("T",size.depth);
    expression("P",size.pitch); expression("R",size.radius);
    ScrewExpression("B=if (L<="+std::to_string(size.fullLength)+") (L) else ("+std::to_string(size.b)+")");
    ScrewExpression("C=P/2"); ScrewExpression("HC=D*0.06");
    double origin[3]{},up[3]{0,0,1},down[3]{0,0,-1};
    char height[]="K",diameter[]="DK",shaftLength[]="L",shaftDiameter[]="D";
    tag_t head=NULL_TAG,shaft=NULL_TAG,body=NULL_TAG;
    CheckPreviewUf(UF_MODL_create_cyl1(UF_NULLSIGN,origin,height,diameter,up,&head));
    CheckPreviewUf(UF_MODL_create_cyl1(UF_POSITIVE,origin,shaftLength,shaftDiameter,down,&shaft));
    CheckPreviewUf(UF_MODL_ask_feat_body(shaft,&body));
    UF_OBJ_set_name(body,"GB_T_70_1_SOCKET_HEAD_SCREW");
    const double corner=(size.key+size.keyClearance)/std::sqrt(3.0);
    uf_list_p_t curves=nullptr,features=nullptr;
    CheckPreviewUf(UF_MODL_create_list(&curves));
    for(int i=0;i<6;++i)
    {
        UF_CURVE_line_t line{};
        const double a=i*std::acos(-1.0)/3,b=(i+1)*std::acos(-1.0)/3;
        line.start_point[0]=corner*std::cos(a); line.start_point[1]=corner*std::sin(a); line.start_point[2]=size.d+.1;
        line.end_point[0]=corner*std::cos(b); line.end_point[1]=corner*std::sin(b); line.end_point[2]=size.d+.1;
        tag_t curve=NULL_TAG;
        CheckPreviewUf(UF_CURVE_create_line(&line,&curve));
        CheckPreviewUf(UF_MODL_put_list_item(curves,curve));
        UF_OBJ_set_blank_status(curve,UF_OBJ_BLANKED);
    }
    char zero[]="0",depth[]="T+0.1";
    char* limits[2]{zero,depth};
    CheckPreviewUf(UF_MODL_create_extruded(curves,zero,limits,origin,down,UF_NEGATIVE,&features));
    UF_MODL_delete_list(&curves); UF_MODL_delete_list(&features);
    ScrewChamfer(body,size.d,size.head/2,"HC");
    ScrewChamfer(body,-length,size.d/2.0,"C");
    ScrewChamfer(body,0,size.d/2.0,"R",true);
    CheckPreviewUf(UF_MODL_update());
    double box[6]{}; CheckPreviewUf(UF_MODL_ask_bounding_box(body,box));
    if(std::abs(box[2]+length)>1e-4 || std::abs(box[5]-size.d)>1e-4 || std::abs(box[3]-size.head/2)>1e-4)
        throw std::runtime_error("Generated screw dimensions mismatch");
    auto meshPath=path; meshPath.replace_extension(L".stl");
    auto meshName=ToAnsi(meshPath.wstring());
    char header[]="GB/T 70.1 native NX solid preview";
    void* mesh=nullptr;
    CheckPreviewUf(UF_STD_open_binary_stl_file(meshName.data(),false,header,&mesh));
    int errors=0; UF_STD_stl_error_p_t details=nullptr;
    const int meshCode=UF_STD_put_solid_in_stl_file(mesh,NULL_TAG,body,0,size.d/3.0,.015,&errors,&details);
    bool badMesh=false;
    // The insertion datum deliberately puts the shaft below Z=0. Negative-space
    // warnings concern STL printer coordinates, not missing/nonmanifold facets.
    for(int i=0;i<errors;++i) if(details[i].error_code!=UF_STD_STL_NEGSPACE) badMesh=true;
    UF_free(details);
    const int closeCode=UF_STD_close_stl_file(mesh);
    CheckPreviewUf(meshCode); CheckPreviewUf(closeCode);
    if(badMesh || fs::file_size(meshPath)<=84) throw std::runtime_error("Cannot facet screw preview");
    ScrewThread(body,length);
    CheckPreviewUf(UF_MODL_update());
    CheckPreviewUf(UF_PART_save());
    return part;
}

int wmain(int argc,wchar_t** argv)
{
    if(argc!=2 || fs::exists(argv[1])) {std::cerr<<"Supply a new output library root\n";return 2;}
    bool initialized=false;
    try
    {
        CheckPreviewUf(UF_initialize()); initialized=true;
        AppState state; state.libraryRootOverride=fs::absolute(argv[1]); state.libraryFilter=2;
        std::wstring error; if(!EnsureLibraryFolders(&state,error)) throw std::runtime_error(ToAnsi(error));
        int total=0;
        for(const auto& size:screwSizes)
        {
            const auto family=L"M"+std::to_wstring(size.d)+L" 内六角螺钉";
            const auto directory=state.libraryRootOverride/L"LibParam"/L"国标螺钉"/family;
            fs::create_directories(directory);
            std::wstring choices;
            for(int length:screwLengths)
                if(length>=size.minLength && length<=size.maxLength)
                    choices+=(choices.empty()?L"":L";")+std::to_wstring(length);
            const auto schema=L"expression\tlabel\tmode\tchoices\r\nD\t公称直径 D\tfixed\t\r\nL\t螺钉长度 L\tlist\t"+choices+
                L"\r\nDK\t头部直径 DK\tfixed\t\r\nK\t头部高度 K\tfixed\t\r\nS\t扳手对边 S\tfixed\t\r\nT\t内六角深度 T\tfixed\t\r\nP\t螺距 P\tfixed\t\r\nB\t螺纹长度 B\tderived\t\r\n";
            if(!WriteUtf16File(directory/L"parameter-schema.tsv",schema)) throw std::runtime_error("Cannot save schema");
            std::wstring table=L"SPE\tD\tL\tDK\tK\tS\tT\tP\tB\r\n";
            for(int length:screwLengths)
            {
                if(length<size.minLength || length>size.maxLength) continue;
                const auto specification=L"M"+std::to_wstring(size.d)+L"x"+std::to_wstring(length);
                const auto model=directory/(specification+L".prt");
                const auto part=BuildScrew(model,size,length);
                CheckPreviewUf(UF_PART_close(part,0,2));
                table+=specification;
                for(double value:{static_cast<double>(size.d),static_cast<double>(length),size.head,static_cast<double>(size.d),size.key,size.depth,size.pitch,static_cast<double>(length<=size.fullLength?length:size.b)})
                    table+=L"\t"+ParameterNumber(value);
                table+=L"\r\n";
                state.items.push_back({L"gbt70_1_"+specification,family,L"国标螺钉",fs::relative(model,state.libraryRootOverride).wstring(),specification,true});
                ++total;
                std::cout<<"PASS "<<ToAnsi(specification)<<": parametric solid, socket, symbolic thread, chamfers, envelope\n"<<std::flush;
            }
            if(!WriteUtf16File(directory/L"parameters.tsv",table)) throw std::runtime_error("Cannot save parameter values");
        }
        if(!SaveIndex(&state,error)) throw std::runtime_error(ToAnsi(error));
        std::cout<<"PASS GB/T 70.1 library: "<<total<<" native parametric specifications\n";
        UF_terminate(); return 0;
    }
    catch(const NXOpen::NXException& ex) {std::cerr<<"FAIL NX: "<<ex.Message()<<"\n";}
    catch(const std::exception& ex) {std::cerr<<"FAIL: "<<ex.what()<<"\n";}
    if(initialized) UF_terminate(); return 1;
}
