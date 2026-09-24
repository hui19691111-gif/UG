#pragma once

#include <NXOpen/Body.hxx>
#include <NXOpen/Edge.hxx>
#include <NXOpen/Face.hxx>
#include <uf_modl.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace door_gap_geometry {
inline double Dot(const NXOpen::Vector3d& a, const NXOpen::Vector3d& b) {
    return a.X*b.X+a.Y*b.Y+a.Z*b.Z;
}
inline double Projection(const NXOpen::Point3d& p,
                         const NXOpen::Point3d& origin,
                         const NXOpen::Vector3d& direction) {
    return (p.X-origin.X)*direction.X+
           (p.Y-origin.Y)*direction.Y+
           (p.Z-origin.Z)*direction.Z;
}
inline bool Plane(NXOpen::Face* face, NXOpen::Point3d& point,
                  NXOpen::Vector3d& normal) {
    if (!face || face->SolidFaceType()!=NXOpen::Face::FaceTypePlanar) return false;
    int type=0, sense=1;
    double p[3]{},n[3]{},box[6]{},radius=0,radiusData=0;
    if (UF_MODL_ask_face_data(face->Tag(),&type,p,n,box,&radius,&radiusData,&sense)!=0)
        return false;
    point={p[0],p[1],p[2]};
    normal={n[0]*sense,n[1]*sense,n[2]*sense};
    const double length=std::sqrt(Dot(normal,normal));
    if (length<1.0e-8) return false;
    normal={normal.X/length,normal.Y/length,normal.Z/length};
    return true;
}
inline bool Range(NXOpen::Face* face, const NXOpen::Point3d& origin,
                  const NXOpen::Vector3d& direction,
                  double& minimum,double& maximum) {
    minimum=std::numeric_limits<double>::max();
    maximum=-minimum;
    bool found=false;
    for (auto* edge:face->GetEdges()) {
        if (!edge) continue;
        NXOpen::Point3d a,b; edge->GetVertices(&a,&b);
        for (const auto& p:{a,b}) {
            const double value=Projection(p,origin,direction);
            minimum=(std::min)(minimum,value);
            maximum=(std::max)(maximum,value);
            found=true;
        }
    }
    return found;
}
struct Reference {
    NXOpen::Face* face=nullptr;
    NXOpen::Body* body=nullptr;
    double gap=0;
};
inline Reference FindNearest(const std::vector<NXOpen::Body*>& bodies,
                             tag_t selectedBody,
                             const NXOpen::Point3d& origin,
                             const NXOpen::Vector3d& normal,
                             const NXOpen::Vector3d& outward,
                             const NXOpen::Vector3d& tangent,
                             double boundary,double panelMin,double panelMax,
                             double thickness,double maxGap) {
    Reference result;
    double nearest=std::numeric_limits<double>::max();
    const double span=panelMax-panelMin;
    if (span<=0) return result;
    for (auto* body:bodies) {
        if (!body || body->Tag()==selectedBody || !body->IsSolidBody()) continue;
        for (auto* face:body->GetFaces()) {
            NXOpen::Point3d point; NXOpen::Vector3d faceNormal;
            if (!Plane(face,point,faceNormal) ||
                std::abs(Dot(faceNormal,outward))<0.995) continue;
            const double gap=Projection(point,origin,outward)-boundary;
            if (gap<-0.001 || gap>maxGap || gap>=nearest) continue;
            double minT,maxT,minN,maxN;
            if (!Range(face,origin,tangent,minT,maxT) ||
                !Range(face,origin,normal,minN,maxN)) continue;
            const double overlap=(std::min)(panelMax,maxT)-(std::max)(panelMin,minT);
            const double planeDistance=minN>0?minN:(maxN<0?-maxN:0);
            if (overlap<(std::max)(2.0,span*0.15) ||
                planeDistance>(std::max)(10.0,thickness*4.0)) continue;
            nearest=(std::max)(0.0,gap);
            result={face,body,nearest};
        }
    }
    return result;
}
}
