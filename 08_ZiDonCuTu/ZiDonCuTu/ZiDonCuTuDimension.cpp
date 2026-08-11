#include "ZiDonCuTuDimension.hpp"
#include "ZiDonCuTuSupport.hpp"
#include <NXOpen/Annotations_Annotation.hxx>
#include <NXOpen/Annotations_OrdinateDimension.hxx>
#include <NXOpen/Annotations_OrdinateDimensionBuilder.hxx>
#include <NXOpen/Annotations_HorizontalOrdinateMargin.hxx>
#include <NXOpen/Annotations_DraftingNoteBuilder.hxx>
#include <NXOpen/Annotations_LetteringStyleBuilder.hxx>
#include <NXOpen/Annotations_OrdinateMargin.hxx>
#include <NXOpen/Annotations_OrdinateMarginCollection.hxx>
#include <NXOpen/Annotations_OrdinateOriginDimension.hxx>
#include <NXOpen/Annotations_OriginBuilder.hxx>
#include <NXOpen/Annotations_PlaneBuilder.hxx>
#include <NXOpen/Annotations_RadialDimensionBuilder.hxx>
#include <NXOpen/Annotations_SimpleDraftingAid.hxx>
#include <NXOpen/Annotations_TextWithEditControlsBuilder.hxx>
#include <NXOpen/Annotations_VerticalOrdinateMargin.hxx>
#include <NXOpen/Arc.hxx>
#include <NXOpen/CurveCollection.hxx>
#include <NXOpen/Direction.hxx>
#include <NXOpen/DirectionCollection.hxx>
#include <NXOpen/NXMatrix.hxx>
#include <NXOpen/NXMatrixCollection.hxx>
#include <NXOpen/Point.hxx>
#include <NXOpen/PointCollection.hxx>
#include <NXOpen/SelectDisplayableObjectList.hxx>
#include <NXOpen/SelectNXObjectList.hxx>
#include <NXOpen/SelectView.hxx>
#include <NXOpen/Sketch.hxx>
#include <NXOpen/SketchCollection.hxx>
#include <NXOpen/SketchInDraftingBuilder.hxx>
#include <NXOpen/SketchIncludeGeometryBuilder.hxx>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
constexpr double kDimensionPullInScale = 2.0 / 3.0;
std::unordered_map<std::string, int> g_dimensionLayoutLayers;

void DimensionDebugLog(const std::string& message)
{
	try
	{
		std::ofstream out("D:\\ZiDonCuTu_side_dim_debug.log", std::ios::out | std::ios::app);
		if (out.is_open())
		{
			out << message << std::endl;
		}
	}
	catch (...)
	{
	}
}

struct Segment2d
{
	double x1;
	double y1;
	double x2;
	double y2;
};

std::unordered_map<tag_t, std::vector<Segment2d>> g_dimensionLayoutObstacles;

bool TryGetGuideFaceNormalDrawingDirection(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Face* guideFace,
	double direction2d[2],
	double drawingStart[2]);
double ComputeDrawingCurveProjectedLength(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Drawings::DraftingCurve* draftingCurve);
bool IsLineDraftingCurve(NXOpen::Drawings::DraftingCurve* draftingCurve);
void HoleNoteDebugLog(const std::string& message);
bool TryGetLineCurveData(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Drawings::DraftingCurve* draftingCurve,
	UF_CURVE_line_t& lineData,
	double startDrawingPoint[2],
	double endDrawingPoint[2]);
void Normalize2d(double& x, double& y);

struct AngularPlacementSlot
{
	double vertexX;
	double vertexY;
	double directionX;
	double directionY;
	double distance;
};

std::unordered_map<tag_t, std::vector<AngularPlacementSlot>> g_angularPlacementSlots;

double Clamp01(double value)
{
	if (value < 0.0)
	{
		return 0.0;
	}
	if (value > 1.0)
	{
		return 1.0;
	}
	return value;
}

double PointToSegmentDistance(
	double px,
	double py,
	const Segment2d& segment)
{
	const double vx = segment.x2 - segment.x1;
	const double vy = segment.y2 - segment.y1;
	const double wx = px - segment.x1;
	const double wy = py - segment.y1;
	const double lengthSquared = vx * vx + vy * vy;
	if (lengthSquared < 1e-9)
	{
		return std::sqrt((px - segment.x1) * (px - segment.x1) + (py - segment.y1) * (py - segment.y1));
	}
	const double projection = Clamp01((wx * vx + wy * vy) / lengthSquared);
	const double closestX = segment.x1 + projection * vx;
	const double closestY = segment.y1 + projection * vy;
	const double dx = px - closestX;
	const double dy = py - closestY;
	return std::sqrt(dx * dx + dy * dy);
}

double Cross2d(double ax, double ay, double bx, double by, double cx, double cy)
{
	return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
}

bool BoundingBoxesOverlap(const Segment2d& a, const Segment2d& b, double tolerance)
{
	const double aMinX = std::min(a.x1, a.x2) - tolerance;
	const double aMaxX = std::max(a.x1, a.x2) + tolerance;
	const double aMinY = std::min(a.y1, a.y2) - tolerance;
	const double aMaxY = std::max(a.y1, a.y2) + tolerance;
	const double bMinX = std::min(b.x1, b.x2) - tolerance;
	const double bMaxX = std::max(b.x1, b.x2) + tolerance;
	const double bMinY = std::min(b.y1, b.y2) - tolerance;
	const double bMaxY = std::max(b.y1, b.y2) + tolerance;
	return !(aMaxX < bMinX || bMaxX < aMinX || aMaxY < bMinY || bMaxY < aMinY);
}

bool SegmentsIntersect(const Segment2d& a, const Segment2d& b, double tolerance)
{
	if (!BoundingBoxesOverlap(a, b, tolerance))
	{
		return false;
	}

	const double c1 = Cross2d(a.x1, a.y1, a.x2, a.y2, b.x1, b.y1);
	const double c2 = Cross2d(a.x1, a.y1, a.x2, a.y2, b.x2, b.y2);
	const double c3 = Cross2d(b.x1, b.y1, b.x2, b.y2, a.x1, a.y1);
	const double c4 = Cross2d(b.x1, b.y1, b.x2, b.y2, a.x2, a.y2);
	return (c1 * c2 <= tolerance) && (c3 * c4 <= tolerance);
}

double ComputeObstaclePenalty(
	NXOpen::Drawings::BaseView* baseView,
	const double* drawingPoint,
	const NXOpen::Point3d& candidateOrigin)
{
	if (baseView == NULL || drawingPoint == NULL)
	{
		return 0.0;
	}

	const std::unordered_map<tag_t, std::vector<Segment2d>>::const_iterator iterator =
		g_dimensionLayoutObstacles.find(baseView->Tag());
	if (iterator == g_dimensionLayoutObstacles.end())
	{
		return 0.0;
	}

	const Segment2d route = { drawingPoint[0], drawingPoint[1], candidateOrigin.X, candidateOrigin.Y };
	double penalty = 0.0;
	for (size_t i = 0; i < iterator->second.size(); ++i)
	{
		const Segment2d& obstacle = iterator->second[i];
		const double anchorDistance = PointToSegmentDistance(candidateOrigin.X, candidateOrigin.Y, obstacle);
		const double routeDistance = PointToSegmentDistance(
			(candidateOrigin.X + drawingPoint[0]) * 0.5,
			(candidateOrigin.Y + drawingPoint[1]) * 0.5,
			obstacle);
		if (anchorDistance < 6.0)
		{
			penalty += (6.0 - anchorDistance) * 4.0;
		}
		if (routeDistance < 4.0)
		{
			penalty += (4.0 - routeDistance) * 6.0;
		}
		if (SegmentsIntersect(route, obstacle, 0.01))
		{
			penalty += 50.0;
		}
	}
	return penalty;
}

bool CandidateRouteIntersectsObstacle(
	NXOpen::Drawings::BaseView* baseView,
	const double* drawingPoint,
	const NXOpen::Point3d& candidateOrigin)
{
	if (baseView == NULL || drawingPoint == NULL)
	{
		return false;
	}

	const std::unordered_map<tag_t, std::vector<Segment2d>>::const_iterator iterator =
		g_dimensionLayoutObstacles.find(baseView->Tag());
	if (iterator == g_dimensionLayoutObstacles.end())
	{
		return false;
	}

	const Segment2d route = { drawingPoint[0], drawingPoint[1], candidateOrigin.X, candidateOrigin.Y };
	for (size_t i = 0; i < iterator->second.size(); ++i)
	{
		if (SegmentsIntersect(route, iterator->second[i], 0.01))
		{
			return true;
		}
	}
	return false;
}

bool IsArcDraftingCurve(NXOpen::Drawings::DraftingCurve* draftingCurve)
{
	if (draftingCurve == NULL)
	{
		return false;
	}

	UF_EVAL_p_t evaluator = NULL;
	logical isArc = false;
	if (UF_EVAL_initialize(draftingCurve->Tag(), &evaluator) != 0)
	{
		return false;
	}
	const int askStatus = UF_EVAL_is_arc(evaluator, &isArc);
	UF_EVAL_free(evaluator);
	return askStatus == 0 && isArc;
}

bool CurveReferencesFaceOrEdge(
	NXOpen::Drawings::DraftingCurve* draftingCurve,
	tag_t faceTag,
	const std::unordered_set<tag_t>& edgeTags)
{
	if (draftingCurve == NULL)
	{
		return false;
	}

	const std::vector<NXOpen::NXObject*> parents = draftingCurve->GetDraftingCurveInfo()->GetParents();
	for (size_t i = 0; i < parents.size(); ++i)
	{
		NXOpen::NXObject* parent = parents[i];
		if (parent == NULL)
		{
			continue;
		}
		try
		{
			NXOpen::NXObject* prototype = dynamic_cast<NXOpen::NXObject*>(parent->Prototype());
			if (prototype != NULL)
			{
				parent = prototype;
			}
		}
		catch (...)
		{
		}

		const tag_t parentTag = parent->Tag();
		if (parentTag == faceTag || edgeTags.find(parentTag) != edgeTags.end())
		{
			return true;
		}
	}
	return false;
}

std::unordered_set<tag_t> CollectFaceEdgeTags(NXOpen::Face* face)
{
	std::unordered_set<tag_t> edgeTags;
	if (face == NULL)
	{
		return edgeTags;
	}

	const std::vector<NXOpen::Edge*> edges = face->GetEdges();
	for (size_t i = 0; i < edges.size(); ++i)
	{
		if (edges[i] != NULL)
		{
			edgeTags.insert(edges[i]->Tag());
		}
	}
	return edgeTags;
}

NXOpen::Drawings::DraftingCurve* FindRepresentativeCurveForFace(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Face* face,
	bool preferArc,
	bool useFaceDirection = false,
	bool reverseFaceDirection = false,
	NXOpen::Point3d* selectedPoint = NULL,
	const NXOpen::Point3d* preferredDrawingPoint = NULL,
	const std::vector<NXOpen::Drawings::DraftingCurve*>* candidateCurves = NULL,
	bool requireLineCurve = false)
{
	if (selectedPoint != NULL)
	{
		*selectedPoint = NXOpen::Point3d(0.0, 0.0, 0.0);
	}
	if (baseView == NULL || face == NULL)
	{
		return NULL;
	}

	const std::unordered_set<tag_t> edgeTags = CollectFaceEdgeTags(face);
	NXOpen::Drawings::DraftingCurve* bestCurve = NULL;
	NXOpen::Point3d bestPoint(0.0, 0.0, 0.0);
	double bestScore = -1.0e100;
	double faceDirection[2] = { 0.0, 0.0 };
	double faceDrawingStart[2] = { 0.0, 0.0 };
	const bool hasFaceDirection =
		useFaceDirection &&
		TryGetGuideFaceNormalDrawingDirection(baseView, face, faceDirection, faceDrawingStart);
	if (hasFaceDirection)
	{
		Normalize2d(faceDirection[0], faceDirection[1]);
		if (reverseFaceDirection)
		{
			faceDirection[0] = -faceDirection[0];
			faceDirection[1] = -faceDirection[1];
		}
	}

	const std::vector<NXOpen::Drawings::DraftingCurve*> allCurves =
		candidateCurves != NULL ? std::vector<NXOpen::Drawings::DraftingCurve*>() : CollectDraftingCurves(baseView);
	const std::vector<NXOpen::Drawings::DraftingCurve*>& curves =
		candidateCurves != NULL ? *candidateCurves : allCurves;
	for (size_t i = 0; i < curves.size(); ++i)
	{
		NXOpen::Drawings::DraftingCurve* candidate = curves[i];
		if (candidate == NULL)
		{
			continue;
		}
		if (!CurveReferencesFaceOrEdge(candidate, face->Tag(), edgeTags))
		{
			continue;
		}

		const bool candidateIsLine = IsLineDraftingCurve(candidate);
		const bool candidateIsArc = IsArcDraftingCurve(candidate);
		if (requireLineCurve && !candidateIsLine)
		{
			DimensionDebugLog(std::string("[dimension.faceCurve.skipNonLine] curveTag=") + std::to_string(candidate->Tag()));
			continue;
		}

		NXOpen::Point3d candidatePoint(0.0, 0.0, 0.0);
		NXOpen::Point3d candidateAssociativityPoint(0.0, 0.0, 0.0);
		bool hasCandidatePoint = false;
		if (candidateIsLine)
		{
			UF_CURVE_line_t lineData;
			double startPoint[2] = { 0.0, 0.0 };
			double endPoint[2] = { 0.0, 0.0 };
			if (TryGetLineCurveData(baseView, candidate, lineData, startPoint, endPoint))
			{
				const double projectedDx = endPoint[0] - startPoint[0];
				const double projectedDy = endPoint[1] - startPoint[1];
				const double projectedLength = std::sqrt(projectedDx * projectedDx + projectedDy * projectedDy);
				if (projectedLength <= 1.0e-6)
				{
					DimensionDebugLog(std::string("[dimension.faceCurve.skipZeroLength] curveTag=") + std::to_string(candidate->Tag()));
					continue;
				}
				candidatePoint = NXOpen::Point3d(
					(startPoint[0] + endPoint[0]) * 0.5,
					(startPoint[1] + endPoint[1]) * 0.5,
					0.0);
				candidateAssociativityPoint = NXOpen::Point3d(
					(lineData.start_point[0] + lineData.end_point[0]) * 0.5,
					(lineData.start_point[1] + lineData.end_point[1]) * 0.5,
					(lineData.start_point[2] + lineData.end_point[2]) * 0.5);
				hasCandidatePoint = true;
			}
		}
		else if (candidateIsArc)
		{
			UF_EVAL_p_t evaluator = NULL;
			if (UF_EVAL_initialize(candidate->Tag(), &evaluator) == 0)
			{
				UF_EVAL_arc_t arcData;
				if (UF_EVAL_ask_arc(evaluator, &arcData) == 0)
				{
					double center[2] = { 0.0, 0.0 };
					if (UF_VIEW_map_model_to_drawing(baseView->Tag(), arcData.center, center) == 0)
					{
						candidatePoint = NXOpen::Point3d(center[0], center[1], 0.0);
						candidateAssociativityPoint = NXOpen::Point3d(
							arcData.center[0],
							arcData.center[1],
							arcData.center[2]);
						hasCandidatePoint = true;
					}
				}
				UF_EVAL_free(evaluator);
			}
		}

		if (!hasCandidatePoint)
		{
			continue;
		}

		double score = 0.0;
		if (preferArc && candidateIsArc)
		{
			score += 1000000.0;
		}
		else if (!preferArc && candidateIsLine)
		{
			score += 1000000.0;
		}

		if (hasFaceDirection)
		{
			const double directionalScore =
				(candidatePoint.X - faceDrawingStart[0]) * faceDirection[0] +
				(candidatePoint.Y - faceDrawingStart[1]) * faceDirection[1];
			score += directionalScore * 1000.0;
		}
		if (preferredDrawingPoint != NULL)
		{
			const double dx = candidatePoint.X - preferredDrawingPoint->X;
			const double dy = candidatePoint.Y - preferredDrawingPoint->Y;
			score -= (dx * dx + dy * dy) * 10000.0;
		}
		score += ComputeDrawingCurveProjectedLength(baseView, candidate);

		if (score > bestScore)
		{
			bestScore = score;
			bestCurve = candidate;
			bestPoint = candidateAssociativityPoint;
		}
	}
	if (selectedPoint != NULL)
	{
		*selectedPoint = bestPoint;
	}
	return bestCurve;
}

NXOpen::Drawings::DraftingCurve* FindAssociativeLineCurveForFace(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Face* face,
	bool reverseFaceDirection,
	NXOpen::Point3d* selectedPoint,
	const NXOpen::Point3d* preferredDrawingPoint,
	const std::vector<NXOpen::Drawings::DraftingCurve*>* candidateCurves = NULL)
{
	if (selectedPoint != NULL)
	{
		*selectedPoint = NXOpen::Point3d(0.0, 0.0, 0.0);
	}
	if (baseView == NULL || face == NULL)
	{
		return NULL;
	}

	const std::unordered_set<tag_t> edgeTags = CollectFaceEdgeTags(face);
	if (edgeTags.empty())
	{
		return NULL;
	}

	const std::vector<NXOpen::Drawings::DraftingCurve*> allViewCurves =
		candidateCurves != NULL ? std::vector<NXOpen::Drawings::DraftingCurve*>() : CollectDraftingCurves(baseView);
	const std::vector<NXOpen::Drawings::DraftingCurve*>& sourceCurves =
		candidateCurves != NULL ? *candidateCurves : allViewCurves;
	std::vector<NXOpen::Drawings::DraftingCurve*> edgeReferencedCurves;
	for (size_t i = 0; i < sourceCurves.size(); ++i)
	{
		NXOpen::Drawings::DraftingCurve* candidate = sourceCurves[i];
		if (candidate != NULL &&
			CurveReferencesFaceOrEdge(candidate, NULL_TAG, edgeTags))
		{
			edgeReferencedCurves.push_back(candidate);
		}
	}

	NXOpen::Drawings::DraftingCurve* curve = FindRepresentativeCurveForFace(
		baseView,
		face,
		false,
		true,
		reverseFaceDirection,
		selectedPoint,
		preferredDrawingPoint,
		&edgeReferencedCurves,
		true);
	DimensionDebugLog(
		std::string("[dimension.assoc.edgeLineForFace] faceTag=") +
		std::to_string(face->Tag()) +
		" edgeCandidateCount=" + std::to_string(edgeReferencedCurves.size()) +
		" selectedCurveTag=" + std::to_string(curve != NULL ? curve->Tag() : NULL_TAG));
	return curve;
}

NXOpen::Drawings::DraftingCurve* FindRepresentativeCurveForObject(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::DisplayableObject* object)
{
	if (baseView == NULL || object == NULL)
	{
		return NULL;
	}

	NXOpen::Drawings::DraftingCurve* draftingCurve = dynamic_cast<NXOpen::Drawings::DraftingCurve*>(object);
	if (draftingCurve != NULL)
	{
		return draftingCurve;
	}

	NXOpen::Face* face = dynamic_cast<NXOpen::Face*>(object);
	if (face != NULL)
	{
		return FindRepresentativeCurveForFace(baseView, face, false);
	}

	NXOpen::Edge* edge = dynamic_cast<NXOpen::Edge*>(object);
	if (edge == NULL)
	{
		return NULL;
	}

	std::unordered_set<tag_t> edgeTags;
	edgeTags.insert(edge->Tag());
	const std::vector<NXOpen::Drawings::DraftingCurve*> curves = CollectDraftingCurves(baseView);
	for (size_t i = 0; i < curves.size(); ++i)
	{
		if (CurveReferencesFaceOrEdge(curves[i], NULL_TAG, edgeTags))
		{
			return curves[i];
		}
	}
	return NULL;
}

bool IsLineDraftingCurve(NXOpen::Drawings::DraftingCurve* draftingCurve)
{
	if (draftingCurve == NULL)
	{
		return false;
	}

	UF_EVAL_p_t evaluator = NULL;
	logical isLine = false;
	if (UF_EVAL_initialize(draftingCurve->Tag(), &evaluator) != 0)
	{
		return false;
	}
	const int askStatus = UF_EVAL_is_line(evaluator, &isLine);
	UF_EVAL_free(evaluator);
	return askStatus == 0 && isLine;
}

double ComputeDrawingCurveProjectedLength(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Drawings::DraftingCurve* draftingCurve)
{
	if (baseView == NULL || draftingCurve == NULL)
	{
		return 0.0;
	}

	UF_EVAL_p_t evaluator = NULL;
	if (UF_EVAL_initialize(draftingCurve->Tag(), &evaluator) != 0)
	{
		return 0.0;
	}

	UF_EVAL_line_t lineData;
	const int askStatus = UF_EVAL_ask_line(evaluator, &lineData);
	UF_EVAL_free(evaluator);
	if (askStatus != 0)
	{
		return 0.0;
	}

	double startPoint[2] = { 0.0, 0.0 };
	double endPoint[2] = { 0.0, 0.0 };
	if (UF_VIEW_map_model_to_drawing(baseView->Tag(), lineData.start, startPoint) != 0 ||
		UF_VIEW_map_model_to_drawing(baseView->Tag(), lineData.end, endPoint) != 0)
	{
		return 0.0;
	}

	const double dx = endPoint[0] - startPoint[0];
	const double dy = endPoint[1] - startPoint[1];
	return std::sqrt(dx * dx + dy * dy);
}

bool TryGetLineCurveAnchorPoint(
	NXOpen::Drawings::DraftingCurve* draftingCurve,
	NXOpen::Point3d& anchorPoint)
{
	anchorPoint = NXOpen::Point3d(0.0, 0.0, 0.0);
	if (draftingCurve == NULL)
	{
		return false;
	}

	UF_CURVE_line_t lineData;
	if (UF_CURVE_ask_line_data(draftingCurve->Tag(), &lineData) != 0)
	{
		return false;
	}

	anchorPoint = NXOpen::Point3d(
		lineData.start_point[0],
		lineData.start_point[1],
		lineData.start_point[2]);
	return true;
}

bool TryGetLineCurveData(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Drawings::DraftingCurve* draftingCurve,
	UF_CURVE_line_t& lineData,
	double startDrawingPoint[2],
	double endDrawingPoint[2])
{
	if (baseView == NULL || draftingCurve == NULL || startDrawingPoint == NULL || endDrawingPoint == NULL)
	{
		return false;
	}
	if (UF_CURVE_ask_line_data(draftingCurve->Tag(), &lineData) != 0)
	{
		return false;
	}
	return UF_VIEW_map_model_to_drawing(baseView->Tag(), lineData.start_point, startDrawingPoint) == 0 &&
		UF_VIEW_map_model_to_drawing(baseView->Tag(), lineData.end_point, endDrawingPoint) == 0;
}

bool TryGetDraftingCurveEndpointNearestDrawingPoint(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Drawings::DraftingCurve* draftingCurve,
	const NXOpen::Point3d& referenceDrawingPoint,
	NXOpen::Point3d& nearestEndpoint,
	NXOpen::InferSnapType::SnapType& nearestSnapType)
{
	nearestEndpoint = NXOpen::Point3d(0.0, 0.0, 0.0);
	nearestSnapType = NXOpen::InferSnapType::SnapTypeExist;
	if (baseView == NULL || draftingCurve == NULL)
	{
		return false;
	}

	UF_EVAL_p_t evaluator = NULL;
	if (UF_EVAL_initialize(draftingCurve->Tag(), &evaluator) != 0 || evaluator == NULL)
	{
		return false;
	}

	double limits[2] = { 0.0, 0.0 };
	if (UF_EVAL_ask_limits(evaluator, limits) != 0)
	{
		UF_EVAL_free(evaluator);
		return false;
	}

	double modelEndpoints[2][3] = {};
	double derivatives[3] = { 0.0, 0.0, 0.0 };
	if (UF_EVAL_evaluate(evaluator, 0, limits[0], modelEndpoints[0], derivatives) != 0 ||
		UF_EVAL_evaluate(evaluator, 0, limits[1], modelEndpoints[1], derivatives) != 0)
	{
		UF_EVAL_free(evaluator);
		return false;
	}
	UF_EVAL_free(evaluator);

	double drawingEndpoints[2][2] = {};
	if (UF_VIEW_map_model_to_drawing(baseView->Tag(), modelEndpoints[0], drawingEndpoints[0]) != 0 ||
		UF_VIEW_map_model_to_drawing(baseView->Tag(), modelEndpoints[1], drawingEndpoints[1]) != 0)
	{
		return false;
	}

	const double firstDx = drawingEndpoints[0][0] - referenceDrawingPoint.X;
	const double firstDy = drawingEndpoints[0][1] - referenceDrawingPoint.Y;
	const double secondDx = drawingEndpoints[1][0] - referenceDrawingPoint.X;
	const double secondDy = drawingEndpoints[1][1] - referenceDrawingPoint.Y;
	const int nearestIndex =
		(firstDx * firstDx + firstDy * firstDy) <= (secondDx * secondDx + secondDy * secondDy)
		? 0
		: 1;
	nearestEndpoint = NXOpen::Point3d(
		modelEndpoints[nearestIndex][0],
		modelEndpoints[nearestIndex][1],
		modelEndpoints[nearestIndex][2]);
	nearestSnapType = nearestIndex == 0
		? NXOpen::InferSnapType::SnapTypeStart
		: NXOpen::InferSnapType::SnapTypeEnd;
	return true;
}

bool TryIntersectDrawingLines(
	const double firstStart[2],
	const double firstEnd[2],
	const double secondStart[2],
	const double secondEnd[2],
	double intersection[2])
{
	if (firstStart == NULL || firstEnd == NULL || secondStart == NULL || secondEnd == NULL || intersection == NULL)
	{
		return false;
	}

	const double x1 = firstStart[0];
	const double y1 = firstStart[1];
	const double x2 = firstEnd[0];
	const double y2 = firstEnd[1];
	const double x3 = secondStart[0];
	const double y3 = secondStart[1];
	const double x4 = secondEnd[0];
	const double y4 = secondEnd[1];
	const double denominator = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
	if (std::fabs(denominator) < 1e-9)
	{
		return false;
	}

	intersection[0] =
		((x1 * y2 - y1 * x2) * (x3 - x4) - (x1 - x2) * (x3 * y4 - y3 * x4)) / denominator;
	intersection[1] =
		((x1 * y2 - y1 * x2) * (y3 - y4) - (y1 - y2) * (x3 * y4 - y3 * x4)) / denominator;
	return true;
}

double Distance2d(double ax, double ay, double bx, double by)
{
	const double dx = ax - bx;
	const double dy = ay - by;
	return std::sqrt(dx * dx + dy * dy);
}

void Normalize2d(double& x, double& y)
{
	const double length = std::sqrt(x * x + y * y);
	if (length < 1e-9)
	{
		x = 0.0;
		y = 0.0;
		return;
	}
	x /= length;
	y /= length;
}

bool TryMeasureDrawingLineAngleDegrees(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Drawings::DraftingCurve* firstCurve,
	NXOpen::Drawings::DraftingCurve* secondCurve,
	double& angleDegrees)
{
	angleDegrees = 0.0;
	UF_CURVE_line_t firstLineData;
	UF_CURVE_line_t secondLineData;
	double firstStart[2] = { 0.0, 0.0 };
	double firstEnd[2] = { 0.0, 0.0 };
	double secondStart[2] = { 0.0, 0.0 };
	double secondEnd[2] = { 0.0, 0.0 };
	if (!TryGetLineCurveData(baseView, firstCurve, firstLineData, firstStart, firstEnd) ||
		!TryGetLineCurveData(baseView, secondCurve, secondLineData, secondStart, secondEnd))
	{
		return false;
	}

	double firstX = firstEnd[0] - firstStart[0];
	double firstY = firstEnd[1] - firstStart[1];
	double secondX = secondEnd[0] - secondStart[0];
	double secondY = secondEnd[1] - secondStart[1];
	Normalize2d(firstX, firstY);
	Normalize2d(secondX, secondY);
	if ((std::fabs(firstX) < 1.0e-9 && std::fabs(firstY) < 1.0e-9) ||
		(std::fabs(secondX) < 1.0e-9 && std::fabs(secondY) < 1.0e-9))
	{
		return false;
	}

	const double dot = std::max(-1.0, std::min(1.0, std::fabs(firstX * secondX + firstY * secondY)));
	angleDegrees = std::acos(dot) * 180.0 / PI;
	return true;
}

void Rotate2d(double x, double y, double radians, double& rotatedX, double& rotatedY)
{
	const double cosine = std::cos(radians);
	const double sine = std::sin(radians);
	rotatedX = x * cosine - y * sine;
	rotatedY = x * sine + y * cosine;
}

double AngularPlacementObstaclePenalty(
	NXOpen::Drawings::BaseView* baseView,
	const double intersection[2],
	const NXOpen::Point3d& candidateOrigin)
{
	if (baseView == NULL || intersection == NULL)
	{
		return 0.0;
	}

	const std::unordered_map<tag_t, std::vector<Segment2d>>::const_iterator iterator =
		g_dimensionLayoutObstacles.find(baseView->Tag());
	if (iterator == g_dimensionLayoutObstacles.end())
	{
		return 0.0;
	}

	const Segment2d route = { intersection[0], intersection[1], candidateOrigin.X, candidateOrigin.Y };
	double penalty = 0.0;
	for (size_t i = 0; i < iterator->second.size(); ++i)
	{
		const Segment2d& obstacle = iterator->second[i];
		const bool startsAtAngleVertex =
			Distance2d(obstacle.x1, obstacle.y1, intersection[0], intersection[1]) < 1.0 ||
			Distance2d(obstacle.x2, obstacle.y2, intersection[0], intersection[1]) < 1.0;
		const double textDistance = PointToSegmentDistance(candidateOrigin.X, candidateOrigin.Y, obstacle);
		const double routeMidDistance = PointToSegmentDistance(
			(candidateOrigin.X + intersection[0]) * 0.5,
			(candidateOrigin.Y + intersection[1]) * 0.5,
			obstacle);

		if (!startsAtAngleVertex && textDistance < 12.0)
		{
			penalty += (12.0 - textDistance) * 10.0;
		}
		if (!startsAtAngleVertex && routeMidDistance < 6.0)
		{
			penalty += (6.0 - routeMidDistance) * 8.0;
		}
		if (!startsAtAngleVertex && SegmentsIntersect(route, obstacle, 0.01))
		{
			penalty += 200.0;
		}
	}
	return penalty;
}

double RequiredAngularDistanceForNearbySlots(
	NXOpen::Drawings::BaseView* baseView,
	const double intersection[2],
	double directionX,
	double directionY)
{
	if (baseView == NULL || intersection == NULL)
	{
		return 5.0;
	}

	Normalize2d(directionX, directionY);
	if (std::fabs(directionX) < 1e-9 && std::fabs(directionY) < 1e-9)
	{
		return 5.0;
	}

	const std::unordered_map<tag_t, std::vector<AngularPlacementSlot>>::const_iterator iterator =
		g_angularPlacementSlots.find(baseView->Tag());
	if (iterator == g_angularPlacementSlots.end())
	{
		return 5.0;
	}

	int nearbySimilarCount = 0;
	for (size_t i = 0; i < iterator->second.size(); ++i)
	{
		const AngularPlacementSlot& slot = iterator->second[i];
		const double vertexDistance = Distance2d(slot.vertexX, slot.vertexY, intersection[0], intersection[1]);
		const double directionDot = slot.directionX * directionX + slot.directionY * directionY;
		if (vertexDistance < 18.0 && directionDot > 0.70)
		{
			++nearbySimilarCount;
		}
	}

	return std::min(20.0, 5.0 + nearbySimilarCount * 7.5);
}

NXOpen::Point3d ChooseAngularOriginFromBisector(
	NXOpen::Drawings::BaseView* baseView,
	const double intersection[2],
	double bisectorX,
	double bisectorY)
{
	Normalize2d(bisectorX, bisectorY);
	if (std::fabs(bisectorX) < 1e-9 && std::fabs(bisectorY) < 1e-9)
	{
		return NXOpen::Point3d(intersection[0] + 8.0, intersection[1] + 8.0, 0.0);
	}

	const double angleOffsets[] = { 0.0, 0.2617993878, -0.2617993878, 0.4363323130, -0.4363323130 };
	const double distances[] = { 5.0 * kDimensionPullInScale, 7.0 * kDimensionPullInScale,
		9.0 * kDimensionPullInScale, 12.0 * kDimensionPullInScale,
		16.0 * kDimensionPullInScale, 20.0 * kDimensionPullInScale };
	bool hasBest = false;
	double bestScore = 0.0;
	NXOpen::Point3d bestOrigin(intersection[0] + bisectorX * 8.0 * kDimensionPullInScale,
		intersection[1] + bisectorY * 8.0 * kDimensionPullInScale, 0.0);

	for (size_t angleIndex = 0; angleIndex < sizeof(angleOffsets) / sizeof(angleOffsets[0]); ++angleIndex)
	{
		double directionX = 0.0;
		double directionY = 0.0;
		Rotate2d(bisectorX, bisectorY, angleOffsets[angleIndex], directionX, directionY);
		Normalize2d(directionX, directionY);
		const double requiredDistance = RequiredAngularDistanceForNearbySlots(baseView, intersection, directionX, directionY);
		for (size_t distanceIndex = 0; distanceIndex < sizeof(distances) / sizeof(distances[0]); ++distanceIndex)
		{
			const double distance = distances[distanceIndex];
			NXOpen::Point3d candidate(
				intersection[0] + directionX * distance,
				intersection[1] + directionY * distance,
				0.0);
			const double score =
				AngularPlacementObstaclePenalty(baseView, intersection, candidate) +
				(distance + 1.0e-6 < requiredDistance ? 10000.0 + (requiredDistance - distance) * 100.0 : 0.0) +
				std::fabs(angleOffsets[angleIndex]) * 10.0 +
				distance * 0.25;
			if (!hasBest || score < bestScore)
			{
				hasBest = true;
				bestScore = score;
				bestOrigin = candidate;
			}
		}
	}
	return bestOrigin;
}

void PickLinePointNearIntersection(
	const UF_CURVE_line_t& lineData,
	const double startDrawingPoint[2],
	const double endDrawingPoint[2],
	const double intersection[2],
	NXOpen::Point3d& anchorPoint,
	double directionX,
	double directionY)
{
	const double startDistance = Distance2d(startDrawingPoint[0], startDrawingPoint[1], intersection[0], intersection[1]);
	const double endDistance = Distance2d(endDrawingPoint[0], endDrawingPoint[1], intersection[0], intersection[1]);
	if (startDistance <= endDistance)
	{
		anchorPoint = NXOpen::Point3d(
			lineData.start_point[0],
			lineData.start_point[1],
			lineData.start_point[2]);
		directionX = startDrawingPoint[0] - intersection[0];
		directionY = startDrawingPoint[1] - intersection[1];
	}
	else
	{
		anchorPoint = NXOpen::Point3d(
			lineData.end_point[0],
			lineData.end_point[1],
			lineData.end_point[2]);
		directionX = endDrawingPoint[0] - intersection[0];
		directionY = endDrawingPoint[1] - intersection[1];
	}
}

