#pragma once
#include "TubeGeometry.hpp"
#include <NXOpen/Session.hxx>
namespace NXOpen {namespace Features {class CustomFeature;}}
namespace tube_straighten {
inline constexpr const char* featureClassName="NXOpen::CustomFeature::FanTonSenZi";
struct FeatureResult {tag_t body=0,feature=0;std::vector<tag_t> members;Plan plan;};
void RequireFeatureClass();
Settings ReadFeature(NXOpen::Features::CustomFeature*,tag_t& face);
// Inspect only after NX has rolled back the old construction, since it may
// include cuts on the input tube itself. Caller owns failure/undo transaction.
FeatureResult CreateFeature(tag_t face,const Settings&,NXOpen::Features::CustomFeature*,NXOpen::Session::UndoMarkId);
}
