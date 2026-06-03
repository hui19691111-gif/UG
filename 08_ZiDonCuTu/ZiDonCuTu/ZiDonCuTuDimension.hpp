#ifndef ZIDONCUTU_DIMENSION_H_INCLUDED
#define ZIDONCUTU_DIMENSION_H_INCLUDED

#include "ZiDonCuTu.hpp"
#include "ZiDonCuTuCurve.hpp"

NXOpen::Annotations::RapidDimensionBuilder* CreateRapidDimensionBuilder();
void ApplyDefaultRapidDimensionStyle(NXOpen::Annotations::RapidDimensionBuilder* builder);
NXOpen::NXObject* CommitAndDestroyRapidDimension(NXOpen::Annotations::RapidDimensionBuilder* builder);
void ResetDimensionLayoutState();
void RegisterDimensionLayoutObstacles(
	NXOpen::Drawings::BaseView* baseView,
	const std::vector<NXOpen::Drawings::DraftingCurve*>& draftingCurves);
NXOpen::Annotations::IntersectionSymbol* CreateIntersectionSymbol(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Drawings::DraftingCurve* firstCurve,
	NXOpen::Drawings::DraftingCurve* secondCurve);
bool IsValidIntersectionSymbol(
	const std::vector<NXOpen::Annotations::IntersectionSymbol*>& symbols,
	size_t index);
bool CreateCurveEndToSymbolDimensionFromDrawingPoint(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Drawings::DraftingCurve* curve,
	NXOpen::Annotations::IntersectionSymbol* symbol,
	const double drawingPoint[2],
	double centerX,
	double centerY,
	NXOpen::Face* guideFace = NULL,
	bool reverseGuideDirection = false);
bool CreateCurveEndToFaceTangentDimensionFromModelPoint(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Drawings::DraftingCurve* curve,
	NXOpen::Face* face,
	const double modelPoint[3],
	double centerX,
	double centerY,
	NXOpen::Face* guideFace = NULL,
	bool reverseGuideDirection = false);
bool CreateSymbolToSymbolDimensionFromModelPoint(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Annotations::IntersectionSymbol* firstSymbol,
	NXOpen::Annotations::IntersectionSymbol* secondSymbol,
	const double modelPoint[3],
	double centerX,
	double centerY,
	NXOpen::Face* guideFace = NULL,
	bool reverseGuideDirection = false);
bool CreateFaceToSymbolDimensionFromModelPoint(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Face* face,
	NXOpen::Annotations::IntersectionSymbol* symbol,
	const double modelPoint[3],
	double centerX,
	double centerY,
	NXOpen::Face* guideFace = NULL,
	bool reverseGuideDirection = false);
bool CreateFaceToObjectDimensionFromModelPoint(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Face* firstFace,
	NXOpen::DisplayableObject* secondObject,
	const double modelPoint[3],
	double centerX,
	double centerY,
	NXOpen::Face* guideFace = NULL,
	bool reverseGuideDirection = false);
bool CreateFacePairAngleDimensionFromModelPoint(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Face* firstFace,
	NXOpen::Face* secondFace,
	const double modelPoint[3],
	double centerX,
	double centerY,
	bool allowSupplementaryAngle);
bool CreateCurvePairAngleDimensionFromModelPoint(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Drawings::DraftingCurve* firstCurve,
	NXOpen::Drawings::DraftingCurve* secondCurve,
	const double modelPoint[3],
	double centerX,
	double centerY,
	bool allowSupplementaryAngle);
bool TryGetOuterTangentAssociativityForFace(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Face* face,
	double centerX,
	double centerY,
	NXOpen::Drawings::DraftingCurve*& draftingCurve,
	NXOpen::Point3d& tangentPoint);
bool CreateCurveToCurveEnvelopeDimension(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Drawings::DraftingCurve* firstCurve,
	NXOpen::Drawings::DraftingCurve* secondCurve,
	const NXOpen::Point3d& firstPoint,
	const NXOpen::Point3d& secondPoint,
	NXOpen::Annotations::DimensionMeasurementBuilder::MeasurementMethod measurementMethod,
	const NXOpen::Point3d& originPoint,
	NXOpen::InferSnapType::SnapType firstSnapType,
	NXOpen::InferSnapType::SnapType secondSnapType);
bool CreateRadialDimensionAtDrawingPoint(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Face* face,
	const double drawingPoint[2]);
bool CreateFlatPatternHoleCoordinateDimensions(
	NXOpen::Drawings::BaseView* baseView);
bool CreateFlatPatternHoleAttributeNotes(
	NXOpen::Drawings::BaseView* baseView,
	const std::string& markerStart = "");

#endif // ZIDONCUTU_DIMENSION_H_INCLUDED