bool TryBuildInsideAnglePlacement(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Drawings::DraftingCurve* firstCurve,
	NXOpen::Drawings::DraftingCurve* secondCurve,
	NXOpen::Point3d& firstAnchorPoint,
	NXOpen::Point3d& secondAnchorPoint,
	NXOpen::Point3d& originPoint)
{
	UF_CURVE_line_t firstLineData;
	UF_CURVE_line_t secondLineData;
	double firstStart[2] = { 0.0, 0.0 };
	double firstEnd[2] = { 0.0, 0.0 };
	double secondStart[2] = { 0.0, 0.0 };
	double secondEnd[2] = { 0.0, 0.0 };
	if (!TryGetLineCurveData(baseView, firstCurve, firstLineData, firstStart, firstEnd) ||
		!TryGetLineCurveData(baseView, secondCurve, secondLineData, secondStart, secondEnd))
	{
		return false;
	}

	double intersection[2] = { 0.0, 0.0 };
	if (!TryIntersectDrawingLines(firstStart, firstEnd, secondStart, secondEnd, intersection))
	{
		return false;
	}

	const bool firstStartIsNear =
		Distance2d(firstStart[0], firstStart[1], intersection[0], intersection[1]) <=
		Distance2d(firstEnd[0], firstEnd[1], intersection[0], intersection[1]);
	const bool secondStartIsNear =
		Distance2d(secondStart[0], secondStart[1], intersection[0], intersection[1]) <=
		Distance2d(secondEnd[0], secondEnd[1], intersection[0], intersection[1]);

	firstAnchorPoint = firstStartIsNear
		? NXOpen::Point3d(firstLineData.start_point[0], firstLineData.start_point[1], firstLineData.start_point[2])
		: NXOpen::Point3d(firstLineData.end_point[0], firstLineData.end_point[1], firstLineData.end_point[2]);
	secondAnchorPoint = secondStartIsNear
		? NXOpen::Point3d(secondLineData.start_point[0], secondLineData.start_point[1], secondLineData.start_point[2])
		: NXOpen::Point3d(secondLineData.end_point[0], secondLineData.end_point[1], secondLineData.end_point[2]);

	double firstDirectionX = (firstStartIsNear ? firstEnd[0] : firstStart[0]) - intersection[0];
	double firstDirectionY = (firstStartIsNear ? firstEnd[1] : firstStart[1]) - intersection[1];
	double secondDirectionX = (secondStartIsNear ? secondEnd[0] : secondStart[0]) - intersection[0];
	double secondDirectionY = (secondStartIsNear ? secondEnd[1] : secondStart[1]) - intersection[1];
	Normalize2d(firstDirectionX, firstDirectionY);
	Normalize2d(secondDirectionX, secondDirectionY);

	double bisectorX = firstDirectionX + secondDirectionX;
	double bisectorY = firstDirectionY + secondDirectionY;
	if (std::fabs(bisectorX) < 1e-9 && std::fabs(bisectorY) < 1e-9)
	{
		bisectorX = -firstDirectionY;
		bisectorY = firstDirectionX;
	}
	Normalize2d(bisectorX, bisectorY);

	originPoint = ChooseAngularOriginFromBisector(baseView, intersection, bisectorX, bisectorY);
	return true;
}

NXOpen::Drawings::DraftingCurve* FindRepresentativeLineCurveForFace(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Face* face)
{
	if (baseView == NULL || face == NULL)
	{
		return NULL;
	}

	std::unordered_set<tag_t> edgeTags;
	const std::vector<NXOpen::Edge*> edges = face->GetEdges();
	for (size_t i = 0; i < edges.size(); ++i)
	{
		if (edges[i] != NULL)
		{
			edgeTags.insert(edges[i]->Tag());
		}
	}

	NXOpen::Drawings::DraftingCurve* bestCurve = NULL;
	double bestLength = 0.0;
	const std::vector<NXOpen::Drawings::DraftingCurve*> curves = CollectDraftingCurves(baseView);
	for (size_t i = 0; i < curves.size(); ++i)
	{
		NXOpen::Drawings::DraftingCurve* candidate = curves[i];
		if (!IsLineDraftingCurve(candidate) || !CurveReferencesFaceOrEdge(candidate, face->Tag(), edgeTags))
		{
			continue;
		}

		const double length = ComputeDrawingCurveProjectedLength(baseView, candidate);
		if (length > bestLength && length > 0.5)
		{
			bestLength = length;
			bestCurve = candidate;
		}
	}

	return bestCurve;
}

bool TryFindArcExtremeDrawingPoint(
	NXOpen::Drawings::DraftingCurve* curve,
	double directionX,
	double directionY,
	NXOpen::Point3d& extremePoint)
{
	extremePoint = NXOpen::Point3d(0.0, 0.0, 0.0);
	if (curve == NULL)
	{
		return false;
	}

	UF_EVAL_p_t evaluator = NULL;
	if (UF_EVAL_initialize(curve->Tag(), &evaluator) != 0)
	{
		return false;
	}

	logical isArc = false;
	UF_EVAL_is_arc(evaluator, &isArc);
	if (!isArc)
	{
		UF_EVAL_free(evaluator);
		return false;
	}

	double limits[2] = { 0.0, 0.0 };
	if (UF_EVAL_ask_limits(evaluator, limits) != 0 || std::fabs(limits[1] - limits[0]) < 1.0e-8)
	{
		UF_EVAL_free(evaluator);
		return false;
	}

	bool found = false;
	double bestScore = -1.0e100;
	const int sampleCount = 96;
	for (int i = 0; i <= sampleCount; ++i)
	{
		const double t = limits[0] + (limits[1] - limits[0]) * static_cast<double>(i) / static_cast<double>(sampleCount);
		double pointOnCurve[3] = { 0.0, 0.0, 0.0 };
		double derivatives[3] = { 0.0, 0.0, 0.0 };
		if (UF_EVAL_evaluate(evaluator, 0, t, pointOnCurve, derivatives) != 0)
		{
			continue;
		}

		const double score = pointOnCurve[0] * directionX + pointOnCurve[1] * directionY;
		if (!found || score > bestScore)
		{
			found = true;
			bestScore = score;
			extremePoint = NXOpen::Point3d(pointOnCurve[0], pointOnCurve[1], 0.0);
		}
	}

	UF_EVAL_free(evaluator);
	return found;
}

double PointToLineDistanceSquared2d(
	const NXOpen::Point3d& point,
	const double lineStart[2],
	const double lineEnd[2])
{
	const double vx = lineEnd[0] - lineStart[0];
	const double vy = lineEnd[1] - lineStart[1];
	const double lengthSquared = vx * vx + vy * vy;
	if (lengthSquared < 1.0e-12)
	{
		const double dx = point.X - lineStart[0];
		const double dy = point.Y - lineStart[1];
		return dx * dx + dy * dy;
	}

	const double wx = point.X - lineStart[0];
	const double wy = point.Y - lineStart[1];
	const double cross = vx * wy - vy * wx;
	return (cross * cross) / lengthSquared;
}

bool TryFindArcPointFarthestFromDrawingLine(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Drawings::DraftingCurve* arcCurve,
	const double lineStart[2],
	const double lineEnd[2],
	NXOpen::Point3d& arcPoint)
{
	arcPoint = NXOpen::Point3d(0.0, 0.0, 0.0);
	if (baseView == NULL || arcCurve == NULL || lineStart == NULL || lineEnd == NULL)
	{
		return false;
	}

	UF_EVAL_p_t evaluator = NULL;
	if (UF_EVAL_initialize(arcCurve->Tag(), &evaluator) != 0 || evaluator == NULL)
	{
		return false;
	}

	logical isArc = false;
	if (UF_EVAL_is_arc(evaluator, &isArc) != 0 || !isArc)
	{
		UF_EVAL_free(evaluator);
		return false;
	}

	double limits[2] = { 0.0, 0.0 };
	if (UF_EVAL_ask_limits(evaluator, limits) != 0 ||
		std::fabs(limits[1] - limits[0]) < 1.0e-8)
	{
		UF_EVAL_free(evaluator);
		return false;
	}

	bool found = false;
	double bestDistanceSquared = -1.0;
	const int sampleCount = 128;
	for (int i = 0; i <= sampleCount; ++i)
	{
		const double parameter = limits[0] + (limits[1] - limits[0]) * static_cast<double>(i) / static_cast<double>(sampleCount);
		double pointOnCurve[3] = { 0.0, 0.0, 0.0 };
		double derivatives[3] = { 0.0, 0.0, 0.0 };
		if (UF_EVAL_evaluate(evaluator, 0, parameter, pointOnCurve, derivatives) != 0)
		{
			continue;
		}

		double drawingPoint[2] = { pointOnCurve[0], pointOnCurve[1] };
		UF_VIEW_map_model_to_drawing(baseView->Tag(), pointOnCurve, drawingPoint);
		const NXOpen::Point3d candidateDrawingPoint(drawingPoint[0], drawingPoint[1], 0.0);
		const double distanceSquared = PointToLineDistanceSquared2d(candidateDrawingPoint, lineStart, lineEnd);
		if (!found || distanceSquared > bestDistanceSquared)
		{
			found = true;
			bestDistanceSquared = distanceSquared;
			arcPoint = NXOpen::Point3d(pointOnCurve[0], pointOnCurve[1], pointOnCurve[2]);
		}
	}

	UF_EVAL_free(evaluator);
	return found;
}

bool TryBuildOuterTangentAssociativityForFace(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Face* face,
	double centerX,
	double centerY,
	NXOpen::Drawings::DraftingCurve*& draftingCurve,
	NXOpen::Point3d& tangentPoint)
{
	draftingCurve = NULL;
	tangentPoint = NXOpen::Point3d(0.0, 0.0, 0.0);
	if (baseView == NULL || face == NULL)
	{
		return false;
	}

	double facePoint[3] = { 0.0, 0.0, 0.0 };
	double dir[3] = { 0.0, 0.0, 0.0 };
	double box[6] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
	double radius = 0.0;
	double radData = 0.0;
	int faceType = 0;
	int normDir = 0;
	if (UF_MODL_ask_face_data(face->Tag(), &faceType, facePoint, dir, box, &radius, &radData, &normDir) != 0 ||
		faceType != UF_MODL_CYLINDRICAL_FACE ||
		radius <= 1e-6)
	{
		return false;
	}

	double drawingPoint[2] = { 0.0, 0.0 };
	if (UF_VIEW_map_model_to_drawing(baseView->Tag(), facePoint, drawingPoint) != 0)
	{
		return false;
	}

	std::unordered_set<tag_t> edgeTags;
	const std::vector<NXOpen::Edge*> edges = face->GetEdges();
	for (size_t i = 0; i < edges.size(); ++i)
	{
		if (edges[i] != NULL)
		{
			edgeTags.insert(edges[i]->Tag());
		}
	}

	const double dx = drawingPoint[0] - centerX;
	const double dy = drawingPoint[1] - centerY;
	double tangentDirectionX = 0.0;
	double tangentDirectionY = 0.0;
	if (std::fabs(dx) >= std::fabs(dy))
	{
		tangentDirectionX = dx >= 0.0 ? 1.0 : -1.0;
	}
	else
	{
		tangentDirectionY = dy >= 0.0 ? 1.0 : -1.0;
	}

	double bestScore = 1e18;
	NXOpen::Point3d bestTangentPoint(0.0, 0.0, 0.0);
	bool foundTangentPoint = false;
	const std::vector<NXOpen::Drawings::DraftingCurve*> curves = CollectDraftingCurves(baseView);
	for (size_t i = 0; i < curves.size(); ++i)
	{
		NXOpen::Drawings::DraftingCurve* candidate = curves[i];
		if (!IsArcDraftingCurve(candidate) || !CurveReferencesFaceOrEdge(candidate, face->Tag(), edgeTags))
		{
			continue;
		}

		UF_EVAL_p_t evaluator = NULL;
		if (UF_EVAL_initialize(candidate->Tag(), &evaluator) != 0)
		{
			continue;
		}

		UF_EVAL_arc_t arcData;
		const int askStatus = UF_EVAL_ask_arc(evaluator, &arcData);
		UF_EVAL_free(evaluator);
		if (askStatus != 0)
		{
			continue;
		}

		double curveCenter[2] = { 0.0, 0.0 };
		if (UF_VIEW_map_model_to_drawing(baseView->Tag(), arcData.center, curveCenter) != 0)
		{
			continue;
		}

		NXOpen::Point3d candidateTangentPoint(0.0, 0.0, 0.0);
		if (!TryFindArcExtremeDrawingPoint(candidate, tangentDirectionX, tangentDirectionY, candidateTangentPoint))
		{
			continue;
		}

		const double score =
			std::fabs(curveCenter[0] - drawingPoint[0]) +
			std::fabs(curveCenter[1] - drawingPoint[1]);
		if (score < bestScore)
		{
			bestScore = score;
			draftingCurve = candidate;
			bestTangentPoint = candidateTangentPoint;
			foundTangentPoint = true;
		}
	}

	if (draftingCurve == NULL || !foundTangentPoint)
	{
		return false;
	}

	tangentPoint = bestTangentPoint;
	return true;
}

bool TryBuildRadialDimensionOrigin(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Face* face,
	const double drawingCenter[2],
	NXOpen::Point3d& originPoint)
{
	originPoint = NXOpen::Point3d(0.0, 0.0, 0.0);
	if (baseView == NULL || face == NULL || drawingCenter == NULL)
	{
		return false;
	}

	double direction2d[2] = { 0.0, 0.0 };
	double drawingFaceMidPoint[2] = { 0.0, 0.0 };
	if (!TryGetGuideFaceNormalDrawingDirection(baseView, face, direction2d, drawingFaceMidPoint))
	{
		return false;
	}

	Normalize2d(direction2d[0], direction2d[1]);
	if (std::fabs(direction2d[0]) < 1.0e-9 && std::fabs(direction2d[1]) < 1.0e-9)
	{
		return false;
	}

	originPoint = NXOpen::Point3d(
		drawingFaceMidPoint[0] + direction2d[0] * 5.0,
		drawingFaceMidPoint[1] + direction2d[1] * 5.0,
		0.0);
	return true;
}

NXOpen::Point3d CreateCandidateOrigin(
	const double* drawingPoint,
	int xDirection,
	int yDirection,
	double xOffset,
	double yOffset)
{
	NXOpen::Point3d origin(0.0, 0.0, 0.0);
	if (drawingPoint == NULL)
	{
		return origin;
	}
	origin.X = drawingPoint[0] + static_cast<double>(xDirection) * xOffset;
	origin.Y = drawingPoint[1] + static_cast<double>(yDirection) * yOffset;
	return origin;
}

std::string BuildLayoutKey(
	NXOpen::Drawings::BaseView* baseView,
	const double* drawingPoint,
	double centerX,
	double centerY)
{
	if (baseView == NULL || drawingPoint == NULL)
	{
		return std::string();
	}

	const char* horizontalSide = drawingPoint[0] >= centerX ? "R" : "L";
	const char* verticalSide = drawingPoint[1] >= centerY ? "T" : "B";
	return std::to_string(static_cast<unsigned long long>(baseView->Tag())) + ":" + horizontalSide + verticalSide;
}

double AcquireLayeredOffset(
	NXOpen::Drawings::BaseView* baseView,
	const double* drawingPoint,
	double centerX,
	double centerY,
	double baseOffset,
	double layerGap)
{
	const std::string key = BuildLayoutKey(baseView, drawingPoint, centerX, centerY);
	if (key.empty())
	{
		return baseOffset;
	}

	const int layerIndex = g_dimensionLayoutLayers[key]++;
	return baseOffset + layerGap * layerIndex;
}

NXOpen::Point3d BuildOriginAlongDirection(
	const double* drawingPoint,
	double unitX,
	double unitY,
	double offset)
{
	NXOpen::Point3d origin(0.0, 0.0, 0.0);
	if (drawingPoint == NULL)
	{
		return origin;
	}

	origin.X = drawingPoint[0] + unitX * offset;
	origin.Y = drawingPoint[1] + unitY * offset;
	return origin;
}

NXOpen::Point3d ChooseBestOriginAlongDirection(
	NXOpen::Drawings::BaseView* baseView,
	const double* drawingPoint,
	double unitX,
	double unitY,
	double centerX,
	double centerY,
	double baseOffset,
	double layerGap)
{
	if (drawingPoint == NULL)
	{
		return NXOpen::Point3d(0.0, 0.0, 0.0);
	}

	Normalize2d(unitX, unitY);
	if (std::fabs(unitX) < 1e-9 && std::fabs(unitY) < 1e-9)
	{
		unitX = drawingPoint[0] >= centerX ? 1.0 : -1.0;
		unitY = drawingPoint[1] >= centerY ? 1.0 : -1.0;
		Normalize2d(unitX, unitY);
	}

	const double initialOffset = baseOffset;
	const double maxOffset = std::max(baseOffset, 9.0 * kDimensionPullInScale);

	NXOpen::Point3d bestOrigin = BuildOriginAlongDirection(drawingPoint, unitX, unitY, initialOffset);
	double bestPenalty = ComputeObstaclePenalty(baseView, drawingPoint, bestOrigin);
	bool foundClearCandidate = !CandidateRouteIntersectsObstacle(baseView, drawingPoint, bestOrigin);

	for (double candidateOffset = initialOffset + layerGap;
		candidateOffset <= maxOffset + 1e-6;
		candidateOffset += layerGap)
	{
		const NXOpen::Point3d candidateOrigin =
			BuildOriginAlongDirection(drawingPoint, unitX, unitY, candidateOffset);
		const bool intersects = CandidateRouteIntersectsObstacle(baseView, drawingPoint, candidateOrigin);
		const double penalty = ComputeObstaclePenalty(baseView, drawingPoint, candidateOrigin);

		if (!intersects)
		{
			if (!foundClearCandidate || penalty < bestPenalty - 1e-6)
			{
				bestOrigin = candidateOrigin;
				bestPenalty = penalty;
				foundClearCandidate = true;
			}
			continue;
		}

		if (!foundClearCandidate && penalty < bestPenalty - 1e-6)
		{
			bestOrigin = candidateOrigin;
			bestPenalty = penalty;
		}
	}

	return bestOrigin;
}

NXOpen::Point3d BuildOffsetOrigin(
	NXOpen::Drawings::BaseView* baseView,
	const double* drawingPoint,
	double centerX,
	double centerY,
	double baseOffset,
	double layerGap)
{
	NXOpen::Point3d origin(0.0, 0.0, 0.0);
	if (drawingPoint == NULL)
	{
		return origin;
	}
	double unitX = drawingPoint[0] >= centerX ? 1.0 : -1.0;
	double unitY = drawingPoint[1] >= centerY ? 1.0 : -1.0;
	return ChooseBestOriginAlongDirection(
		baseView,
		drawingPoint,
		unitX,
		unitY,
		centerX,
		centerY,
		baseOffset * kDimensionPullInScale,
		layerGap * kDimensionPullInScale);
}

NXOpen::Point3d BuildOffsetOriginFromModelPoint(
	NXOpen::Drawings::BaseView* baseView,
	const double* modelPoint,
	double centerX,
	double centerY,
	double offset)
{
	double drawingPoint[2] = { 0.0, 0.0 };
	if (baseView == NULL || modelPoint == NULL)
	{
		return NXOpen::Point3d(0.0, 0.0, 0.0);
	}
	double mutableModelPoint[3] = { modelPoint[0], modelPoint[1], modelPoint[2] };
	UF_VIEW_map_model_to_drawing(baseView->Tag(), mutableModelPoint, drawingPoint);
	const double layerGap = offset >= 8.0 ? 4.0 : 3.0;
	return BuildOffsetOrigin(baseView, drawingPoint, centerX, centerY, offset, layerGap);
}

NXOpen::Point3d BuildAssociativityPointFromModelPoint(
	NXOpen::Drawings::BaseView* baseView,
	const double* modelPoint)
{
	if (baseView == NULL || modelPoint == NULL)
	{
		return NXOpen::Point3d(0.0, 0.0, 0.0);
	}

	double mutableModelPoint[3] = { modelPoint[0], modelPoint[1], modelPoint[2] };
	double drawingPoint[2] = { 0.0, 0.0 };
	if (UF_VIEW_map_model_to_drawing(baseView->Tag(), mutableModelPoint, drawingPoint) != 0)
	{
		return NXOpen::Point3d(0.0, 0.0, 0.0);
	}
	return NXOpen::Point3d(drawingPoint[0], drawingPoint[1], 0.0);
}

bool TryGetGuideFaceNormalDrawingDirection(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Face* guideFace,
	double direction2d[2],
	double drawingStart[2])
{
	direction2d[0] = 0.0;
	direction2d[1] = 0.0;
	drawingStart[0] = 0.0;
	drawingStart[1] = 0.0;
	if (baseView == NULL || guideFace == NULL)
	{
		return false;
	}

	double point[3] = { 0.0, 0.0, 0.0 };
	double dir[3] = { 0.0, 0.0, 0.0 };
	double box[6] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
	double radius = 0.0;
	double radData = 0.0;
	int type = 0;
	int normDir = 0;
	if (UF_MODL_ask_face_data(guideFace->Tag(), &type, point, dir, box, &radius, &radData, &normDir) != 0)
	{
		return false;
	}

	double faceMidPoint[3] = { point[0], point[1], point[2] };
	double uvs[4] = { 0.0, 0.0, 0.0, 0.0 };
	if (UF_MODL_ask_face_uv_minmax(guideFace->Tag(), uvs) == 0)
	{
		double param[2];
		param[0] = (uvs[0] + uvs[1]) * 0.5;
		param[1] = (uvs[2] + uvs[3]) * 0.5;
		double u1[3], v1[3], u2[3], v2[3], unitNorm[3], radii[2];
		if (UF_MODL_ask_face_props(guideFace->Tag(), param, faceMidPoint, u1, v1, u2, v2, unitNorm, radii) != 0)
		{
			faceMidPoint[0] = point[0];
			faceMidPoint[1] = point[1];
			faceMidPoint[2] = point[2];
		}
	}
	if (workPart == NULL)
	{
		return false;
	}

	NXOpen::Point* facePointObject = workPart->Points()->CreatePoint(
		NXOpen::Point3d(faceMidPoint[0], faceMidPoint[1], faceMidPoint[2]));
	if (facePointObject == NULL)
	{
		return false;
	}

	NXOpen::Direction* faceNormalDirection = workPart->Directions()->CreateDirection(
		guideFace,
		facePointObject,
		NXOpen::SenseForward,
		NXOpen::SmartObject::UpdateOptionWithinModeling);
	if (faceNormalDirection == NULL)
	{
		return false;
	}

	const NXOpen::Vector3d normalVector = faceNormalDirection->Vector();
	double modelEnd[3] =
	{
		faceMidPoint[0] + normalVector.X,
		faceMidPoint[1] + normalVector.Y,
		faceMidPoint[2] + normalVector.Z
	};

	double drawingEnd[2] = { 0.0, 0.0 };
	if (UF_VIEW_map_model_to_drawing(baseView->Tag(), faceMidPoint, drawingStart) != 0 ||
		UF_VIEW_map_model_to_drawing(baseView->Tag(), modelEnd, drawingEnd) != 0)
	{
		return false;
	}

	direction2d[0] = drawingEnd[0] - drawingStart[0];
	direction2d[1] = drawingEnd[1] - drawingStart[1];
	const double drawingLength = std::sqrt(
		direction2d[0] * direction2d[0] +
		direction2d[1] * direction2d[1]);
	return drawingLength > 1e-6;
}

NXOpen::Point3d BuildOffsetOriginFromGuideFace(
	NXOpen::Drawings::BaseView* baseView,
	const double* drawingPoint,
	NXOpen::Face* guideFace,
	double centerX,
	double centerY,
	double baseOffset,
	double layerGap,
	bool reverseGuideDirection)
{
	if (drawingPoint == NULL)
	{
		return NXOpen::Point3d(0.0, 0.0, 0.0);
	}

	double direction2d[2] = { 0.0, 0.0 };
	double drawingStart[2] = { 0.0, 0.0 };
	if (!TryGetGuideFaceNormalDrawingDirection(baseView, guideFace, direction2d, drawingStart))
	{
		return BuildOffsetOrigin(baseView, drawingPoint, centerX, centerY, baseOffset, layerGap);
	}

	const double directionLength = std::sqrt(
		direction2d[0] * direction2d[0] +
		direction2d[1] * direction2d[1]);
	if (directionLength <= 1e-6)
	{
		return BuildOffsetOrigin(baseView, drawingPoint, centerX, centerY, baseOffset, layerGap);
	}

	double unitX = direction2d[0] / directionLength;
	double unitY = direction2d[1] / directionLength;
	if (reverseGuideDirection)
	{
		unitX = -unitX;
		unitY = -unitY;
	}
	return ChooseBestOriginAlongDirection(
		baseView,
		drawingStart,
		unitX,
		unitY,
		centerX,
		centerY,
		baseOffset,
		layerGap);
}

NXOpen::Point3d BuildOffsetOriginFromModelPointAndGuideFace(
	NXOpen::Drawings::BaseView* baseView,
	const double* modelPoint,
	NXOpen::Face* guideFace,
	double centerX,
	double centerY,
	double offset,
	bool reverseGuideDirection)
{
	double drawingPoint[2] = { 0.0, 0.0 };
	if (baseView == NULL || modelPoint == NULL)
	{
		return NXOpen::Point3d(0.0, 0.0, 0.0);
	}
	double mutableModelPoint[3] = { modelPoint[0], modelPoint[1], modelPoint[2] };
	if (UF_VIEW_map_model_to_drawing(baseView->Tag(), mutableModelPoint, drawingPoint) != 0)
	{
		return NXOpen::Point3d(0.0, 0.0, 0.0);
	}
	const double layerGap = offset >= 8.0 ? 4.0 : 3.0;
	return BuildOffsetOriginFromGuideFace(baseView, drawingPoint, guideFace, centerX, centerY, offset, layerGap, reverseGuideDirection);
}
}

NXOpen::Annotations::RapidDimensionBuilder* CreateRapidDimensionBuilder()
{
	NXOpen::Annotations::Dimension* nullDimension(NULL);
	return workPart->Dimensions()->CreateRapidDimensionBuilder(nullDimension);
}

void RegisterAngularDimensionObstacle(
	NXOpen::Drawings::BaseView* baseView,
	const double intersection[2],
	const NXOpen::Point3d& originPoint)
{
	if (baseView == NULL || intersection == NULL)
	{
		return;
	}

	std::vector<Segment2d>& obstacles = g_dimensionLayoutObstacles[baseView->Tag()];
	obstacles.push_back(Segment2d{
		intersection[0],
		intersection[1],
		originPoint.X,
		originPoint.Y
	});
	obstacles.push_back(Segment2d{
		originPoint.X - 8.0,
		originPoint.Y,
		originPoint.X + 8.0,
		originPoint.Y
	});
	obstacles.push_back(Segment2d{
		originPoint.X,
		originPoint.Y - 5.0,
		originPoint.X,
		originPoint.Y + 5.0
	});

	double directionX = originPoint.X - intersection[0];
	double directionY = originPoint.Y - intersection[1];
	const double distance = std::sqrt(directionX * directionX + directionY * directionY);
	Normalize2d(directionX, directionY);
	if (distance > 1.0e-6)
	{
		g_angularPlacementSlots[baseView->Tag()].push_back(AngularPlacementSlot{
			intersection[0],
			intersection[1],
			directionX,
			directionY,
			distance
		});
	}
}

bool TryGetOuterTangentAssociativityForFace(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Face* face,
	double centerX,
	double centerY,
	NXOpen::Drawings::DraftingCurve*& draftingCurve,
	NXOpen::Point3d& tangentPoint)
{
	return TryBuildOuterTangentAssociativityForFace(
		baseView,
		face,
		centerX,
		centerY,
		draftingCurve,
		tangentPoint);
}

bool CreateCurvePairAngleDimensionFromModelPoint(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Drawings::DraftingCurve* firstCurve,
	NXOpen::Drawings::DraftingCurve* secondCurve,
	const double modelPoint[3],
	double centerX,
	double centerY,
	bool allowSupplementaryAngle)
{
	if (baseView == NULL || firstCurve == NULL || secondCurve == NULL || modelPoint == NULL || firstCurve == secondCurve)
	{
		return false;
	}

	double measuredAngleDegrees = 0.0;
	if (TryMeasureDrawingLineAngleDegrees(baseView, firstCurve, secondCurve, measuredAngleDegrees) &&
		std::fabs(measuredAngleDegrees - 90.0) < 0.02)
	{
		std::ostringstream log;
		log << "[dimension.angle.skip90]"
			<< " viewTag=" << baseView->Tag()
			<< " firstCurveTag=" << firstCurve->Tag()
			<< " secondCurveTag=" << secondCurve->Tag()
			<< " angle=" << measuredAngleDegrees;
		DimensionDebugLog(log.str());
		return false;
	}

	NXOpen::Annotations::BaseAngularDimension* nullAngularDimension(NULL);
	NXOpen::Annotations::BaseAngularDimensionBuilder* builder = allowSupplementaryAngle
		? static_cast<NXOpen::Annotations::BaseAngularDimensionBuilder*>(
			workPart->Dimensions()->CreateMajorAngularDimensionBuilder(nullAngularDimension))
		: static_cast<NXOpen::Annotations::BaseAngularDimensionBuilder*>(
			workPart->Dimensions()->CreateMinorAngularDimensionBuilder(nullAngularDimension));
	if (builder == NULL)
	{
		return false;
	}

	NXOpen::Point3d firstAnchorPoint(0.0, 0.0, 0.0);
	NXOpen::Point3d secondAnchorPoint(0.0, 0.0, 0.0);
	NXOpen::Point3d originPoint(0.0, 0.0, 0.0);
	double intersection[2] = { 0.0, 0.0 };
	bool hasDrawingIntersection = false;
	bool hasInsideAnglePlacement = TryBuildInsideAnglePlacement(
		baseView,
		firstCurve,
		secondCurve,
		firstAnchorPoint,
		secondAnchorPoint,
		originPoint);
	if (!hasInsideAnglePlacement &&
		(!TryGetLineCurveAnchorPoint(firstCurve, firstAnchorPoint) ||
		 !TryGetLineCurveAnchorPoint(secondCurve, secondAnchorPoint)))
	{
		builder->Destroy();
		return false;
	}
	if (hasInsideAnglePlacement)
	{
		double firstStart[2] = { 0.0, 0.0 };
		double firstEnd[2] = { 0.0, 0.0 };
		double secondStart[2] = { 0.0, 0.0 };
		double secondEnd[2] = { 0.0, 0.0 };
		UF_CURVE_line_t firstLineData;
		UF_CURVE_line_t secondLineData;
		hasDrawingIntersection =
			TryGetLineCurveData(baseView, firstCurve, firstLineData, firstStart, firstEnd) &&
			TryGetLineCurveData(baseView, secondCurve, secondLineData, secondStart, secondEnd) &&
			TryIntersectDrawingLines(firstStart, firstEnd, secondStart, secondEnd, intersection);
	}

	NXOpen::View* nullView(NULL);
	NXOpen::Point3d point(0.0, 0.0, 0.0);
	builder->Origin()->Plane()->SetPlaneMethod(NXOpen::Annotations::PlaneBuilder::PlaneMethodTypeXyPlane);
	builder->Origin()->SetInferRelativeToGeometry(false);
	builder->Origin()->SetAnchor(NXOpen::Annotations::OriginBuilder::AlignmentPositionMidCenter);
	builder->Style()->DimensionStyle()->SetDimensionReferenceIncludeType(NXOpen::Annotations::ReferenceIncludeTypeOnlyValue);
	builder->Style()->DimensionStyle()->SetNarrowDisplayType(NXOpen::Annotations::NarrowDisplayOptionNone);
	builder->Style()->DimensionStyle()->SetTextCentered(true);
	builder->FirstAssociativity()->SetValue(
		NXOpen::InferSnapType::SnapTypeNone, firstCurve, baseView, firstAnchorPoint, NULL, nullView, point);
	builder->SecondAssociativity()->SetValue(
		NXOpen::InferSnapType::SnapTypeNone, secondCurve, baseView, secondAnchorPoint, NULL, nullView, point);
	if (!hasInsideAnglePlacement)
	{
		originPoint = BuildOffsetOriginFromModelPoint(baseView, modelPoint, centerX, centerY, 5.0);
	}
	builder->Origin()->Origin()->SetValue(
		NULL,
		nullView,
		originPoint);

	NXOpen::NXObject* object = builder->Commit();
	builder->Destroy();
	if (object != NULL && hasDrawingIntersection)
	{
		RegisterAngularDimensionObstacle(baseView, intersection, originPoint);
	}
	return object != NULL;
}

bool CreateFacePairAngleDimensionFromModelPoint(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Face* firstFace,
	NXOpen::Face* secondFace,
	const double modelPoint[3],
	double centerX,
	double centerY,
	bool allowSupplementaryAngle)
{
	if (baseView == NULL || firstFace == NULL || secondFace == NULL || modelPoint == NULL)
	{
		return false;
	}

	NXOpen::Drawings::DraftingCurve* firstCurve = FindRepresentativeLineCurveForFace(baseView, firstFace);
	NXOpen::Drawings::DraftingCurve* secondCurve = FindRepresentativeLineCurveForFace(baseView, secondFace);
	if (firstCurve == NULL || secondCurve == NULL || firstCurve == secondCurve)
	{
		return false;
	}
	return CreateCurvePairAngleDimensionFromModelPoint(
		baseView,
		firstCurve,
		secondCurve,
		modelPoint,
		centerX,
		centerY,
		allowSupplementaryAngle);
}

void ApplyDefaultRapidDimensionStyle(NXOpen::Annotations::RapidDimensionBuilder* builder)
{
	if (builder == NULL)
	{
		return;
	}
	builder->Origin()->SetInferRelativeToGeometry(false);
	builder->Origin()->SetInferRelativeToGeometryFromLeader(false);
	builder->Origin()->SetAnchor(NXOpen::Annotations::OriginBuilder::AlignmentPositionMidCenter);
	builder->Style()->DimensionStyle()->SetTextArrowPlacement(NXOpen::Annotations::TextPlacementAutomatic);
	builder->Style()->DimensionStyle()->SetTextCentered(true);
	builder->Style()->DimensionStyle()->SetNarrowDisplayType(NXOpen::Annotations::NarrowDisplayOptionNone);
}

