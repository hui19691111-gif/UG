#include "ZiDonBiaoZhuDimension.hpp"

#include <NXOpen/Annotations_BaseAngularDimension.hxx>
#include <NXOpen/Annotations_BaseAngularDimensionBuilder.hxx>
#include <NXOpen/Annotations_MajorAngularDimensionBuilder.hxx>
#include <NXOpen/Annotations_MinorAngularDimensionBuilder.hxx>
#include <NXOpen/Annotations_RadialDimensionBuilder.hxx>
#include <uf_curve.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
struct Segment2d
{
	double x1;
	double y1;
	double x2;
	double y2;
};

struct OuterRadiusFaceGroups
{
	std::vector<NXOpen::Face*> rightAngleFaces;
	std::vector<NXOpen::Face*> notRightAngleFaces;
	std::vector<NXOpen::Face*> straightAngleFaces;
};

std::unordered_map<std::string, int> g_dimensionLayoutLayers;
std::unordered_map<tag_t, std::vector<Segment2d>> g_dimensionLayoutObstacles;

NXOpen::Part* CurrentWorkPart()
{
	NXOpen::Session* session = NXOpen::Session::GetSession();
	return session == NULL ? NULL : session->Parts()->Work();
}

std::vector<NXOpen::Drawings::DraftingCurve*> CollectDraftingCurves(NXOpen::Drawings::BaseView* baseView)
{
	std::vector<NXOpen::Drawings::DraftingCurve*> curves;
	NXOpen::Drawings::DraftingView* draftingView = dynamic_cast<NXOpen::Drawings::DraftingView*>(baseView);
	if (draftingView == NULL)
	{
		return curves;
	}

	NXOpen::Drawings::DraftingBodyCollection* bodyCollection = draftingView->DraftingBodies();
	for (NXOpen::Drawings::DraftingBodyCollection::iterator bodyIt = bodyCollection->begin(); bodyIt != bodyCollection->end(); ++bodyIt)
	{
		NXOpen::Drawings::DraftingBody* draftingBody = *bodyIt;
		if (draftingBody == NULL)
		{
			continue;
		}

		NXOpen::Drawings::DraftingCurveCollection* curveCollection = draftingBody->DraftingCurves();
		for (NXOpen::Drawings::DraftingCurveCollection::iterator curveIt = curveCollection->begin(); curveIt != curveCollection->end(); ++curveIt)
		{
			if (*curveIt != NULL)
			{
				curves.push_back(*curveIt);
			}
		}
	}
	return curves;
}

double MeasureMinimumDistance(NXOpen::Part* part, NXOpen::NXObject* firstObject, NXOpen::NXObject* secondObject)
{
	if (part == NULL || firstObject == NULL || secondObject == NULL)
	{
		return 999999.0;
	}

	NXOpen::MeasureDistance* measureDistance =
		part->MeasureManager()->NewDistance(NULL, NXOpen::MeasureManager::MeasureTypeMinimum, firstObject, secondObject);
	return measureDistance == NULL ? 999999.0 : measureDistance->Value();
}

bool MapModelPointToDrawing(
	NXOpen::Drawings::BaseView* baseView,
	const double modelPoint[3],
	double drawingPoint[2])
{
	if (baseView == NULL || modelPoint == NULL || drawingPoint == NULL)
	{
		return false;
	}

	double mutableModelPoint[3] = { modelPoint[0], modelPoint[1], modelPoint[2] };
	return UF_VIEW_map_model_to_drawing(baseView->Tag(), mutableModelPoint, drawingPoint) == 0;
}

bool AreModelPointsCoincidentInDrawing(
	NXOpen::Drawings::BaseView* baseView,
	const double firstPoint[3],
	const double secondPoint[3],
	double tolerance)
{
	double firstDrawingPoint[2] = { 0.0, 0.0 };
	double secondDrawingPoint[2] = { 0.0, 0.0 };
	if (!MapModelPointToDrawing(baseView, firstPoint, firstDrawingPoint) ||
		!MapModelPointToDrawing(baseView, secondPoint, secondDrawingPoint))
	{
		return false;
	}

	return std::fabs(firstDrawingPoint[0] - secondDrawingPoint[0]) < tolerance &&
		std::fabs(firstDrawingPoint[1] - secondDrawingPoint[1]) < tolerance;
}

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

double PointToSegmentDistance(double px, double py, const Segment2d& segment)
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

bool BoundingBoxesOverlap(const Segment2d& first, const Segment2d& second, double tolerance)
{
	const double firstMinX = std::min(first.x1, first.x2) - tolerance;
	const double firstMaxX = std::max(first.x1, first.x2) + tolerance;
	const double firstMinY = std::min(first.y1, first.y2) - tolerance;
	const double firstMaxY = std::max(first.y1, first.y2) + tolerance;
	const double secondMinX = std::min(second.x1, second.x2) - tolerance;
	const double secondMaxX = std::max(second.x1, second.x2) + tolerance;
	const double secondMinY = std::min(second.y1, second.y2) - tolerance;
	const double secondMaxY = std::max(second.y1, second.y2) + tolerance;
	return !(firstMaxX < secondMinX || secondMaxX < firstMinX || firstMaxY < secondMinY || secondMaxY < firstMinY);
}

