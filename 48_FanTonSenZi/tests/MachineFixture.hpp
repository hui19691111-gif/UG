#pragma once
// Independent square arc + tangent straight + sharp corner fixture, with two
// circular wall openings on the straight leg after the corner.
inline tag_t MachineFixture(const std::filesystem::path& file){
    tag_t part=0;Check(UF_PART_new(file.string().c_str(),METRIC,&part));
    auto make=[](std::vector<Vec> points,double depth,bool revolve){
        std::vector<tag_t> curves;for(size_t i=0;i<points.size();++i){auto a=points[i],b=points[(i+1)%points.size()];UF_CURVE_line_t line={{a.x,a.y,a.z},{b.x,b.y,b.z}};tag_t c=0;Check(UF_CURVE_create_line(&line,&c));curves.push_back(c);}
        char zero[]="0",quarter[]="90";auto length=std::to_string(depth);char* limits[]={zero,revolve?quarter:length.data()},*offsets[]={zero,zero};double origin[3]={},axis[]={0,0,1};tag_t body=0;
        if(revolve){tag_t* features=nullptr;int count=0;Check(UF_MODL_create_revolution(curves.data(),static_cast<int>(curves.size()),nullptr,limits,offsets,origin,false,true,origin,axis,UF_NULLSIGN,&features,&count));Require(count==1,"Hybrid fixture revolution");Check(UF_MODL_ask_feat_body(features[0],&body));UF_free(features);}
        else{uf_list_p_t list=nullptr,features=nullptr;Check(UF_MODL_create_list(&list));for(auto c:curves)Check(UF_MODL_put_list_item(list,c));Check(UF_MODL_create_extruded(list,zero,limits,origin,axis,UF_NULLSIGN,&features));tag_t feature=0;Check(UF_MODL_ask_list_item(features,0,&feature));Check(UF_MODL_ask_feat_body(feature,&body));UF_MODL_delete_list(&features);UF_MODL_delete_list(&list);}
        return body;
    };
    auto outer=make({{170,0,0},{200,0,0},{200,0,40},{170,0,40}},0,true);
    auto inner=make({{172,0,2},{198,0,2},{198,0,38},{172,0,38}},0,true);
    auto leg=make({{0,200,0},{-150,200,0},{-150,50,0},{-120,50,0},{-120,170,0},{0,170,0}},40,false);
    auto bore=make({{0,198,2},{-148,198,2},{-148,50,2},{-122,50,2},{-122,172,2},{0,172,2}},36,false);
    Check(UF_MODL_unite_bodies(outer,leg));Check(UF_MODL_unite_bodies(inner,bore));tag_t feature=0;Check(UF_MODL_subtract_bodies_with_retained_options(outer,inner,false,false,&feature));
    double origin[]={-135,90,-1},axis[]={0,0,1};char diameter[]="8",length[]="42";tag_t drill=0;Check(UF_MODL_create_cyl1(UF_NULLSIGN,origin,length,diameter,axis,&feature));Check(UF_MODL_ask_feat_body(feature,&drill));Check(UF_MODL_subtract_bodies_with_retained_options(outer,drill,false,false,&feature));
    Check(UF_PART_save());Check(UF_PART_close(part,0,1));return outer;
}