void ForceCommittedAnnotationOrigin(NXOpen::NXObject* object, const NXOpen::Point3d* originPoint)
{
	if (object == NULL || originPoint == NULL)
	{
		return;
	}

	NXOpen::Annotations::Annotation* annotation =
		dynamic_cast<NXOpen::Annotations::Annotation*>(object);
	if (annotation == NULL)
	{
		return;
	}

	try
	{
		NXOpen::Annotations::Annotation::AssociativeOriginData assocOrigin;
		NXOpen::View* nullView(NULL);
		NXOpen::Point* nullPoint(NULL);
		NXOpen::Annotations::Annotation* nullAnnotation(NULL);
		assocOrigin.OriginType = NXOpen::Annotations::AssociativeOriginTypeDrag;
		assocOrigin.View = nullView;
		assocOrigin.ViewOfGeometry = nullView;
		assocOrigin.PointOnGeometry = nullPoint;
		assocOrigin.VertAnnotation = nullAnnotation;
		assocOrigin.VertAlignmentPosition = NXOpen::Annotations::AlignmentPositionTopLeft;
		assocOrigin.HorizAnnotation = nullAnnotation;
		assocOrigin.HorizAlignmentPosition = NXOpen::Annotations::AlignmentPositionTopLeft;
		assocOrigin.AlignedAnnotation = nullAnnotation;
		assocOrigin.DimensionLine = 0;
		assocOrigin.AssociatedView = nullView;
		assocOrigin.AssociatedPoint = nullPoint;
		assocOrigin.OffsetAnnotation = nullAnnotation;
		assocOrigin.OffsetAlignmentPosition = NXOpen::Annotations::AlignmentPositionTopLeft;
		assocOrigin.XOffsetFactor = 0.0;
		assocOrigin.YOffsetFactor = 0.0;
		assocOrigin.StackAlignmentPosition = NXOpen::Annotations::StackAlignmentPositionAbove;
		annotation->SetAssociativeOrigin(assocOrigin, *originPoint);
		annotation->SetAnnotationOrigin(*originPoint);
		std::ostringstream log;
		log << "[dimension.forceOrigin.ok] tag=" << object->Tag()
			<< " origin=(" << originPoint->X << "," << originPoint->Y << "," << originPoint->Z << ")";
		DimensionDebugLog(log.str());
	}
	catch (NXOpen::NXException& ex)
	{
		DimensionDebugLog(std::string("[dimension.forceOrigin.NXException] message=") + ex.what());
	}
	catch (std::exception& ex)
	{
		DimensionDebugLog(std::string("[dimension.forceOrigin.std] message=") + ex.what());
	}
	catch (...)
	{
		DimensionDebugLog("[dimension.forceOrigin.unknown]");
	}
}

NXOpen::NXObject* CommitAndDestroyRapidDimension(
	NXOpen::Annotations::RapidDimensionBuilder* builder,
	const NXOpen::Point3d* forcedOriginPoint)
{
	if (builder == NULL)
	{
		return NULL;
	}
	NXOpen::NXObject* object = NULL;
	try
	{
		object = builder->Commit();
		ForceCommittedAnnotationOrigin(object, forcedOriginPoint);
	}
	catch (NXOpen::NXException& ex)
	{
		DimensionDebugLog(std::string("[dimension.commit.NXException] message=") + ex.what());
	}
	catch (std::exception& ex)
	{
		DimensionDebugLog(std::string("[dimension.commit.std] message=") + ex.what());
	}
	catch (...)
	{
		DimensionDebugLog("[dimension.commit.unknown]");
	}
	try
	{
		builder->Destroy();
	}
	catch (...)
	{
	}
	return object;
}

void ResetDimensionLayoutState()
{
	g_dimensionLayoutLayers.clear();
	g_dimensionLayoutObstacles.clear();
	g_angularPlacementSlots.clear();
}

void RegisterDimensionLayoutObstacles(
	NXOpen::Drawings::BaseView* baseView,
	const std::vector<NXOpen::Drawings::DraftingCurve*>& draftingCurves)
{
	if (baseView == NULL)
	{
		return;
	}

	std::vector<Segment2d>& obstacles = g_dimensionLayoutObstacles[baseView->Tag()];
	obstacles.clear();
	for (size_t i = 0; i < draftingCurves.size(); ++i)
	{
		if (draftingCurves[i] == NULL)
		{
			continue;
		}

		UF_EVAL_p_t evaluator;
		if (UF_EVAL_initialize(draftingCurves[i]->Tag(), &evaluator) != 0)
		{
			continue;
		}

		logical isLine = false;
		UF_EVAL_is_line(evaluator, &isLine);
		if (!isLine)
		{
			UF_EVAL_free(evaluator);
			continue;
		}

		UF_EVAL_line_t lineData;
		if (UF_EVAL_ask_line(evaluator, &lineData) != 0)
		{
			UF_EVAL_free(evaluator);
			continue;
		}
		UF_EVAL_free(evaluator);

		double startPoint[2] = { 0.0, 0.0 };
		double endPoint[2] = { 0.0, 0.0 };
		if (UF_VIEW_map_model_to_drawing(baseView->Tag(), lineData.start, startPoint) != 0 ||
			UF_VIEW_map_model_to_drawing(baseView->Tag(), lineData.end, endPoint) != 0)
		{
			continue;
		}

		Segment2d segment = { startPoint[0], startPoint[1], endPoint[0], endPoint[1] };
		const double dx = segment.x2 - segment.x1;
		const double dy = segment.y2 - segment.y1;
		if (std::sqrt(dx * dx + dy * dy) > 1e-6)
		{
			obstacles.push_back(segment);
		}
	}
}

NXOpen::Annotations::IntersectionSymbol* CreateIntersectionSymbol(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Drawings::DraftingCurve* firstCurve,
	NXOpen::Drawings::DraftingCurve* secondCurve)
{
	if (baseView == NULL || firstCurve == NULL || secondCurve == NULL)
	{
		return NULL;
	}
	NXOpen::Point3d origin(0, 0, 0);
	NXOpen::Annotations::IntersectionSymbol* nullSymbol(NULL);
	NXOpen::Annotations::IntersectionSymbolBuilder* builder =
		workPart->Annotations()->IntersectionSymbols()->CreateIntersectionSymbolBuilder(nullSymbol);
	builder->SetExtension(0.20000000000000001);
	builder->SetColor(workPart->Colors()->Find("Black"));
	builder->IntersectionObject1()->SetValue(firstCurve, baseView, origin);
	builder->IntersectionObject2()->SetValue(secondCurve, baseView, origin);
	NXOpen::NXObject* object = builder->Commit();
	builder->Destroy();
	return dynamic_cast<NXOpen::Annotations::IntersectionSymbol*>(object);
}

bool IsValidIntersectionSymbol(
	const std::vector<NXOpen::Annotations::IntersectionSymbol*>& symbols,
	size_t index)
{
	return index < symbols.size() && symbols[index] != NULL;
}

bool CreateCurveEndToSymbolDimensionFromDrawingPoint(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Drawings::DraftingCurve* curve,
	NXOpen::Annotations::IntersectionSymbol* symbol,
	const double drawingPoint[2],
	double centerX,
	double centerY,
	NXOpen::Face* guideFace,
	bool reverseGuideDirection)
{
	{
		std::ostringstream log;
		log << "[dimension.enter.curveEndToSymbol]"
			<< " viewTag=" << (baseView != NULL ? baseView->Tag() : NULL_TAG)
			<< " curveTag=" << (curve != NULL ? curve->Tag() : NULL_TAG)
			<< " symbolTag=" << (symbol != NULL ? symbol->Tag() : NULL_TAG)
			<< " guideTag=" << (guideFace != NULL ? guideFace->Tag() : NULL_TAG)
			<< " drawingPoint=(" << (drawingPoint != NULL ? drawingPoint[0] : 0.0) << "," << (drawingPoint != NULL ? drawingPoint[1] : 0.0) << ")"
			<< " center=(" << centerX << "," << centerY << ")"
			<< " reverse=" << (reverseGuideDirection ? 1 : 0);
		DimensionDebugLog(log.str());
	}
	if (baseView == NULL || curve == NULL || symbol == NULL || drawingPoint == NULL)
	{
		return false;
	}

	NXOpen::Annotations::RapidDimensionBuilder* builder = CreateRapidDimensionBuilder();
	if (builder == NULL)
	{
		return false;
	}

	NXOpen::Point3d point(0.0, 0.0, 0.0);
	NXOpen::View* nullView(NULL);

	logical isLine = false;
	UF_EVAL_p_t evaluator = NULL;
	if (UF_EVAL_initialize(curve->Tag(), &evaluator) == 0)
	{
		UF_EVAL_is_line(evaluator, &isLine);
		UF_EVAL_free(evaluator);
	}
	DimensionDebugLog(std::string("[dimension.method.curveEndToSymbol] method=") + (isLine ? "Perpendicular" : "PointToPoint"));
	builder->Measurement()->SetMethod(
		isLine
		? NXOpen::Annotations::DimensionMeasurementBuilder::MeasurementMethodPerpendicular
		: NXOpen::Annotations::DimensionMeasurementBuilder::MeasurementMethodPointToPoint);
	NXOpen::Point3d firstAssociativityPoint(0.0, 0.0, 0.0);
	NXOpen::InferSnapType::SnapType firstSnapType = NXOpen::InferSnapType::SnapTypeEnd;
	const NXOpen::Point3d referenceDrawingPoint(drawingPoint[0], drawingPoint[1], 0.0);
	if (!TryGetDraftingCurveEndpointNearestDrawingPoint(
			baseView,
			curve,
			referenceDrawingPoint,
			firstAssociativityPoint,
			firstSnapType))
	{
		builder->Destroy();
		DimensionDebugLog("[dimension.curveEndToSymbol.noValidEndpoint]");
		return false;
	}
	builder->FirstAssociativity()->SetValue(
		firstSnapType, curve, baseView, firstAssociativityPoint, NULL, nullView, point);
	builder->SecondAssociativity()->SetValue(
		NXOpen::InferSnapType::SnapTypeExist, symbol, baseView, point, NULL, nullView, point);
	ApplyDefaultRapidDimensionStyle(builder);
	const NXOpen::Point3d originPoint =
		BuildOffsetOriginFromGuideFace(baseView, drawingPoint, guideFace, centerX, centerY, 5.0, 3.0, reverseGuideDirection);
	builder->Origin()->Origin()->SetValue(
		NULL,
		nullView,
		originPoint);
	CommitAndDestroyRapidDimension(builder, &originPoint);
	return true;
}

bool CreateCurveEndToFaceTangentDimensionFromModelPoint(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Drawings::DraftingCurve* curve,
	NXOpen::Face* face,
	const double modelPoint[3],
	double centerX,
	double centerY,
	NXOpen::Face* guideFace,
	bool reverseGuideDirection)
{
	if (baseView == NULL || curve == NULL || face == NULL || modelPoint == NULL)
	{
		return false;
	}

	try
	{
		NXOpen::Annotations::RapidDimensionBuilder* builder = CreateRapidDimensionBuilder();
		if (builder == NULL)
		{
			return false;
		}

		NXOpen::Point3d point(0.0, 0.0, 0.0);
		NXOpen::View* nullView(NULL);
		ApplyDefaultRapidDimensionStyle(builder);
		builder->Measurement()->SetMethod(NXOpen::Annotations::DimensionMeasurementBuilder::MeasurementMethodPerpendicular);
		UF_CURVE_line_t firstLineData;
		double firstLineStart[2] = { 0.0, 0.0 };
		double firstLineEnd[2] = { 0.0, 0.0 };
		const bool hasFirstLine =
			TryGetLineCurveData(baseView, curve, firstLineData, firstLineStart, firstLineEnd);
		const NXOpen::Point3d modelAssociativityPoint =
			BuildAssociativityPointFromModelPoint(baseView, modelPoint);
		NXOpen::Point3d firstAssociativityPoint(0.0, 0.0, 0.0);
		NXOpen::InferSnapType::SnapType firstSnapType = NXOpen::InferSnapType::SnapTypeEnd;
		if (!TryGetDraftingCurveEndpointNearestDrawingPoint(
				baseView,
				curve,
				modelAssociativityPoint,
				firstAssociativityPoint,
				firstSnapType))
		{
			builder->Destroy();
			DimensionDebugLog("[dimension.curveEndToFace.noValidEndpoint]");
			return false;
		}
		builder->FirstAssociativity()->SetValue(
			firstSnapType, curve, baseView, firstAssociativityPoint, NULL, nullView, point);
		const NXOpen::Point3d originPoint =
			BuildOffsetOriginFromModelPointAndGuideFace(baseView, modelPoint, guideFace != NULL ? guideFace : face, centerX, centerY, 5.0, reverseGuideDirection);

		NXOpen::Drawings::DraftingCurve* tangentCurve = NULL;
		NXOpen::Point3d tangentPoint(0.0, 0.0, 0.0);
		if (TryGetOuterTangentAssociativityForFace(baseView, face, centerX, centerY, tangentCurve, tangentPoint))
		{
			NXOpen::Point3d farthestTangentPoint(0.0, 0.0, 0.0);
			if (hasFirstLine &&
				TryFindArcPointFarthestFromDrawingLine(baseView, tangentCurve, firstLineStart, firstLineEnd, farthestTangentPoint))
			{
				tangentPoint = farthestTangentPoint;
			}
			builder->SecondAssociativity()->SetValue(
				NXOpen::InferSnapType::SnapTypeDrfTangent, tangentCurve, baseView, tangentPoint, NULL, nullView, point);
		}
		else
		{
			NXOpen::Point3d faceAssociativityPoint(0.0, 0.0, 0.0);
			NXOpen::Drawings::DraftingCurve* faceCurve =
				FindAssociativeLineCurveForFace(
					baseView,
					face,
					reverseGuideDirection,
					&faceAssociativityPoint,
					&originPoint);
			if (faceCurve == NULL)
			{
				builder->Destroy();
				return false;
			}
			builder->SecondAssociativity()->SetValue(
				NXOpen::InferSnapType::SnapTypeExist, faceCurve, baseView, faceAssociativityPoint, NULL, nullView, point);
		}
		builder->Origin()->Origin()->SetValue(
			NULL,
			nullView,
			originPoint);
		return CommitAndDestroyRapidDimension(builder, &originPoint) != NULL;
	}
	catch (NXOpen::NXException& ex)
	{
		DimensionDebugLog(std::string("[dimension.curveFace.NXException] message=") + ex.what());
		return false;
	}
	catch (std::exception& ex)
	{
		DimensionDebugLog(std::string("[dimension.curveFace.std] message=") + ex.what());
		return false;
	}
	catch (...)
	{
		DimensionDebugLog("[dimension.curveFace.unknown]");
		return false;
	}
}

bool CreateSymbolToSymbolDimensionFromModelPoint(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Annotations::IntersectionSymbol* firstSymbol,
	NXOpen::Annotations::IntersectionSymbol* secondSymbol,
	const double modelPoint[3],
	double centerX,
	double centerY,
	NXOpen::Face* guideFace,
	bool reverseGuideDirection)
{
	{
		std::ostringstream log;
		log << "[dimension.enter.symbolToSymbol]"
			<< " viewTag=" << (baseView != NULL ? baseView->Tag() : NULL_TAG)
			<< " firstSymbolTag=" << (firstSymbol != NULL ? firstSymbol->Tag() : NULL_TAG)
			<< " secondSymbolTag=" << (secondSymbol != NULL ? secondSymbol->Tag() : NULL_TAG)
			<< " guideTag=" << (guideFace != NULL ? guideFace->Tag() : NULL_TAG)
			<< " modelPoint=(" << (modelPoint != NULL ? modelPoint[0] : 0.0) << "," << (modelPoint != NULL ? modelPoint[1] : 0.0) << "," << (modelPoint != NULL ? modelPoint[2] : 0.0) << ")"
			<< " center=(" << centerX << "," << centerY << ")"
			<< " reverse=" << (reverseGuideDirection ? 1 : 0)
			<< " method=PointToPoint";
		DimensionDebugLog(log.str());
	}
	if (baseView == NULL || firstSymbol == NULL || secondSymbol == NULL || modelPoint == NULL)
	{
		return false;
	}

	NXOpen::Annotations::RapidDimensionBuilder* builder = CreateRapidDimensionBuilder();
	if (builder == NULL)
	{
		return false;
	}

	NXOpen::Point3d point(0.0, 0.0, 0.0);
	NXOpen::View* nullView(NULL);
	ApplyDefaultRapidDimensionStyle(builder);
	builder->Measurement()->SetMethod(NXOpen::Annotations::DimensionMeasurementBuilder::MeasurementMethodPointToPoint);
	builder->FirstAssociativity()->SetValue(
		NXOpen::InferSnapType::SnapTypeExist, firstSymbol, baseView, point, NULL, nullView, point);
	builder->SecondAssociativity()->SetValue(
		NXOpen::InferSnapType::SnapTypeExist, secondSymbol, baseView, point, NULL, nullView, point);
	const NXOpen::Point3d originPoint =
		BuildOffsetOriginFromModelPointAndGuideFace(baseView, modelPoint, guideFace, centerX, centerY, 5.0, reverseGuideDirection);
	builder->Origin()->Origin()->SetValue(NULL, nullView, originPoint);
	CommitAndDestroyRapidDimension(builder, &originPoint);
	return true;
}

bool CreateFaceToSymbolDimensionFromModelPoint(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Face* face,
	NXOpen::Annotations::IntersectionSymbol* symbol,
	const double modelPoint[3],
	double centerX,
	double centerY,
	NXOpen::Face* guideFace,
	bool reverseGuideDirection)
{
	if (baseView == NULL || face == NULL || symbol == NULL || modelPoint == NULL)
	{
		return false;
	}

	try
	{
		NXOpen::Annotations::RapidDimensionBuilder* builder = CreateRapidDimensionBuilder();
		if (builder == NULL)
		{
			return false;
		}

		NXOpen::Point3d point(0.0, 0.0, 0.0);
		NXOpen::View* nullView(NULL);
		ApplyDefaultRapidDimensionStyle(builder);
		const NXOpen::Point3d modelAssociativityPoint =
			BuildAssociativityPointFromModelPoint(baseView, modelPoint);
		const NXOpen::Point3d originPoint =
			BuildOffsetOriginFromModelPointAndGuideFace(baseView, modelPoint, guideFace != NULL ? guideFace : face, centerX, centerY, 5.0, reverseGuideDirection);
		NXOpen::Drawings::DraftingCurve* tangentCurve = NULL;
		NXOpen::Point3d tangentPoint(0.0, 0.0, 0.0);
		if (TryGetOuterTangentAssociativityForFace(baseView, face, centerX, centerY, tangentCurve, tangentPoint))
		{
			builder->FirstAssociativity()->SetValue(
				NXOpen::InferSnapType::SnapTypeDrfTangent, tangentCurve, baseView, tangentPoint, NULL, nullView, point);
		}
		else
		{
			NXOpen::Point3d faceAssociativityPoint(0.0, 0.0, 0.0);
			NXOpen::Drawings::DraftingCurve* faceCurve =
				FindAssociativeLineCurveForFace(
					baseView,
					face,
					reverseGuideDirection,
					&faceAssociativityPoint,
					&originPoint);
			if (faceCurve == NULL)
			{
				builder->Destroy();
				return false;
			}
			builder->FirstAssociativity()->SetValue(
				NXOpen::InferSnapType::SnapTypeExist, faceCurve, baseView, faceAssociativityPoint, NULL, nullView, point);
		}
		builder->SecondAssociativity()->SetValue(
			NXOpen::InferSnapType::SnapTypeExist, symbol, baseView, point, NULL, nullView, point);
		builder->Origin()->Origin()->SetValue(
			NULL,
			nullView,
			originPoint);
		return CommitAndDestroyRapidDimension(builder, &originPoint) != NULL;
	}
	catch (NXOpen::NXException& ex)
	{
		DimensionDebugLog(std::string("[dimension.faceSymbol.NXException] message=") + ex.what());
		return false;
	}
	catch (std::exception& ex)
	{
		DimensionDebugLog(std::string("[dimension.faceSymbol.std] message=") + ex.what());
		return false;
	}
	catch (...)
	{
		DimensionDebugLog("[dimension.faceSymbol.unknown]");
		return false;
	}
}

bool CreateFaceToObjectDimensionFromModelPoint(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Face* firstFace,
	NXOpen::DisplayableObject* secondObject,
	const double modelPoint[3],
	double centerX,
	double centerY,
	NXOpen::Face* guideFace,
	bool reverseGuideDirection,
	bool forceMeasurementMethod,
	NXOpen::Annotations::DimensionMeasurementBuilder::MeasurementMethod measurementMethod,
	const std::vector<NXOpen::Drawings::DraftingCurve*>* candidateCurves)
{
	{
		std::ostringstream log;
		log << "[dimension.enter.faceToObject]"
			<< " viewTag=" << (baseView != NULL ? baseView->Tag() : NULL_TAG)
			<< " firstFaceTag=" << (firstFace != NULL ? firstFace->Tag() : NULL_TAG)
			<< " secondTag=" << (secondObject != NULL ? secondObject->Tag() : NULL_TAG)
			<< " secondIsFace=" << (dynamic_cast<NXOpen::Face*>(secondObject) != NULL ? 1 : 0)
			<< " guideTag=" << (guideFace != NULL ? guideFace->Tag() : NULL_TAG)
			<< " modelPoint=(" << (modelPoint != NULL ? modelPoint[0] : 0.0) << "," << (modelPoint != NULL ? modelPoint[1] : 0.0) << "," << (modelPoint != NULL ? modelPoint[2] : 0.0) << ")"
			<< " center=(" << centerX << "," << centerY << ")"
			<< " reverse=" << (reverseGuideDirection ? 1 : 0)
			<< " method=" << (forceMeasurementMethod ? "Forced" : "Perpendicular");
		DimensionDebugLog(log.str());
	}
	if (baseView == NULL || firstFace == NULL || secondObject == NULL || modelPoint == NULL)
	{
		return false;
	}

	try
	{
		NXOpen::Annotations::RapidDimensionBuilder* builder = CreateRapidDimensionBuilder();
		if (builder == NULL)
		{
			return false;
		}

		NXOpen::Point3d point(0.0, 0.0, 0.0);
		NXOpen::View* nullView(NULL);
		ApplyDefaultRapidDimensionStyle(builder);
		builder->Measurement()->SetMethod(
			forceMeasurementMethod
			? measurementMethod
			: NXOpen::Annotations::DimensionMeasurementBuilder::MeasurementMethodPerpendicular);
		const NXOpen::Point3d modelAssociativityPoint =
			BuildAssociativityPointFromModelPoint(baseView, modelPoint);
		const NXOpen::Point3d originPoint =
			BuildOffsetOriginFromModelPointAndGuideFace(baseView, modelPoint, guideFace != NULL ? guideFace : firstFace, centerX, centerY, 5.0, reverseGuideDirection);
		NXOpen::Drawings::DraftingCurve* tangentCurve = NULL;
		NXOpen::Point3d tangentPoint(0.0, 0.0, 0.0);
		double firstLineStart[2] = { 0.0, 0.0 };
		double firstLineEnd[2] = { 0.0, 0.0 };
		bool firstLineAvailable = false;
		if (forceMeasurementMethod)
		{
			NXOpen::Point3d firstAssociativityPoint(0.0, 0.0, 0.0);
			NXOpen::Drawings::DraftingCurve* firstCurve =
				FindAssociativeLineCurveForFace(
					baseView,
					firstFace,
					reverseGuideDirection,
					&firstAssociativityPoint,
					&originPoint,
					candidateCurves);
			if (firstCurve == NULL)
			{
				builder->Destroy();
				return false;
			}
			builder->FirstAssociativity()->SetValue(
				NXOpen::InferSnapType::SnapTypeExist, firstCurve, baseView, firstAssociativityPoint, NULL, nullView, point);
			UF_CURVE_line_t firstLineData;
			firstLineAvailable = TryGetLineCurveData(baseView, firstCurve, firstLineData, firstLineStart, firstLineEnd);
			DimensionDebugLog(std::string("[dimension.assoc.faceToObject.firstCurve] curveTag=") + std::to_string(firstCurve->Tag()));
		}
		else if (TryGetOuterTangentAssociativityForFace(baseView, firstFace, centerX, centerY, tangentCurve, tangentPoint))
		{
			builder->FirstAssociativity()->SetValue(
				NXOpen::InferSnapType::SnapTypeDrfTangent, tangentCurve, baseView, tangentPoint, NULL, nullView, point);
		}
		else
		{
			NXOpen::Point3d firstAssociativityPoint(0.0, 0.0, 0.0);
			NXOpen::Drawings::DraftingCurve* firstCurve =
				FindAssociativeLineCurveForFace(
					baseView,
					firstFace,
					reverseGuideDirection,
					&firstAssociativityPoint,
					&originPoint);
			if (firstCurve == NULL)
			{
				builder->Destroy();
				return false;
			}
			DimensionDebugLog(
				std::string("[dimension.assoc.faceToObject.firstCurve] curveTag=") +
				std::to_string(firstCurve->Tag()));
			builder->FirstAssociativity()->SetValue(
				NXOpen::InferSnapType::SnapTypeExist, firstCurve, baseView, firstAssociativityPoint, NULL, nullView, point);
			UF_CURVE_line_t firstLineData;
			firstLineAvailable = TryGetLineCurveData(baseView, firstCurve, firstLineData, firstLineStart, firstLineEnd);
		}

		NXOpen::Face* secondFace = dynamic_cast<NXOpen::Face*>(secondObject);
		if (forceMeasurementMethod && secondFace != NULL)
		{
			NXOpen::Point3d secondAssociativityPoint = modelAssociativityPoint;
			NXOpen::Drawings::DraftingCurve* secondCurve =
				FindAssociativeLineCurveForFace(
					baseView,
					secondFace,
					reverseGuideDirection,
					&secondAssociativityPoint,
					&originPoint,
					candidateCurves);
			if (secondCurve == NULL)
			{
				builder->Destroy();
				return false;
			}
			builder->SecondAssociativity()->SetValue(
				NXOpen::InferSnapType::SnapTypeExist, secondCurve, baseView, secondAssociativityPoint, NULL, nullView, point);
			DimensionDebugLog(std::string("[dimension.assoc.faceToObject.secondCurve] curveTag=") + std::to_string(secondCurve->Tag()));
		}
		else if (secondFace != NULL &&
			TryGetOuterTangentAssociativityForFace(baseView, secondFace, centerX, centerY, tangentCurve, tangentPoint))
		{
			NXOpen::Point3d farthestTangentPoint(0.0, 0.0, 0.0);
			if (firstLineAvailable &&
				TryFindArcPointFarthestFromDrawingLine(baseView, tangentCurve, firstLineStart, firstLineEnd, farthestTangentPoint))
			{
				tangentPoint = farthestTangentPoint;
			}
			builder->SecondAssociativity()->SetValue(
				NXOpen::InferSnapType::SnapTypeDrfTangent, tangentCurve, baseView, tangentPoint, NULL, nullView, point);
		}
		else
		{
			NXOpen::Point3d secondAssociativityPoint = modelAssociativityPoint;
			NXOpen::InferSnapType::SnapType secondSnapType = NXOpen::InferSnapType::SnapTypeExist;
			NXOpen::Face* fallbackSecondFace = dynamic_cast<NXOpen::Face*>(secondObject);
			NXOpen::Drawings::DraftingCurve* suppliedSecondCurve =
				dynamic_cast<NXOpen::Drawings::DraftingCurve*>(secondObject);
			NXOpen::Drawings::DraftingCurve* secondCurve = fallbackSecondFace != NULL
				? FindAssociativeLineCurveForFace(
					baseView,
					fallbackSecondFace,
					reverseGuideDirection,
					&secondAssociativityPoint,
					&originPoint)
				: FindRepresentativeCurveForObject(baseView, secondObject);
			if (secondCurve == NULL)
			{
				builder->Destroy();
				return false;
			}
			if (suppliedSecondCurve != NULL)
			{
				if (!TryGetDraftingCurveEndpointNearestDrawingPoint(
						baseView,
						suppliedSecondCurve,
						modelAssociativityPoint,
						secondAssociativityPoint,
						secondSnapType))
				{
					builder->Destroy();
					DimensionDebugLog("[dimension.faceToObject.secondCurve.noValidEndpoint]");
					return false;
				}
			}
			builder->SecondAssociativity()->SetValue(
				secondSnapType, secondCurve, baseView, secondAssociativityPoint, NULL, nullView, point);
		}
		builder->Origin()->Origin()->SetValue(
			NULL,
			nullView,
			originPoint);
		return CommitAndDestroyRapidDimension(builder, &originPoint) != NULL;
	}
	catch (NXOpen::NXException& ex)
	{
		DimensionDebugLog(std::string("[dimension.faceObject.NXException] message=") + ex.what());
		return false;
	}
	catch (std::exception& ex)
	{
		DimensionDebugLog(std::string("[dimension.faceObject.std] message=") + ex.what());
		return false;
	}
	catch (...)
	{
		DimensionDebugLog("[dimension.faceObject.unknown]");
		return false;
	}
}

bool CreateCurveToCurveEnvelopeDimension(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Drawings::DraftingCurve* firstCurve,
	NXOpen::Drawings::DraftingCurve* secondCurve,
	const NXOpen::Point3d& firstPoint,
	const NXOpen::Point3d& secondPoint,
	NXOpen::Annotations::DimensionMeasurementBuilder::MeasurementMethod measurementMethod,
	const NXOpen::Point3d& originPoint,
	NXOpen::InferSnapType::SnapType firstSnapType,
	NXOpen::InferSnapType::SnapType secondSnapType)
{
	if (baseView == NULL || firstCurve == NULL || secondCurve == NULL)
	{
		return false;
	}

	NXOpen::Annotations::RapidDimensionBuilder* builder = CreateRapidDimensionBuilder();
	if (builder == NULL)
	{
		return false;
	}

	NXOpen::Point3d point(0.0, 0.0, 0.0);
	NXOpen::View* nullView(NULL);
	builder->FirstAssociativity()->SetValue(firstSnapType, firstCurve, baseView, firstPoint, NULL, nullView, point);
	builder->SecondAssociativity()->SetValue(secondSnapType, secondCurve, baseView, secondPoint, NULL, nullView, point);
	ApplyDefaultRapidDimensionStyle(builder);
	builder->Measurement()->SetMethod(measurementMethod);
	builder->Origin()->Origin()->SetValue(NULL, nullView, originPoint);
	CommitAndDestroyRapidDimension(builder, &originPoint);
	return true;
}

bool CreateRadialDimensionAtDrawingPoint(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Face* face,
	const double drawingPoint[2])
{
	if (baseView == NULL || face == NULL || drawingPoint == NULL)
	{
		return false;
	}

	NXOpen::Drawings::DraftingCurve* faceCurve = FindRepresentativeCurveForFace(baseView, face, true);
	if (faceCurve == NULL)
	{
		return false;
	}

	NXOpen::Annotations::Dimension* nullDimension(NULL);
	NXOpen::Annotations::RadialDimensionBuilder* builder =
		workPart->Dimensions()->CreateRadialDimensionBuilder(nullDimension);
	if (builder == NULL)
	{
		return false;
	}

	NXOpen::Point3d point(drawingPoint[0], drawingPoint[1], 0.0);
	builder->FirstAssociativity()->SetValue(faceCurve, baseView, point);
	NXOpen::Point3d originPoint;
	if (TryBuildRadialDimensionOrigin(baseView, face, drawingPoint, originPoint))
	{
		NXOpen::View* nullView(NULL);
		builder->Origin()->Origin()->SetValue(NULL, nullView, originPoint);
	}
	NXOpen::NXObject* object = builder->Commit();
	builder->Destroy();
	return object != NULL;
}

namespace
{
struct HoleCoordinateExtents
{
	double minX;
	double minY;
	double maxX;
	double maxY;
	bool valid;
};

struct HoleCoordinateCandidate
{
	NXOpen::Drawings::DraftingCurve* curve;
	NXOpen::Point3d drawingCenter;
	double drawingRadius;
};

struct FlatPatternHoleNoteGroup
{
	NXOpen::Drawings::DraftingCurve* representativeCurve;
	NXOpen::Point3d drawingCenter;
	double drawingRadius;
	double diameter;
	std::string typeText;
	std::string specText;
	int count;
	std::vector<NXOpen::Point3d> markerCenters;
};

struct HoleNoteRuleRecord
{
	std::string annotationType;
	std::string series;
	std::string size;
	std::string threadSpec;
	std::string lengthText;
	double bottomHole;
	std::string displayText;
	std::string standardComment;
	std::string note;

