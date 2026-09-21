#pragma once
#include "SphereGeometry.hpp"
#include <NXOpen/Session.hxx>
namespace NXOpen {namespace Features {class CustomFeature;}}
namespace sphere_unfold {
inline constexpr const char* featureClassName="NXOpen::CustomFeature::QiuMianZanKai";
void RequireFeatureClass();
Settings ReadFeature(NXOpen::Features::CustomFeature*,tag_t& cylinder,tag_t& sphere);
Result CreateFeature(const Plan&,NXOpen::Features::CustomFeature*,NXOpen::Session::UndoMarkId);
}
