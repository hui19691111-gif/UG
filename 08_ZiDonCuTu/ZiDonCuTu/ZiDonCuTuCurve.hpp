#ifndef ZIDONCUTU_CURVE_H_INCLUDED
#define ZIDONCUTU_CURVE_H_INCLUDED

#include "ZiDonCuTu.hpp"

std::vector<NXOpen::Drawings::DraftingBody*> CollectDraftingBodies(NXOpen::Drawings::BaseView* baseView);
std::vector<NXOpen::Drawings::DraftingCurve*> CollectDraftingCurves(NXOpen::Drawings::DraftingBody* draftingBody);
std::vector<NXOpen::Drawings::DraftingCurve*> CollectDraftingCurves(NXOpen::Drawings::BaseView* baseView);
NXOpen::Drawings::DraftingBody* FindDraftingBodyForModelBody(NXOpen::Drawings::BaseView* baseView, NXOpen::Body* modelBody);

#endif // ZIDONCUTU_CURVE_H_INCLUDED