	HoleNoteRuleRecord() : bottomHole(0.0)
	{
	}
};

struct HoleOrdinatePlacementState
{
	std::vector<std::vector<double>> rightSideLanes;
	std::vector<std::vector<double>> topSideLanes;
};

int AcquireHoleOrdinateLane(
	std::vector<std::vector<double>>& lanes,
	double axisValue,
	double minimumSpacing)
{
	for (size_t laneIndex = 0; laneIndex < lanes.size(); ++laneIndex)
	{
		bool available = true;
		for (size_t positionIndex = 0; positionIndex < lanes[laneIndex].size(); ++positionIndex)
		{
			if (std::fabs(lanes[laneIndex][positionIndex] - axisValue) < minimumSpacing)
			{
				available = false;
				break;
			}
		}

		if (available)
		{
			lanes[laneIndex].push_back(axisValue);
			return static_cast<int>(laneIndex);
		}
	}

	lanes.push_back(std::vector<double>(1, axisValue));
	return static_cast<int>(lanes.size() - 1);
}

NXOpen::Point3d ComputeHoleOrdinateMarginLocation(
	const HoleCoordinateExtents& extents,
	const HoleCoordinateCandidate& hole,
	bool verticalDimension,
	HoleOrdinatePlacementState& placementState)
{
	const double width = std::max(1.0, extents.maxX - extents.minX);
	const double height = std::max(1.0, extents.maxY - extents.minY);
	const double baseXOffset = std::max(12.0, width * 0.10) * kDimensionPullInScale;
	const double baseYOffset = std::max(12.0, height * 0.10) * kDimensionPullInScale;
	const double laneGap = std::max(10.0, std::min(width, height) * 0.04) * kDimensionPullInScale;
	const double minimumLabelSpacing = std::max(14.0, std::min(width, height) * 0.10);

	if (verticalDimension)
	{
		const int lane = AcquireHoleOrdinateLane(
			placementState.rightSideLanes,
			hole.drawingCenter.Y,
			minimumLabelSpacing);
		return NXOpen::Point3d(
			extents.maxX + baseXOffset + static_cast<double>(lane) * laneGap,
			hole.drawingCenter.Y,
			0.0);
	}

	const int lane = AcquireHoleOrdinateLane(
		placementState.topSideLanes,
		hole.drawingCenter.X,
		minimumLabelSpacing);
	return NXOpen::Point3d(
		hole.drawingCenter.X,
		extents.maxY + baseYOffset + static_cast<double>(lane) * laneGap,
		0.0);
}

bool TryGetDraftingLineData(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Drawings::DraftingCurve* curve,
	double startPoint[2],
	double endPoint[2],
	NXOpen::Point3d& modelStart,
	NXOpen::Point3d& modelEnd)
{
	if (baseView == NULL || curve == NULL || startPoint == NULL || endPoint == NULL)
	{
		return false;
	}

	UF_EVAL_p_t evaluator;
	if (UF_EVAL_initialize(curve->Tag(), &evaluator) != 0)
	{
		return false;
	}

	logical isLine = false;
	UF_EVAL_is_line(evaluator, &isLine);
	if (!isLine)
	{
		UF_EVAL_free(evaluator);
		return false;
	}

	UF_EVAL_line_t lineData;
	if (UF_EVAL_ask_line(evaluator, &lineData) != 0)
	{
		UF_EVAL_free(evaluator);
		return false;
	}
	UF_EVAL_free(evaluator);

	{
		std::ostringstream rawLineLog;
		rawLineLog << "[HoleCoord] rawLine tag=" << curve->Tag()
			<< " start=(" << lineData.start[0] << "," << lineData.start[1] << "," << lineData.start[2]
			<< ") end=(" << lineData.end[0] << "," << lineData.end[1] << "," << lineData.end[2] << ")";
		LogPluginMessage(rawLineLog.str());
	}

	startPoint[0] = lineData.start[0];
	startPoint[1] = lineData.start[1];
	endPoint[0] = lineData.end[0];
	endPoint[1] = lineData.end[1];

	modelStart = NXOpen::Point3d(lineData.start[0], lineData.start[1], lineData.start[2]);
	modelEnd = NXOpen::Point3d(lineData.end[0], lineData.end[1], lineData.end[2]);
	return true;
}

bool TryGetDraftingArcData(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Drawings::DraftingCurve* curve,
	double drawingCenter[2],
	double& drawingRadius)
{
	if (baseView == NULL || curve == NULL || drawingCenter == NULL)
	{
		return false;
	}

	UF_EVAL_p_t evaluator;
	if (UF_EVAL_initialize(curve->Tag(), &evaluator) != 0)
	{
		return false;
	}

	logical isArc = false;
	UF_EVAL_is_arc(evaluator, &isArc);
	if (!isArc)
	{
		UF_EVAL_free(evaluator);
		return false;
	}

	UF_EVAL_arc_t arcData;
	if (UF_EVAL_ask_arc(evaluator, &arcData) != 0)
	{
		UF_EVAL_free(evaluator);
		return false;
	}
	UF_EVAL_free(evaluator);

	drawingCenter[0] = arcData.center[0];
	drawingCenter[1] = arcData.center[1];
	drawingRadius = arcData.radius;
	return true;
}

bool TryGetDraftingFullCircleData(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Drawings::DraftingCurve* curve,
	double drawingCenter[2],
	double& drawingRadius)
{
	if (baseView == NULL || curve == NULL || drawingCenter == NULL)
	{
		return false;
	}

	UF_EVAL_p_t evaluator;
	if (UF_EVAL_initialize(curve->Tag(), &evaluator) != 0)
	{
		return false;
	}

	logical isArc = false;
	UF_EVAL_is_arc(evaluator, &isArc);
	if (!isArc)
	{
		UF_EVAL_free(evaluator);
		return false;
	}

	double limits[2] = { 0.0, 0.0 };
	if (UF_EVAL_ask_limits(evaluator, limits) != 0)
	{
		UF_EVAL_free(evaluator);
		return false;
	}

	UF_EVAL_arc_t arcData;
	if (UF_EVAL_ask_arc(evaluator, &arcData) != 0)
	{
		UF_EVAL_free(evaluator);
		return false;
	}
	UF_EVAL_free(evaluator);

	const double angleSpan = std::fabs(limits[1] - limits[0]);
	if (std::fabs(angleSpan - (2.0 * PI)) > 0.05)
	{
		return false;
	}

	drawingCenter[0] = arcData.center[0];
	drawingCenter[1] = arcData.center[1];
	drawingRadius = arcData.radius;
	return true;
}

bool TryGetDraftingFullCircleDrawingData(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Drawings::DraftingCurve* curve,
	double drawingCenter[2],
	double& drawingRadius)
{
	if (baseView == NULL || curve == NULL || drawingCenter == NULL)
	{
		return false;
	}

	UF_EVAL_p_t evaluator;
	if (UF_EVAL_initialize(curve->Tag(), &evaluator) != 0)
	{
		return false;
	}

	logical isArc = false;
	UF_EVAL_is_arc(evaluator, &isArc);
	if (!isArc)
	{
		UF_EVAL_free(evaluator);
		return false;
	}

	double limits[2] = { 0.0, 0.0 };
	if (UF_EVAL_ask_limits(evaluator, limits) != 0)
	{
		UF_EVAL_free(evaluator);
		return false;
	}

	UF_EVAL_arc_t arcData;
	if (UF_EVAL_ask_arc(evaluator, &arcData) != 0)
	{
		UF_EVAL_free(evaluator);
		return false;
	}
	UF_EVAL_free(evaluator);

	const double angleSpan = std::fabs(limits[1] - limits[0]);
	if (std::fabs(angleSpan - (2.0 * PI)) > 0.05)
	{
		return false;
	}

	double modelCenter[3] = { arcData.center[0], arcData.center[1], arcData.center[2] };
	double modelRadiusPoint[3] = {
		arcData.center[0] + arcData.radius,
		arcData.center[1],
		arcData.center[2]
	};
	double mappedCenter[2] = { 0.0, 0.0 };
	double mappedRadiusPoint[2] = { 0.0, 0.0 };
	if (UF_VIEW_map_model_to_drawing(baseView->Tag(), modelCenter, mappedCenter) != 0 ||
		UF_VIEW_map_model_to_drawing(baseView->Tag(), modelRadiusPoint, mappedRadiusPoint) != 0)
	{
		return false;
	}

	drawingCenter[0] = mappedCenter[0];
	drawingCenter[1] = mappedCenter[1];
	const double dx = mappedRadiusPoint[0] - mappedCenter[0];
	const double dy = mappedRadiusPoint[1] - mappedCenter[1];
	drawingRadius = std::sqrt(dx * dx + dy * dy);
	return true;
}

std::string TrimHoleNoteText(const std::string& text)
{
	size_t begin = 0;
	while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])))
	{
		++begin;
	}

	size_t end = text.size();
	while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])))
	{
		--end;
	}

	return text.substr(begin, end - begin);
}

bool TryGetStringUserAttribute(
	NXOpen::NXObject* object,
	const std::vector<NXOpen::NXString>& titles,
	std::string& value)
{
	if (object == NULL)
	{
		return false;
	}

	for (size_t i = 0; i < titles.size(); ++i)
	{
		try
		{
			if (object->HasUserAttribute(titles[i], NXOpen::NXObject::AttributeType::AttributeTypeString, -1))
			{
				const NXOpen::NXString nxValue = object->GetStringUserAttribute(titles[i], -1);
				value = TrimHoleNoteText(nxValue.GetText());
				if (value.empty())
				{
					value = TrimHoleNoteText(nxValue.GetLocaleText());
				}
				if (!value.empty())
				{
					return true;
				}
			}
		}
		catch (...)
		{
		}
	}

	return false;
}

bool TryGetCylindricalFaceDiameter(NXOpen::Face* face, double& diameter)
{
	if (face == NULL)
	{
		return false;
	}

	int faceType = 0;
	int normDir = 0;
	double facePoint[3] = { 0.0, 0.0, 0.0 };
	double dir[3] = { 0.0, 0.0, 0.0 };
	double box[6] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
	double radius = 0.0;
	double radData = 0.0;
	if (UF_MODL_ask_face_data(face->Tag(), &faceType, facePoint, dir, box, &radius, &radData, &normDir) != 0 ||
		faceType != UF_MODL_CYLINDRICAL_FACE ||
		radius <= 1e-6)
	{
		return false;
	}

	diameter = radius * 2.0;
	return true;
}

bool TryAcceptHoleCylinderFace(
	NXOpen::Face* face,
	double draftingDiameter,
	NXOpen::Face*& cylinderFace,
	double& cylinderDiameter)
{
	(void)draftingDiameter;
	double faceDiameter = 0.0;
	if (!TryGetCylindricalFaceDiameter(face, faceDiameter))
	{
		return false;
	}

	cylinderFace = face;
	cylinderDiameter = faceDiameter;
	return true;
}

bool TryResolveHoleCylinderFaceFromObject(
	NXOpen::NXObject* object,
	double draftingDiameter,
	NXOpen::Face*& cylinderFace,
	double& cylinderDiameter)
{
	if (object == NULL)
	{
		return false;
	}

	NXOpen::Face* face = dynamic_cast<NXOpen::Face*>(object);
	if (TryAcceptHoleCylinderFace(face, draftingDiameter, cylinderFace, cylinderDiameter))
	{
		return true;
	}

	NXOpen::Edge* edge = dynamic_cast<NXOpen::Edge*>(object);
	if (edge != NULL)
	{
		const std::vector<NXOpen::Face*> faces = edge->GetFaces();
		for (size_t i = 0; i < faces.size(); ++i)
		{
			if (TryAcceptHoleCylinderFace(faces[i], draftingDiameter, cylinderFace, cylinderDiameter))
			{
				return true;
			}
		}
	}

	return false;
}

std::string ToUpperHoleNoteText(std::string value)
{
	std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch)
		{
			return static_cast<char>(std::toupper(ch));
		});
	return value;
}

bool HoleNoteTextContainsCounterbore(const std::string& value)
{
	if (value.empty())
	{
		return false;
	}

	if (value.find("\xE6\xB2\x89\xE5\xAD\x94") != std::string::npos)
	{
		return true;
	}

	const std::string upperValue = ToUpperHoleNoteText(value);
	return upperValue.find("COUNTERBORE") != std::string::npos ||
		upperValue.find("CBORE") != std::string::npos ||
		upperValue.find("CB") != std::string::npos;
}

bool HoleNoteTextIsPlainHole(const std::string& value)
{
	const std::string text = TrimHoleNoteText(value);
	if (text.empty())
	{
		return false;
	}
	if (HoleNoteTextContainsCounterbore(text))
	{
		return false;
	}
	if (text == "\xE5\xAD\x94" || text == "\xE6\x99\xAE\xE9\x80\x9A\xE5\xAD\x94")
	{
		return true;
	}

	const std::string upperValue = ToUpperHoleNoteText(text);
	return upperValue == "HOLE" || upperValue == "PLAIN HOLE" || upperValue == "NORMAL HOLE";
}

void ReplaceAllHoleNoteText(std::string& text, const std::string& from, const std::string& to)
{
	if (from.empty())
	{
		return;
	}

	size_t position = 0;
	while ((position = text.find(from, position)) != std::string::npos)
	{
		text.replace(position, from.size(), to);
		position += to.size();
	}
}

std::string UnescapeHoleNoteIniValue(const std::string& value)
{
	std::string text;
	for (size_t index = 0; index < value.size(); ++index)
	{
		const char ch = value[index];
		if (ch != '\\' || index + 1 >= value.size())
		{
			text.push_back(ch);
			continue;
		}

		const char escaped = value[++index];
		switch (escaped)
		{
		case 'n':
			text.push_back('\n');
			break;
		case 'r':
			text.push_back('\r');
			break;
		case '\\':
			text.push_back('\\');
			break;
		default:
			text.push_back(escaped);
			break;
		}
	}
	return text;
}

bool SetHoleNoteRuleField(HoleNoteRuleRecord& record, const std::string& key, const std::string& value)
{
	if (key == "annotationType")
	{
		record.annotationType = value;
	}
	else if (key == "series")
	{
		record.series = value;
	}
	else if (key == "size")
	{
		record.size = value;
	}
	else if (key == "threadSpec")
	{
		record.threadSpec = value;
	}
	else if (key == "lengthText")
	{
		record.lengthText = value;
	}
	else if (key == "displayText")
	{
		record.displayText = value;
	}
	else if (key == "standardComment")
	{
		record.standardComment = value;
	}
	else if (key == "note")
	{
		record.note = value;
	}
	else if (key == "bottomHole")
	{
		const std::string trimmed = TrimHoleNoteText(value);
		if (trimmed.empty())
		{
			record.bottomHole = 0.0;
			return true;
		}

		char* end = NULL;
		const double parsed = std::strtod(trimmed.c_str(), &end);
		if (end == trimmed.c_str())
		{
			return false;
		}
		record.bottomHole = parsed;
	}
	return true;
}

bool HoleNoteRuleHasContent(const HoleNoteRuleRecord& record)
{
	return !record.annotationType.empty() ||
		!record.series.empty() ||
		!record.size.empty() ||
		!record.threadSpec.empty() ||
		!record.displayText.empty() ||
		record.bottomHole > 1.0e-6;
}

std::vector<HoleNoteRuleRecord> LoadHoleNoteRulesFromIni(const std::string& path)
{
	std::vector<HoleNoteRuleRecord> rules;
	std::ifstream input(path.c_str(), std::ios::in | std::ios::binary);
	if (!input.is_open())
	{
		return rules;
	}

	HoleNoteRuleRecord current;
	bool inRule = false;
	std::string line;
	while (std::getline(input, line))
	{
		if (!line.empty() && line[line.size() - 1] == '\r')
		{
			line.erase(line.size() - 1);
		}
		if (line.size() >= 3 &&
			static_cast<unsigned char>(line[0]) == 0xEF &&
			static_cast<unsigned char>(line[1]) == 0xBB &&
			static_cast<unsigned char>(line[2]) == 0xBF)
		{
			line.erase(0, 3);
		}

		const std::string trimmed = TrimHoleNoteText(line);
		if (trimmed.empty() || trimmed[0] == ';' || trimmed[0] == '#')
		{
			continue;
		}

		if (trimmed[0] == '[' && trimmed[trimmed.size() - 1] == ']')
		{
			if (inRule && HoleNoteRuleHasContent(current))
			{
				rules.push_back(current);
			}
			current = HoleNoteRuleRecord();
			inRule = true;
			continue;
		}

		const size_t equals = trimmed.find('=');
		if (equals == std::string::npos)
		{
			continue;
		}
		const std::string key = TrimHoleNoteText(trimmed.substr(0, equals));
		const std::string value = UnescapeHoleNoteIniValue(TrimHoleNoteText(trimmed.substr(equals + 1)));
		SetHoleNoteRuleField(current, key, value);
	}

	if (inRule && HoleNoteRuleHasContent(current))
	{
		rules.push_back(current);
	}
	return rules;
}

std::vector<HoleNoteRuleRecord> GetHoleNoteRules()
{
	const std::vector<std::string> paths = {
		"D:\\UG智辉钣金插件\\config\\KonBiaoZuRules.ini"
	};
	for (size_t i = 0; i < paths.size(); ++i)
	{
		std::vector<HoleNoteRuleRecord> rules = LoadHoleNoteRulesFromIni(paths[i]);
		if (!rules.empty())
		{
			std::ostringstream log;
			log << "[HoleNote] loaded rules=" << rules.size() << " path=" << paths[i];
			HoleNoteDebugLog(log.str());
			return rules;
		}
	}
	return std::vector<HoleNoteRuleRecord>();
}

bool HoleNoteRuleIsCounterboreRule(const HoleNoteRuleRecord& rule)
{
	const std::string combined = rule.annotationType + "|" + rule.series + "|" + rule.note + "|" + rule.displayText;
	return combined.find("\xE6\xB2\x89\xE5\xAD\x94") != std::string::npos ||
		HoleNoteTextContainsCounterbore(combined);
}

std::string FormatHoleRuleNumber(double value)
{
	std::ostringstream out;
	out << std::fixed << std::setprecision(2) << value;
	return out.str();
}

std::string ApplyHoleNoteRuleTemplate(const std::string& templateText, const HoleNoteRuleRecord& rule)
{
	std::string result = templateText;
	ReplaceAllHoleNoteText(result, "\x7B\xE5\xAD\x90\xE7\xB1\xBB\xE5\x9E\x8B\x7D", rule.series);
	ReplaceAllHoleNoteText(result, "\x7B\xE8\xA7\x84\xE6\xA0\xBC\x7D", rule.size);
	ReplaceAllHoleNoteText(result, "\x7B\xE8\x9E\xBA\xE7\xBA\xB9\x7D", rule.threadSpec);
	ReplaceAllHoleNoteText(result, "\x7B\xE5\xBA\x95\xE5\xAD\x94\x7D", FormatHoleRuleNumber(rule.bottomHole));
	ReplaceAllHoleNoteText(result, "\x7B\xE9\x95\xBF\xE5\xBA\xA6\x7D", rule.lengthText);
	return TrimHoleNoteText(result);
}

std::string BuildMatchedRuleHoleNoteText(const HoleNoteRuleRecord& rule)
{
	if (!rule.displayText.empty())
	{
		const std::string text = ApplyHoleNoteRuleTemplate(rule.displayText, rule);
		if (!text.empty())
		{
			return text;
		}
	}
	if (!rule.threadSpec.empty())
	{
		return rule.threadSpec;
	}
	return rule.size;
}

bool HoleNoteRuleIsPemRule(const HoleNoteRuleRecord& rule)
{
	return rule.annotationType == "\xE5\x8E\x8B\xE9\x93\x86\xE8\x9E\xBA\xE6\xAF\x8D\xE6\xA0\x87\xE6\xB3\xA8" ||
		rule.annotationType == "\xE5\x8E\x8B\xE9\x93\x86\xE8\x9E\xBA\xE9\x92\x89\xE6\xA0\x87\xE6\xB3\xA8" ||
		rule.annotationType == "\xE5\x8E\x8B\xE9\x93\x86\xE8\x9E\xBA\xE6\xAF\x8D\xE6\x9F\xB1\xE6\xA0\x87\xE6\xB3\xA8";
}

std::string HoleNoteRuleTypeForRecord(const HoleNoteRuleRecord& rule)
{
	if (rule.annotationType == "\xE5\xAD\x94\xE6\xA0\x87\xE6\xB3\xA8")
	{
		return "\xE5\xAD\x94";
	}
	if (rule.annotationType == "\xE8\x9E\xBA\xE7\x89\x99\xE6\xA0\x87\xE6\xB3\xA8")
	{
		return "\xE6\x94\xBB\xE7\x89\x99";
	}
	if (rule.annotationType == "\xE7\x84\x8A\xE6\x8E\xA5\xE8\x9E\xBA\xE6\xAF\x8D\xE6\xA0\x87\xE6\xB3\xA8")
	{
		return "\xE7\x84\x8A\xE6\x8E\xA5\xE8\x9E\xBA\xE6\xAF\x8D";
	}
	if (HoleNoteRuleIsPemRule(rule))
	{
		return rule.series.empty() ? rule.annotationType : rule.series;
	}

	const std::string suffix = "\xE6\xA0\x87\xE6\xB3\xA8";
	if (rule.annotationType.size() > suffix.size() &&
		rule.annotationType.substr(rule.annotationType.size() - suffix.size()) == suffix)
	{
		return rule.annotationType.substr(0, rule.annotationType.size() - suffix.size());
	}
	return rule.annotationType.empty() ? "\xE5\xAD\x94" : rule.annotationType;
}

bool TryBuildMatchedRuleHoleNoteText(double diameter, std::string& typeText, std::string& noteText)
{
	const std::vector<HoleNoteRuleRecord> rules = GetHoleNoteRules();
	const double tolerance = 0.001;
	for (size_t i = 0; i < rules.size(); ++i)
	{
		const HoleNoteRuleRecord& rule = rules[i];
		if (rule.bottomHole <= 1.0e-6 || std::fabs(rule.bottomHole - diameter) >= tolerance)
		{
			continue;
		}
		if (HoleNoteRuleIsCounterboreRule(rule))
		{
			continue;
		}

		typeText = HoleNoteRuleTypeForRecord(rule);
		if (typeText.empty())
		{
			typeText = "\xE5\xAD\x94";
		}
		noteText = BuildMatchedRuleHoleNoteText(rule);
		return !noteText.empty();
	}
	return false;
}

int BuildCounterboreScrewSize(double diameter)
{
	if (diameter <= 1.0)
	{
		return 0;
	}

	const int screwSize = static_cast<int>(std::ceil(diameter - 0.001)) - 1;
	return screwSize > 0 ? screwSize : 0;
}

bool TryAskPlanarFaceNormalZ(tag_t faceTag, double& normalZ)
{
	normalZ = 0.0;
	if (faceTag == NULL_TAG)
	{
		return false;
	}

	int faceType = 0;
	double point[3] = { 0.0, 0.0, 0.0 };
	double dir[3] = { 0.0, 0.0, 0.0 };
	double box[6] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
	double radius = 0.0;
	double radData = 0.0;
	int normDir = 1;
	if (UF_MODL_ask_face_data(faceTag, &faceType, point, dir, box, &radius, &radData, &normDir) != 0 ||
		faceType != UF_MODL_PLANAR_FACE)
	{
		return false;
	}

	double uvMinMax[4] = { 0.0, 0.0, 0.0, 0.0 };
	if (UF_MODL_ask_face_uv_minmax(faceTag, uvMinMax) != 0)
	{
		return false;
	}

	double param[2] = {
		(uvMinMax[0] + uvMinMax[1]) * 0.5,
		(uvMinMax[2] + uvMinMax[3]) * 0.5
	};
	double facePoint[3] = { 0.0, 0.0, 0.0 };
	double u1[3] = { 0.0, 0.0, 0.0 };
	double v1[3] = { 0.0, 0.0, 0.0 };
	double u2[3] = { 0.0, 0.0, 0.0 };
	double v2[3] = { 0.0, 0.0, 0.0 };
	double unitNorm[3] = { 0.0, 0.0, 0.0 };
	double radii[2] = { 0.0, 0.0 };
	if (UF_MODL_ask_face_props(faceTag, param, facePoint, u1, v1, u2, v2, unitNorm, radii) != 0)
	{
		return false;
	}

	const double length = std::sqrt(unitNorm[0] * unitNorm[0] + unitNorm[1] * unitNorm[1] + unitNorm[2] * unitNorm[2]);
	if (length < 1.0e-9)
	{
		return false;
	}

	normalZ = unitNorm[2] / length;
	return true;
}

std::string ResolveCounterboreSideText(NXOpen::Face* cylinderFace)
{
	if (cylinderFace == NULL)
	{
		return std::string();
	}

	int frontCount = 0;
	int backCount = 0;
	int unknownCount = 0;
	const std::vector<NXOpen::Edge*> edges = cylinderFace->GetEdges();
	for (size_t edgeIndex = 0; edgeIndex < edges.size(); ++edgeIndex)
	{
		if (edges[edgeIndex] == NULL)
		{
			continue;
		}

		const std::vector<NXOpen::Face*> faces = edges[edgeIndex]->GetFaces();
		for (size_t faceIndex = 0; faceIndex < faces.size(); ++faceIndex)
		{
			NXOpen::Face* face = faces[faceIndex];
			if (face == NULL || face->Tag() == cylinderFace->Tag())
			{
				continue;
			}

			double normalZ = 0.0;
			if (!TryAskPlanarFaceNormalZ(face->Tag(), normalZ))
			{
				++unknownCount;
				continue;
			}

			if (normalZ > 0.15)
			{
				++frontCount;
			}
			else if (normalZ < -0.15)
			{
				++backCount;
			}
			else
			{
				++unknownCount;
			}
		}
	}

	if (frontCount > 0 && backCount == 0)
	{
		return "\xE6\xAD\xA3\xE9\x9D\xA2";
	}
	if (backCount > 0 && frontCount == 0)
	{
		return "\xE8\x83\x8C\xE9\x9D\xA2";
	}
	return std::string();
}

void AccumulateCounterborePlanarFaceSide(tag_t faceTag, int& frontCount, int& backCount, int& unknownCount)
{
	double normalZ = 0.0;
	if (!TryAskPlanarFaceNormalZ(faceTag, normalZ))
	{
		++unknownCount;
		return;
	}

	if (normalZ > 0.15)
	{
		++frontCount;
	}
	else if (normalZ < -0.15)
	{
		++backCount;
	}
	else
	{
		++unknownCount;
	}
}

std::string BuildCounterboreSideTextFromCounts(int frontCount, int backCount)
{
	if (frontCount > 0 && backCount == 0)
	{
		return "\xE6\xAD\xA3\xE9\x9D\xA2";
	}
	if (backCount > 0 && frontCount == 0)
	{
		return "\xE8\x83\x8C\xE9\x9D\xA2";
	}
	return std::string();
}

std::string BuildCounterboreSpecText(double diameter, const std::string& sideText)
{
	std::ostringstream out;
	const int screwSize = BuildCounterboreScrewSize(diameter);
	if (screwSize > 0)
	{
		out << "M" << screwSize << "\xE8\x9E\xBA\xE4\xB8\x9D\xE6\xB2\x89\xE5\xAD\x94";
	}
	else
	{
		out << "\xE6\xB2\x89\xE5\xAD\x94";
	}

	if (!sideText.empty())
	{
		out << "(" << sideText << ")";
	}
	return out.str();
}

bool FeatureTypeContains(tag_t featureTag, const char* expectedType)
{
	if (featureTag == NULL_TAG || expectedType == NULL)
	{
		return false;
	}

	char* featureType = NULL;
	if (UF_MODL_ask_feat_type(featureTag, &featureType) != 0 || featureType == NULL)
	{
		return false;
	}

	const std::string actual = ToUpperHoleNoteText(featureType);
	UF_free(featureType);
	return actual.find(ToUpperHoleNoteText(expectedType)) != std::string::npos;
}

bool TryAskArcDataByTag(tag_t objectTag, double& diameter, double center[3])
{
	diameter = 0.0;
	if (center != NULL)
	{
		center[0] = 0.0;
		center[1] = 0.0;
		center[2] = 0.0;
	}
	if (objectTag == NULL_TAG || center == NULL)
	{
		return false;
	}

	UF_EVAL_p_t evaluator = NULL;
	if (UF_EVAL_initialize(objectTag, &evaluator) != 0 || evaluator == NULL)
	{
		return false;
	}

	logical isArc = false;
	UF_EVAL_is_arc(evaluator, &isArc);
	if (!isArc)
	{
		UF_EVAL_free(evaluator);
		return false;
	}

	UF_EVAL_arc_t arcData;
	if (UF_EVAL_ask_arc(evaluator, &arcData) != 0)
	{
		UF_EVAL_free(evaluator);
		return false;
	}
	UF_EVAL_free(evaluator);

	if (arcData.radius <= 1e-6)
	{
		return false;
	}

	center[0] = arcData.center[0];
	center[1] = arcData.center[1];
	center[2] = arcData.center[2];
	diameter = arcData.radius * 2.0;
	return true;
}

void ResetHoleNoteDebugLog()
{
	std::ofstream stream("D:\\UG智辉钣金插件\\logs\\hole_note_debug.log", std::ios::out | std::ios::trunc);
	stream << "[HoleNote] reset\n";
}

void HoleNoteDebugLog(const std::string& message)
{
	std::ofstream stream("D:\\UG智辉钣金插件\\logs\\hole_note_debug.log", std::ios::out | std::ios::app);
	stream << message << "\n";
}

std::string AskFeatureTypeText(tag_t featureTag)
{
	if (featureTag == NULL_TAG)
	{
		return "";
	}

	char* featureType = NULL;
	if (UF_MODL_ask_feat_type(featureTag, &featureType) != 0 || featureType == NULL)
	{
		return "";
	}

	const std::string result = featureType;
	UF_free(featureType);
	return result;
}

void LogHoleNoteParentDetails(NXOpen::Drawings::DraftingCurve* curve)
{
	if (curve == NULL)
	{
		HoleNoteDebugLog("[HoleNote] parent detail: curve=null");
		return;
	}

	try
	{
		const std::vector<NXOpen::NXObject*> nxParents = curve->GetDraftingCurveInfo()->GetParents();
		std::ostringstream nxLog;
		nxLog << "[HoleNote] NXOpen parents count=" << nxParents.size();
		HoleNoteDebugLog(nxLog.str());
		for (size_t i = 0; i < nxParents.size(); ++i)
		{
			tag_t parentTag = nxParents[i] != NULL ? nxParents[i]->Tag() : NULL_TAG;
			int type = 0;
			int subtype = 0;
			if (parentTag != NULL_TAG)
			{
				UF_OBJ_ask_type_and_subtype(parentTag, &type, &subtype);
			}
			tag_t featureTag = NULL_TAG;
			if (parentTag != NULL_TAG)
			{
				UF_MODL_ask_object_feat(parentTag, &featureTag);
			}
			std::ostringstream line;
			line << "[HoleNote]   NXParent[" << i << "] tag=" << parentTag
				<< " type=" << type
				<< " subtype=" << subtype
				<< " feat=" << featureTag
				<< " featType=" << AskFeatureTypeText(featureTag);
			HoleNoteDebugLog(line.str());
		}
	}
	catch (const NXOpen::NXException& ex)
	{
		HoleNoteDebugLog(std::string("[HoleNote] NXOpen parents failed: ") + ex.Message());
	}
	catch (...)
	{
		HoleNoteDebugLog("[HoleNote] NXOpen parents failed: unknown");
	}

	int parentsCount = 0;
	tag_t* parentsTags = NULL;
	if (UF_DRAW_ask_drafting_curve_parents(curve->Tag(), &parentsCount, &parentsTags) != 0 || parentsTags == NULL)
	{
		HoleNoteDebugLog("[HoleNote] UF parents failed or empty");
		return;
	}

	std::ostringstream countLog;
	countLog << "[HoleNote] UF parents count=" << parentsCount;
	HoleNoteDebugLog(countLog.str());
	for (int i = 0; i < parentsCount; ++i)
	{
		int type = 0;
		int subtype = 0;
		if (parentsTags[i] != NULL_TAG)
		{
			UF_OBJ_ask_type_and_subtype(parentsTags[i], &type, &subtype);
		}
		tag_t featureTag = NULL_TAG;
		if (parentsTags[i] != NULL_TAG)
		{
			UF_MODL_ask_object_feat(parentsTags[i], &featureTag);
		}
		double center[3] = { 0.0, 0.0, 0.0 };
		double diameter = 0.0;
		const bool isArc = TryAskArcDataByTag(parentsTags[i], diameter, center);
		std::ostringstream line;
		line << "[HoleNote]   UFParent[" << i << "] tag=" << parentsTags[i]
			<< " type=" << type
			<< " subtype=" << subtype
			<< " feat=" << featureTag
			<< " featType=" << AskFeatureTypeText(featureTag)
			<< " arc=" << (isArc ? "yes" : "no")
			<< " dia=" << diameter
			<< " center=(" << center[0] << "," << center[1] << "," << center[2] << ")";
		HoleNoteDebugLog(line.str());
	}
	UF_free(parentsTags);
}

bool TryResolveFlatPatternParentCurveForHoleNote(
	NXOpen::Drawings::DraftingCurve* curve,
	tag_t& parentCurveTag,
	tag_t& flatPatternFeatureTag,
	double parentCenter[3],
	double& parentDiameter)
{
	parentCurveTag = NULL_TAG;
	flatPatternFeatureTag = NULL_TAG;
	parentDiameter = 0.0;
	if (parentCenter != NULL)
	{
		parentCenter[0] = 0.0;
		parentCenter[1] = 0.0;
		parentCenter[2] = 0.0;
	}
	if (curve == NULL || parentCenter == NULL)
	{
		return false;
	}

	int parentsCount = 0;
	tag_t* parentsTags = NULL;
	if (UF_DRAW_ask_drafting_curve_parents(curve->Tag(), &parentsCount, &parentsTags) != 0 || parentsTags == NULL)
	{
		return false;
	}

	bool resolved = false;
	for (int i = 0; i < parentsCount && !resolved; ++i)
	{
		if (parentsTags[i] == NULL_TAG)
		{
			continue;
		}

		double candidateCenter[3] = { 0.0, 0.0, 0.0 };
		double candidateDiameter = 0.0;
		if (!TryAskArcDataByTag(parentsTags[i], candidateDiameter, candidateCenter))
		{
			continue;
		}

		tag_t featureTag = NULL_TAG;
		if (UF_MODL_ask_object_feat(parentsTags[i], &featureTag) != 0 || featureTag == NULL_TAG)
		{
			continue;
		}

		if (!FeatureTypeContains(featureTag, "FLAT_PATTERN"))
		{
			continue;
		}

		parentCurveTag = parentsTags[i];
		flatPatternFeatureTag = featureTag;
		parentCenter[0] = candidateCenter[0];
		parentCenter[1] = candidateCenter[1];
		parentCenter[2] = candidateCenter[2];
		parentDiameter = candidateDiameter;
		resolved = true;
	}

	UF_free(parentsTags);
	return resolved;
}

bool TryResolveHoleCylinderFaceFromBodyEdges(
	tag_t bodyTag,
	const double targetCenter[3],
	double targetDiameter,
	NXOpen::Face*& cylinderFace,
	double& cylinderDiameter)
{
	if (bodyTag == NULL_TAG || targetCenter == NULL)
	{
		return false;
	}

	uf_list_p_t edgeList = NULL;
	if (UF_MODL_ask_body_edges(bodyTag, &edgeList) != 0 || edgeList == NULL)
	{
		std::ostringstream log;
		log << "[HoleNote]     body edges failed body=" << bodyTag;
		HoleNoteDebugLog(log.str());
		return false;
	}

	bool resolved = false;
	int edgeCount = 0;
	UF_MODL_ask_list_count(edgeList, &edgeCount);
	int arcCount = 0;
	int centerDiameterMatchCount = 0;
	for (int i = 0; i < edgeCount && !resolved; ++i)
	{
		tag_t edgeTag = NULL_TAG;
		if (UF_MODL_ask_list_item(edgeList, i, &edgeTag) != 0 || edgeTag == NULL_TAG)
		{
			continue;
		}

		double edgeCenter[3] = { 0.0, 0.0, 0.0 };
		double edgeDiameter = 0.0;
		if (!TryAskArcDataByTag(edgeTag, edgeDiameter, edgeCenter))
		{
			continue;
		}
		++arcCount;

		if (std::fabs(edgeDiameter - targetDiameter) > 0.05 ||
			std::fabs(edgeCenter[0] - targetCenter[0]) > 0.05 ||
			std::fabs(edgeCenter[1] - targetCenter[1]) > 0.05)
		{
			continue;
		}
		++centerDiameterMatchCount;

		NXOpen::NXObject* edgeObject = NULL;
		try
		{
			edgeObject = dynamic_cast<NXOpen::NXObject*>(NXOpen::NXObjectManager::Get(edgeTag));
		}
		catch (...)
		{
			edgeObject = NULL;
		}

		resolved = TryResolveHoleCylinderFaceFromObject(edgeObject, 0.0, cylinderFace, cylinderDiameter);
	}

	UF_MODL_delete_list(&edgeList);
	{
		std::ostringstream log;
		log << "[HoleNote]     body=" << bodyTag
			<< " edgeCount=" << edgeCount
			<< " arcCount=" << arcCount
			<< " matchCount=" << centerDiameterMatchCount
			<< " resolved=" << (resolved ? "true" : "false");
		HoleNoteDebugLog(log.str());
	}
	return resolved;
}

