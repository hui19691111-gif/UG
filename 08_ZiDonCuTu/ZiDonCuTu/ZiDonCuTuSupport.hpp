#ifndef ZIDONCUTU_SUPPORT_H_INCLUDED
#define ZIDONCUTU_SUPPORT_H_INCLUDED

#include "ZiDonCuTu.hpp"
#include <string>

void RefreshNxParts();
void LogPluginMessage(const std::string& message);
void ClearSupportPerformanceCaches();
std::string GetPluginRoot();
std::string PluginPath(const char* first, const char* second);
bool RunUserExitLibrary(const char* dllName);
double MeasureMinimumDistance(NXOpen::Part* part, NXOpen::NXObject* first, NXOpen::NXObject* second);
bool MapModelPointToDrawing(
	NXOpen::Drawings::BaseView* baseView,
	const double modelPoint[3],
	double drawingPoint[2]);
bool AreModelPointsCoincidentInDrawing(
	NXOpen::Drawings::BaseView* baseView,
	const double firstPoint[3],
	const double secondPoint[3],
	double tolerance);
std::vector<NXOpen::Face*> CollectAdjacentFaces(
	NXOpen::Part* part,
	NXOpen::NXObject* target,
	const std::vector<NXOpen::Face*>& candidates,
	double tolerance);
bool TryPromoteUniqueDimensionFace(
	std::vector<NXOpen::Face*>& firstFaces,
	std::vector<NXOpen::Face*>& secondFaces,
	std::vector<NXOpen::Face*>& dimensionFaces);
NXOpen::Face* FindFaceAtDistance(
	NXOpen::Part* part,
	NXOpen::NXObject* reference,
	const std::vector<NXOpen::Face*>& candidates,
	double targetDistance,
	double tolerance);

#endif // ZIDONCUTU_SUPPORT_H_INCLUDED
