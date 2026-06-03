#ifndef ZIDONBIAOZHU_DIMENSION_H_INCLUDED
#define ZIDONBIAOZHU_DIMENSION_H_INCLUDED

#include "ZiDonBiaoZhu.hpp"

void AnnotateSideViewDimensionsFromAutomaticFlow(
	NXOpen::Part* workPart,
	NXOpen::Drawings::BaseView* baseView,
	const std::vector<NXOpen::Face*>& outerRadiusFaces,
	const std::vector<NXOpen::Face*>& adjacentPlanarFaces,
	const std::vector<NXOpen::Face*>& innerRadiusFaces,
	const std::vector<NXOpen::Face*>& firstCutRadiusFaces,
	const std::vector<NXOpen::Face*>& allPlanarFaces,
	const std::vector<NXOpen::Face*>& excludedFirstCutFaces,
	const std::vector<NXOpen::Drawings::DraftingCurve*>& secondDimensionCurves,
	const std::vector<NXOpen::Drawings::DraftingCurve*>& draftingCurves,
	double bodyThickness,
	double centerX,
	double centerY);

#endif