std::vector<tag_t> CollectHoleNoteFlatSolidFeatures(tag_t flatPatternFeatureTag)
{
	std::vector<tag_t> flatSolidFeatures;
	std::unordered_set<tag_t> seen;
	if (flatPatternFeatureTag != NULL_TAG)
	{
		int numParents = 0;
		tag_t* parentFeatures = NULL;
		int numChildren = 0;
		tag_t* childFeatures = NULL;
		if (UF_MODL_ask_feat_relatives(flatPatternFeatureTag, &numParents, &parentFeatures, &numChildren, &childFeatures) == 0 &&
			parentFeatures != NULL)
		{
			std::ostringstream log;
			log << "[HoleNote]   flatPattern relatives parents=" << numParents << " children=" << numChildren;
			HoleNoteDebugLog(log.str());
			for (int i = 0; i < numParents; ++i)
			{
				std::ostringstream featureLog;
				featureLog << "[HoleNote]     parentFeature[" << i << "] tag=" << parentFeatures[i]
					<< " type=" << AskFeatureTypeText(parentFeatures[i]);
				HoleNoteDebugLog(featureLog.str());
				if (FeatureTypeContains(parentFeatures[i], "SB_FLAT_SOLID") && seen.insert(parentFeatures[i]).second)
				{
					flatSolidFeatures.push_back(parentFeatures[i]);
				}
			}
		}
		if (parentFeatures != NULL)
		{
			UF_free(parentFeatures);
		}
		if (childFeatures != NULL)
		{
			UF_free(childFeatures);
		}
	}

	if (!flatSolidFeatures.empty() || workPart == NULL)
	{
		return flatSolidFeatures;
	}

	tag_t featureTag = NULL_TAG;
	while (UF_OBJ_cycle_objs_in_part(workPart->Tag(), UF_feature_type, &featureTag) == 0 && featureTag != NULL_TAG)
	{
		if (!FeatureTypeContains(featureTag, "SB_FLAT_SOLID"))
		{
			continue;
		}
		if (seen.insert(featureTag).second)
		{
			flatSolidFeatures.push_back(featureTag);
		}
	}
	std::ostringstream log;
	log << "[HoleNote]   fallback SB_FLAT_SOLID count=" << flatSolidFeatures.size();
	HoleNoteDebugLog(log.str());
	return flatSolidFeatures;
}

bool TryResolveHoleCylinderFaceFromFlatPattern(
	NXOpen::Drawings::DraftingCurve* curve,
	NXOpen::Face*& cylinderFace,
	double& cylinderDiameter)
{
	tag_t parentCurveTag = NULL_TAG;
	tag_t flatPatternFeatureTag = NULL_TAG;
	double parentCenter[3] = { 0.0, 0.0, 0.0 };
	double parentDiameter = 0.0;
	if (!TryResolveFlatPatternParentCurveForHoleNote(
		curve,
		parentCurveTag,
		flatPatternFeatureTag,
		parentCenter,
		parentDiameter))
	{
		HoleNoteDebugLog("[HoleNote]   flat pattern parent curve not resolved");
		return false;
	}

	bool resolved = false;
	{
		std::ostringstream log;
		log << "[HoleNote]   flatPattern parentCurve=" << parentCurveTag
			<< " feature=" << flatPatternFeatureTag
			<< " parentDia=" << parentDiameter
			<< " parentCenter=(" << parentCenter[0] << "," << parentCenter[1] << "," << parentCenter[2] << ")";
		HoleNoteDebugLog(log.str());
	}
	const std::vector<tag_t> flatSolidFeatures = CollectHoleNoteFlatSolidFeatures(flatPatternFeatureTag);
	{
		std::ostringstream log;
		log << "[HoleNote]   SB_FLAT_SOLID candidates=" << flatSolidFeatures.size();
		HoleNoteDebugLog(log.str());
	}
	for (size_t i = 0; i < flatSolidFeatures.size() && !resolved; ++i)
	{
		tag_t bodyTag = NULL_TAG;
		if (UF_MODL_ask_feat_body(flatSolidFeatures[i], &bodyTag) != 0 || bodyTag == NULL_TAG)
		{
			std::ostringstream log;
			log << "[HoleNote]     SB feature " << flatSolidFeatures[i] << " body not found";
			HoleNoteDebugLog(log.str());
			continue;
		}
		std::ostringstream log;
		log << "[HoleNote]     SB feature " << flatSolidFeatures[i] << " body=" << bodyTag;
		HoleNoteDebugLog(log.str());
		resolved = TryResolveHoleCylinderFaceFromBodyEdges(
			bodyTag,
			parentCenter,
			parentDiameter,
			cylinderFace,
			cylinderDiameter);
	}

	return resolved;
}

std::string ResolveCounterboreSideTextFromFlatPattern(NXOpen::Drawings::DraftingCurve* curve)
{
	if (curve == NULL)
	{
		return std::string();
	}

	tag_t parentCurveTag = NULL_TAG;
	tag_t flatPatternFeatureTag = NULL_TAG;
	double parentCenter[3] = { 0.0, 0.0, 0.0 };
	double parentDiameter = 0.0;
	if (!TryResolveFlatPatternParentCurveForHoleNote(
		curve,
		parentCurveTag,
		flatPatternFeatureTag,
		parentCenter,
		parentDiameter))
	{
		HoleNoteDebugLog("[HoleNote]   counterbore side: flat pattern parent curve not resolved");
		return std::string();
	}

	const std::vector<tag_t> flatSolidFeatures = CollectHoleNoteFlatSolidFeatures(flatPatternFeatureTag);
	if (flatSolidFeatures.empty())
	{
		HoleNoteDebugLog("[HoleNote]   counterbore side: no SB_FLAT_SOLID candidates");
		return std::string();
	}

	int frontCount = 0;
	int backCount = 0;
	int unknownCount = 0;
	const double centerTolerance = 0.02;
	const double diameterTolerance = 0.02;
	for (size_t featureIndex = 0; featureIndex < flatSolidFeatures.size(); ++featureIndex)
	{
		tag_t bodyTag = NULL_TAG;
		if (UF_MODL_ask_feat_body(flatSolidFeatures[featureIndex], &bodyTag) != 0 || bodyTag == NULL_TAG)
		{
			continue;
		}

		uf_list_p_t edgeList = NULL;
		if (UF_MODL_ask_body_edges(bodyTag, &edgeList) != 0 || edgeList == NULL)
		{
			continue;
		}

		int edgeCount = 0;
		UF_MODL_ask_list_count(edgeList, &edgeCount);
		for (int edgeIndex = 0; edgeIndex < edgeCount; ++edgeIndex)
		{
			tag_t edgeTag = NULL_TAG;
			if (UF_MODL_ask_list_item(edgeList, edgeIndex, &edgeTag) != 0 || edgeTag == NULL_TAG)
			{
				continue;
			}

			double edgeCenter[3] = { 0.0, 0.0, 0.0 };
			double edgeDiameter = 0.0;
			if (!TryAskArcDataByTag(edgeTag, edgeDiameter, edgeCenter))
			{
				continue;
			}

			if (std::fabs(edgeDiameter - parentDiameter) > diameterTolerance ||
				std::fabs(edgeCenter[0] - parentCenter[0]) > centerTolerance ||
				std::fabs(edgeCenter[1] - parentCenter[1]) > centerTolerance)
			{
				continue;
			}

			uf_list_p_t faceList = NULL;
			if (UF_MODL_ask_edge_faces(edgeTag, &faceList) != 0 || faceList == NULL)
			{
				continue;
			}

			int faceCount = 0;
			UF_MODL_ask_list_count(faceList, &faceCount);
			for (int faceIndex = 0; faceIndex < faceCount; ++faceIndex)
			{
				tag_t faceTag = NULL_TAG;
				if (UF_MODL_ask_list_item(faceList, faceIndex, &faceTag) != 0 || faceTag == NULL_TAG)
				{
					continue;
				}

				int faceType = 0;
				double point[3] = { 0.0, 0.0, 0.0 };
				double dir[3] = { 0.0, 0.0, 0.0 };
				double box[6] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
				double radius = 0.0;
				double radData = 0.0;
				int normDir = 1;
				if (UF_MODL_ask_face_data(faceTag, &faceType, point, dir, box, &radius, &radData, &normDir) != 0 ||
					faceType != UF_MODL_PLANAR_FACE)
				{
					continue;
				}

				AccumulateCounterborePlanarFaceSide(faceTag, frontCount, backCount, unknownCount);
				break;
			}

			UF_MODL_delete_list(&faceList);
		}

		UF_MODL_delete_list(&edgeList);
	}

	{
		std::ostringstream log;
		log << "[HoleNote]   counterbore side by flat pattern parentCurve=" << parentCurveTag
			<< " dia=" << parentDiameter
			<< " front=" << frontCount
			<< " back=" << backCount
			<< " unknown=" << unknownCount;
		HoleNoteDebugLog(log.str());
	}
	return BuildCounterboreSideTextFromCounts(frontCount, backCount);
}

bool TryResolveHoleCylinderFace(
	NXOpen::Drawings::DraftingCurve* curve,
	double draftingDiameter,
	NXOpen::Face*& cylinderFace,
	double& cylinderDiameter)
{
	cylinderFace = NULL;
	cylinderDiameter = 0.0;
	if (curve == NULL)
	{
		return false;
	}

	const std::vector<NXOpen::NXObject*> parents = curve->GetDraftingCurveInfo()->GetParents();
	for (size_t i = 0; i < parents.size(); ++i)
	{
		if (TryResolveHoleCylinderFaceFromObject(parents[i], draftingDiameter, cylinderFace, cylinderDiameter))
		{
			return true;
		}
	}

	int parentsCount = 0;
	tag_t* parentsTags = NULL;
	if (UF_DRAW_ask_drafting_curve_parents(curve->Tag(), &parentsCount, &parentsTags) != 0 || parentsTags == NULL)
	{
		return false;
	}

	bool resolved = false;
	for (int i = 0; i < parentsCount && !resolved; ++i)
	{
		if (parentsTags[i] == NULL_TAG)
		{
			continue;
		}

		NXOpen::NXObject* parentObject = NULL;
		try
		{
			parentObject = dynamic_cast<NXOpen::NXObject*>(NXOpen::NXObjectManager::Get(parentsTags[i]));
		}
		catch (...)
		{
			parentObject = NULL;
		}

		resolved = TryResolveHoleCylinderFaceFromObject(parentObject, draftingDiameter, cylinderFace, cylinderDiameter);
	}

	UF_free(parentsTags);
	if (resolved)
	{
		return true;
	}

	return TryResolveHoleCylinderFaceFromFlatPattern(curve, cylinderFace, cylinderDiameter);
}

std::string BuildHoleAttributeGroupKey(double diameter, const std::string& typeText, const std::string& specText)
{
	std::ostringstream key;
	key << static_cast<long long>(std::llround(diameter * 1000.0))
		<< "|" << typeText
		<< "|" << specText;
	return key.str();
}

std::string BuildHoleAttributeNoteText(const FlatPatternHoleNoteGroup& group)
{
	std::ostringstream text;
	if (HoleNoteTextIsPlainHole(group.typeText))
	{
		if (group.count > 1)
		{
			text << group.count << "-";
		}
		return text.str();
	}
	const std::string noteText = group.specText.empty() ? group.typeText : group.specText;
	if (noteText.empty())
	{
		return std::string();
	}
	if (group.count > 1)
	{
		text << group.count << "-";
	}
	text << noteText;
	return text.str();
}

std::string BuildNextHoleMarkerCode(const std::string& current)
{
	if (current.empty())
	{
		return current;
	}
	std::string next = current;
	for (int i = static_cast<int>(next.size()) - 1; i >= 0; --i)
	{
		if (next[i] >= 'A' && next[i] < 'Z')
		{
			++next[i];
			return next;
		}
		if (next[i] >= 'a' && next[i] < 'z')
		{
			++next[i];
			return next;
		}
	}
	return next + "A";
}

std::string BuildMarkedHoleAttributeNoteText(const std::string& markerCode, const std::string& noteText)
{
	if (markerCode.empty())
	{
		return noteText;
	}
	if (noteText.empty())
	{
		return markerCode;
	}
	return markerCode + " " + noteText;
}

NXOpen::Point3d ComputeHoleAttributeNotePoint(
	const HoleCoordinateExtents& extents,
	const FlatPatternHoleNoteGroup& group,
	int index)
{
	const double width = std::max(1.0, extents.maxX - extents.minX);
	const double height = std::max(1.0, extents.maxY - extents.minY);
	const double offset = std::max(6.0, std::min(width, height) * 0.035);
	const bool useRightSide = group.drawingCenter.X <= (extents.minX + extents.maxX) * 0.5;
	const double signX = useRightSide ? 1.0 : -1.0;
	const double staggerY = static_cast<double>(index % 3) * std::max(3.0, offset * 0.55);
	return NXOpen::Point3d(
		group.drawingCenter.X + signX * (group.drawingRadius + offset),
		group.drawingCenter.Y + group.drawingRadius + offset + staggerY,
		0.0);
}

bool TryFindOuterConcentricHoleNoteCircle(
	NXOpen::Drawings::BaseView* baseView,
	const std::vector<NXOpen::Drawings::DraftingCurve*>& curves,
	const double targetCenter[2],
	double targetRadius,
	NXOpen::Drawings::DraftingCurve*& outerCurve,
	double outerCenter[2],
	double& outerRadius)
{
	outerCurve = NULL;
	outerRadius = targetRadius;
	if (outerCenter != NULL && targetCenter != NULL)
	{
		outerCenter[0] = targetCenter[0];
		outerCenter[1] = targetCenter[1];
	}
	if (baseView == NULL || targetCenter == NULL || outerCenter == NULL)
	{
		return false;
	}

	const double centerTolerance = 0.02;
	for (size_t i = 0; i < curves.size(); ++i)
	{
		if (curves[i] == NULL)
		{
			continue;
		}

		double candidateCenter[2] = { 0.0, 0.0 };
		double candidateRadius = 0.0;
		if (!TryGetDraftingFullCircleDrawingData(baseView, curves[i], candidateCenter, candidateRadius) ||
			candidateRadius <= 1.0e-6)
		{
			continue;
		}

		if (std::fabs(candidateCenter[0] - targetCenter[0]) > centerTolerance ||
			std::fabs(candidateCenter[1] - targetCenter[1]) > centerTolerance)
		{
			continue;
		}

		if (outerCurve == NULL || candidateRadius > outerRadius)
		{
			outerCurve = curves[i];
			outerRadius = candidateRadius;
			outerCenter[0] = candidateCenter[0];
			outerCenter[1] = candidateCenter[1];
		}
	}

	return outerCurve != NULL;
}

bool CreateFlatPatternHoleAttributeDimension(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Drawings::DraftingCurve* circleCurve,
	const NXOpen::Point3d& drawingPoint,
	const std::string& text)
{
	if (baseView == NULL || circleCurve == NULL)
	{
		return false;
	}

	NXOpen::Annotations::Dimension* nullDimension(NULL);
	NXOpen::Annotations::RadialDimensionBuilder* builder =
		workPart->Dimensions()->CreateRadialDimensionBuilder(nullDimension);
	if (builder == NULL)
	{
		return false;
	}

	NXOpen::Point3d associativityPoint(
		circleCurve->Tag() != NULL_TAG ? drawingPoint.X : 0.0,
		circleCurve->Tag() != NULL_TAG ? drawingPoint.Y : 0.0,
		0.0);
	builder->FirstAssociativity()->SetValue(circleCurve, baseView, associativityPoint);
	builder->Origin()->Plane()->SetPlaneMethod(NXOpen::Annotations::PlaneBuilder::PlaneMethodTypeXyPlane);
	builder->Origin()->SetInferRelativeToGeometry(false);
	builder->Origin()->SetOriginPoint(drawingPoint);
	builder->Style()->LetteringStyle()->SetGeneralTextSize(3.0);
	builder->Style()->DimensionStyle()->SetDimensionReferenceIncludeType(
		NXOpen::Annotations::ReferenceIncludeTypeOnlyValue);
	builder->Style()->DimensionStyle()->SetDimensionValuePrecision(2);
	builder->Style()->UnitsStyle()->SetDisplayTrailingZeros(true);

	std::vector<NXOpen::NXString> emptyLines(0);
	if (!text.empty())
	{
		std::vector<NXOpen::NXString> lines(1);
		lines[0] = NXOpen::NXString(text.c_str(), NXOpen::NXString::UTF8);
		builder->AppendedText()->SetBefore(lines);
	}
	else
	{
		builder->AppendedText()->SetBefore(emptyLines);
	}
	builder->AppendedText()->SetAfter(emptyLines);
	builder->AppendedText()->SetAbove(emptyLines);
	builder->AppendedText()->SetBelow(emptyLines);

	NXOpen::NXObject* created = NULL;
	try
	{
		created = builder->Commit();
		if (created != NULL)
		{
			std::ostringstream log;
			log << "[HoleNote] committed hole dimension tag=" << created->Tag();
			HoleNoteDebugLog(log.str());
		}
	}
	catch (const NXOpen::NXException& ex)
	{
		LogPluginMessage(std::string("[HoleNote] create hole dimension failed: ") + ex.Message());
		HoleNoteDebugLog(std::string("[HoleNote] create hole dimension failed: ") + ex.Message());
	}
	builder->Destroy();
	return created != NULL;
}

bool CreateFlatPatternHoleMarkerNote(
	NXOpen::Drawings::BaseView* baseView,
	const NXOpen::Point3d& drawingPoint,
	const std::string& markerText)
{
	if (baseView == NULL || markerText.empty())
	{
		return false;
	}

	NXOpen::Drawings::DraftingView* draftingView =
		dynamic_cast<NXOpen::Drawings::DraftingView*>(NXOpen::NXObjectManager::Get(baseView->Tag()));
	if (draftingView == NULL)
	{
		return false;
	}

	NXOpen::Annotations::SimpleDraftingAid* nullAid(NULL);
	NXOpen::Annotations::DraftingNoteBuilder* builder =
		workPart->Annotations()->CreateDraftingNoteBuilder(nullAid);
	if (builder == NULL)
	{
		return false;
	}

	NXOpen::NXObject* created = NULL;
	try
	{
		NXOpen::Annotations::Annotation::AssociativeOriginData assocOrigin;
		assocOrigin.OriginType = NXOpen::Annotations::AssociativeOriginTypeRelativeToView;
		assocOrigin.View = draftingView;
		builder->Origin()->SetAssociativeOrigin(assocOrigin);
		builder->Origin()->SetInferRelativeToGeometry(true);
		builder->Style()->LetteringStyle()->SetGeneralTextSize(2.0);
		builder->Origin()->SetOriginPoint(drawingPoint);

		std::vector<NXOpen::NXString> text(1);
		text[0] = markerText.c_str();
		builder->Text()->TextBlock()->SetText(text);

		created = builder->Commit();
		if (created != NULL)
		{
			std::ostringstream log;
			log << "[HoleNote] committed marker note tag=" << created->Tag()
				<< " text='" << markerText << "' point=(" << drawingPoint.X << "," << drawingPoint.Y << ")";
			HoleNoteDebugLog(log.str());
		}
	}
	catch (const NXOpen::NXException& ex)
	{
		LogPluginMessage(std::string("[HoleNote] create marker note failed: ") + ex.Message());
		HoleNoteDebugLog(std::string("[HoleNote] create marker note failed: ") + ex.Message());
	}

	builder->Destroy();
	return created != NULL;
}

HoleCoordinateExtents CalculateHoleCoordinateExtents(
	NXOpen::Drawings::BaseView* baseView,
	const std::vector<NXOpen::Drawings::DraftingCurve*>& curves)
{
	HoleCoordinateExtents extents = { 0.0, 0.0, 0.0, 0.0, false };
	double minXValue = 0.0;
	double minYValue = 0.0;
	double maxXValue = 0.0;
	double maxYValue = 0.0;
	tag_t minXCurveTag = NULL_TAG;
	tag_t minYCurveTag = NULL_TAG;
	tag_t maxXCurveTag = NULL_TAG;
	tag_t maxYCurveTag = NULL_TAG;

	auto expandExtents = [&](
		double curveMinX,
		double curveMinY,
		double curveMaxX,
		double curveMaxY,
		tag_t curveTag) -> void
	{
		if (!extents.valid)
		{
			extents.minX = curveMinX;
			extents.minY = curveMinY;
			extents.maxX = curveMaxX;
			extents.maxY = curveMaxY;
			extents.valid = true;
			minXValue = curveMinX;
			minYValue = curveMinY;
			maxXValue = curveMaxX;
			maxYValue = curveMaxY;
			minXCurveTag = curveTag;
			minYCurveTag = curveTag;
			maxXCurveTag = curveTag;
			maxYCurveTag = curveTag;
			return;
		}
		if (curveMinX < extents.minX)
		{
			extents.minX = curveMinX;
			minXValue = curveMinX;
			minXCurveTag = curveTag;
		}
		if (curveMinY < extents.minY)
		{
			extents.minY = curveMinY;
			minYValue = curveMinY;
			minYCurveTag = curveTag;
		}
		if (curveMaxX > extents.maxX)
		{
			extents.maxX = curveMaxX;
			maxXValue = curveMaxX;
			maxXCurveTag = curveTag;
		}
		if (curveMaxY > extents.maxY)
		{
			extents.maxY = curveMaxY;
			maxYValue = curveMaxY;
			maxYCurveTag = curveTag;
		}
	};

	for (size_t i = 0; i < curves.size(); ++i)
	{
		double startPoint[2] = { 0.0, 0.0 };
		double endPoint[2] = { 0.0, 0.0 };
		NXOpen::Point3d modelStart(0.0, 0.0, 0.0);
		NXOpen::Point3d modelEnd(0.0, 0.0, 0.0);
		if (TryGetDraftingLineData(baseView, curves[i], startPoint, endPoint, modelStart, modelEnd))
		{
			const double curveMinX = std::min(startPoint[0], endPoint[0]);
			const double curveMinY = std::min(startPoint[1], endPoint[1]);
			const double curveMaxX = std::max(startPoint[0], endPoint[0]);
			const double curveMaxY = std::max(startPoint[1], endPoint[1]);
			expandExtents(curveMinX, curveMinY, curveMaxX, curveMaxY, curves[i]->Tag());
		}
	}

	if (!extents.valid)
	{
		for (size_t i = 0; i < curves.size(); ++i)
		{
			double center[2] = { 0.0, 0.0 };
			double radius = 0.0;
			if (TryGetDraftingArcData(baseView, curves[i], center, radius))
			{
				const double curveMinX = center[0] - radius;
				const double curveMinY = center[1] - radius;
				const double curveMaxX = center[0] + radius;
				const double curveMaxY = center[1] + radius;
				expandExtents(curveMinX, curveMinY, curveMaxX, curveMaxY, curves[i]->Tag());
			}
		}
	}
	if (extents.valid)
	{
		std::ostringstream sourceLog;
		sourceLog << "[HoleCoord] extentSources minX=" << minXValue << " tag=" << minXCurveTag
			<< " minY=" << minYValue << " tag=" << minYCurveTag
			<< " maxX=" << maxXValue << " tag=" << maxXCurveTag
			<< " maxY=" << maxYValue << " tag=" << maxYCurveTag;
		LogPluginMessage(sourceLog.str());
	}
	return extents;
}

bool FindFlatPatternBoundaryCurves(
	NXOpen::Drawings::BaseView* baseView,
	const std::vector<NXOpen::Drawings::DraftingCurve*>& curves,
	const HoleCoordinateExtents& extents,
	NXOpen::Drawings::DraftingCurve*& leftCurve,
	NXOpen::Drawings::DraftingCurve*& bottomCurve,
	NXOpen::Drawings::DraftingCurve*& rightCurve,
	NXOpen::Drawings::DraftingCurve*& topCurve)
{
	leftCurve = NULL;
	bottomCurve = NULL;
	rightCurve = NULL;
	topCurve = NULL;
	if (baseView == NULL || !extents.valid)
	{
		return false;
	}

	double bestLeft = 1.0e99;
	double bestBottom = 1.0e99;
	double bestRight = 1.0e99;
	double bestTop = 1.0e99;
	for (size_t i = 0; i < curves.size(); ++i)
	{
		double startPoint[2] = { 0.0, 0.0 };
		double endPoint[2] = { 0.0, 0.0 };
		NXOpen::Point3d modelStart(0.0, 0.0, 0.0);
		NXOpen::Point3d modelEnd(0.0, 0.0, 0.0);
		if (!TryGetDraftingLineData(baseView, curves[i], startPoint, endPoint, modelStart, modelEnd))
		{
			continue;
		}

		const double dx = std::fabs(startPoint[0] - endPoint[0]);
		const double dy = std::fabs(startPoint[1] - endPoint[1]);
		const double midX = (startPoint[0] + endPoint[0]) * 0.5;
		const double midY = (startPoint[1] + endPoint[1]) * 0.5;
		if (dy > dx * 2.0)
		{
			const double leftScore = std::fabs(midX - extents.minX) + std::fabs(std::min(startPoint[1], endPoint[1]) - extents.minY) * 0.1;
			if (leftScore < bestLeft)
			{
				bestLeft = leftScore;
				leftCurve = curves[i];
			}

			const double rightScore = std::fabs(midX - extents.maxX) + std::fabs(std::max(startPoint[1], endPoint[1]) - extents.maxY) * 0.1;
			if (rightScore < bestRight)
			{
				bestRight = rightScore;
				rightCurve = curves[i];
			}
		}
		if (dx > dy * 2.0)
		{
			const double bottomScore = std::fabs(midY - extents.minY) + std::fabs(std::min(startPoint[0], endPoint[0]) - extents.minX) * 0.1;
			if (bottomScore < bestBottom)
			{
				bestBottom = bottomScore;
				bottomCurve = curves[i];
			}

			const double topScore = std::fabs(midY - extents.maxY) + std::fabs(std::max(startPoint[0], endPoint[0]) - extents.maxX) * 0.1;
			if (topScore < bestTop)
			{
				bestTop = topScore;
				topCurve = curves[i];
			}
		}
	}

	return leftCurve != NULL && bottomCurve != NULL && rightCurve != NULL && topCurve != NULL;
}

std::vector<HoleCoordinateCandidate> CollectRoundHoleCandidates(
	NXOpen::Drawings::BaseView* baseView,
	const std::vector<NXOpen::Drawings::DraftingCurve*>& curves,
	const HoleCoordinateExtents& extents)
{
	std::vector<HoleCoordinateCandidate> holes;
	if (baseView == NULL || !extents.valid)
	{
		return holes;
	}

	const double width = extents.maxX - extents.minX;
	const double height = extents.maxY - extents.minY;
	const double maxRadius = std::min(width, height) * 0.18;
	for (size_t i = 0; i < curves.size(); ++i)
	{
		double center[2] = { 0.0, 0.0 };
		double radius = 0.0;
		if (!TryGetDraftingFullCircleData(baseView, curves[i], center, radius))
		{
			continue;
		}

		if (radius < 0.05 || radius > maxRadius)
		{
			continue;
		}

		const bool inside =
			center[0] > extents.minX + radius &&
			center[0] < extents.maxX - radius &&
			center[1] > extents.minY + radius &&
			center[1] < extents.maxY - radius;
		if (inside)
		{
			HoleCoordinateCandidate candidate;
			candidate.curve = curves[i];
			candidate.drawingCenter = NXOpen::Point3d(center[0], center[1], 0.0);
			candidate.drawingRadius = radius;
			holes.push_back(candidate);
		}
	}
	return holes;
}

NXOpen::Sketch* CreateDraftingSketch(NXOpen::Drawings::BaseView* baseView)
{
	if (baseView == NULL)
	{
		return NULL;
	}

	NXOpen::SketchInDraftingBuilder* builder = workPart->Sketches()->CreateSketchInDraftingBuilder();
	if (builder == NULL)
	{
		return NULL;
	}

	builder->View()->SetValue(baseView);
	NXOpen::NXObject* object = NULL;
	try
	{
		object = builder->Commit();
	}
	catch (const NXOpen::NXException& ex)
	{
		LogPluginMessage(std::string("[HoleCoord] create sketch failed: ") + ex.Message());
		builder->Destroy();
		return NULL;
	}
	builder->Destroy();

	NXOpen::Sketch* sketch = dynamic_cast<NXOpen::Sketch*>(object);
	if (sketch != NULL)
	{
		sketch->Activate(NXOpen::Sketch::ViewReorientFalse);
	}
	return sketch;
}

void IncludeCurvesIntoActiveSketch(
	NXOpen::Drawings::DraftingCurve* firstCurve,
	NXOpen::Drawings::DraftingCurve* secondCurve)
{
	if (::theSession == NULL || ::theSession->ActiveSketch() == NULL)
	{
		return;
	}

	NXOpen::SmartObject* nullObject(NULL);
	NXOpen::SketchIncludeGeometryBuilder* includeBuilder =
		workPart->Sketches()->CreateSketchIncludeGeometryBuilder(nullObject);
	if (includeBuilder == NULL)
	{
		return;
	}

	if (firstCurve != NULL)
	{
		includeBuilder->ObjectsToInclude()->Add(firstCurve);
	}
	if (secondCurve != NULL)
	{
		includeBuilder->ObjectsToInclude()->Add(secondCurve);
	}

	try
	{
		includeBuilder->Commit();
	}
	catch (const NXOpen::NXException&)
	{
	}
	includeBuilder->Destroy();
}

NXOpen::Point* CreateFixedDraftingPointInActiveSketch(
	const NXOpen::Point3d& point3d)
{
	if (::theSession == NULL || ::theSession->ActiveSketch() == NULL)
	{
		return NULL;
	}

	NXOpen::Point* point = NULL;
	try
	{
		point = workPart->Points()->CreatePoint(point3d);
		point->SetCoordinates(point3d);
		::theSession->ActiveSketch()->AddGeometry(
			point,
			NXOpen::Sketch::InferConstraintsOptionInferNoConstraints);
		::theSession->ActiveSketch()->Update();
		::theSession->ActiveSketch()->UpdateNavigator();
	}
	catch (const NXOpen::NXException& ex)
	{
		LogPluginMessage(std::string("[HoleCoord] create fixed sketch point failed: ") + ex.Message());
		return NULL;
	}
	return point;
}

NXOpen::Point* CreateDraftingCornerPoint(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Drawings::DraftingCurve* firstCurve,
	NXOpen::Drawings::DraftingCurve* secondCurve,
	const NXOpen::Point3d& seedPoint,
	const NXOpen::Point3d& finalPoint)
{
	if (baseView == NULL || ::theSession == NULL || ::theSession->ActiveSketch() == NULL)
	{
		return NULL;
	}

	if (firstCurve != NULL && secondCurve != NULL)
	{
		try
		{
			NXOpen::Point* apparentPoint = workPart->Points()->CreatePoint(
				firstCurve,
				secondCurve,
				seedPoint,
				baseView,
				NXOpen::SmartObject::UpdateOptionAfterModeling);
			if (apparentPoint != NULL)
			{
				apparentPoint->SetCoordinates(finalPoint);
				::theSession->ActiveSketch()->AddGeometry(
					apparentPoint,
					NXOpen::Sketch::InferConstraintsOptionInferNoConstraints);
				IncludeCurvesIntoActiveSketch(firstCurve, secondCurve);
				::theSession->ActiveSketch()->Update();
				::theSession->ActiveSketch()->UpdateNavigator();
				LogPluginMessage("[HoleCoord] corner point uses view intersection");
				return apparentPoint;
			}
		}
		catch (const NXOpen::NXException& ex)
		{
			LogPluginMessage(std::string("[HoleCoord] view intersection failed: ") + ex.Message());
		}

		NXOpen::Point* seed = workPart->Points()->CreatePoint(seedPoint);
		NXOpen::Point* nullPoint(NULL);
		try
		{
			workPart->Points()->CreatePoint(
				firstCurve,
				secondCurve,
				nullPoint,
				seed,
				NXOpen::SmartObject::UpdateOptionAfterModeling);
		}
		catch (const NXOpen::NXException&)
		{
		}

		try
		{
			NXOpen::Point* virtualPoint = workPart->Points()->CreateVirtualIntersectionPoint(
				firstCurve,
				secondCurve,
				nullPoint,
				seed,
				NXOpen::SmartObject::UpdateOptionAfterModeling);
			if (virtualPoint != NULL)
			{
				virtualPoint->SetCoordinates(finalPoint);
				virtualPoint->RemoveParameters();
				virtualPoint->SetCoordinates(finalPoint);
				::theSession->ActiveSketch()->AddGeometry(
					virtualPoint,
					NXOpen::Sketch::InferConstraintsOptionInferNoConstraints);
				IncludeCurvesIntoActiveSketch(firstCurve, secondCurve);
				::theSession->ActiveSketch()->Update();
				::theSession->ActiveSketch()->UpdateNavigator();
				LogPluginMessage("[HoleCoord] corner point uses virtual intersection");
				return virtualPoint;
			}
		}
		catch (const NXOpen::NXException& ex)
		{
			LogPluginMessage(std::string("[HoleCoord] create virtual intersection failed: ") + ex.Message());
		}
	}

	LogPluginMessage("[HoleCoord] corner point falls back to fixed sketch point");
	return CreateFixedDraftingPointInActiveSketch(finalPoint);
}

NXOpen::Annotations::OrdinateOriginDimension* FindCommittedOrdinateOrigin(
	const std::vector<NXOpen::NXObject*>& objects)
{
	for (size_t i = 0; i < objects.size(); ++i)
	{
		NXOpen::Annotations::OrdinateOriginDimension* origin =
			dynamic_cast<NXOpen::Annotations::OrdinateOriginDimension*>(objects[i]);
		if (origin != NULL)
		{
			return origin;
		}
	}
	return NULL;
}