bool SegmentsIntersect(const Segment2d& first, const Segment2d& second, double tolerance)
{
	if (!BoundingBoxesOverlap(first, second, tolerance))
	{
		return false;
	}

	const double c1 = Cross2d(first.x1, first.y1, first.x2, first.y2, second.x1, second.y1);
	const double c2 = Cross2d(first.x1, first.y1, first.x2, first.y2, second.x2, second.y2);
	const double c3 = Cross2d(second.x1, second.y1, second.x2, second.y2, first.x1, first.y1);
	const double c4 = Cross2d(second.x1, second.y1, second.x2, second.y2, first.x2, first.y2);
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
		if (parents[i] == NULL)
		{
			continue;
		}

		const tag_t parentTag = parents[i]->Tag();
		if (parentTag == faceTag || edgeTags.find(parentTag) != edgeTags.end())
		{
			return true;
		}
	}
	return false;
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

	originPoint = NXOpen::Point3d(
		intersection[0] + bisectorX * 9.0,
		intersection[1] + bisectorY * 9.0,
		0.0);
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

	double bestScore = 1e18;
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

		const double score =
			std::fabs(curveCenter[0] - drawingPoint[0]) +
			std::fabs(curveCenter[1] - drawingPoint[1]);
		if (score < bestScore)
		{
			bestScore = score;
			draftingCurve = candidate;
		}
	}

	if (draftingCurve == NULL)
	{
		return false;
	}

	const double dx = drawingPoint[0] - centerX;
	const double dy = drawingPoint[1] - centerY;
	if (std::fabs(dx) >= std::fabs(dy))
	{
		tangentPoint = NXOpen::Point3d(dx >= 0.0 ? 999999.0 : -999999.0, drawingPoint[1], 0.0);
	}
	else
	{
		tangentPoint = NXOpen::Point3d(drawingPoint[0], dy >= 0.0 ? 999999.0 : -999999.0, 0.0);
	}
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

	const double offset = AcquireLayeredOffset(baseView, drawingPoint, centerX, centerY, baseOffset, layerGap);
	const int defaultXDirection = drawingPoint[0] >= centerX ? 1 : -1;
	const int defaultYDirection = drawingPoint[1] >= centerY ? 1 : -1;
	const int oppositeXDirection = -defaultXDirection;
	const int oppositeYDirection = -defaultYDirection;
	std::vector<NXOpen::Point3d> candidates;
	candidates.push_back(CreateCandidateOrigin(drawingPoint, defaultXDirection, defaultYDirection, offset, offset));
	candidates.push_back(CreateCandidateOrigin(drawingPoint, defaultXDirection, defaultYDirection, offset + 2.0, std::max(2.0, offset - 2.0)));
	candidates.push_back(CreateCandidateOrigin(drawingPoint, defaultXDirection, defaultYDirection, std::max(2.0, offset - 2.0), offset + 2.0));
	candidates.push_back(CreateCandidateOrigin(drawingPoint, defaultXDirection, defaultYDirection, offset + 4.0, offset));
	candidates.push_back(CreateCandidateOrigin(drawingPoint, defaultXDirection, defaultYDirection, offset, offset + 4.0));
	candidates.push_back(CreateCandidateOrigin(drawingPoint, oppositeXDirection, defaultYDirection, offset + 2.0, offset));
	candidates.push_back(CreateCandidateOrigin(drawingPoint, defaultXDirection, oppositeYDirection, offset, offset + 2.0));
	candidates.push_back(CreateCandidateOrigin(drawingPoint, oppositeXDirection, oppositeYDirection, offset + 2.0, offset + 2.0));
	candidates.push_back(CreateCandidateOrigin(drawingPoint, defaultXDirection, 0, offset + 6.0, 0.0));
	candidates.push_back(CreateCandidateOrigin(drawingPoint, 0, defaultYDirection, 0.0, offset + 6.0));
	candidates.push_back(CreateCandidateOrigin(drawingPoint, oppositeXDirection, 0, offset + 6.0, 0.0));
	candidates.push_back(CreateCandidateOrigin(drawingPoint, 0, oppositeYDirection, 0.0, offset + 6.0));

	double bestScore = 1e18;
	NXOpen::Point3d bestOrigin = candidates[0];
	bool foundNonIntersectingCandidate = false;
	for (size_t i = 0; i < candidates.size(); ++i)
	{
		const bool intersectsObstacle = CandidateRouteIntersectsObstacle(baseView, drawingPoint, candidates[i]);
		const double dx = candidates[i].X - drawingPoint[0];
		const double dy = candidates[i].Y - drawingPoint[1];
		const double travelPenalty = std::sqrt(dx * dx + dy * dy) * 0.2;
		const double obstaclePenalty = ComputeObstaclePenalty(baseView, drawingPoint, candidates[i]);
		if (foundNonIntersectingCandidate && intersectsObstacle)
		{
			continue;
		}

		double score = travelPenalty + obstaclePenalty + static_cast<double>(i) * 0.5;
		if (intersectsObstacle)
		{
			score += 1000.0;
		}
		if (score < bestScore)
		{
			bestScore = score;
			bestOrigin = candidates[i];
			foundNonIntersectingCandidate = !intersectsObstacle;
		}
	}
	return bestOrigin;
}

NXOpen::Point3d BuildOffsetOriginFromModelPoint(
	NXOpen::Drawings::BaseView* baseView,
	const double* modelPoint,
	double centerX,
	double centerY,
	double offset)
{
	if (baseView == NULL || modelPoint == NULL)
	{
		return NXOpen::Point3d(0.0, 0.0, 0.0);
	}

	double drawingPoint[2] = { 0.0, 0.0 };
	double mutableModelPoint[3] = { modelPoint[0], modelPoint[1], modelPoint[2] };
	UF_VIEW_map_model_to_drawing(baseView->Tag(), mutableModelPoint, drawingPoint);
	const double layerGap = offset >= 8.0 ? 4.0 : 3.0;
	return BuildOffsetOrigin(baseView, drawingPoint, centerX, centerY, offset, layerGap);
}

NXOpen::Annotations::RapidDimensionBuilder* CreateRapidDimensionBuilder()
{
	NXOpen::Part* part = CurrentWorkPart();
	if (part == NULL)
	{
		return NULL;
	}

	NXOpen::Annotations::Dimension* nullDimension(NULL);
	return part->Dimensions()->CreateRapidDimensionBuilder(nullDimension);
}

void ApplyDefaultRapidDimensionStyle(NXOpen::Annotations::RapidDimensionBuilder* builder)
{
	if (builder == NULL)
	{
		return;
	}
	builder->Style()->DimensionStyle()->SetTextCentered(true);
	builder->Style()->DimensionStyle()->SetNarrowDisplayType(NXOpen::Annotations::NarrowDisplayOptionNone);
}

