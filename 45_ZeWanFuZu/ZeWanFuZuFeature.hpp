#pragma once
#include "ZeWanFuZuGeometry.hpp"
namespace NXOpen { namespace Features { class CustomFeature; } }
namespace bend_assist {
constexpr const char* featureClass="NXOpen::CustomFeature::ZeWanFuZu";
constexpr const char* internalAttribute="internalFeatures";
constexpr const char* curvesAttribute="profileCurves";
void RequireFeatureClass();
tag_t CreateFeature(const Plan&,const Settings&);
void EditFeature(NXOpen::Features::CustomFeature*,const Settings&);
Settings FeatureSettings(NXOpen::Features::CustomFeature*);
FaceInfo FeatureFace(NXOpen::Features::CustomFeature*);
}