NXOpen::Annotations::OrdinateOriginDimension* CreateHoleCoordinateOrdinateOrigin(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Point* originPoint)
{
	if (baseView == NULL || originPoint == NULL)
	{
		return NULL;
	}

	NXOpen::Annotations::DimensionData* dimensionData = workPart->Annotations()->NewDimensionData();
	if (dimensionData == NULL)
	{
		return NULL;
	}

	NXOpen::Annotations::Associativity* associativity = workPart->Annotations()->NewAssociativity();
	if (associativity == NULL)
	{
		delete dimensionData;
		return NULL;
	}

	const NXOpen::Point3d originPointCoords = originPoint->Coordinates();
	associativity->SetFirstObject(originPoint);
	associativity->SetSecondObject(NULL);
	associativity->SetObjectView(baseView);
	associativity->SetPointOption(NXOpen::Annotations::AssociativityPointOptionControl);
	associativity->SetLineOption(NXOpen::Annotations::AssociativityLineOptionBaseLine);
	associativity->SetFirstDefinitionPoint(NXOpen::Point3d(0.0, 0.0, 0.0));
	associativity->SetSecondDefinitionPoint(NXOpen::Point3d(0.0, 0.0, 0.0));
	associativity->SetAngle(0.0);
	associativity->SetPickPoint(originPointCoords);

	std::vector<NXOpen::Annotations::Associativity*> associativities(1, associativity);
	dimensionData->SetAssociativity(1, associativities);

	NXOpen::Annotations::OrdinateOriginDimension* ordinateOrigin = NULL;
	try
	{
		ordinateOrigin = workPart->Dimensions()->CreateOrdinateOriginDimension(dimensionData, originPointCoords);
	}
	catch (const NXOpen::NXException& ex)
	{
		LogPluginMessage(std::string("[HoleCoord] create ordinate origin failed: ") + ex.Message());
	}

	delete associativity;
	delete dimensionData;
	return ordinateOrigin;
}

NXOpen::Annotations::OrdinateMargin* CreateHoleCoordinateExplicitMargin(
	NXOpen::Annotations::OrdinateOriginDimension* ordinateOrigin,
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Point* anchorPoint,
	double offsetDistance,
	bool verticalDimension)
{
	if (ordinateOrigin == NULL || baseView == NULL || anchorPoint == NULL)
	{
		return NULL;
	}

	NXOpen::Annotations::Associativity* associativity = workPart->Annotations()->NewAssociativity();
	if (associativity == NULL)
	{
		return NULL;
	}

	const NXOpen::Point3d anchorPointCoords = anchorPoint->Coordinates();
	associativity->SetFirstObject(anchorPoint);
	associativity->SetSecondObject(NULL);
	associativity->SetObjectView(baseView);
	associativity->SetPointOption(NXOpen::Annotations::AssociativityPointOptionControl);
	associativity->SetLineOption(NXOpen::Annotations::AssociativityLineOptionBaseLine);
	associativity->SetFirstDefinitionPoint(NXOpen::Point3d(0.0, 0.0, 0.0));
	associativity->SetSecondDefinitionPoint(NXOpen::Point3d(0.0, 0.0, 0.0));
	associativity->SetAngle(0.0);
	associativity->SetPickPoint(anchorPointCoords);

	NXOpen::Annotations::OrdinateMargin* ordinateMargin = NULL;
	try
	{
		ordinateMargin = verticalDimension
			? static_cast<NXOpen::Annotations::OrdinateMargin*>(
				workPart->Annotations()->OrdinateMargins()->CreateVerticalMargin(
					ordinateOrigin,
					associativity,
					offsetDistance))
			: static_cast<NXOpen::Annotations::OrdinateMargin*>(
				workPart->Annotations()->OrdinateMargins()->CreateHorizontalMargin(
					ordinateOrigin,
					associativity,
					offsetDistance));
	}
	catch (const NXOpen::NXException& ex)
	{
		LogPluginMessage(std::string("[HoleCoord] create explicit margin failed: ") + ex.Message());
	}

	delete associativity;
	return ordinateMargin;
}

NXOpen::Annotations::OrdinateDimensionBuilder* CreateHoleCoordinateOrdinateBuilder()
{
	NXOpen::Annotations::OrdinateDimension* nullDimension(NULL);
	NXOpen::Annotations::OrdinateDimensionBuilder* builder =
		workPart->Dimensions()->CreateOrdinateDimensionBuilder(nullDimension);
	if (builder == NULL)
	{
		return NULL;
	}

	std::vector<NXOpen::NXString> emptyLines(0);
	builder->AppendedText()->SetBefore(emptyLines);
	builder->AppendedText()->SetAfter(emptyLines);
	builder->AppendedText()->SetAbove(emptyLines);
	builder->AppendedText()->SetBelow(emptyLines);
	builder->Baseline()->SetActivateBaseline(true);
	builder->Origin()->Plane()->SetPlaneMethod(NXOpen::Annotations::PlaneBuilder::PlaneMethodTypeXyPlane);
	builder->Origin()->SetInferRelativeToGeometry(false);
	builder->Origin()->SetAnchor(NXOpen::Annotations::OriginBuilder::AlignmentPositionMidCenter);
	builder->Style()->DimensionStyle()->SetDimensionReferenceIncludeType(NXOpen::Annotations::ReferenceIncludeTypeOnlyValue);
	builder->Style()->DimensionStyle()->SetNarrowDisplayType(NXOpen::Annotations::NarrowDisplayOptionNone);
	builder->Style()->OrdinateStyle()->SetMarginFirstOffset(5.0);
	builder->Style()->OrdinateStyle()->SetDisplayDimensionLine(NXOpen::Annotations::OrdinateLineArrowDisplayOptionNone);
	builder->Style()->DimensionStyle()->SetShowAsReferenceDimension(false);
	builder->Style()->DimensionStyle()->SetTextCentered(false);
	builder->Style()->LineArrowStyle()->SetLeaderOrientation(NXOpen::Annotations::LeaderSideLeft);
	return builder;
}

NXOpen::Annotations::Annotation::AssociativeOriginData CreateDragAssociativeOrigin()
{
	NXOpen::Annotations::Annotation::AssociativeOriginData assocOrigin;
	NXOpen::View* nullView(NULL);
	NXOpen::Point* nullPoint(NULL);
	NXOpen::Annotations::Annotation* nullAnnotation(NULL);
	assocOrigin.OriginType = NXOpen::Annotations::AssociativeOriginTypeDrag;
	assocOrigin.View = nullView;
	assocOrigin.ViewOfGeometry = nullView;
	assocOrigin.PointOnGeometry = nullPoint;
	assocOrigin.VertAnnotation = nullAnnotation;
	assocOrigin.VertAlignmentPosition = NXOpen::Annotations::AlignmentPositionTopLeft;
	assocOrigin.HorizAnnotation = nullAnnotation;
	assocOrigin.HorizAlignmentPosition = NXOpen::Annotations::AlignmentPositionTopLeft;
	assocOrigin.AlignedAnnotation = nullAnnotation;
	assocOrigin.DimensionLine = 0;
	assocOrigin.AssociatedView = nullView;
	assocOrigin.AssociatedPoint = nullPoint;
	assocOrigin.OffsetAnnotation = nullAnnotation;
	assocOrigin.OffsetAlignmentPosition = NXOpen::Annotations::AlignmentPositionTopLeft;
	assocOrigin.XOffsetFactor = 0.0;
	assocOrigin.YOffsetFactor = 0.0;
	assocOrigin.StackAlignmentPosition = NXOpen::Annotations::StackAlignmentPositionAbove;
	return assocOrigin;
}

bool CreateMultiHoleOrdinateDimension(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Point* originPoint,
	NXOpen::Point* boundaryPoint,
	const NXOpen::Point3d& originCurvePoint,
	const std::vector<HoleCoordinateCandidate>& holes,
	bool verticalDimension,
	const NXOpen::Point3d& marginLocation,
	NXOpen::Annotations::OrdinateMargin* activeMargin,
	NXOpen::Annotations::OrdinateOriginDimension*& ordinateOrigin)
{
	if (baseView == NULL || originPoint == NULL || boundaryPoint == NULL || holes.empty())
	{
		return false;
	}

	NXOpen::Annotations::OrdinateDimensionBuilder* builder = CreateHoleCoordinateOrdinateBuilder();
	if (builder == NULL)
	{
		return false;
	}

	builder->SetType(NXOpen::Annotations::BaseOrdinateDimensionBuilder::TypesMultipleDimension);
	builder->SetAllowDuplicates(false);

	NXOpen::View* nullView(NULL);
	NXOpen::Point3d zeroPoint(0.0, 0.0, 0.0);
	NXOpen::Point3d originPointCoords = originPoint->Coordinates();
	NXOpen::Point3d boundaryPointCoords = boundaryPoint->Coordinates();
	if (ordinateOrigin != NULL)
	{
		builder->OrdinateOrigin()->SetValue(ordinateOrigin);
	}
	else
	{
		builder->OrdinateOrigin()->SetValue(
			NXOpen::InferSnapType::SnapTypeExist,
			originPoint,
			baseView,
			originPointCoords,
			NULL,
			nullView,
			zeroPoint);
	}

	builder->SetActiveHorizontalMargin(NULL);
	builder->SetActiveVerticalMargin(NULL);
	if (verticalDimension)
	{
		builder->SetActiveVerticalMargin(activeMargin);
	}
	else
	{
		builder->SetActiveHorizontalMargin(activeMargin);
	}

	std::vector<NXOpen::DisplayableObject*> objects;
	std::vector<NXOpen::View*> views;
	objects.reserve(holes.size());
	views.reserve(holes.size());
	for (size_t i = 0; i < holes.size(); ++i)
	{
		objects.push_back(holes[i].curve);
		views.push_back(baseView);
	}
	const bool added = builder->AutoAssociativities()->AddWithViews(objects, views);
	builder->Origin()->SetAssociativeOrigin(CreateDragAssociativeOrigin());

	std::ostringstream layoutLog;
	layoutLog << "[HoleCoord] "
		<< (verticalDimension ? "vertical" : "horizontal")
		<< " originCurve=(" << originCurvePoint.X << "," << originCurvePoint.Y
		<< ") originPoint=(" << originPointCoords.X << "," << originPointCoords.Y
		<< ") targets=" << holes.size()
		<< " added=" << (added ? "true" : "false")
		<< " marginView=(" << marginLocation.X << "," << marginLocation.Y << ")";
	LogPluginMessage(layoutLog.str());

	builder->Origin()->Origin()->SetValue(boundaryPoint, baseView, marginLocation);
	if (verticalDimension)
	{
		builder->SetVerticalInferredMarginLocation(marginLocation);
	}
	else
	{
		builder->SetHorizontalInferredMarginLocation(marginLocation);
		builder->Baseline()->SetActivatePerpendicular(true);
		builder->Baseline()->SetActivateBaseline(false);
	}

	bool success = false;
	try
	{
		const bool valid = builder->Validate();
		LogPluginMessage(std::string("[HoleCoord] ")
			+ (verticalDimension ? "vertical" : "horizontal")
			+ " validate="
			+ (valid ? "true" : "false"));
		LogPluginMessage(std::string("[HoleCoord] ")
			+ (verticalDimension ? "vertical" : "horizontal")
			+ " boundary=("
			+ std::to_string(boundaryPointCoords.X) + ","
			+ std::to_string(boundaryPointCoords.Y) + ")");
		NXOpen::NXObject* object = builder->Commit();
		std::vector<NXOpen::NXObject*> committed = builder->GetCommittedObjects();
		if (ordinateOrigin == NULL)
		{
			ordinateOrigin = FindCommittedOrdinateOrigin(committed);
			if (ordinateOrigin == NULL)
			{
				NXOpen::Annotations::OrdinateDimension* ordinateDimension =
					dynamic_cast<NXOpen::Annotations::OrdinateDimension*>(object);
				if (ordinateDimension != NULL)
				{
					ordinateOrigin = ordinateDimension->GetOrdinateOrigin();
				}
			}
		}
		success = (object != NULL);
		LogPluginMessage(std::string("[HoleCoord] ")
			+ (verticalDimension ? "vertical" : "horizontal")
			+ " commit="
			+ (success ? "ok" : "null")
			+ " origin="
			+ (ordinateOrigin != NULL ? "ok" : "null"));
	}
	catch (const NXOpen::NXException& ex)
	{
		LogPluginMessage(std::string("[HoleCoord] ")
			+ (verticalDimension ? "vertical" : "horizontal")
			+ " commit failed: " + ex.Message());
	}
	builder->Destroy();
	return success;
}

void BendNoteDebugLog(const std::string& message)
{
	try
	{
		std::ofstream out("D:\\UG智辉钣金插件\\logs\\bend_note_debug.log", std::ios::out | std::ios::app);
		if (out.is_open())
		{
			out << message << std::endl;
		}
	}
	catch (...)
	{
	}
}

bool DraftingCurveReferencesObject(
	NXOpen::Drawings::DraftingCurve* curve,
	tag_t objectTag)
{
	if (curve == NULL || objectTag == NULL_TAG)
	{
		return false;
	}

	try
	{
		const std::vector<NXOpen::NXObject*> parents = curve->GetDraftingCurveInfo()->GetParents();
		for (size_t i = 0; i < parents.size(); ++i)
		{
			NXOpen::NXObject* parent = parents[i];
			if (parent == NULL)
			{
				continue;
			}
			if (parent->Tag() == objectTag)
			{
				return true;
			}
			try
			{
				NXOpen::NXObject* prototype = dynamic_cast<NXOpen::NXObject*>(parent->Prototype());
				if (prototype != NULL && prototype->Tag() == objectTag)
				{
					return true;
				}
			}
			catch (...)
			{
			}
		}
	}
	catch (...)
	{
	}

	int parentsCount = 0;
	tag_t* parentTags = NULL;
	if (UF_DRAW_ask_drafting_curve_parents(curve->Tag(), &parentsCount, &parentTags) != 0 || parentTags == NULL)
	{
		return false;
	}

	bool matched = false;
	for (int i = 0; i < parentsCount; ++i)
	{
		if (parentTags[i] == objectTag)
		{
			matched = true;
			break;
		}
	}
	UF_free(parentTags);
	return matched;
}

NXOpen::Drawings::DraftingCurve* FindDraftingCurveForFlatPatternCurve(
	const std::vector<NXOpen::Drawings::DraftingCurve*>& curves,
	NXOpen::Curve* flatPatternCurve)
{
	if (flatPatternCurve == NULL)
	{
		return NULL;
	}

	const tag_t flatPatternCurveTag = flatPatternCurve->Tag();
	for (size_t i = 0; i < curves.size(); ++i)
	{
		if (DraftingCurveReferencesObject(curves[i], flatPatternCurveTag))
		{
			return curves[i];
		}
	}
	return NULL;
}

bool TryMeasureArcSpanDegrees(tag_t objectTag, double& degrees)
{
	degrees = 0.0;
	if (objectTag == NULL_TAG)
	{
		return false;
	}

	UF_EVAL_p_t evaluator = NULL;
	if (UF_EVAL_initialize(objectTag, &evaluator) != 0 || evaluator == NULL)
	{
		return false;
	}

	logical isArc = false;
	if (UF_EVAL_is_arc(evaluator, &isArc) != 0 || !isArc)
	{
		UF_EVAL_free(evaluator);
		return false;
	}

	double limits[2] = { 0.0, 0.0 };
	if (UF_EVAL_ask_limits(evaluator, limits) != 0)
	{
		UF_EVAL_free(evaluator);
		return false;
	}
	UF_EVAL_free(evaluator);

	double span = std::fabs(limits[1] - limits[0]);
	while (span > (2.0 * PI))
	{
		span -= 2.0 * PI;
	}
	degrees = span * 180.0 / PI;
	return degrees > 1.0e-3 && degrees <= 360.0;
}

bool TryMeasureBendAngleDegrees(NXOpen::Face* bendFace, double& degrees)
{
	degrees = 0.0;
	if (bendFace == NULL)
	{
		return false;
	}

	int faceType = 0;
	double facePoint[3] = { 0.0, 0.0, 0.0 };
	double faceDirection[3] = { 0.0, 0.0, 0.0 };
	double faceBox[6] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
	double faceRadius = 0.0;
	double faceRadData = 0.0;
	int faceNormDir = 0;
	double uvMinMax[4] = { 0.0, 0.0, 0.0, 0.0 };
	if (UF_MODL_ask_face_data(
		bendFace->Tag(),
		&faceType,
		facePoint,
		faceDirection,
		faceBox,
		&faceRadius,
		&faceRadData,
		&faceNormDir) == 0 &&
		faceType == UF_MODL_CYLINDRICAL_FACE &&
		UF_MODL_ask_face_uv_minmax(bendFace->Tag(), uvMinMax) == 0)
	{
		double span = std::fabs(uvMinMax[1] - uvMinMax[0]);
		while (span > (2.0 * PI))
		{
			span -= 2.0 * PI;
		}
		const double faceDegrees = span * 180.0 / PI;
		if (faceDegrees > 1.0e-3 && faceDegrees < 359.0)
		{
			degrees = faceDegrees;
			std::ostringstream log;
			log << "[BendNote] angleSource=face"
				<< " faceTag=" << bendFace->Tag()
				<< " span=" << degrees
				<< " radius=" << faceRadius
				<< " u=(" << uvMinMax[0] << "," << uvMinMax[1] << ")"
				<< " v=(" << uvMinMax[2] << "," << uvMinMax[3] << ")";
			BendNoteDebugLog(log.str());
			return true;
		}
	}

	std::ostringstream log;
	log << "[BendNote] angleSource=none"
		<< " faceTag=" << bendFace->Tag();
	BendNoteDebugLog(log.str());
	return false;
}

std::string FormatBendNoteText(const char* directionUtf8, double angleDegrees)
{
	std::ostringstream text;
	text << directionUtf8;
	const double rounded = std::round(angleDegrees * 10.0) / 10.0;
	if (std::fabs(rounded - std::round(rounded)) < 1.0e-6)
	{
		text << static_cast<int>(std::round(rounded));
	}
	else
	{
		text << std::fixed << std::setprecision(1) << rounded;
	}
	text << "\xC2\xB0";
	return text.str();
}

bool CreateFlatPatternBendNote(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Drawings::DraftingCurve* curve,
	const NXOpen::Point3d& drawingPoint,
	const std::string& noteText,
	double textHeight)
{
	if (baseView == NULL || curve == NULL || noteText.empty())
	{
		return false;
	}

	NXOpen::Drawings::DraftingView* draftingView =
		dynamic_cast<NXOpen::Drawings::DraftingView*>(NXOpen::NXObjectManager::Get(baseView->Tag()));
	if (draftingView == NULL)
	{
		return false;
	}

	NXOpen::Annotations::SimpleDraftingAid* nullAid(NULL);
	NXOpen::Annotations::DraftingNoteBuilder* builder =
		workPart->Annotations()->CreateDraftingNoteBuilder(nullAid);
	if (builder == NULL)
	{
		return false;
	}

	NXOpen::NXObject* created = NULL;
	try
	{
		NXOpen::Annotations::Annotation::AssociativeOriginData assocOrigin;
		NXOpen::Point* nullPoint(NULL);
		NXOpen::Annotations::Annotation* nullAnnotation(NULL);
		assocOrigin.OriginType = NXOpen::Annotations::AssociativeOriginTypeRelativeToView;
		assocOrigin.View = draftingView;
		assocOrigin.ViewOfGeometry = draftingView;
		assocOrigin.PointOnGeometry = nullPoint;
		assocOrigin.VertAnnotation = nullAnnotation;
		assocOrigin.VertAlignmentPosition = NXOpen::Annotations::AlignmentPositionTopLeft;
		assocOrigin.HorizAnnotation = nullAnnotation;
		assocOrigin.HorizAlignmentPosition = NXOpen::Annotations::AlignmentPositionTopLeft;
		assocOrigin.AlignedAnnotation = nullAnnotation;
		assocOrigin.DimensionLine = 0;
		assocOrigin.AssociatedView = draftingView;
		assocOrigin.AssociatedPoint = nullPoint;
		assocOrigin.OffsetAnnotation = nullAnnotation;
		assocOrigin.OffsetAlignmentPosition = NXOpen::Annotations::AlignmentPositionTopLeft;
		assocOrigin.XOffsetFactor = 0.0;
		assocOrigin.YOffsetFactor = 0.0;
		assocOrigin.StackAlignmentPosition = NXOpen::Annotations::StackAlignmentPositionAbove;
		builder->Origin()->SetAssociativeOrigin(assocOrigin);
		builder->Origin()->SetInferRelativeToGeometry(false);
		builder->Origin()->SetAnchor(NXOpen::Annotations::OriginBuilder::AlignmentPositionMidCenter);
		builder->Style()->LetteringStyle()->SetGeneralTextSize(std::max(0.1, textHeight));
		builder->Origin()->SetOriginPoint(drawingPoint);

		std::vector<NXOpen::NXString> lines(1);
		lines[0] = NXOpen::NXString(noteText.c_str(), NXOpen::NXString::UTF8);
		builder->Text()->TextBlock()->SetText(lines);

		created = builder->Commit();
		std::ostringstream log;
		log << "[BendNote] commit=" << (created != NULL ? "ok" : "null")
			<< " curveTag=" << curve->Tag()
			<< " text='" << noteText << "'"
			<< " point=(" << drawingPoint.X << "," << drawingPoint.Y << ")";
		BendNoteDebugLog(log.str());
	}
	catch (const NXOpen::NXException& ex)
	{
		BendNoteDebugLog(std::string("[BendNote] create failed: ") + ex.Message());
	}

	builder->Destroy();
	return created != NULL;
}

bool CreateFlatPatternBendNotesForObjects(
	NXOpen::Drawings::BaseView* baseView,
	const std::vector<NXOpen::Drawings::DraftingCurve*>& curves,
	const std::vector<NXOpen::Features::FlatPattern::ObjectDataFace>& objects,
	const char* directionUtf8,
	double textHeight,
	std::unordered_set<tag_t>& usedFlatPatternCurveTags)
{
	bool createdAny = false;
	for (size_t i = 0; i < objects.size(); ++i)
	{
		NXOpen::Curve* flatPatternCurve = objects[i].FlatPatternObject;
		if (flatPatternCurve == NULL || flatPatternCurve->Tag() == NULL_TAG)
		{
			continue;
		}
		if (!usedFlatPatternCurveTags.insert(flatPatternCurve->Tag()).second)
		{
			continue;
		}

		NXOpen::Drawings::DraftingCurve* draftingCurve =
			FindDraftingCurveForFlatPatternCurve(curves, flatPatternCurve);
		if (draftingCurve == NULL)
		{
			std::ostringstream log;
			log << "[BendNote] no drafting curve for flatCurveTag=" << flatPatternCurve->Tag();
			BendNoteDebugLog(log.str());
			continue;
		}

		double startPoint[2] = { 0.0, 0.0 };
		double endPoint[2] = { 0.0, 0.0 };
		UF_CURVE_line_t lineData;
		if (!TryGetLineCurveData(baseView, draftingCurve, lineData, startPoint, endPoint))
		{
			std::ostringstream log;
			log << "[BendNote] cannot map line to drawing tag=" << draftingCurve->Tag();
			BendNoteDebugLog(log.str());
			continue;
		}

		double angleDegrees = 0.0;
		bool angleMeasured = TryMeasureBendAngleDegrees(objects[i].FormedBodyObject, angleDegrees);
		if (!angleMeasured)
		{
			std::ostringstream log;
			log << "[BendNote] skip no cylindrical angle"
				<< " flatCurveTag=" << flatPatternCurve->Tag();
			BendNoteDebugLog(log.str());
			continue;
		}
		angleDegrees = 180.0 - angleDegrees;
		if (angleDegrees < 0.0)
		{
			angleDegrees = -angleDegrees;
		}

		const NXOpen::Point3d drawingPoint(
			(startPoint[0] + endPoint[0]) * 0.5,
			(startPoint[1] + endPoint[1]) * 0.5,
			0.0);
		{
			std::ostringstream log;
			log << "[BendNote] mappedLine curveTag=" << draftingCurve->Tag()
				<< " start=(" << startPoint[0] << "," << startPoint[1] << ")"
				<< " end=(" << endPoint[0] << "," << endPoint[1] << ")"
				<< " mid=(" << drawingPoint.X << "," << drawingPoint.Y << ")";
			BendNoteDebugLog(log.str());
		}
		const std::string noteText = FormatBendNoteText(directionUtf8, angleDegrees);
		createdAny = CreateFlatPatternBendNote(baseView, draftingCurve, drawingPoint, noteText, textHeight) || createdAny;
	}
	return createdAny;
}

bool BreakFlatPatternBendLinesForObjects(
	NXOpen::Drawings::BaseView* baseView,
	const std::vector<NXOpen::Drawings::DraftingCurve*>& curves,
	const std::vector<NXOpen::Features::FlatPattern::ObjectDataFace>& objects,
	double edgeDistance,
	double keepLength,
	const char* directionName,
	std::unordered_set<tag_t>& usedFlatPatternCurveTags)
{
	if (baseView == NULL || keepLength <= 0.0 || edgeDistance < 0.0)
	{
		return false;
	}

	bool editedAny = false;
	for (size_t i = 0; i < objects.size(); ++i)
	{
		NXOpen::Curve* flatPatternCurve = objects[i].FlatPatternObject;
		if (flatPatternCurve == NULL || flatPatternCurve->Tag() == NULL_TAG)
		{
			continue;
		}
		if (!usedFlatPatternCurveTags.insert(flatPatternCurve->Tag()).second)
		{
			continue;
		}

		NXOpen::Drawings::DraftingCurve* draftingCurve =
			FindDraftingCurveForFlatPatternCurve(curves, flatPatternCurve);
		if (draftingCurve == NULL)
		{
			std::ostringstream log;
			log << "[BendLineBreak] no drafting curve direction=" << directionName
				<< " flatCurveTag=" << flatPatternCurve->Tag();
			BendNoteDebugLog(log.str());
			continue;
		}

		double curveLength = 0.0;
		try
		{
			curveLength = draftingCurve->GetLength();
		}
		catch (const NXOpen::NXException& ex)
		{
			BendNoteDebugLog(std::string("[BendLineBreak] get length failed: ") + ex.Message());
			continue;
		}

		const double visibleStartLength = edgeDistance + keepLength;
		const double visibleEndStartLength = curveLength - edgeDistance - keepLength;
		if (curveLength <= (edgeDistance + keepLength) * 2.0 + 1.0e-6)
		{
			std::ostringstream log;
			log << "[BendLineBreak] skip short curve direction=" << directionName
				<< " curveTag=" << draftingCurve->Tag()
				<< " length=" << curveLength
				<< " edgeDistance=" << edgeDistance
				<< " keep=" << keepLength;
			BendNoteDebugLog(log.str());
			continue;
		}

		std::vector<double> segmentStart;
		std::vector<double> segmentEnd;
		segmentStart.reserve(3);
		segmentEnd.reserve(3);
		if (edgeDistance > 1.0e-6)
		{
			segmentStart.push_back(0.0);
			segmentEnd.push_back(edgeDistance / curveLength);
		}
		segmentStart.push_back(visibleStartLength / curveLength);
		segmentEnd.push_back(visibleEndStartLength / curveLength);
		if (edgeDistance > 1.0e-6)
		{
			segmentStart.push_back((curveLength - edgeDistance) / curveLength);
			segmentEnd.push_back(1.0);
		}
		try
		{
			baseView->DependentDisplay()->ApplySegmentEdit(
				draftingCurve,
				NXOpen::ViewDependentDisplayManager::FontInvisible,
				NXOpen::ViewDependentDisplayManager::WidthObject,
				segmentStart,
				segmentEnd);
			editedAny = true;

			std::ostringstream log;
			log << "[BendLineBreak] edited direction=" << directionName
				<< " curveTag=" << draftingCurve->Tag()
				<< " length=" << curveLength
				<< " edgeDistance=" << edgeDistance
				<< " keep=" << keepLength
				<< " hiddenSegments=" << segmentStart.size();
			for (size_t segmentIndex = 0; segmentIndex < segmentStart.size(); ++segmentIndex)
			{
				log << " hide" << segmentIndex << "=(" << segmentStart[segmentIndex] << "," << segmentEnd[segmentIndex] << ")";
			}
			BendNoteDebugLog(log.str());
		}
		catch (const NXOpen::NXException& ex)
		{
			BendNoteDebugLog(std::string("[BendLineBreak] segment edit failed: ") + ex.Message());
		}
	}
	return editedAny;
}

bool TrimLargeArcMarkerCurve(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Drawings::DraftingCurve* curve,
	double edgeDistance,
	double keepLength)
{
	if (baseView == NULL || curve == NULL)
	{
		return false;
	}
	double curveLength = 0.0;
	try
	{
		curveLength = curve->GetLength();
	}
	catch (const NXOpen::NXException& ex)
	{
		BendNoteDebugLog(std::string("[LargeArcMarker] get curve length failed: ") + ex.Message());
		return false;
	}
	if (curveLength <= (edgeDistance + keepLength) * 2.0 + 1.0e-6)
	{
		std::ostringstream log;
		log << "[LargeArcMarker] keep full short curve tag=" << curve->Tag()
			<< " length=" << curveLength;
		BendNoteDebugLog(log.str());
		return true;
	}

	std::vector<double> hiddenStart;
	std::vector<double> hiddenEnd;
	if (edgeDistance > 1.0e-6)
	{
		hiddenStart.push_back(0.0);
		hiddenEnd.push_back(edgeDistance / curveLength);
	}
	hiddenStart.push_back((edgeDistance + keepLength) / curveLength);
	hiddenEnd.push_back((curveLength - edgeDistance - keepLength) / curveLength);
	if (edgeDistance > 1.0e-6)
	{
		hiddenStart.push_back((curveLength - edgeDistance) / curveLength);
		hiddenEnd.push_back(1.0);
	}
	try
	{
		baseView->DependentDisplay()->ApplySegmentEdit(
			curve,
			NXOpen::ViewDependentDisplayManager::FontInvisible,
			NXOpen::ViewDependentDisplayManager::WidthObject,
			hiddenStart,
			hiddenEnd);
		return true;
	}
	catch (const NXOpen::NXException& ex)
	{
		BendNoteDebugLog(std::string("[LargeArcMarker] segment edit failed: ") + ex.Message());
	}
	return false;
}

struct BendLineNotchEndpoint
{
	double x;
	double y;
	double z;
};

struct BendLineNotchGroup
{
	double dirX;
	double dirY;
	double normalOffset;
	double minProjection;
	double maxProjection;
	BendLineNotchEndpoint minPoint;
	BendLineNotchEndpoint maxPoint;
	std::vector<tag_t> curveTags;
};

double Projection2d(double x, double y, double dirX, double dirY)
{
	return x * dirX + y * dirY;
}

double NormalOffset2d(double x, double y, double dirX, double dirY)
{
	return -dirY * x + dirX * y;
}

double PointToLineDistanceByNormal(double x, double y, double dirX, double dirY, double normalOffset)
{
	return std::fabs(NormalOffset2d(x, y, dirX, dirY) - normalOffset);
}

void GetVisibleArcAnglesForBendLineEnd(
	double bendDirX,
	double bendDirY,
	bool isMinEndpoint,
	double& startAngle,
	double& endAngle)
{
	const double bendAngle = std::atan2(bendDirY, bendDirX);
	if (isMinEndpoint)
	{
		startAngle = bendAngle - 0.5 * PI;
		endAngle = bendAngle + 0.5 * PI;
	}
	else
	{
		startAngle = bendAngle + 0.5 * PI;
		endAngle = bendAngle + 1.5 * PI;
	}
}

bool HideBoundarySegmentForNotch(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Drawings::DraftingCurve* boundaryCurve,
	double x,
	double y,
	double radius)
{
	if (baseView == NULL || boundaryCurve == NULL || radius <= 0.0)
	{
		return false;
	}

	UF_CURVE_line_t lineData;
	if (UF_CURVE_ask_line_data(boundaryCurve->Tag(), &lineData) != 0)
	{
		return false;
	}

	const double dx = lineData.end_point[0] - lineData.start_point[0];
	const double dy = lineData.end_point[1] - lineData.start_point[1];
	const double length = std::sqrt(dx * dx + dy * dy);
	if (length <= 1.0e-9)
	{
		return false;
	}
	const double projection = Clamp01(((x - lineData.start_point[0]) * dx + (y - lineData.start_point[1]) * dy) / (length * length));
	const double halfSegment = radius / length;
	std::vector<double> segmentStart(1);
	std::vector<double> segmentEnd(1);
	segmentStart[0] = Clamp01(projection - halfSegment);
	segmentEnd[0] = Clamp01(projection + halfSegment);
	if (segmentEnd[0] - segmentStart[0] <= 1.0e-6)
	{
		return false;
	}

	try
	{
		baseView->DependentDisplay()->ApplySegmentEdit(
			boundaryCurve,
			NXOpen::ViewDependentDisplayManager::FontInvisible,
			NXOpen::ViewDependentDisplayManager::WidthObject,
			segmentStart,
			segmentEnd);
		std::ostringstream log;
		log << "[BendLineNotch] boundary break curveTag=" << boundaryCurve->Tag()
			<< " point=(" << x << "," << y << ")"
			<< " radius=" << radius
			<< " segment=(" << segmentStart[0] << "," << segmentEnd[0] << ")";
		BendNoteDebugLog(log.str());
		return true;
	}
	catch (const NXOpen::NXException& ex)
	{
		BendNoteDebugLog(std::string("[BendLineNotch] boundary segment edit failed: ") + ex.Message());
	}
	return false;
}

bool HideBoundarySegmentBetweenNotchIntersections(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Drawings::DraftingCurve* boundaryCurve,
	double firstX,
	double firstY,
	double secondX,
	double secondY)
{
	if (baseView == NULL || boundaryCurve == NULL)
	{
		return false;
	}

	UF_CURVE_line_t lineData;
	if (UF_CURVE_ask_line_data(boundaryCurve->Tag(), &lineData) != 0)
	{
		return false;
	}

	const double dx = lineData.end_point[0] - lineData.start_point[0];
	const double dy = lineData.end_point[1] - lineData.start_point[1];
	const double lengthSquared = dx * dx + dy * dy;
	if (lengthSquared <= 1.0e-12)
	{
		return false;
	}

	const double firstProjection =
		((firstX - lineData.start_point[0]) * dx + (firstY - lineData.start_point[1]) * dy) / lengthSquared;
	const double secondProjection =
		((secondX - lineData.start_point[0]) * dx + (secondY - lineData.start_point[1]) * dy) / lengthSquared;
	std::vector<double> segmentStart(1);
	std::vector<double> segmentEnd(1);
	segmentStart[0] = Clamp01(std::min(firstProjection, secondProjection));
	segmentEnd[0] = Clamp01(std::max(firstProjection, secondProjection));
	if (segmentEnd[0] - segmentStart[0] <= 1.0e-6)
	{
		return false;
	}

	try
	{
		baseView->DependentDisplay()->ApplySegmentEdit(
			boundaryCurve,
			NXOpen::ViewDependentDisplayManager::FontInvisible,
			NXOpen::ViewDependentDisplayManager::WidthObject,
			segmentStart,
			segmentEnd);
		std::ostringstream log;
		log << "[BendLineNotch] boundary break by arc intersection curveTag=" << boundaryCurve->Tag()
			<< " first=(" << firstX << "," << firstY << ")"
			<< " second=(" << secondX << "," << secondY << ")"
			<< " segment=(" << segmentStart[0] << "," << segmentEnd[0] << ")";
		BendNoteDebugLog(log.str());
		return true;
	}
	catch (const NXOpen::NXException& ex)
	{
		BendNoteDebugLog(std::string("[BendLineNotch] boundary intersection segment edit failed: ") + ex.Message());
	}
	return false;
}