NXOpen::NXObject* CommitAndDestroyRapidDimension(NXOpen::Annotations::RapidDimensionBuilder* builder)
{
	if (builder == NULL)
	{
		return NULL;
	}
	NXOpen::NXObject* object = builder->Commit();
	builder->Destroy();
	return object;
}

void ResetDimensionLayoutState()
{
	g_dimensionLayoutLayers.clear();
	g_dimensionLayoutObstacles.clear();
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

		UF_EVAL_p_t evaluator = NULL;
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

bool TryGetOuterTangentAssociativityForFace(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Face* face,
	double centerX,
	double centerY,
	NXOpen::Drawings::DraftingCurve*& draftingCurve,
	NXOpen::Point3d& tangentPoint)
{
	return TryBuildOuterTangentAssociativityForFace(baseView, face, centerX, centerY, draftingCurve, tangentPoint);
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
	NXOpen::Part* part = CurrentWorkPart();
	if (part == NULL || baseView == NULL || firstCurve == NULL || secondCurve == NULL || modelPoint == NULL || firstCurve == secondCurve)
	{
		return false;
	}

	NXOpen::Annotations::BaseAngularDimension* nullAngularDimension(NULL);
	NXOpen::Annotations::BaseAngularDimensionBuilder* builder = allowSupplementaryAngle
		? static_cast<NXOpen::Annotations::BaseAngularDimensionBuilder*>(
			part->Dimensions()->CreateMajorAngularDimensionBuilder(nullAngularDimension))
		: static_cast<NXOpen::Annotations::BaseAngularDimensionBuilder*>(
			part->Dimensions()->CreateMinorAngularDimensionBuilder(nullAngularDimension));
	if (builder == NULL)
	{
		return false;
	}

	NXOpen::Point3d firstAnchorPoint(0.0, 0.0, 0.0);
	NXOpen::Point3d secondAnchorPoint(0.0, 0.0, 0.0);
	NXOpen::Point3d originPoint(0.0, 0.0, 0.0);
	const bool hasInsidePlacement =
		TryBuildInsideAnglePlacement(baseView, firstCurve, secondCurve, firstAnchorPoint, secondAnchorPoint, originPoint);
	if (!hasInsidePlacement &&
		(!TryGetLineCurveAnchorPoint(firstCurve, firstAnchorPoint) ||
			!TryGetLineCurveAnchorPoint(secondCurve, secondAnchorPoint)))
	{
		builder->Destroy();
		return false;
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
	builder->Origin()->Origin()->SetValue(
		NULL,
		nullView,
		hasInsidePlacement ? originPoint : BuildOffsetOriginFromModelPoint(baseView, modelPoint, centerX, centerY, 7.0));

	NXOpen::NXObject* object = builder->Commit();
	builder->Destroy();
	return object != NULL;
}

NXOpen::Annotations::IntersectionSymbol* CreateIntersectionSymbol(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Drawings::DraftingCurve* firstCurve,
	NXOpen::Drawings::DraftingCurve* secondCurve)
{
	NXOpen::Part* part = CurrentWorkPart();
	if (part == NULL || baseView == NULL || firstCurve == NULL || secondCurve == NULL)
	{
		return NULL;
	}

	NXOpen::Point3d origin(0.0, 0.0, 0.0);
	NXOpen::Annotations::IntersectionSymbol* nullSymbol(NULL);
	NXOpen::Annotations::IntersectionSymbolBuilder* builder =
		part->Annotations()->IntersectionSymbols()->CreateIntersectionSymbolBuilder(nullSymbol);
	builder->SetExtension(0.20000000000000001);
	builder->SetColor(part->Colors()->Find("Black"));
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
	double centerY)
{
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

	UF_EVAL_p_t evaluator = NULL;
	logical isLine = false;
	if (UF_EVAL_initialize(curve->Tag(), &evaluator) == 0)
	{
		UF_EVAL_is_line(evaluator, &isLine);
		UF_EVAL_free(evaluator);
	}
	builder->Measurement()->SetMethod(
		isLine
		? NXOpen::Annotations::DimensionMeasurementBuilder::MeasurementMethodPerpendicular
		: NXOpen::Annotations::DimensionMeasurementBuilder::MeasurementMethodPointToPoint);
	builder->FirstAssociativity()->SetValue(
		NXOpen::InferSnapType::SnapTypeEnd, curve, baseView, point, NULL, nullView, point);
	builder->SecondAssociativity()->SetValue(
		NXOpen::InferSnapType::SnapTypeExist, symbol, baseView, point, NULL, nullView, point);
	ApplyDefaultRapidDimensionStyle(builder);
	builder->Origin()->Origin()->SetValue(
		NULL,
		nullView,
		BuildOffsetOrigin(baseView, drawingPoint, centerX, centerY, 8.0, 4.0));
	CommitAndDestroyRapidDimension(builder);
	return true;
}

bool CreateSymbolToSymbolDimensionFromModelPoint(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Annotations::IntersectionSymbol* firstSymbol,
	NXOpen::Annotations::IntersectionSymbol* secondSymbol,
	const double modelPoint[3],
	double centerX,
	double centerY)
{
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
	builder->Origin()->Origin()->SetValue(
		NULL,
		nullView,
		BuildOffsetOriginFromModelPoint(baseView, modelPoint, centerX, centerY, 5.0));
	CommitAndDestroyRapidDimension(builder);
	return true;
}

bool CreateFaceToSymbolDimensionFromModelPoint(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Face* face,
	NXOpen::Annotations::IntersectionSymbol* symbol,
	const double modelPoint[3],
	double centerX,
	double centerY)
{
	if (baseView == NULL || face == NULL || symbol == NULL || modelPoint == NULL)
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
	NXOpen::Drawings::DraftingCurve* tangentCurve = NULL;
	NXOpen::Point3d tangentPoint(0.0, 0.0, 0.0);
	if (TryGetOuterTangentAssociativityForFace(baseView, face, centerX, centerY, tangentCurve, tangentPoint))
	{
		builder->FirstAssociativity()->SetValue(
			NXOpen::InferSnapType::SnapTypeDrfTangent, tangentCurve, baseView, tangentPoint, NULL, nullView, point);
	}
	else
	{
		builder->FirstAssociativity()->SetValue(face, baseView, point);
	}
	builder->SecondAssociativity()->SetValue(
		NXOpen::InferSnapType::SnapTypeExist, symbol, baseView, point, NULL, nullView, point);
	builder->Origin()->Origin()->SetValue(
		NULL,
		nullView,
		BuildOffsetOriginFromModelPoint(baseView, modelPoint, centerX, centerY, 5.0));
	CommitAndDestroyRapidDimension(builder);
	return true;
}

bool CreateFaceToObjectDimensionFromModelPoint(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Face* firstFace,
	NXOpen::DisplayableObject* secondObject,
	const double modelPoint[3],
	double centerX,
	double centerY)
{
	if (baseView == NULL || firstFace == NULL || secondObject == NULL || modelPoint == NULL)
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
	NXOpen::Drawings::DraftingCurve* tangentCurve = NULL;
	NXOpen::Point3d tangentPoint(0.0, 0.0, 0.0);
	if (TryGetOuterTangentAssociativityForFace(baseView, firstFace, centerX, centerY, tangentCurve, tangentPoint))
	{
		builder->FirstAssociativity()->SetValue(
			NXOpen::InferSnapType::SnapTypeDrfTangent, tangentCurve, baseView, tangentPoint, NULL, nullView, point);
	}
	else
	{
		builder->FirstAssociativity()->SetValue(firstFace, baseView, point);
	}

	NXOpen::Face* secondFace = dynamic_cast<NXOpen::Face*>(secondObject);
	if (secondFace != NULL &&
		TryGetOuterTangentAssociativityForFace(baseView, secondFace, centerX, centerY, tangentCurve, tangentPoint))
	{
		builder->SecondAssociativity()->SetValue(
			NXOpen::InferSnapType::SnapTypeDrfTangent, tangentCurve, baseView, tangentPoint, NULL, nullView, point);
	}
	else
	{
		builder->SecondAssociativity()->SetValue(
			NXOpen::InferSnapType::SnapTypeExist, secondObject, baseView, point, NULL, nullView, point);
	}
	builder->Origin()->Origin()->SetValue(
		NULL,
		nullView,
		BuildOffsetOriginFromModelPoint(baseView, modelPoint, centerX, centerY, 5.0));
	CommitAndDestroyRapidDimension(builder);
	return true;
}

bool CreateRadialDimensionAtDrawingPoint(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Face* face,
	const double drawingPoint[2])
{
	NXOpen::Part* part = CurrentWorkPart();
	if (part == NULL || baseView == NULL || face == NULL || drawingPoint == NULL)
	{
		return false;
	}

	NXOpen::Annotations::Dimension* nullDimension(NULL);
	NXOpen::Annotations::RadialDimensionBuilder* builder =
		part->Dimensions()->CreateRadialDimensionBuilder(nullDimension);
	if (builder == NULL)
	{
		return false;
	}

	NXOpen::Point3d point(drawingPoint[0], drawingPoint[1], 0.0);
	builder->FirstAssociativity()->SetValue(face, baseView, point);
	NXOpen::NXObject* object = builder->Commit();
	builder->Destroy();
	return object != NULL;
}

std::vector<NXOpen::Face*> CollectAdjacentFaces(
	NXOpen::Part* part,
	NXOpen::NXObject* target,
	const std::vector<NXOpen::Face*>& candidates,
	double tolerance)
{
	std::vector<NXOpen::Face*> adjacentFaces;
	if (part == NULL || target == NULL)
	{
		return adjacentFaces;
	}

	for (size_t i = 0; i < candidates.size(); i++)
	{
		if (candidates[i] == NULL)
		{
			continue;
		}

		double distance = MeasureMinimumDistance(part, target, candidates[i]);
		if (distance < tolerance)
		{
			adjacentFaces.push_back(candidates[i]);
		}
	}
	return adjacentFaces;
}

bool TryPromoteUniqueDimensionFace(
	std::vector<NXOpen::Face*>& firstFaces,
	std::vector<NXOpen::Face*>& secondFaces,
	std::vector<NXOpen::Face*>& dimensionFaces)
{
	for (size_t i = 0; i < firstFaces.size(); i++)
	{
		for (size_t j = 0; j < secondFaces.size();)
		{
			if (firstFaces[i] == NULL || secondFaces[j] == NULL || firstFaces[i] != secondFaces[j])
			{
				j++;
				continue;
			}

			firstFaces.erase(firstFaces.begin() + i);
			if (firstFaces.empty())
			{
				return false;
			}

			dimensionFaces.clear();
			dimensionFaces.push_back(firstFaces[0]);
			firstFaces.push_back(secondFaces[j]);
			secondFaces.erase(secondFaces.begin() + j);
			return true;
		}
	}
	return false;
}

OuterRadiusFaceGroups ClassifyOuterRadiusFacesForView(
	NXOpen::Part* part,
	const std::vector<NXOpen::Face*>& outerRadiusFaces,
	NXOpen::Drawings::BaseView* baseView)
{
	OuterRadiusFaceGroups groups;
	if (part == NULL || baseView == NULL)
	{
		return groups;
	}

	for (size_t i = 0; i < outerRadiusFaces.size(); i++)
	{
		if (outerRadiusFaces[i] == NULL)
		{
			continue;
		}

		double point[3], dir[3], box[6], radius, radData;
		int type, normDir;
		UF_MODL_ask_face_data(outerRadiusFaces[i]->Tag(), &type, point, dir, box, &radius, &radData, &normDir);

		std::vector<NXOpen::Edge*> projectedLineEdges;
		std::vector<NXOpen::Edge*> outerEdges = outerRadiusFaces[i]->GetEdges();
		for (size_t edgeIndex = 0; edgeIndex < outerEdges.size(); edgeIndex++)
		{
			UF_EVAL_p_t evaluator = NULL;
			logical isLine = false;
			if (UF_EVAL_initialize(outerEdges[edgeIndex]->Tag(), &evaluator) != 0)
			{
				continue;
			}
			UF_EVAL_is_line(evaluator, &isLine);
			if (!isLine)
			{
				UF_EVAL_free(evaluator);
				continue;
			}

			UF_EVAL_line_t lineCoords;
			UF_EVAL_ask_line(evaluator, &lineCoords);
			UF_EVAL_free(evaluator);
			double startPoint[3] = { lineCoords.start[0], lineCoords.start[1], lineCoords.start[2] };
			double endPoint[3] = { lineCoords.end[0], lineCoords.end[1], lineCoords.end[2] };
			if (AreModelPointsCoincidentInDrawing(baseView, startPoint, endPoint, 0.01))
			{
				projectedLineEdges.push_back(outerEdges[edgeIndex]);
			}
		}

		if (projectedLineEdges.size() < 2 || projectedLineEdges[0] == NULL || projectedLineEdges[1] == NULL)
		{
			continue;
		}

		double projectedDistance = MeasureMinimumDistance(part, projectedLineEdges[0], projectedLineEdges[1]);
		if (std::fabs(projectedDistance - std::sqrt(2 * std::pow(radius, 2))) < 0.001)
		{
			groups.rightAngleFaces.push_back(outerRadiusFaces[i]);
		}
		else if (std::fabs(projectedDistance - radius * 2) > 0.01)
		{
			groups.notRightAngleFaces.push_back(outerRadiusFaces[i]);
		}
		else
		{
			groups.straightAngleFaces.push_back(outerRadiusFaces[i]);
		}
	}
	return groups;
}

void CollectFirstCutDimensionFacesForView(
	NXOpen::Part* part,
	const std::vector<NXOpen::Face*>& firstCutRadiusFaces,
	const std::vector<NXOpen::Face*>& planarFaces,
	const std::vector<NXOpen::Face*>& excludedFaces,
	std::vector<NXOpen::Face*>& firstDimensionFaces)
{
	for (size_t i = 0; i < firstCutRadiusFaces.size(); i++)
	{
		if (i >= excludedFaces.size())
		{
			continue;
		}

		for (size_t j = 0; j < planarFaces.size(); j++)
		{
			double distance = MeasureMinimumDistance(part, planarFaces[j], firstCutRadiusFaces[i]);
			if (distance < 0.01 && planarFaces[j] != excludedFaces[i])
			{
				firstDimensionFaces.push_back(planarFaces[j]);
			}
		}
	}
}

NXOpen::Face* FindInnerRFaceSharingDrawingCenter(
	NXOpen::Drawings::BaseView* baseView,
	NXOpen::Face* outerFace,
	const std::vector<NXOpen::Face*>& innerRadiusFaces)
{
	if (baseView == NULL || outerFace == NULL)
	{
		return NULL;
	}

	double outerPoint[3], dir[3], box[6], radius, radData;
	int type, normDir;
	UF_MODL_ask_face_data(outerFace->Tag(), &type, outerPoint, dir, box, &radius, &radData, &normDir);

	for (size_t i = 0; i < innerRadiusFaces.size(); i++)
	{
		if (innerRadiusFaces[i] == NULL)
		{
			continue;
		}

		double innerPoint[3];
		UF_MODL_ask_face_data(innerRadiusFaces[i]->Tag(), &type, innerPoint, dir, box, &radius, &radData, &normDir);
		if (AreModelPointsCoincidentInDrawing(baseView, outerPoint, innerPoint, 0.01))
		{
			return innerRadiusFaces[i];
		}
	}
	return NULL;
}

std::vector<NXOpen::Annotations::IntersectionSymbol*> CreateNot90IntersectionSymbols(
	const std::vector<NXOpen::Face*>& waiRFaceNot90,
	const std::vector<NXOpen::Face*>& xingLinFace,
	const std::vector<NXOpen::Drawings::DraftingCurve*>& draftingCurves,
	NXOpen::Drawings::BaseView* baseView)
{
	std::vector<NXOpen::Annotations::IntersectionSymbol*> symbols(waiRFaceNot90.size(), NULL);
	NXOpen::Part* part = CurrentWorkPart();
	if (part == NULL || baseView == NULL)
	{
		return symbols;
	}

	for (size_t i = 0; i < waiRFaceNot90.size(); i++)
	{
		std::vector<NXOpen::Drawings::DraftingCurve*> intersectionCurves;
		for (size_t a = 0; a < xingLinFace.size(); a++)
		{
			double distance1 = MeasureMinimumDistance(part, waiRFaceNot90[i], xingLinFace[a]);
			if (distance1 >= 0.01)
			{
				continue;
			}

			double firstLineStart[2] = { 0.0, 0.0 };
			double firstLineEnd[2] = { 0.0, 0.0 };
			bool foundReferenceLine = false;
			std::vector<NXOpen::Edge*> firstEdges = xingLinFace[a]->GetEdges();
			for (size_t b = 0; b < firstEdges.size(); b++)
			{
				UF_EVAL_p_t evaluator = NULL;
				logical isLine = false;
				if (UF_EVAL_initialize(firstEdges[b]->Tag(), &evaluator) != 0)
				{
					continue;
				}
				UF_EVAL_is_line(evaluator, &isLine);
				UF_EVAL_free(evaluator);
				if (!isLine)
				{
					continue;
				}

				double aa[3], bb[3];
				NXOpen::Point3d startPoint;
				NXOpen::Point3d endPoint;
				firstEdges[b]->GetVertices(&startPoint, &endPoint);
				aa[0] = startPoint.X; aa[1] = startPoint.Y; aa[2] = startPoint.Z;
				bb[0] = endPoint.X; bb[1] = endPoint.Y; bb[2] = endPoint.Z;
				double firstDrawing[2], secondDrawing[2];
				if (!MapModelPointToDrawing(baseView, aa, firstDrawing) ||
					!MapModelPointToDrawing(baseView, bb, secondDrawing))
				{
					continue;
				}
				if (std::fabs(firstDrawing[0] - secondDrawing[0]) > 0.01 ||
					std::fabs(firstDrawing[1] - secondDrawing[1]) > 0.01)
				{
					firstLineStart[0] = firstDrawing[0];
					firstLineStart[1] = firstDrawing[1];
					firstLineEnd[0] = secondDrawing[0];
					firstLineEnd[1] = secondDrawing[1];
					foundReferenceLine = true;
					break;
				}
			}
			if (!foundReferenceLine)
			{
				continue;
			}

			for (size_t b = 0; b < draftingCurves.size(); b++)
			{
				UF_EVAL_p_t evaluator = NULL;
				logical isLine = false;
				if (UF_EVAL_initialize(draftingCurves[b]->Tag(), &evaluator) != 0)
				{
					continue;
				}
				UF_EVAL_is_line(evaluator, &isLine);
				UF_EVAL_free(evaluator);
				if (!isLine)
				{
					continue;
				}

				UF_CURVE_line_t lineCoords;
				UF_CURVE_ask_line_data(draftingCurves[b]->Tag(), &lineCoords);
				double pointa1[2];
				double pointa2[2];
				if (!MapModelPointToDrawing(baseView, lineCoords.start_point, pointa1) ||
					!MapModelPointToDrawing(baseView, lineCoords.end_point, pointa2))
				{
					continue;
				}
				const bool sameDirection =
					std::fabs(firstLineStart[0] - pointa1[0]) < 0.001 &&
					std::fabs(firstLineStart[1] - pointa1[1]) < 0.001 &&
					std::fabs(firstLineEnd[0] - pointa2[0]) < 0.001 &&
					std::fabs(firstLineEnd[1] - pointa2[1]) < 0.001;
				const bool oppositeDirection =
					std::fabs(firstLineStart[0] - pointa2[0]) < 0.001 &&
					std::fabs(firstLineStart[1] - pointa2[1]) < 0.001 &&
					std::fabs(firstLineEnd[0] - pointa1[0]) < 0.001 &&
					std::fabs(firstLineEnd[1] - pointa1[1]) < 0.001;
				if (sameDirection || oppositeDirection)
				{
					intersectionCurves.push_back(draftingCurves[b]);
					break;
				}
			}
		}

		if (intersectionCurves.size() < 2 || intersectionCurves[0] == NULL || intersectionCurves[1] == NULL)
		{
			continue;
		}

		double point1[3], dir[3], box[6], radius1, radData;
		int type1, normDir;
		UF_MODL_ask_face_data(waiRFaceNot90[i]->Tag(), &type1, point1, dir, box, &radius1, &radData, &normDir);
		CreateCurvePairAngleDimensionFromModelPoint(
			baseView,
			intersectionCurves[0],
			intersectionCurves[1],
			point1,
			0.0,
			0.0,
			false);

		NXOpen::Annotations::IntersectionSymbol* intersectionSymbol =
			CreateIntersectionSymbol(baseView, intersectionCurves[0], intersectionCurves[1]);
		if (intersectionSymbol != NULL)
		{
			symbols[i] = intersectionSymbol;
		}
	}
	return symbols;
}
}

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
	double centerY)
{
	if (workPart == NULL || baseView == NULL)
	{
		return;
	}

	ResetDimensionLayoutState();
	RegisterDimensionLayoutObstacles(baseView, draftingCurves);

	OuterRadiusFaceGroups outerRadiusGroups = ClassifyOuterRadiusFacesForView(workPart, outerRadiusFaces, baseView);
	std::vector<NXOpen::Face*> waiRFace90 = outerRadiusGroups.rightAngleFaces;
	std::vector<NXOpen::Face*> waiRFaceNot90 = outerRadiusGroups.notRightAngleFaces;
	std::vector<NXOpen::Face*> waiRFace180 = outerRadiusGroups.straightAngleFaces;
	std::vector<NXOpen::Annotations::IntersectionSymbol*> symbols =
		CreateNot90IntersectionSymbols(waiRFaceNot90, adjacentPlanarFaces, draftingCurves, baseView);

	std::vector<NXOpen::Face*> firstDimensionFaces;
	CollectFirstCutDimensionFacesForView(
		workPart,
		firstCutRadiusFaces,
		allPlanarFaces,
		excludedFirstCutFaces,
		firstDimensionFaces);

	if (!secondDimensionCurves.empty())
	{
		for (size_t i = 0; i < firstDimensionFaces.size(); i++)
		{
			if (i >= firstCutRadiusFaces.size() || i >= secondDimensionCurves.size())
			{
				continue;
			}

			for (size_t ia = 0; ia < waiRFace90.size(); ia++)
			{
				if (firstCutRadiusFaces[i] != waiRFace90[ia])
				{
					continue;
				}

				double point1c[3], dir[3], box[6], radius1, radData;
				int type1, normDir;
				UF_MODL_ask_face_data(firstCutRadiusFaces[i]->Tag(), &type1, point1c, dir, box, &radius1, &radData, &normDir);
				CreateFaceToObjectDimensionFromModelPoint(
					baseView,
					firstDimensionFaces[i],
					secondDimensionCurves[i],
					point1c,
					centerX,
					centerY);
			}

			for (size_t a = 0; a < waiRFaceNot90.size(); a++)
			{
				if (a >= symbols.size() || symbols[a] == NULL || firstCutRadiusFaces[i] != waiRFaceNot90[a])
				{
					continue;
				}

				double point1[3], dir[3], box[6], radius1, radData, drawingPoint[2];
				int type1, normDir;
				UF_MODL_ask_face_data(firstCutRadiusFaces[i]->Tag(), &type1, point1, dir, box, &radius1, &radData, &normDir);
				UF_VIEW_map_model_to_drawing(baseView->Tag(), point1, drawingPoint);
				CreateCurveEndToSymbolDimensionFromDrawingPoint(
					baseView,
					secondDimensionCurves[i],
					symbols[a],
					drawingPoint,
					centerX,
					centerY);
			}
		}
	}

	for (size_t i = 0; i < outerRadiusFaces.size(); i++)
	{
		double point1a[3], dir[3], box[6], radius1, radData, drawingPoint[2];
		int type1, normDir;
		UF_MODL_ask_face_data(outerRadiusFaces[i]->Tag(), &type1, point1a, dir, box, &radius1, &radData, &normDir);
		UF_VIEW_map_model_to_drawing(baseView->Tag(), point1a, drawingPoint);
		if (radius1 - 0.5 > bodyThickness)
		{
			CreateRadialDimensionAtDrawingPoint(baseView, outerRadiusFaces[i], drawingPoint);
		}
	}

	for (size_t i = 0; i < waiRFace90.size(); i++)
	{
		std::vector<NXOpen::Face*> firstAdjacent = CollectAdjacentFaces(workPart, waiRFace90[i], adjacentPlanarFaces, 0.001);
		for (size_t q = i + 1; q < waiRFace90.size(); q++)
		{
			std::vector<NXOpen::Face*> secondAdjacent = CollectAdjacentFaces(workPart, waiRFace90[q], adjacentPlanarFaces, 0.001);
			std::vector<NXOpen::Face*> workingFirstAdjacent = firstAdjacent;
			std::vector<NXOpen::Face*> dimensionFaces;
			TryPromoteUniqueDimensionFace(workingFirstAdjacent, secondAdjacent, dimensionFaces);

			double point1[3], point2[3], dir[3], box[6], radius1, radData;
			int type1, normDir;
			UF_MODL_ask_face_data(waiRFace90[i]->Tag(), &type1, point1, dir, box, &radius1, &radData, &normDir);
			UF_MODL_ask_face_data(waiRFace90[q]->Tag(), &type1, point2, dir, box, &radius1, &radData, &normDir);
			if (dimensionFaces.size() == 1 && secondAdjacent.size() == 1)
			{
				CreateFaceToObjectDimensionFromModelPoint(
					baseView,
					dimensionFaces[0],
					secondAdjacent[0],
					point1,
					centerX,
					centerY);
			}

			if (secondAdjacent.size() == 2)
			{
				std::vector<NXOpen::Face*> innerAdjacent;
				for (size_t v = 0; v < innerRadiusFaces.size(); v++)
				{
					double distance1 = MeasureMinimumDistance(workPart, innerRadiusFaces[v], waiRFace90[q]);
					if (std::fabs(distance1 - bodyThickness) < 0.01)
					{
						innerAdjacent = CollectAdjacentFaces(workPart, innerRadiusFaces[v], adjacentPlanarFaces, 0.001);
						break;
					}
				}

				std::vector<NXOpen::Face*> firstAdjacentForInner = workingFirstAdjacent;
				std::vector<NXOpen::Face*> innerDimensionFaces;
				TryPromoteUniqueDimensionFace(firstAdjacentForInner, innerAdjacent, innerDimensionFaces);
				if (innerAdjacent.size() == 1 && innerDimensionFaces.size() == 1)
				{
					for (size_t c = 0; c < secondAdjacent.size(); c++)
					{
						double distance1 = MeasureMinimumDistance(workPart, innerAdjacent[0], secondAdjacent[c]);
						if (std::fabs(distance1 - bodyThickness) < 0.01)
						{
							CreateFaceToObjectDimensionFromModelPoint(
								baseView,
								innerDimensionFaces[0],
								secondAdjacent[c],
								point1,
								centerX,
								centerY);
						}
					}
				}
			}
		}
	}

	for (size_t i = 0; i < waiRFaceNot90.size(); i++)
	{
		std::vector<NXOpen::Face*> firstAdjacent = CollectAdjacentFaces(workPart, waiRFaceNot90[i], adjacentPlanarFaces, 0.001);
		for (size_t q = i + 1; q < waiRFaceNot90.size(); q++)
		{
			std::vector<NXOpen::Face*> workingFirstAdjacent = firstAdjacent;
			std::vector<NXOpen::Face*> secondAdjacent = CollectAdjacentFaces(workPart, waiRFaceNot90[q], adjacentPlanarFaces, 0.001);
			std::vector<NXOpen::Face*> dimensionFaces;
			TryPromoteUniqueDimensionFace(workingFirstAdjacent, secondAdjacent, dimensionFaces);

			double point1[3], point2[3], dir[3], box[6], radius1, radData;
			int type1, normDir;
			UF_MODL_ask_face_data(waiRFaceNot90[i]->Tag(), &type1, point1, dir, box, &radius1, &radData, &normDir);
			UF_MODL_ask_face_data(waiRFaceNot90[q]->Tag(), &type1, point2, dir, box, &radius1, &radData, &normDir);
			if (secondAdjacent.size() == 1 && dimensionFaces.size() == 1)
			{
				if (!IsValidIntersectionSymbol(symbols, i) || !IsValidIntersectionSymbol(symbols, q))
				{
					continue;
				}
				CreateSymbolToSymbolDimensionFromModelPoint(
					baseView,
					symbols[i],
					symbols[q],
					point1,
					centerX,
					centerY);
			}

			if (secondAdjacent.size() == 2)
			{
				std::vector<NXOpen::Face*> firstAdjacentForInner = workingFirstAdjacent;
				std::vector<NXOpen::Face*> innerAdjacent;
				NXOpen::Face* concentricInnerFace =
					FindInnerRFaceSharingDrawingCenter(baseView, waiRFaceNot90[q], innerRadiusFaces);
				if (concentricInnerFace != NULL)
				{
					innerAdjacent = CollectAdjacentFaces(workPart, concentricInnerFace, adjacentPlanarFaces, 0.001);
				}

				std::vector<NXOpen::Face*> innerDimensionFaces;
				TryPromoteUniqueDimensionFace(firstAdjacentForInner, innerAdjacent, innerDimensionFaces);
				if (innerAdjacent.size() == 1 && innerDimensionFaces.size() == 1)
				{
					for (size_t c = 0; c < secondAdjacent.size(); c++)
					{
						double distance1 = MeasureMinimumDistance(workPart, innerAdjacent[0], secondAdjacent[c]);
						if (std::fabs(distance1 - bodyThickness) < 0.01)
						{
							if (!IsValidIntersectionSymbol(symbols, i) || !IsValidIntersectionSymbol(symbols, q))
							{
								continue;
							}
							CreateSymbolToSymbolDimensionFromModelPoint(
								baseView,
								symbols[i],
								symbols[q],
								point1,
								centerX,
								centerY);
						}
					}
				}
			}
		}
	}

	for (size_t i = 0; i < waiRFace90.size(); i++)
	{
		std::vector<NXOpen::Face*> firstAdjacent = CollectAdjacentFaces(workPart, waiRFace90[i], adjacentPlanarFaces, 0.01);
		for (size_t q = 0; q < waiRFaceNot90.size(); q++)
		{
			std::vector<NXOpen::Face*> workingFirstAdjacent = firstAdjacent;
			std::vector<NXOpen::Face*> secondAdjacent = CollectAdjacentFaces(workPart, waiRFaceNot90[q], adjacentPlanarFaces, 0.001);
			std::vector<NXOpen::Face*> dimensionFaces;
			TryPromoteUniqueDimensionFace(workingFirstAdjacent, secondAdjacent, dimensionFaces);

			double point1[3], point2[3], dir[3], box[6], radius1, radData;
			int type1, normDir;
			UF_MODL_ask_face_data(waiRFace90[i]->Tag(), &type1, point1, dir, box, &radius1, &radData, &normDir);
			UF_MODL_ask_face_data(waiRFaceNot90[q]->Tag(), &type1, point2, dir, box, &radius1, &radData, &normDir);
			if (secondAdjacent.size() == 1 && dimensionFaces.size() == 1)
			{
				if (!IsValidIntersectionSymbol(symbols, q))
				{
					continue;
				}
				CreateFaceToSymbolDimensionFromModelPoint(
					baseView,
					dimensionFaces[0],
					symbols[q],
					point1,
					centerX,
					centerY);
			}

			if (secondAdjacent.size() == 2)
			{
				std::vector<NXOpen::Face*> firstAdjacentForInner = workingFirstAdjacent;
				std::vector<NXOpen::Face*> innerAdjacent;
				NXOpen::Face* concentricInnerFace =
					FindInnerRFaceSharingDrawingCenter(baseView, waiRFaceNot90[q], innerRadiusFaces);
				if (concentricInnerFace != NULL)
				{
					innerAdjacent = CollectAdjacentFaces(workPart, concentricInnerFace, adjacentPlanarFaces, 0.001);
				}

				std::vector<NXOpen::Face*> innerDimensionFaces;
				TryPromoteUniqueDimensionFace(firstAdjacentForInner, innerAdjacent, innerDimensionFaces);
				if (innerAdjacent.size() == 1 && innerDimensionFaces.size() == 1)
				{
					for (size_t c = 0; c < secondAdjacent.size(); c++)
					{
						double distance1 = MeasureMinimumDistance(workPart, innerAdjacent[0], secondAdjacent[c]);
						if (std::fabs(distance1 - bodyThickness) < 0.01)
						{
							if (!IsValidIntersectionSymbol(symbols, q))
							{
								continue;
							}
							CreateFaceToSymbolDimensionFromModelPoint(
								baseView,
								innerDimensionFaces[0],
								symbols[q],
								point1,
								centerX,
								centerY);
						}
					}
				}
			}
		}
	}

	for (size_t i = 0; i < waiRFace90.size(); i++)
	{
		std::vector<NXOpen::Face*> firstAdjacent = CollectAdjacentFaces(workPart, waiRFace90[i], adjacentPlanarFaces, 0.01);
		for (size_t q = 0; q < waiRFace180.size(); q++)
		{
			std::vector<NXOpen::Face*> workingFirstAdjacent = firstAdjacent;
			std::vector<NXOpen::Face*> secondAdjacent = CollectAdjacentFaces(workPart, waiRFace180[q], adjacentPlanarFaces, 0.001);
			std::vector<NXOpen::Face*> dimensionFaces;
			TryPromoteUniqueDimensionFace(workingFirstAdjacent, secondAdjacent, dimensionFaces);

			double point1[3], point2[3], dir[3], box[6], radius1, radData;
			int type1, normDir;
			UF_MODL_ask_face_data(waiRFace90[i]->Tag(), &type1, point1, dir, box, &radius1, &radData, &normDir);
			UF_MODL_ask_face_data(waiRFace180[q]->Tag(), &type1, point2, dir, box, &radius1, &radData, &normDir);
			if (secondAdjacent.size() == 1 && dimensionFaces.size() == 1)
			{
				CreateFaceToObjectDimensionFromModelPoint(
					baseView,
					dimensionFaces[0],
					waiRFace180[q],
					point1,
					centerX,
					centerY);
			}

			if (secondAdjacent.size() == 2)
			{
				std::vector<NXOpen::Face*> firstAdjacentForInner = workingFirstAdjacent;
				std::vector<NXOpen::Face*> innerAdjacent;
				NXOpen::Face* concentricInnerFace =
					FindInnerRFaceSharingDrawingCenter(baseView, waiRFace180[q], innerRadiusFaces);
				if (concentricInnerFace != NULL)
				{
					innerAdjacent = CollectAdjacentFaces(workPart, concentricInnerFace, adjacentPlanarFaces, 0.001);
				}

				std::vector<NXOpen::Face*> innerDimensionFaces;
				TryPromoteUniqueDimensionFace(firstAdjacentForInner, innerAdjacent, innerDimensionFaces);
				if (innerAdjacent.size() == 1 && innerDimensionFaces.size() == 1)
				{
					for (size_t c = 0; c < secondAdjacent.size(); c++)
					{
						double distance1 = MeasureMinimumDistance(workPart, innerAdjacent[0], secondAdjacent[c]);
						if (std::fabs(distance1 - bodyThickness) < 0.01)
						{
							CreateFaceToObjectDimensionFromModelPoint(
								baseView,
								innerDimensionFaces[0],
								secondAdjacent[c],
								point1,
								centerX,
								centerY);
						}
					}
				}
			}
		}
	}
}