double DistancePointToRawLineSegment(
	double pointX,
	double pointY,
	const UF_CURVE_line_t& lineData,
	double& projection)
{
	const double dx = lineData.end_point[0] - lineData.start_point[0];
	const double dy = lineData.end_point[1] - lineData.start_point[1];
	const double lengthSquared = dx * dx + dy * dy;
	if (lengthSquared <= 1.0e-12)
	{
		projection = 0.0;
		const double ex = pointX - lineData.start_point[0];
		const double ey = pointY - lineData.start_point[1];
		return std::sqrt(ex * ex + ey * ey);
	}
	projection =
		((pointX - lineData.start_point[0]) * dx + (pointY - lineData.start_point[1]) * dy) / lengthSquared;
	const double clampedProjection = Clamp01(projection);
	const double closestX = lineData.start_point[0] + dx * clampedProjection;
	const double closestY = lineData.start_point[1] + dy * clampedProjection;
	const double ex = pointX - closestX;
	const double ey = pointY - closestY;
	return std::sqrt(ex * ex + ey * ey);
}

NXOpen::Drawings::DraftingCurve* FindBoundaryCurveIntersectingNotchArc(
	const std::vector<NXOpen::Drawings::DraftingCurve*>& curves,
	const std::vector<tag_t>& excludedCurveTags,
	double firstX,
	double firstY,
	double secondX,
	double secondY)
{
	NXOpen::Drawings::DraftingCurve* bestCurve = NULL;
	double bestScore = 1.0e99;
	const double intersectionTolerance = 0.08;
	for (size_t i = 0; i < curves.size(); ++i)
	{
		if (curves[i] == NULL)
		{
			continue;
		}
		if (std::find(excludedCurveTags.begin(), excludedCurveTags.end(), curves[i]->Tag()) != excludedCurveTags.end())
		{
			continue;
		}

		UF_CURVE_line_t lineData;
		if (UF_CURVE_ask_line_data(curves[i]->Tag(), &lineData) != 0)
		{
			continue;
		}

		double firstProjection = 0.0;
		double secondProjection = 0.0;
		const double firstDistance = DistancePointToRawLineSegment(firstX, firstY, lineData, firstProjection);
		const double secondDistance = DistancePointToRawLineSegment(secondX, secondY, lineData, secondProjection);
		if (firstDistance > intersectionTolerance || secondDistance > intersectionTolerance)
		{
			continue;
		}
		if (firstProjection < -0.02 || firstProjection > 1.02 || secondProjection < -0.02 || secondProjection > 1.02)
		{
			continue;
		}

		const double score = firstDistance + secondDistance;
		if (score < bestScore)
		{
			bestScore = score;
			bestCurve = curves[i];
		}
	}
	return bestCurve;
}

struct BendLineNotchBoundaryCircleEdit
{
	NXOpen::Drawings::DraftingCurve* boundaryCurve;
	std::vector<double> segmentStart;
	std::vector<double> segmentEnd;
	double centerX;
	double centerY;
	double radius;
};

struct BendLineNotchCircleEdit
{
	NXOpen::Arc* circle;
	double centerX;
	double centerY;
	double radius;
	double visibleAngle;
	std::vector<double> intersectionAngles;
};

double NormalizeAngleDelta(double angle)
{
	while (angle <= -PI)
	{
		angle += 2.0 * PI;
	}
	while (angle > PI)
	{
		angle -= 2.0 * PI;
	}
	return angle;
}

double NormalizeAnglePositive(double angle)
{
	while (angle < 0.0)
	{
		angle += 2.0 * PI;
	}
	while (angle >= 2.0 * PI)
	{
		angle -= 2.0 * PI;
	}
	return angle;
}

void AddUniqueSortedParameter(std::vector<double>& values, double value)
{
	if (value < -1.0e-7 || value > 1.0 + 1.0e-7)
	{
		return;
	}
	value = Clamp01(value);
	for (size_t i = 0; i < values.size(); ++i)
	{
		if (std::fabs(values[i] - value) <= 1.0e-6)
		{
			return;
		}
	}
	values.push_back(value);
	std::sort(values.begin(), values.end());
}

bool CollectBoundaryCircleEdit(
	NXOpen::Drawings::DraftingCurve* curve,
	double centerX,
	double centerY,
	double radius,
	BendLineNotchBoundaryCircleEdit& edit,
	std::vector<double>& intersectionAngles)
{
	if (curve == NULL || radius <= 0.0)
	{
		return false;
	}

	UF_CURVE_line_t lineData;
	if (UF_CURVE_ask_line_data(curve->Tag(), &lineData) != 0)
	{
		return false;
	}

	const double dx = lineData.end_point[0] - lineData.start_point[0];
	const double dy = lineData.end_point[1] - lineData.start_point[1];
	const double lengthSquared = dx * dx + dy * dy;
	if (lengthSquared <= 1.0e-12)
	{
		return false;
	}

	const double fx = lineData.start_point[0] - centerX;
	const double fy = lineData.start_point[1] - centerY;
	const double a = lengthSquared;
	const double b = 2.0 * (fx * dx + fy * dy);
	const double c = fx * fx + fy * fy - radius * radius;
	const double discriminant = b * b - 4.0 * a * c;

	std::vector<double> splitParameters;
	splitParameters.push_back(0.0);
	splitParameters.push_back(1.0);

	if (discriminant >= -1.0e-7)
	{
		const double root = std::sqrt(std::max(0.0, discriminant));
		const double t1 = (-b - root) / (2.0 * a);
		const double t2 = (-b + root) / (2.0 * a);
		const double roots[2] = { t1, t2 };
		for (int rootIndex = 0; rootIndex < 2; ++rootIndex)
		{
			const double t = roots[rootIndex];
			if (t < -1.0e-7 || t > 1.0 + 1.0e-7)
			{
				continue;
			}
			AddUniqueSortedParameter(splitParameters, t);
			const double ix = lineData.start_point[0] + dx * Clamp01(t);
			const double iy = lineData.start_point[1] + dy * Clamp01(t);
			intersectionAngles.push_back(std::atan2(iy - centerY, ix - centerX));
		}
	}

	edit.boundaryCurve = curve;
	edit.centerX = centerX;
	edit.centerY = centerY;
	edit.radius = radius;
	for (size_t i = 1; i < splitParameters.size(); ++i)
	{
		const double start = splitParameters[i - 1];
		const double end = splitParameters[i];
		if (end - start <= 1.0e-6)
		{
			continue;
		}
		const double mid = (start + end) * 0.5;
		const double mx = lineData.start_point[0] + dx * mid - centerX;
		const double my = lineData.start_point[1] + dy * mid - centerY;
		if (mx * mx + my * my <= radius * radius + 1.0e-6)
		{
			edit.segmentStart.push_back(start);
			edit.segmentEnd.push_back(end);
		}
	}

	return !edit.segmentStart.empty();
}

bool HideBoundarySegmentsInsideNotchCircle(
	NXOpen::Drawings::BaseView* baseView,
	const BendLineNotchBoundaryCircleEdit& edit)
{
	if (baseView == NULL || edit.boundaryCurve == NULL || edit.segmentStart.empty())
	{
		return false;
	}

	try
	{
		baseView->DependentDisplay()->ApplySegmentEdit(
			edit.boundaryCurve,
			NXOpen::ViewDependentDisplayManager::FontInvisible,
			NXOpen::ViewDependentDisplayManager::WidthObject,
			edit.segmentStart,
			edit.segmentEnd);
		std::ostringstream log;
		log << "[BendLineNotch] boundary break inside circle curveTag=" << edit.boundaryCurve->Tag()
			<< " center=(" << edit.centerX << "," << edit.centerY << ")"
			<< " radius=" << edit.radius
			<< " hiddenSegments=" << edit.segmentStart.size();
		for (size_t i = 0; i < edit.segmentStart.size(); ++i)
		{
			log << " hide" << i << "=(" << edit.segmentStart[i] << "," << edit.segmentEnd[i] << ")";
		}
		BendNoteDebugLog(log.str());
		return true;
	}
	catch (const NXOpen::NXException& ex)
	{
		BendNoteDebugLog(std::string("[BendLineNotch] boundary circle segment edit failed: ") + ex.Message());
	}
	return false;
}

bool HideCircleSegmentsOutsideVisibleNotchArc(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Arc* circle,
	const std::vector<double>& intersectionAngles,
	double visibleAngle)
{
	if (baseView == NULL || circle == NULL)
	{
		return false;
	}

	std::vector<double> parameters;
	for (size_t i = 0; i < intersectionAngles.size(); ++i)
	{
		const double parameter = NormalizeAnglePositive(intersectionAngles[i]) / (2.0 * PI);
		AddUniqueSortedParameter(parameters, parameter >= 1.0 - 1.0e-7 ? 0.0 : parameter);
	}
	if (parameters.size() < 2)
	{
		return false;
	}

	int keepIndex = -1;
	double bestVisibleScore = -1.0e99;
	for (size_t i = 0; i < parameters.size(); ++i)
	{
		const double start = parameters[i];
		const double end = parameters[(i + 1) % parameters.size()];
		const double length = end > start ? end - start : end + 1.0 - start;
		if (length <= 1.0e-6)
		{
			continue;
		}
		double midParameter = start + length * 0.5;
		if (midParameter >= 1.0)
		{
			midParameter -= 1.0;
		}
		const double midAngle = midParameter * 2.0 * PI;
		const double score = std::cos(NormalizeAngleDelta(midAngle - visibleAngle));
		if (score > bestVisibleScore)
		{
			bestVisibleScore = score;
			keepIndex = static_cast<int>(i);
		}
	}
	if (keepIndex < 0)
	{
		return false;
	}

	const double keepStart = parameters[keepIndex];
	const double keepEnd = parameters[(keepIndex + 1) % parameters.size()];
	const bool keepWrapsZero = keepEnd < keepStart;
	std::vector<double> segmentStart;
	std::vector<double> segmentEnd;
	if (keepWrapsZero)
	{
		if (keepStart - keepEnd > 1.0e-6)
		{
			segmentStart.push_back(keepEnd);
			segmentEnd.push_back(keepStart);
		}
	}
	else
	{
		if (keepStart > 1.0e-6)
		{
			segmentStart.push_back(0.0);
			segmentEnd.push_back(keepStart);
		}
		if (1.0 - keepEnd > 1.0e-6)
		{
			segmentStart.push_back(keepEnd);
			segmentEnd.push_back(1.0);
		}
	}
	if (segmentStart.empty())
	{
		return false;
	}

	try
	{
		baseView->DependentDisplay()->ApplySegmentEdit(
			circle,
			NXOpen::ViewDependentDisplayManager::FontInvisible,
			NXOpen::ViewDependentDisplayManager::WidthObject,
			segmentStart,
			segmentEnd);
		std::ostringstream log;
		log << "[BendLineNotch] circle break by intersections arcTag=" << circle->Tag()
			<< " intersectionAngles=" << intersectionAngles.size()
			<< " keep=(" << keepStart << "," << keepEnd << ")"
			<< " wrap=" << (keepWrapsZero ? "true" : "false")
			<< " hiddenSegments=" << segmentStart.size();
		BendNoteDebugLog(log.str());
		return true;
	}
	catch (const NXOpen::NXException& ex)
	{
		BendNoteDebugLog(std::string("[BendLineNotch] circle segment edit failed: ") + ex.Message());
	}
	return false;
}

void AddNotchCirclePairIntersectionAngles(
	BendLineNotchCircleEdit& first,
	BendLineNotchCircleEdit& second)
{
	if (first.circle == NULL || second.circle == NULL || first.radius <= 0.0 || second.radius <= 0.0)
	{
		return;
	}

	const double dx = second.centerX - first.centerX;
	const double dy = second.centerY - first.centerY;
	const double distanceSquared = dx * dx + dy * dy;
	if (distanceSquared <= 1.0e-12)
	{
		return;
	}

	const double distance = std::sqrt(distanceSquared);
	const double tolerance = 1.0e-6;
	if (distance > first.radius + second.radius + tolerance ||
		distance < std::fabs(first.radius - second.radius) - tolerance)
	{
		return;
	}

	const double a = (first.radius * first.radius - second.radius * second.radius + distanceSquared) / (2.0 * distance);
	const double heightSquared = first.radius * first.radius - a * a;
	if (heightSquared < -tolerance)
	{
		return;
	}
	const double height = std::sqrt(std::max(0.0, heightSquared));
	const double ux = dx / distance;
	const double uy = dy / distance;
	const double baseX = first.centerX + a * ux;
	const double baseY = first.centerY + a * uy;
	const double px = -uy * height;
	const double py = ux * height;

	const double intersectionXs[2] = { baseX + px, baseX - px };
	const double intersectionYs[2] = { baseY + py, baseY - py };
	for (int i = 0; i < 2; ++i)
	{
		if (i == 1 && height <= tolerance)
		{
			continue;
		}
		first.intersectionAngles.push_back(
			std::atan2(intersectionYs[i] - first.centerY, intersectionXs[i] - first.centerX));
		second.intersectionAngles.push_back(
			std::atan2(intersectionYs[i] - second.centerY, intersectionXs[i] - second.centerX));
	}

	std::ostringstream log;
	log << "[BendLineNotch] circle-circle intersections"
		<< " firstArcTag=" << first.circle->Tag()
		<< " secondArcTag=" << second.circle->Tag()
		<< " points=" << (height <= tolerance ? 1 : 2);
	BendNoteDebugLog(log.str());
}

void HideNotchCircleSegmentsAfterIntersections(
	NXOpen::Drawings::BaseView* baseView,
	std::vector<BendLineNotchCircleEdit>& circleEdits)
{
	for (size_t i = 0; i < circleEdits.size(); ++i)
	{
		for (size_t j = i + 1; j < circleEdits.size(); ++j)
		{
			AddNotchCirclePairIntersectionAngles(circleEdits[i], circleEdits[j]);
		}
	}

	for (size_t i = 0; i < circleEdits.size(); ++i)
	{
		HideCircleSegmentsOutsideVisibleNotchArc(
			baseView,
			circleEdits[i].circle,
			circleEdits[i].intersectionAngles,
			circleEdits[i].visibleAngle);
	}
}

std::vector<BendLineNotchBoundaryCircleEdit> MergeBoundaryCircleEdits(
	const std::vector<BendLineNotchBoundaryCircleEdit>& edits)
{
	std::vector<BendLineNotchBoundaryCircleEdit> merged;
	for (size_t i = 0; i < edits.size(); ++i)
	{
		if (edits[i].boundaryCurve == NULL || edits[i].segmentStart.empty())
		{
			continue;
		}

		size_t targetIndex = merged.size();
		for (size_t j = 0; j < merged.size(); ++j)
		{
			if (merged[j].boundaryCurve == edits[i].boundaryCurve)
			{
				targetIndex = j;
				break;
			}
		}
		if (targetIndex == merged.size())
		{
			merged.push_back(edits[i]);
			continue;
		}

		for (size_t segmentIndex = 0; segmentIndex < edits[i].segmentStart.size(); ++segmentIndex)
		{
			merged[targetIndex].segmentStart.push_back(edits[i].segmentStart[segmentIndex]);
			merged[targetIndex].segmentEnd.push_back(edits[i].segmentEnd[segmentIndex]);
		}
	}

	for (size_t i = 0; i < merged.size(); ++i)
	{
		std::vector<std::pair<double, double> > segments;
		for (size_t segmentIndex = 0; segmentIndex < merged[i].segmentStart.size(); ++segmentIndex)
		{
			const double start = Clamp01(std::min(merged[i].segmentStart[segmentIndex], merged[i].segmentEnd[segmentIndex]));
			const double end = Clamp01(std::max(merged[i].segmentStart[segmentIndex], merged[i].segmentEnd[segmentIndex]));
			if (end - start > 1.0e-6)
			{
				segments.push_back(std::make_pair(start, end));
			}
		}
		std::sort(segments.begin(), segments.end());
		merged[i].segmentStart.clear();
		merged[i].segmentEnd.clear();
		for (size_t segmentIndex = 0; segmentIndex < segments.size(); ++segmentIndex)
		{
			if (!merged[i].segmentStart.empty() && segments[segmentIndex].first <= merged[i].segmentEnd.back() + 1.0e-6)
			{
				merged[i].segmentEnd.back() = std::max(merged[i].segmentEnd.back(), segments[segmentIndex].second);
			}
			else
			{
				merged[i].segmentStart.push_back(segments[segmentIndex].first);
				merged[i].segmentEnd.push_back(segments[segmentIndex].second);
			}
		}
	}
	return merged;
}

bool GetNotchArcAnglesFromBoundaryIntersections(
	const std::vector<double>& intersectionAngles,
	double visibleAngle,
	double fallbackStartAngle,
	double fallbackEndAngle,
	double& startAngle,
	double& endAngle)
{
	double negativeDelta = -1.0e99;
	double positiveDelta = 1.0e99;
	for (size_t i = 0; i < intersectionAngles.size(); ++i)
	{
		const double delta = NormalizeAngleDelta(intersectionAngles[i] - visibleAngle);
		if (delta < -1.0e-5 && delta > negativeDelta)
		{
			negativeDelta = delta;
		}
		else if (delta > 1.0e-5 && delta < positiveDelta)
		{
			positiveDelta = delta;
		}
	}
	if (negativeDelta < -1.0e90 || positiveDelta > 1.0e90)
	{
		startAngle = fallbackStartAngle;
		endAngle = fallbackEndAngle;
		return false;
	}

	startAngle = visibleAngle + negativeDelta;
	endAngle = visibleAngle + positiveDelta;
	if (endAngle <= startAngle)
	{
		endAngle += 2.0 * PI;
	}
	return true;
}

void AddBendLineNotchGroup(
	std::vector<BendLineNotchGroup>& groups,
	double startX,
	double startY,
	double startZ,
	double endX,
	double endY,
	double endZ,
	tag_t curveTag)
{
	double dirX = endX - startX;
	double dirY = endY - startY;
	const double length = std::sqrt(dirX * dirX + dirY * dirY);
	if (length <= 1.0e-6)
	{
		return;
	}
	dirX /= length;
	dirY /= length;
	if (dirX < -1.0e-6 || (std::fabs(dirX) <= 1.0e-6 && dirY < 0.0))
	{
		dirX = -dirX;
		dirY = -dirY;
	}

	const double lineTolerance = 0.05;
	for (size_t i = 0; i < groups.size(); ++i)
	{
		const double dot = std::fabs(groups[i].dirX * dirX + groups[i].dirY * dirY);
		const double startDistance = PointToLineDistanceByNormal(startX, startY, groups[i].dirX, groups[i].dirY, groups[i].normalOffset);
		const double endDistance = PointToLineDistanceByNormal(endX, endY, groups[i].dirX, groups[i].dirY, groups[i].normalOffset);
		if (dot < 0.9999 || startDistance > lineTolerance || endDistance > lineTolerance)
		{
			continue;
		}

		const double startProjection = Projection2d(startX, startY, groups[i].dirX, groups[i].dirY);
		const double endProjection = Projection2d(endX, endY, groups[i].dirX, groups[i].dirY);
		if (startProjection < groups[i].minProjection)
		{
			groups[i].minProjection = startProjection;
			groups[i].minPoint.x = startX;
			groups[i].minPoint.y = startY;
			groups[i].minPoint.z = startZ;
		}
		if (startProjection > groups[i].maxProjection)
		{
			groups[i].maxProjection = startProjection;
			groups[i].maxPoint.x = startX;
			groups[i].maxPoint.y = startY;
			groups[i].maxPoint.z = startZ;
		}
		if (endProjection < groups[i].minProjection)
		{
			groups[i].minProjection = endProjection;
			groups[i].minPoint.x = endX;
			groups[i].minPoint.y = endY;
			groups[i].minPoint.z = endZ;
		}
		if (endProjection > groups[i].maxProjection)
		{
			groups[i].maxProjection = endProjection;
			groups[i].maxPoint.x = endX;
			groups[i].maxPoint.y = endY;
			groups[i].maxPoint.z = endZ;
		}
		groups[i].curveTags.push_back(curveTag);
		return;
	}

	BendLineNotchGroup group;
	group.dirX = dirX;
	group.dirY = dirY;
	group.normalOffset = NormalOffset2d(startX, startY, dirX, dirY);
	const double startProjection = Projection2d(startX, startY, dirX, dirY);
	const double endProjection = Projection2d(endX, endY, dirX, dirY);
	if (startProjection <= endProjection)
	{
		group.minProjection = startProjection;
		group.maxProjection = endProjection;
		group.minPoint.x = startX;
		group.minPoint.y = startY;
		group.minPoint.z = startZ;
		group.maxPoint.x = endX;
		group.maxPoint.y = endY;
		group.maxPoint.z = endZ;
	}
	else
	{
		group.minProjection = endProjection;
		group.maxProjection = startProjection;
		group.minPoint.x = endX;
		group.minPoint.y = endY;
		group.minPoint.z = endZ;
		group.maxPoint.x = startX;
		group.maxPoint.y = startY;
		group.maxPoint.z = startZ;
	}
	group.curveTags.push_back(curveTag);
	groups.push_back(group);
}

std::vector<BendLineNotchGroup> BuildBendLineNotchGroups(
	NXOpen::Drawings::BaseView* baseView,
	const std::vector<NXOpen::Drawings::DraftingCurve*>& curves,
	const std::vector<NXOpen::Features::FlatPattern::ObjectDataFace>& objects,
	const char* directionName)
{
	std::vector<BendLineNotchGroup> groups;
	std::unordered_set<tag_t> usedFlatPatternCurveTags;
	for (size_t i = 0; i < objects.size(); ++i)
	{
		NXOpen::Curve* flatPatternCurve = objects[i].FlatPatternObject;
		if (flatPatternCurve == NULL || flatPatternCurve->Tag() == NULL_TAG)
		{
			continue;
		}
		if (!usedFlatPatternCurveTags.insert(flatPatternCurve->Tag()).second)
		{
			continue;
		}

		NXOpen::Drawings::DraftingCurve* draftingCurve =
			FindDraftingCurveForFlatPatternCurve(curves, flatPatternCurve);
		if (draftingCurve == NULL)
		{
			std::ostringstream log;
			log << "[BendLineNotch] no drafting curve direction=" << directionName
				<< " flatCurveTag=" << flatPatternCurve->Tag();
			BendNoteDebugLog(log.str());
			continue;
		}

		UF_CURVE_line_t lineData;
		if (UF_CURVE_ask_line_data(draftingCurve->Tag(), &lineData) != 0)
		{
			std::ostringstream log;
			log << "[BendLineNotch] cannot read line direction=" << directionName
				<< " curveTag=" << draftingCurve->Tag();
			BendNoteDebugLog(log.str());
			continue;
		}
		AddBendLineNotchGroup(
			groups,
			lineData.start_point[0],
			lineData.start_point[1],
			lineData.start_point[2],
			lineData.end_point[0],
			lineData.end_point[1],
			lineData.end_point[2],
			draftingCurve->Tag());
	}
	return groups;
}

std::vector<NXOpen::Drawings::DraftingCurve*> CollectExteriorDraftingCurvesForFlatPattern(
	NXOpen::Features::FlatPattern* flatPattern,
	const std::vector<NXOpen::Drawings::DraftingCurve*>& curves)
{
	std::vector<NXOpen::Drawings::DraftingCurve*> exteriorDraftingCurves;
	if (flatPattern == NULL || curves.empty())
	{
		return exteriorDraftingCurves;
	}

	std::vector<NXOpen::Features::FlatPattern::ObjectDataEdge> exteriorObjects;
	try
	{
		flatPattern->GetExteriorCurves(exteriorObjects);
	}
	catch (const NXOpen::NXException& ex)
	{
		BendNoteDebugLog(std::string("[BendLineNotch] get exterior curves failed: ") + ex.Message());
		return exteriorDraftingCurves;
	}

	size_t missingDraftingCurveCount = 0;
	std::unordered_set<tag_t> usedDraftingCurveTags;
	for (size_t i = 0; i < exteriorObjects.size(); ++i)
	{
		NXOpen::Curve* flatPatternCurve = exteriorObjects[i].FlatPatternObject;
		if (flatPatternCurve == NULL || flatPatternCurve->Tag() == NULL_TAG)
		{
			++missingDraftingCurveCount;
			continue;
		}

		NXOpen::Drawings::DraftingCurve* draftingCurve =
			FindDraftingCurveForFlatPatternCurve(curves, flatPatternCurve);
		if (draftingCurve == NULL || draftingCurve->Tag() == NULL_TAG)
		{
			++missingDraftingCurveCount;
			continue;
		}
		if (!usedDraftingCurveTags.insert(draftingCurve->Tag()).second)
		{
			continue;
		}
		exteriorDraftingCurves.push_back(draftingCurve);
	}

	std::ostringstream log;
	log << "[BendLineNotch] exteriorCurves objects=" << exteriorObjects.size()
		<< " mapped=" << exteriorDraftingCurves.size()
		<< " missing=" << missingDraftingCurveCount;
	BendNoteDebugLog(log.str());
	return exteriorDraftingCurves;
}

bool CreateNotchesForBendLineGroups(
	NXOpen::Drawings::BaseView* baseView,
	const std::vector<NXOpen::Drawings::DraftingCurve*>& exteriorDraftingCurves,
	const std::vector<BendLineNotchGroup>& groups,
	double diameter,
	const char* directionName,
	std::unordered_set<std::string>& usedEndpointKeys)
{
	if (baseView == NULL || workPart == NULL || diameter <= 0.0 || groups.empty())
	{
		return false;
	}
	if (exteriorDraftingCurves.empty())
	{
		BendNoteDebugLog("[BendLineNotch] no exterior drafting curves for notch boundary");
		return false;
	}

	NXOpen::NXMatrix* wcsMatrix = NULL;
	try
	{
		wcsMatrix = dynamic_cast<NXOpen::NXMatrix*>(workPart->NXMatrices()->FindObject("WCS"));
	}
	catch (...)
	{
		wcsMatrix = NULL;
	}
	if (wcsMatrix == NULL)
	{
		BendNoteDebugLog("[BendLineNotch] WCS matrix not found");
		return false;
	}

	const double radius = std::max(0.05, diameter * 0.5);
	std::vector<tag_t> allBendLineCurveTags;
	for (size_t groupIndex = 0; groupIndex < groups.size(); ++groupIndex)
	{
		allBendLineCurveTags.insert(
			allBendLineCurveTags.end(),
			groups[groupIndex].curveTags.begin(),
			groups[groupIndex].curveTags.end());
	}
	bool createdAny = false;
	std::vector<BendLineNotchBoundaryCircleEdit> boundaryEdits;
	std::vector<BendLineNotchCircleEdit> circleEdits;

	try
	{
		baseView->Expand();
		for (size_t groupIndex = 0; groupIndex < groups.size(); ++groupIndex)
		{
			const BendLineNotchEndpoint endpoints[2] = { groups[groupIndex].minPoint, groups[groupIndex].maxPoint };
			for (int endpointIndex = 0; endpointIndex < 2; ++endpointIndex)
			{
				const double x = endpoints[endpointIndex].x;
				const double y = endpoints[endpointIndex].y;
				std::ostringstream keyStream;
				keyStream << directionName << "|"
					<< static_cast<long long>(std::llround(x * 1000.0)) << "|"
					<< static_cast<long long>(std::llround(y * 1000.0));
				if (!usedEndpointKeys.insert(keyStream.str()).second)
				{
					continue;
				}

				double startAngle = 0.0;
				double endAngle = PI;
				GetVisibleArcAnglesForBendLineEnd(
					groups[groupIndex].dirX,
					groups[groupIndex].dirY,
					endpointIndex == 0,
					startAngle,
					endAngle);
				const double visibleAngle = endpointIndex == 0
					? std::atan2(groups[groupIndex].dirY, groups[groupIndex].dirX)
					: std::atan2(groups[groupIndex].dirY, groups[groupIndex].dirX) + PI;
				std::vector<double> intersectionAngles;
				std::vector<BendLineNotchBoundaryCircleEdit> endpointBoundaryEdits;
				NXOpen::Drawings::DraftingCurve* colorSourceCurve = NULL;
				size_t skippedBendLine = 0;
				size_t skippedUnsupportedCurve = 0;
				for (size_t curveIndex = 0; curveIndex < exteriorDraftingCurves.size(); ++curveIndex)
				{
					NXOpen::Drawings::DraftingCurve* exteriorCurve = exteriorDraftingCurves[curveIndex];
					if (exteriorCurve == NULL)
					{
						continue;
					}
					if (std::find(
						allBendLineCurveTags.begin(),
						allBendLineCurveTags.end(),
						exteriorCurve->Tag()) != allBendLineCurveTags.end())
					{
						++skippedBendLine;
						continue;
					}

					BendLineNotchBoundaryCircleEdit edit;
					if (!CollectBoundaryCircleEdit(
						exteriorCurve,
						x,
						y,
						radius,
						edit,
						intersectionAngles))
					{
						++skippedUnsupportedCurve;
						continue;
					}
					if (colorSourceCurve == NULL)
					{
						colorSourceCurve = exteriorCurve;
					}
					endpointBoundaryEdits.push_back(edit);
				}
				const bool arcLimitedByBoundary = false;
				NXOpen::Arc* arc = workPart->Curves()->CreateArc(
					NXOpen::Point3d(x, y, endpoints[endpointIndex].z),
					wcsMatrix,
					radius,
					0.0,
					2.0 * PI);
				if (arc == NULL)
				{
					continue;
				}
				BendLineNotchCircleEdit circleEdit;
				circleEdit.circle = arc;
				circleEdit.centerX = x;
				circleEdit.centerY = y;
				circleEdit.radius = radius;
				circleEdit.visibleAngle = visibleAngle;
				circleEdit.intersectionAngles = intersectionAngles;
				circleEdits.push_back(circleEdit);
				if (colorSourceCurve != NULL)
				{
					UF_OBJ_disp_props_t boundaryDisplayProperties;
					if (UF_OBJ_ask_display_properties(colorSourceCurve->Tag(), &boundaryDisplayProperties) == 0)
					{
						UF_OBJ_set_color(arc->Tag(), boundaryDisplayProperties.color);
					}
				}

				if (!endpointBoundaryEdits.empty())
				{
					boundaryEdits.insert(boundaryEdits.end(), endpointBoundaryEdits.begin(), endpointBoundaryEdits.end());
				}
				else
				{
					std::ostringstream noBoundaryLog;
					noBoundaryLog << "[BendLineNotch] no boundary inside circle direction=" << directionName
						<< " point=(" << x << "," << y << ")"
						<< " radius=" << radius;
					BendNoteDebugLog(noBoundaryLog.str());
				}
				createdAny = true;

				std::ostringstream log;
				log << "[BendLineNotch] create direction=" << directionName
					<< " group=" << groupIndex
					<< " point=(" << x << "," << y << ")"
					<< " diameter=" << diameter
					<< " sourceCurves=" << groups[groupIndex].curveTags.size()
					<< " arcTag=" << arc->Tag()
					<< " boundaryEdits=" << endpointBoundaryEdits.size()
					<< " intersectionAngles=" << intersectionAngles.size()
					<< " skippedBendLine=" << skippedBendLine
					<< " skippedUnsupportedCurve=" << skippedUnsupportedCurve
					<< " exteriorCandidates=" << exteriorDraftingCurves.size()
					<< " arcLimitedByBoundary=" << (arcLimitedByBoundary ? "true" : "false")
					<< " bendDir=(" << groups[groupIndex].dirX << "," << groups[groupIndex].dirY << ")"
					<< " endpoint=" << (endpointIndex == 0 ? "min" : "max")
					<< " createCircle=true"
					<< " visibleAngle=" << visibleAngle;
				BendNoteDebugLog(log.str());
			}
		}
	}
	catch (const NXOpen::NXException& ex)
	{
		BendNoteDebugLog(std::string("[BendLineNotch] create arc failed: ") + ex.Message());
	}

	HideNotchCircleSegmentsAfterIntersections(baseView, circleEdits);

	try
	{
		workPart->Views()->UnexpandWork();
	}
	catch (...)
	{
	}

	const std::vector<BendLineNotchBoundaryCircleEdit> mergedBoundaryEdits =
		MergeBoundaryCircleEdits(boundaryEdits);
	for (size_t i = 0; i < mergedBoundaryEdits.size(); ++i)
	{
		HideBoundarySegmentsInsideNotchCircle(
			baseView,
			mergedBoundaryEdits[i]);
	}

	return createdAny;
}
}

bool CreateFlatPatternHoleCoordinateDimensions(
	NXOpen::Drawings::BaseView* baseView)
{
	if (baseView == NULL)
	{
		return false;
	}

	LogPluginMessage("[HoleCoord] start CreateFlatPatternHoleCoordinateDimensions");
	const std::vector<NXOpen::Drawings::DraftingCurve*> curves = CollectDraftingCurves(baseView);
	if (curves.empty())
	{
		LogPluginMessage("[HoleCoord] no drafting curves");
		return false;
	}

	HoleCoordinateExtents extents = CalculateHoleCoordinateExtents(baseView, curves);
	if (!extents.valid)
	{
		LogPluginMessage("[HoleCoord] extents invalid");
		return false;
	}
	{
		std::ostringstream extentsLog;
		const NXOpen::Point3d viewOrigin = baseView->GetDrawingReferencePoint();
		extentsLog << "[HoleCoord] extents(raw) min=("
			<< extents.minX << "," << extents.minY
			<< ") max=("
			<< extents.maxX << "," << extents.maxY
			<< ") drawingViewOrigin=("
			<< viewOrigin.X << "," << viewOrigin.Y << "," << viewOrigin.Z
			<< ")";
		LogPluginMessage(extentsLog.str());
	}

	std::vector<HoleCoordinateCandidate> holes = CollectRoundHoleCandidates(baseView, curves, extents);
	LogPluginMessage("[HoleCoord] round holes=" + std::to_string(holes.size()));
	if (holes.empty())
	{
		return false;
	}

	NXOpen::Sketch* sketch = CreateDraftingSketch(baseView);
	if (sketch == NULL)
	{
		LogPluginMessage("[HoleCoord] drafting sketch create failed");
		return false;
	}

	const NXOpen::Point3d lowerLeft(extents.minX, extents.minY, 0.0);
	const NXOpen::Point3d upperRight(extents.maxX, extents.maxY, 0.0);
	NXOpen::Point* originPoint = CreateFixedDraftingPointInActiveSketch(lowerLeft);
	NXOpen::Point* boundaryPoint = CreateFixedDraftingPointInActiveSketch(upperRight);
	::theSession->ActiveSketch()->Deactivate(
		NXOpen::Sketch::ViewReorientFalse,
		NXOpen::Sketch::UpdateLevelModel);

	if (originPoint == NULL || boundaryPoint == NULL)
	{
		LogPluginMessage("[HoleCoord] origin/boundary sketch point create failed");
		return false;
	}

	NXOpen::Annotations::OrdinateOriginDimension* ordinateOrigin =
		CreateHoleCoordinateOrdinateOrigin(baseView, originPoint);
	if (ordinateOrigin == NULL)
	{
		LogPluginMessage("[HoleCoord] explicit ordinate origin create failed");
		return false;
	}

	std::vector<HoleCoordinateCandidate> verticalHoles;
	std::vector<HoleCoordinateCandidate> horizontalHoles;
	verticalHoles.reserve(holes.size());
	horizontalHoles.reserve(holes.size());
	for (size_t i = 0; i < holes.size(); ++i)
	{
		verticalHoles.push_back(holes[i]);
		horizontalHoles.push_back(holes[i]);
	}

	std::sort(
		verticalHoles.begin(),
		verticalHoles.end(),
		[](const HoleCoordinateCandidate& left, const HoleCoordinateCandidate& right)
		{
			return left.drawingCenter.Y < right.drawingCenter.Y;
		});
	std::sort(
		horizontalHoles.begin(),
		horizontalHoles.end(),
		[](const HoleCoordinateCandidate& left, const HoleCoordinateCandidate& right)
		{
			return left.drawingCenter.X < right.drawingCenter.X;
		});

	const double width = std::max(1.0, extents.maxX - extents.minX);
	const double height = std::max(1.0, extents.maxY - extents.minY);
	const NXOpen::Point3d verticalMarginLocation(extents.maxX + std::max(12.0, width * 0.10), extents.minY, 0.0);
	const NXOpen::Point3d horizontalMarginLocation(extents.minX, extents.maxY + std::max(12.0, height * 0.10), 0.0);
	NXOpen::Annotations::OrdinateMargin* minVerticalMargin =
		CreateHoleCoordinateExplicitMargin(ordinateOrigin, baseView, originPoint, -5.0, true);
	NXOpen::Annotations::OrdinateMargin* minHorizontalMargin =
		CreateHoleCoordinateExplicitMargin(ordinateOrigin, baseView, originPoint, -5.0, false);
	NXOpen::Annotations::OrdinateMargin* verticalMargin =
		CreateHoleCoordinateExplicitMargin(ordinateOrigin, baseView, boundaryPoint, 5.0, true);
	NXOpen::Annotations::OrdinateMargin* horizontalMargin =
		CreateHoleCoordinateExplicitMargin(ordinateOrigin, baseView, boundaryPoint, 5.0, false);
	LogPluginMessage(std::string("[HoleCoord] minMargins=")
		+ (minHorizontalMargin != NULL ? "H" : "_")
		+ (minVerticalMargin != NULL ? "V" : "_")
		+ " maxMargins="
		+ (horizontalMargin != NULL ? "H" : "_")
		+ (verticalMargin != NULL ? "V" : "_"));

	bool createdAny = false;
	createdAny = CreateMultiHoleOrdinateDimension(
		baseView,
		originPoint,
		boundaryPoint,
		lowerLeft,
		verticalHoles,
		true,
		verticalMarginLocation,
		verticalMargin,
		ordinateOrigin) || createdAny;
	createdAny = CreateMultiHoleOrdinateDimension(
		baseView,
		originPoint,
		boundaryPoint,
		lowerLeft,
		horizontalHoles,
		false,
		horizontalMarginLocation,
		horizontalMargin,
		ordinateOrigin) || createdAny;
	LogPluginMessage(std::string("[HoleCoord] createdAny=") + (createdAny ? "true" : "false"));
	return createdAny;
}

bool CreateFlatPatternBendNotes(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Features::FlatPattern* flatPattern,
	double textHeight)
{
	try
	{
		std::ofstream reset("D:\\UG智辉钣金插件\\logs\\bend_note_debug.log", std::ios::out | std::ios::trunc);
		reset << "[BendNote] reset\n";
	}
	catch (...)
	{
	}

	if (baseView == NULL || flatPattern == NULL)
	{
		BendNoteDebugLog("[BendNote] baseView or flatPattern is null");
		return false;
	}

	const std::vector<NXOpen::Drawings::DraftingCurve*> curves = CollectDraftingCurves(baseView);
	if (curves.empty())
	{
		BendNoteDebugLog("[BendNote] no drafting curves");
		return false;
	}

	bool createdAny = false;
	std::unordered_set<tag_t> usedFlatPatternCurveTags;
	try
	{
		std::vector<NXOpen::Features::FlatPattern::ObjectDataFace> upObjects;
		flatPattern->GetBendUpCenterLines(upObjects);
		std::ostringstream log;
		log << "[BendNote] upObjects=" << upObjects.size();
		BendNoteDebugLog(log.str());
		createdAny = CreateFlatPatternBendNotesForObjects(
			baseView,
			curves,
			upObjects,
			"\xE4\xB8\x8A\xE6\x8A\x98",
			textHeight,
			usedFlatPatternCurveTags) || createdAny;
	}
	catch (const NXOpen::NXException& ex)
	{
		BendNoteDebugLog(std::string("[BendNote] get bend up failed: ") + ex.Message());
	}

	try
	{
		std::vector<NXOpen::Features::FlatPattern::ObjectDataFace> downObjects;
		flatPattern->GetBendDownCenterLines(downObjects);
		std::ostringstream log;
		log << "[BendNote] downObjects=" << downObjects.size();
		BendNoteDebugLog(log.str());
		createdAny = CreateFlatPatternBendNotesForObjects(
			baseView,
			curves,
			downObjects,
			"\xE4\xB8\x8B\xE6\x8A\x98",
			textHeight,
			usedFlatPatternCurveTags) || createdAny;
	}
	catch (const NXOpen::NXException& ex)
	{
		BendNoteDebugLog(std::string("[BendNote] get bend down failed: ") + ex.Message());
	}

	BendNoteDebugLog(std::string("[BendNote] createdAny=") + (createdAny ? "true" : "false"));
	return createdAny;
}

bool BreakFlatPatternBendLines(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Features::FlatPattern* flatPattern,
	double edgeDistance,
	double upKeepLength,
	double downKeepLength)
{
	if (baseView == NULL || flatPattern == NULL)
	{
		BendNoteDebugLog("[BendLineBreak] baseView or flatPattern is null");
		return false;
	}

	const std::vector<NXOpen::Drawings::DraftingCurve*> curves = CollectDraftingCurves(baseView);
	if (curves.empty())
	{
		BendNoteDebugLog("[BendLineBreak] no drafting curves");
		return false;
	}

	bool editedAny = false;
	std::unordered_set<tag_t> usedFlatPatternCurveTags;
	try
	{
		std::vector<NXOpen::Features::FlatPattern::ObjectDataFace> upObjects;
		flatPattern->GetBendUpCenterLines(upObjects);
		std::ostringstream log;
		log << "[BendLineBreak] upObjects=" << upObjects.size()
			<< " edgeDistance=" << edgeDistance
			<< " keep=" << upKeepLength;
		BendNoteDebugLog(log.str());
		editedAny = BreakFlatPatternBendLinesForObjects(
			baseView,
			curves,
			upObjects,
			std::max(0.0, edgeDistance),
			std::max(0.1, upKeepLength),
			"up",
			usedFlatPatternCurveTags) || editedAny;
	}
	catch (const NXOpen::NXException& ex)
	{
		BendNoteDebugLog(std::string("[BendLineBreak] get bend up failed: ") + ex.Message());
	}

	try
	{
		std::vector<NXOpen::Features::FlatPattern::ObjectDataFace> downObjects;
		flatPattern->GetBendDownCenterLines(downObjects);
		std::ostringstream log;
		log << "[BendLineBreak] downObjects=" << downObjects.size()
			<< " edgeDistance=" << edgeDistance
			<< " keep=" << downKeepLength;
		BendNoteDebugLog(log.str());
		editedAny = BreakFlatPatternBendLinesForObjects(
			baseView,
			curves,
			downObjects,
			std::max(0.0, edgeDistance),
			std::max(0.1, downKeepLength),
			"down",
			usedFlatPatternCurveTags) || editedAny;
	}
	catch (const NXOpen::NXException& ex)
	{
		BendNoteDebugLog(std::string("[BendLineBreak] get bend down failed: ") + ex.Message());
	}

	BendNoteDebugLog(std::string("[BendLineBreak] editedAny=") + (editedAny ? "true" : "false"));
	return editedAny;
}

bool CreateFlatPatternLargeArcMarkerLines(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Features::FlatPattern* flatPattern,
	double radiusThreshold,
	double edgeDistance,
	double keepLength)
{
	if (baseView == NULL || flatPattern == NULL || workPart == NULL)
	{
		BendNoteDebugLog("[LargeArcMarker] baseView, flatPattern, or workPart is null");
		return false;
	}

	const std::vector<NXOpen::Drawings::DraftingCurve*> viewCurves = CollectDraftingCurves(baseView);
	std::vector<NXOpen::Features::FlatPattern::ObjectDataEdge> tangentObjects;
	std::vector<NXOpen::Features::FlatPattern::ObjectDataFace> bendObjects;
	try
	{
		flatPattern->GetBendTangentLines(tangentObjects);
		std::vector<NXOpen::Features::FlatPattern::ObjectDataFace> upObjects;
		std::vector<NXOpen::Features::FlatPattern::ObjectDataFace> downObjects;
		flatPattern->GetBendUpCenterLines(upObjects);
		flatPattern->GetBendDownCenterLines(downObjects);
		bendObjects.insert(bendObjects.end(), upObjects.begin(), upObjects.end());
		bendObjects.insert(bendObjects.end(), downObjects.begin(), downObjects.end());
	}
	catch (const NXOpen::NXException& ex)
	{
		BendNoteDebugLog(std::string("[LargeArcMarker] get bend tangent lines failed: ") + ex.Message());
		return false;
	}

	struct TangentCandidate
	{
		NXOpen::Drawings::DraftingCurve* curve;
		double start[2];
		double end[2];
	};
	std::vector<TangentCandidate> tangentCandidates;
	for (size_t tangentIndex = 0; tangentIndex < tangentObjects.size(); ++tangentIndex)
	{
		NXOpen::Drawings::DraftingCurve* markerCurve = FindDraftingCurveForFlatPatternCurve(
			viewCurves,
			tangentObjects[tangentIndex].FlatPatternObject);
		if (markerCurve == NULL && tangentObjects[tangentIndex].FlatSolidObject != NULL)
		{
			for (size_t curveIndex = 0; curveIndex < viewCurves.size(); ++curveIndex)
			{
				if (DraftingCurveReferencesObject(
					viewCurves[curveIndex],
					tangentObjects[tangentIndex].FlatSolidObject->Tag()))
				{
					markerCurve = viewCurves[curveIndex];
					break;
				}
			}
		}
		if (markerCurve != NULL)
		{
			UF_CURVE_line_t lineData;
			double start[2] = { 0.0, 0.0 };
			double end[2] = { 0.0, 0.0 };
			if (TryGetLineCurveData(baseView, markerCurve, lineData, start, end))
			{
				TangentCandidate candidate{};
				candidate.curve = markerCurve;
				candidate.start[0] = start[0];
				candidate.start[1] = start[1];
				candidate.end[0] = end[0];
				candidate.end[1] = end[1];
				tangentCandidates.push_back(candidate);
			}
		}
	}

	std::vector<NXOpen::Drawings::DraftingCurve*> markerCurves;
	std::unordered_set<tag_t> markerCurveTags;
	size_t largeBendCount = 0;
	for (size_t bendIndex = 0; bendIndex < bendObjects.size(); ++bendIndex)
	{
		NXOpen::Face* formedFace = bendObjects[bendIndex].FormedBodyObject;
		double faceRadius = 0.0;
		if (formedFace == NULL)
		{
			continue;
		}
		int faceType = 0;
		double facePoint[3] = { 0.0, 0.0, 0.0 };
		double faceDirection[3] = { 0.0, 0.0, 0.0 };
		double faceBox[6] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
		double radialData = 0.0;
		int normalDirection = 0;
		if (UF_MODL_ask_face_data(
			formedFace->Tag(),
			&faceType,
			facePoint,
			faceDirection,
			faceBox,
			&faceRadius,
			&radialData,
			&normalDirection) != 0 ||
			faceRadius <= radiusThreshold + 1.0e-9)
		{
			continue;
		}

		NXOpen::Drawings::DraftingCurve* centerCurve = FindDraftingCurveForFlatPatternCurve(
			viewCurves,
			bendObjects[bendIndex].FlatPatternObject);
		UF_CURVE_line_t centerLineData;
		double centerStart[2] = { 0.0, 0.0 };
		double centerEnd[2] = { 0.0, 0.0 };
		if (centerCurve == NULL ||
			!TryGetLineCurveData(baseView, centerCurve, centerLineData, centerStart, centerEnd))
		{
			continue;
		}

		double directionX = centerEnd[0] - centerStart[0];
		double directionY = centerEnd[1] - centerStart[1];
		const double centerLength = std::sqrt(directionX * directionX + directionY * directionY);
		if (centerLength <= 1.0e-6)
		{
			continue;
		}
		directionX /= centerLength;
		directionY /= centerLength;
		const double normalX = -directionY;
		const double normalY = directionX;
		const double centerMidX = (centerStart[0] + centerEnd[0]) * 0.5;
		const double centerMidY = (centerStart[1] + centerEnd[1]) * 0.5;
		const double centerOffset = centerMidX * normalX + centerMidY * normalY;
		const double centerProjection0 = centerStart[0] * directionX + centerStart[1] * directionY;
		const double centerProjection1 = centerEnd[0] * directionX + centerEnd[1] * directionY;
		const double centerMinProjection = std::min(centerProjection0, centerProjection1);
		const double centerMaxProjection = std::max(centerProjection0, centerProjection1);

		NXOpen::Drawings::DraftingCurve* negativeCurve = NULL;
		NXOpen::Drawings::DraftingCurve* positiveCurve = NULL;
		double negativeDistance = DBL_MAX;
		double positiveDistance = DBL_MAX;
		for (size_t candidateIndex = 0; candidateIndex < tangentCandidates.size(); ++candidateIndex)
		{
			const TangentCandidate& candidate = tangentCandidates[candidateIndex];
			double candidateX = candidate.end[0] - candidate.start[0];
			double candidateY = candidate.end[1] - candidate.start[1];
			const double candidateLength = std::sqrt(candidateX * candidateX + candidateY * candidateY);
			if (candidateLength <= 1.0e-6)
			{
				continue;
			}
			candidateX /= candidateLength;
			candidateY /= candidateLength;
			if (std::fabs(candidateX * directionX + candidateY * directionY) < 0.999)
			{
				continue;
			}
			const double candidateProjection0 = candidate.start[0] * directionX + candidate.start[1] * directionY;
			const double candidateProjection1 = candidate.end[0] * directionX + candidate.end[1] * directionY;
			const double candidateMinProjection = std::min(candidateProjection0, candidateProjection1);
			const double candidateMaxProjection = std::max(candidateProjection0, candidateProjection1);
			if (candidateMaxProjection < centerMinProjection - 0.5 ||
				candidateMinProjection > centerMaxProjection + 0.5)
			{
				continue;
			}
			const double candidateMidX = (candidate.start[0] + candidate.end[0]) * 0.5;
			const double candidateMidY = (candidate.start[1] + candidate.end[1]) * 0.5;
			const double signedDistance = candidateMidX * normalX + candidateMidY * normalY - centerOffset;
			if (signedDistance < -1.0e-4 && -signedDistance < negativeDistance)
			{
				negativeDistance = -signedDistance;
				negativeCurve = candidate.curve;
			}
			else if (signedDistance > 1.0e-4 && signedDistance < positiveDistance)
			{
				positiveDistance = signedDistance;
				positiveCurve = candidate.curve;
			}
		}

		if (negativeCurve != NULL && positiveCurve != NULL)
		{
			++largeBendCount;
			if (markerCurveTags.insert(negativeCurve->Tag()).second)
			{
				markerCurves.push_back(negativeCurve);
			}
			if (markerCurveTags.insert(positiveCurve->Tag()).second)
			{
				markerCurves.push_back(positiveCurve);
			}
			std::ostringstream selectionLog;
			selectionLog << "[LargeArcMarker] selected radius=" << faceRadius
				<< " threshold=" << radiusThreshold
				<< " centerCurveTag=" << centerCurve->Tag()
				<< " negativeTag=" << negativeCurve->Tag()
				<< " positiveTag=" << positiveCurve->Tag();
			BendNoteDebugLog(selectionLog.str());
		}
	}
	try
	{
		if (!markerCurves.empty())
		{
			std::vector<NXOpen::DisplayableObject*> markerObjects(markerCurves.begin(), markerCurves.end());
			baseView->DependentDisplay()->RemoveErasureOnObjectAndSubobjects(markerObjects, true);
		}
	}
	catch (const NXOpen::NXException& ex)
	{
		BendNoteDebugLog(std::string("[LargeArcMarker] delete selected drafting-curve erasures failed: ") + ex.Message());
	}

	bool editedAny = false;
	for (size_t i = 0; i < markerCurves.size(); ++i)
	{
		editedAny = TrimLargeArcMarkerCurve(
			baseView,
			markerCurves[i],
			std::max(0.0, edgeDistance),
			std::max(0.1, keepLength)) || editedAny;
	}
	try
	{
		// This is part of the journaled View Dependent Edit workflow and makes
		// the restored erasures/segment edits take effect in the displayed view.
		NXOpen::Session::GetSession()->CleanUpFacetedFacesAndEdges();
	}
	catch (...)
	{
	}

	std::ostringstream log;
	log << "[LargeArcMarker] bendObjects=" << bendObjects.size()
		<< " largeBends=" << largeBendCount
		<< " radiusThreshold=" << radiusThreshold
		<< " tangentObjects=" << tangentObjects.size()
		<< " viewCurves=" << viewCurves.size()
		<< " markers=" << markerCurves.size()
		<< " edgeDistance=" << edgeDistance
		<< " keepLength=" << keepLength
		<< " edited=" << (editedAny ? "true" : "false");
	BendNoteDebugLog(log.str());
	return editedAny;
}

bool CreateFlatPatternBendLineNotches(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Features::FlatPattern* flatPattern,
	bool upEnabled,
	double upDiameter,
	bool downEnabled,
	double downDiameter)
{
	if (baseView == NULL || flatPattern == NULL || (!upEnabled && !downEnabled))
	{
		BendNoteDebugLog("[BendLineNotch] skipped disabled or missing input");
		return false;
	}

	const std::vector<NXOpen::Drawings::DraftingCurve*> curves = CollectDraftingCurves(baseView);
	if (curves.empty())
	{
		BendNoteDebugLog("[BendLineNotch] no drafting curves");
		return false;
	}

	const std::vector<NXOpen::Drawings::DraftingCurve*> exteriorDraftingCurves =
		CollectExteriorDraftingCurvesForFlatPattern(flatPattern, curves);
	if (exteriorDraftingCurves.empty())
	{
		BendNoteDebugLog("[BendLineNotch] exterior drafting curves empty");
		return false;
	}

	bool createdAny = false;
	std::unordered_set<std::string> usedEndpointKeys;
	if (upEnabled)
	{
		try
		{
			std::vector<NXOpen::Features::FlatPattern::ObjectDataFace> upObjects;
			flatPattern->GetBendUpCenterLines(upObjects);
			std::vector<BendLineNotchGroup> groups = BuildBendLineNotchGroups(baseView, curves, upObjects, "up");
			std::ostringstream log;
			log << "[BendLineNotch] upObjects=" << upObjects.size()
				<< " groups=" << groups.size()
				<< " diameter=" << upDiameter;
			BendNoteDebugLog(log.str());
			createdAny = CreateNotchesForBendLineGroups(
				baseView,
				exteriorDraftingCurves,
				groups,
				std::max(0.1, upDiameter),
				"up",
				usedEndpointKeys) || createdAny;
		}
		catch (const NXOpen::NXException& ex)
		{
			BendNoteDebugLog(std::string("[BendLineNotch] get bend up failed: ") + ex.Message());
		}
	}

	if (downEnabled)
	{
		try
		{
			std::vector<NXOpen::Features::FlatPattern::ObjectDataFace> downObjects;
			flatPattern->GetBendDownCenterLines(downObjects);
			std::vector<BendLineNotchGroup> groups = BuildBendLineNotchGroups(baseView, curves, downObjects, "down");
			std::ostringstream log;
			log << "[BendLineNotch] downObjects=" << downObjects.size()
				<< " groups=" << groups.size()
				<< " diameter=" << downDiameter;
			BendNoteDebugLog(log.str());
			createdAny = CreateNotchesForBendLineGroups(
				baseView,
				exteriorDraftingCurves,
				groups,
				std::max(0.1, downDiameter),
				"down",
				usedEndpointKeys) || createdAny;
		}
		catch (const NXOpen::NXException& ex)
		{
			BendNoteDebugLog(std::string("[BendLineNotch] get bend down failed: ") + ex.Message());
		}
	}

	BendNoteDebugLog(std::string("[BendLineNotch] createdAny=") + (createdAny ? "true" : "false"));
	return createdAny;
}

bool CreateFlatPatternHoleAttributeNotes(
	NXOpen::Drawings::BaseView* baseView,
	const std::string& markerStart)
{
	ResetHoleNoteDebugLog();
	if (baseView == NULL)
	{
		HoleNoteDebugLog("[HoleNote] baseView=null");
		return false;
	}

	{
		std::ostringstream log;
		log << "[HoleNote] start viewTag=" << baseView->Tag();
		HoleNoteDebugLog(log.str());
	}

	const std::vector<NXOpen::Drawings::DraftingCurve*> curves = CollectDraftingCurves(baseView);
	{
		std::ostringstream log;
		log << "[HoleNote] drafting curves=" << curves.size();
		HoleNoteDebugLog(log.str());
	}
	if (curves.empty())
	{
		return false;
	}

	HoleCoordinateExtents extents = CalculateHoleCoordinateExtents(baseView, curves);
	if (!extents.valid)
	{
		HoleNoteDebugLog("[HoleNote] extents invalid");
		return false;
	}
	{
		std::ostringstream log;
		log << "[HoleNote] extents min=(" << extents.minX << "," << extents.minY
			<< ") max=(" << extents.maxX << "," << extents.maxY << ")";
		HoleNoteDebugLog(log.str());
	}

	std::unordered_map<std::string, FlatPatternHoleNoteGroup> groups;
	std::unordered_set<std::string> visitedCircles;
	std::unordered_set<std::string> visitedCounterbores;
	int fullCircleCount = 0;
	for (size_t i = 0; i < curves.size(); ++i)
	{
		double center[2] = { 0.0, 0.0 };
		double radius = 0.0;
		if (!TryGetDraftingFullCircleDrawingData(baseView, curves[i], center, radius) || radius <= 1e-6)
		{
			continue;
		}

		std::ostringstream circleKeyStream;
		circleKeyStream << static_cast<long long>(std::llround(center[0] * 1000.0))
			<< "|" << static_cast<long long>(std::llround(center[1] * 1000.0))
			<< "|" << static_cast<long long>(std::llround(radius * 2000.0));
		const std::string circleKey = circleKeyStream.str();
		if (!visitedCircles.insert(circleKey).second)
		{
			continue;
		}
		++fullCircleCount;

		{
			std::ostringstream log;
			log << "[HoleNote] circle tag=" << curves[i]->Tag()
				<< " center=(" << center[0] << "," << center[1] << ")"
				<< " radius=" << radius
				<< " drawingDia=" << radius * 2.0;
			HoleNoteDebugLog(log.str());
		}
		LogHoleNoteParentDetails(curves[i]);

		NXOpen::Face* cylinderFace = NULL;
		double cylinderDiameter = 0.0;
		if (!TryResolveHoleCylinderFace(curves[i], radius * 2.0, cylinderFace, cylinderDiameter) || cylinderFace == NULL)
		{
			HoleNoteDebugLog("[HoleNote]   skip: cylinder face not resolved");
			continue;
		}
		{
			std::ostringstream log;
			log << "[HoleNote]   cylinderFace=" << cylinderFace->Tag()
				<< " cylinderDia=" << cylinderDiameter;
			HoleNoteDebugLog(log.str());
		}

		std::string typeText;
		std::string specText;
		const std::vector<NXOpen::NXString> typeTitles = {
			NXOpen::NXString("\xE7\xB1\xBB\xE5\x9E\x8B", NXOpen::NXString::UTF8),
			NXOpen::NXString("type"),
			NXOpen::NXString("Type"),
			NXOpen::NXString("TYPE")
		};
		const std::vector<NXOpen::NXString> specTitles = {
			NXOpen::NXString("\xE8\xA7\x84\xE6\xA0\xBC", NXOpen::NXString::UTF8),
			NXOpen::NXString("spec"),
			NXOpen::NXString("Spec"),
			NXOpen::NXString("SPEC"),
			NXOpen::NXString("\xE5\x9E\x8B\xE5\x8F\xB7", NXOpen::NXString::UTF8)
		};
		const bool hasTypeAttribute = TryGetStringUserAttribute(cylinderFace, typeTitles, typeText);
		bool isCounterbore = HoleNoteTextContainsCounterbore(typeText);
		bool hasSpecAttribute = false;
		if (!isCounterbore)
		{
			hasSpecAttribute = TryGetStringUserAttribute(cylinderFace, specTitles, specText);
		}
		const bool hasHoleAttributes = hasTypeAttribute || hasSpecAttribute;
		NXOpen::Drawings::DraftingCurve* effectiveCurve = curves[i];
		double effectiveCenter[2] = { center[0], center[1] };
		double effectiveRadius = radius;
		double groupDiameter = cylinderDiameter;
		if (isCounterbore)
		{
			NXOpen::Drawings::DraftingCurve* outerCurve = NULL;
			double outerCenter[2] = { center[0], center[1] };
			double outerRadius = radius;
			if (TryFindOuterConcentricHoleNoteCircle(baseView, curves, center, radius, outerCurve, outerCenter, outerRadius) &&
				outerCurve != NULL)
			{
				effectiveCurve = outerCurve;
				effectiveCenter[0] = outerCenter[0];
				effectiveCenter[1] = outerCenter[1];
				effectiveRadius = outerRadius;
				NXOpen::Face* outerCylinderFace = NULL;
				double outerCylinderDiameter = 0.0;
				if (TryResolveHoleCylinderFace(effectiveCurve, effectiveRadius * 2.0, outerCylinderFace, outerCylinderDiameter) &&
					outerCylinderFace != NULL && outerCylinderDiameter > 1.0e-6)
				{
					cylinderDiameter = outerCylinderDiameter;
					cylinderFace = outerCylinderFace;
				}
			}

			{
				std::ostringstream counterboreKey;
				counterboreKey << static_cast<long long>(std::llround(effectiveCenter[0] * 1000.0))
					<< ":" << static_cast<long long>(std::llround(effectiveCenter[1] * 1000.0))
					<< ":" << static_cast<long long>(std::llround(effectiveRadius * 2000.0));
				if (!visitedCounterbores.insert(counterboreKey.str()).second)
				{
					HoleNoteDebugLog("[HoleNote]   skip: duplicate counterbore candidate");
					continue;
				}
			}

			std::string sideText = ResolveCounterboreSideTextFromFlatPattern(effectiveCurve);
			if (sideText.empty())
			{
				sideText = ResolveCounterboreSideText(cylinderFace);
			}
			specText = BuildCounterboreSpecText(cylinderDiameter, sideText);
			const int screwSize = BuildCounterboreScrewSize(cylinderDiameter);
			if (screwSize > 0)
			{
				groupDiameter = static_cast<double>(screwSize);
			}
		}
		else if (!hasHoleAttributes)
		{
			NXOpen::Drawings::DraftingCurve* outerCurve = NULL;
			double outerCenter[2] = { center[0], center[1] };
			double outerRadius = radius;
			const bool hasOuterConcentricCircle =
				TryFindOuterConcentricHoleNoteCircle(baseView, curves, center, radius, outerCurve, outerCenter, outerRadius) &&
				outerCurve != NULL &&
				outerRadius > radius + 0.02;
			if (hasOuterConcentricCircle)
			{
				effectiveCurve = outerCurve;
				effectiveCenter[0] = outerCenter[0];
				effectiveCenter[1] = outerCenter[1];
				effectiveRadius = outerRadius;
				NXOpen::Face* outerCylinderFace = NULL;
				double outerCylinderDiameter = 0.0;
				if (TryResolveHoleCylinderFace(effectiveCurve, effectiveRadius * 2.0, outerCylinderFace, outerCylinderDiameter) &&
					outerCylinderFace != NULL && outerCylinderDiameter > 1.0e-6)
				{
					cylinderDiameter = outerCylinderDiameter;
					cylinderFace = outerCylinderFace;
				}

				std::ostringstream counterboreKey;
				counterboreKey << static_cast<long long>(std::llround(effectiveCenter[0] * 1000.0))
					<< ":" << static_cast<long long>(std::llround(effectiveCenter[1] * 1000.0))
					<< ":" << static_cast<long long>(std::llround(effectiveRadius * 2000.0));
				if (!visitedCounterbores.insert(counterboreKey.str()).second)
				{
					HoleNoteDebugLog("[HoleNote]   skip: duplicate auto counterbore candidate");
					continue;
				}

				std::string sideText = ResolveCounterboreSideTextFromFlatPattern(effectiveCurve);
				if (sideText.empty())
				{
					sideText = ResolveCounterboreSideText(cylinderFace);
				}
				typeText = "\xE6\xB2\x89\xE5\xAD\x94";
				specText = BuildCounterboreSpecText(cylinderDiameter, sideText);
				const int screwSize = BuildCounterboreScrewSize(cylinderDiameter);
				if (screwSize > 0)
				{
					groupDiameter = static_cast<double>(screwSize);
				}
				isCounterbore = true;
				HoleNoteDebugLog("[HoleNote]   fallback: concentric circles -> counterbore");
			}
			else
			{
				std::string ruleNoteText;
				std::string ruleTypeText;
				if (TryBuildMatchedRuleHoleNoteText(cylinderDiameter, ruleTypeText, ruleNoteText))
				{
					typeText = ruleTypeText;
					specText = ruleNoteText;
					HoleNoteDebugLog("[HoleNote]   fallback: bottom hole rule -> matched note");
				}
				else
				{
					typeText = "\xE5\xAD\x94";
					specText.clear();
					HoleNoteDebugLog("[HoleNote]   fallback: plain diameter note");
				}
			}
		}
		{
			std::ostringstream log;
			log << "[HoleNote]   attrs type='" << typeText << "' spec='" << specText << "'";
			HoleNoteDebugLog(log.str());
		}
		if (typeText.empty() && specText.empty())
		{
			HoleNoteDebugLog("[HoleNote]   skip: no type/spec/fallback note");
			continue;
		}

		const std::string groupKey = BuildHoleAttributeGroupKey(groupDiameter, typeText, specText);
		std::unordered_map<std::string, FlatPatternHoleNoteGroup>::iterator groupIt = groups.find(groupKey);
		if (groupIt == groups.end())
		{
			FlatPatternHoleNoteGroup group;
			group.representativeCurve = effectiveCurve;
			group.drawingCenter = NXOpen::Point3d(effectiveCenter[0], effectiveCenter[1], 0.0);
			group.drawingRadius = effectiveRadius;
			group.diameter = cylinderDiameter;
			group.typeText = typeText;
			group.specText = specText;
			group.count = 1;
			group.markerCenters.push_back(group.drawingCenter);
			groups.insert(std::make_pair(groupKey, group));
		}
		else
		{
			groupIt->second.count += 1;
			groupIt->second.markerCenters.push_back(NXOpen::Point3d(effectiveCenter[0], effectiveCenter[1], 0.0));
		}
	}

	std::vector<FlatPatternHoleNoteGroup> orderedGroups;
	orderedGroups.reserve(groups.size());
	for (std::unordered_map<std::string, FlatPatternHoleNoteGroup>::const_iterator it = groups.begin(); it != groups.end(); ++it)
	{
		orderedGroups.push_back(it->second);
	}

	std::sort(
		orderedGroups.begin(),
		orderedGroups.end(),
		[](const FlatPatternHoleNoteGroup& left, const FlatPatternHoleNoteGroup& right)
		{
			if (std::fabs(left.drawingCenter.Y - right.drawingCenter.Y) > 0.001)
			{
				return left.drawingCenter.Y > right.drawingCenter.Y;
			}
			return left.drawingCenter.X < right.drawingCenter.X;
		});

	bool createdAny = false;
	{
		std::ostringstream log;
		log << "[HoleNote] fullCircles=" << fullCircleCount << " groups=" << orderedGroups.size();
	HoleNoteDebugLog(log.str());
	}
	std::string currentMarker = markerStart;
	for (size_t i = 0; i < orderedGroups.size(); ++i)
	{
		std::string noteText = BuildHoleAttributeNoteText(orderedGroups[i]);
		const bool createHoleDiameter =
			HoleNoteTextIsPlainHole(orderedGroups[i].typeText) || !noteText.empty();
		std::string groupMarker;
		if (!markerStart.empty() && orderedGroups.size() > 1 && orderedGroups[i].count > 1)
		{
			groupMarker = currentMarker;
		}
		if (createHoleDiameter)
		{
			{
				const NXOpen::Point3d notePoint = ComputeHoleAttributeNotePoint(extents, orderedGroups[i], static_cast<int>(i));
				const bool created = CreateFlatPatternHoleAttributeDimension(
					baseView,
					orderedGroups[i].representativeCurve,
					notePoint,
					noteText);
				{
					std::ostringstream log;
					log << "[HoleNote] create hole dimension text='" << noteText
						<< "' point=(" << notePoint.X << "," << notePoint.Y << ")"
						<< " result=" << (created ? "ok" : "failed");
					HoleNoteDebugLog(log.str());
				}
				createdAny = created || createdAny;
			}
		}

		if (!groupMarker.empty())
		{
			for (size_t markerIndex = 0; markerIndex < orderedGroups[i].markerCenters.size(); ++markerIndex)
			{
				NXOpen::Point3d markerPoint = orderedGroups[i].markerCenters[markerIndex];
				const double markerOffset = std::max(1.2, orderedGroups[i].drawingRadius + 0.8);
				markerPoint.X += markerOffset;
				markerPoint.Y += markerOffset;
				const bool markerCreated = CreateFlatPatternHoleMarkerNote(baseView, markerPoint, groupMarker);
				createdAny = markerCreated || createdAny;
			}
			currentMarker = BuildNextHoleMarkerCode(currentMarker);
		}
	}
	HoleNoteDebugLog(std::string("[HoleNote] createdAny=") + (createdAny ? "true" : "false"));

	return createdAny;
}
