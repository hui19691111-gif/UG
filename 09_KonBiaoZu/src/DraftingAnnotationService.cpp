#include "DraftingAnnotationService.hpp"

#include <NXOpen/Annotations_Annotation.hxx>
#include <NXOpen/Annotations_AnnotationManager.hxx>
#include <NXOpen/Annotations_DimensionCollection.hxx>
#include <NXOpen/Annotations_DraftingNoteBuilder.hxx>
#include <NXOpen/Annotations_LetteringStyleBuilder.hxx>
#include <NXOpen/Annotations_RadialDimensionBuilder.hxx>
#include <NXOpen/Annotations_SimpleDraftingAid.hxx>
#include <NXOpen/BasePart.hxx>
#include <NXOpen/DisplayableObject.hxx>
#include <NXOpen/Drawings_DraftingBody.hxx>
#include <NXOpen/Drawings_DraftingBodyCollection.hxx>
#include <NXOpen/Drawings_DraftingCurve.hxx>
#include <NXOpen/Drawings_DraftingCurveCollection.hxx>
#include <NXOpen/Drawings_DraftingView.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/SelectNXObject.hxx>
#include <NXOpen/View.hxx>

#include <Windows.h>

#include <uf_curve.h>
#include <uf_draw.h>
#include <uf_defs.h>
#include <uf.h>
#include <uf_eval.h>
#include <uf_modl.h>
#include <uf_modl_utilities.h>
#include <uf_obj.h>
#include <uf_object_types.h>
#include <uf_smd.h>
#include <uf_view.h>

#include <array>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <set>
#include <sstream>
#include <tuple>

namespace
{
    using DebugFaceColorState = std::pair<tag_t, int>;

    constexpr double kTwoPi = 6.28318530717958647692;

    std::vector<DebugFaceColorState>& GetDebugFaceColorStates()
    {
        static std::vector<DebugFaceColorState> states;
        return states;
    }

    std::string Utf8ToSystem(const std::string& utf8)
    {
        if (utf8.empty())
        {
            return {};
        }

        const int wideSize = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
        if (wideSize <= 0)
        {
            return utf8;
        }

        std::wstring wide(static_cast<size_t>(wideSize), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, wide.data(), wideSize);

        const int ansiSize = WideCharToMultiByte(CP_ACP, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (ansiSize <= 0)
        {
            return utf8;
        }

        std::string ansi(static_cast<size_t>(ansiSize), '\0');
        WideCharToMultiByte(CP_ACP, 0, wide.c_str(), -1, ansi.data(), ansiSize, nullptr, nullptr);
        if (!ansi.empty() && ansi.back() == '\0')
        {
            ansi.pop_back();
        }
        return ansi;
    }

    bool TryAskEvaluatedArc(tag_t objectTag, UF_EVAL_arc_t& arcData)
    {
        if (objectTag == NULL_TAG)
        {
            return false;
        }

        UF_EVAL_p_t evaluator = nullptr;
        if (UF_EVAL_initialize(objectTag, &evaluator) != 0 || evaluator == nullptr)
        {
            return false;
        }

        bool isArc = false;
        const int isArcStatus = UF_EVAL_is_arc(evaluator, &isArc);
        const int arcStatus = (isArcStatus == 0 && isArc) ? UF_EVAL_ask_arc(evaluator, &arcData) : 1;
        UF_EVAL_free(evaluator);
        return isArcStatus == 0 && arcStatus == 0;
    }

    bool TryAskArcData(tag_t objectTag, double& diameter, std::array<double, 3>& center)
    {
        if (objectTag == NULL_TAG)
        {
            return false;
        }

        UF_EVAL_arc_t evalArc {};
        if (TryAskEvaluatedArc(objectTag, evalArc))
        {
            diameter = evalArc.radius * 2.0;
            center[0] = evalArc.center[0];
            center[1] = evalArc.center[1];
            center[2] = evalArc.center[2];
            return diameter > 0.0;
        }

        int objectType = 0;
        int objectSubtype = 0;
        if (UF_OBJ_ask_type_and_subtype(objectTag, &objectType, &objectSubtype) != 0)
        {
            return false;
        }

        if (objectType != UF_circle_type && objectType != UF_conic_type)
        {
            return false;
        }

        UF_CURVE_arc_t curveArc {};
        if (UF_CURVE_ask_arc_data(objectTag, &curveArc) != 0)
        {
            return false;
        }

        diameter = curveArc.radius * 2.0;
        center[0] = curveArc.arc_center[0];
        center[1] = curveArc.arc_center[1];
        center[2] = curveArc.arc_center[2];
        return diameter > 0.0;
    }

    bool TryAskArcDiameter(tag_t objectTag, double& diameter)
    {
        std::array<double, 3> center {};
        return TryAskArcData(objectTag, diameter, center);
    }

    tag_t ResolveViewTagForObject(tag_t objectTag)
    {
        if (objectTag == NULL_TAG)
        {
            return NULL_TAG;
        }

        int numberEdits = 0;
        UF_VIEW_vde_data_t* vdeData = nullptr;
        if (UF_VIEW_ask_vde_data(objectTag, &numberEdits, &vdeData) == 0 && vdeData != nullptr)
        {
            const tag_t resolvedView = vdeData->view_tag;
            UF_free(vdeData);
            if (resolvedView != NULL_TAG)
            {
                return resolvedView;
            }
        }

        int isViewDependent = 0;
        std::array<char, UF_OBJ_NAME_BUFSIZE> viewName {};
        if (uc6409(objectTag, &isViewDependent, viewName.data()) == 0 && isViewDependent == 1)
        {
            tag_t resolvedView = NULL_TAG;
            if (UF_VIEW_ask_tag_of_view_name(viewName.data(), &resolvedView) == 0 && resolvedView != NULL_TAG)
            {
                return resolvedView;
            }
        }

        tag_t workView = NULL_TAG;
        if (UF_VIEW_ask_work_view(&workView) == 0 && workView != NULL_TAG)
        {
            UF_VIEW_type_t viewType = UF_VIEW_MODEL_TYPE;
            UF_VIEW_subtype_t viewSubtype = UF_VIEW_INVALID_SUBTYPE;
            if (UF_VIEW_ask_type(workView, &viewType, &viewSubtype) == 0 &&
                viewType == UF_VIEW_DRAWING_MEMBER_TYPE)
            {
                return workView;
            }
        }

        return NULL_TAG;
    }

    bool IsFullCircle(tag_t objectTag, double& diameter, std::array<double, 3>& center)
    {
        UF_EVAL_arc_t arcData {};
        if (!TryAskEvaluatedArc(objectTag, arcData))
        {
            return false;
        }

        if (std::abs(arcData.limits[0]) > 0.001 || std::abs(arcData.limits[1] - kTwoPi) > 0.001)
        {
            return false;
        }

        diameter = arcData.radius * 2.0;
        center[0] = arcData.center[0];
        center[1] = arcData.center[1];
        center[2] = arcData.center[2];
        return diameter > 0.0;
    }

    std::tuple<long long, long long, long long, long long> BuildArcKey(tag_t objectTag, double diameter)
    {
        std::array<double, 3> center {};
        double currentDiameter = 0.0;
        if (!TryAskArcData(objectTag, currentDiameter, center))
        {
            return {static_cast<long long>(objectTag), 0, 0, 0};
        }

        const long long x = static_cast<long long>(std::llround(center[0] * 1000.0));
        const long long y = static_cast<long long>(std::llround(center[1] * 1000.0));
        const long long z = static_cast<long long>(std::llround(center[2] * 1000.0));
        const long long d = static_cast<long long>(std::llround(diameter * 1000.0));
        return {x, y, z, d};
    }

    bool AreCentersClose(const std::array<double, 3>& left, const std::array<double, 3>& right, double tolerance)
    {
        return std::abs(left[0] - right[0]) <= tolerance &&
               std::abs(left[1] - right[1]) <= tolerance &&
               std::abs(left[2] - right[2]) <= tolerance;
    }

    bool AreDrawingCentersClose(const KonBiaoZu::DraftingCircleMatch& left, const KonBiaoZu::DraftingCircleMatch& right, double tolerance)
    {
        return std::abs(left.drawingCenter.X - right.drawingCenter.X) <= tolerance &&
               std::abs(left.drawingCenter.Y - right.drawingCenter.Y) <= tolerance;
    }

    bool HasConcentricPartner(
        const std::vector<KonBiaoZu::DraftingCircleMatch>& circles,
        size_t index,
        double centerTolerance,
        double diameterTolerance)
    {
        if (index >= circles.size())
        {
            return false;
        }

        for (size_t otherIndex = 0; otherIndex < circles.size(); ++otherIndex)
        {
            if (otherIndex == index)
            {
                continue;
            }

            if (!AreDrawingCentersClose(circles[index], circles[otherIndex], centerTolerance))
            {
                continue;
            }

            if (std::abs(circles[index].diameter - circles[otherIndex].diameter) > diameterTolerance)
            {
                return true;
            }
        }

        return false;
    }

    double DotProduct(const std::array<double, 3>& left, const std::array<double, 3>& right)
    {
        return left[0] * right[0] + left[1] * right[1] + left[2] * right[2];
    }

    double VectorLength(const std::array<double, 3>& vector)
    {
        return std::sqrt(DotProduct(vector, vector));
    }

    bool Normalize(std::array<double, 3>& vector)
    {
        const double length = VectorLength(vector);
        if (length <= 1.0e-9)
        {
            return false;
        }

        vector[0] /= length;
        vector[1] /= length;
        vector[2] /= length;
        return true;
    }

    bool TryGetViewDirection(tag_t viewTag, std::array<double, 3>& viewDirection)
    {
        double matrix[9] = {0.0};
        if (UF_VIEW_ask_rotation(viewTag, matrix) != 0)
        {
            return false;
        }

        viewDirection = {matrix[6], matrix[7], matrix[8]};
        return Normalize(viewDirection);
    }

    bool TryBuildCircleMatch(
        tag_t viewTag,
        tag_t curveTag,
        const std::array<double, 3>& center,
        double diameter,
        KonBiaoZu::DraftingCircleMatch& match)
    {
        double drawingPoint[2] = {0.0, 0.0};
        double modelPoint[3] = {center[0], center[1], center[2]};
        if (UF_VIEW_map_model_to_drawing(viewTag, modelPoint, drawingPoint) != 0)
        {
            return false;
        }

        match.curveTag = curveTag;
        match.modelCenter = NXOpen::Point3d(center[0], center[1], center[2]);
        match.drawingCenter = NXOpen::Point3d(drawingPoint[0], drawingPoint[1], 0.0);
        match.diameter = diameter;
        return true;
    }

    std::vector<KonBiaoZu::DraftingCircleMatch> CollectFullCirclesInView(tag_t viewTag)
    {
        std::vector<KonBiaoZu::DraftingCircleMatch> circles;
        if (viewTag == NULL_TAG)
        {
            return circles;
        }

        NXOpen::Drawings::DraftingView* draftingView =
            dynamic_cast<NXOpen::Drawings::DraftingView*>(NXOpen::NXObjectManager::Get(viewTag));
        if (draftingView == nullptr)
        {
            return circles;
        }

        std::set<std::tuple<long long, long long, long long, long long>> visitedKeys;
        NXOpen::Drawings::DraftingBodyCollection* bodyCollection = draftingView->DraftingBodies();
        for (auto bodyIt = bodyCollection->begin(); bodyIt != bodyCollection->end(); ++bodyIt)
        {
            NXOpen::Drawings::DraftingBody* draftingBody = *bodyIt;
            if (draftingBody == nullptr)
            {
                continue;
            }

            NXOpen::Drawings::DraftingCurveCollection* curveCollection = draftingBody->DraftingCurves();
            for (auto curveIt = curveCollection->begin(); curveIt != curveCollection->end(); ++curveIt)
            {
                NXOpen::Drawings::DraftingCurve* draftingCurve = *curveIt;
                if (draftingCurve == nullptr)
                {
                    continue;
                }

                double diameter = 0.0;
                std::array<double, 3> center {};
                if (!IsFullCircle(draftingCurve->Tag(), diameter, center))
                {
                    continue;
                }

                const auto key = BuildArcKey(draftingCurve->Tag(), diameter);
                if (!visitedKeys.insert(key).second)
                {
                    continue;
                }

                KonBiaoZu::DraftingCircleMatch match;
                if (TryBuildCircleMatch(viewTag, draftingCurve->Tag(), center, diameter, match))
                {
                    circles.push_back(match);
                }
            }
        }

        return circles;
    }

    std::string FormatDiameterText(double value)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(2) << value;
        std::string text = out.str();
        while (!text.empty() && text.back() == '0')
        {
            text.pop_back();
        }
        if (!text.empty() && text.back() == '.')
        {
            text.pop_back();
        }
        return text;
    }

    std::vector<std::string> SplitLines(const std::string& text)
    {
        std::vector<std::string> lines;
        std::istringstream stream(text);
        std::string line;
        while (std::getline(stream, line))
        {
            lines.push_back(line);
        }
        if (lines.empty())
        {
            lines.push_back(text);
        }
        return lines;
    }

    std::string ToUpperCopy(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
            return static_cast<char>(std::toupper(ch));
        });
        return value;
    }

    bool FeatureTypeContains(tag_t featureTag, const char* expectedType)
    {
        if (featureTag == NULL_TAG || expectedType == nullptr)
        {
            return false;
        }

        char* featureType = nullptr;
        if (UF_MODL_ask_feat_type(featureTag, &featureType) != 0 || featureType == nullptr)
        {
            return false;
        }

        const std::string actual = ToUpperCopy(featureType);
        UF_free(featureType);
        return actual.find(ToUpperCopy(expectedType)) != std::string::npos;
    }

    bool TryResolveFlatPatternParentCurve(
        tag_t draftingCurveTag,
        tag_t& parentCurveTag,
        tag_t& flatPatternFeatureTag,
        std::array<double, 3>& curveCenter,
        double& curveDiameter)
    {
        parentCurveTag = NULL_TAG;
        flatPatternFeatureTag = NULL_TAG;
        curveCenter = {};
        curveDiameter = 0.0;

        int parentsCount = 0;
        tag_t* parents = nullptr;
        if (UF_DRAW_ask_drafting_curve_parents(draftingCurveTag, &parentsCount, &parents) != 0 ||
            parentsCount <= 0 || parents == nullptr)
        {
            return false;
        }

        bool resolved = false;
        for (int parentIndex = 0; parentIndex < parentsCount && !resolved; ++parentIndex)
        {
            const tag_t currentParent = parents[parentIndex];
            if (!TryAskArcData(currentParent, curveDiameter, curveCenter))
            {
                continue;
            }

            tag_t featureTag = NULL_TAG;
            if (UF_MODL_ask_object_feat(currentParent, &featureTag) != 0 || featureTag == NULL_TAG)
            {
                continue;
            }

            if (!FeatureTypeContains(featureTag, "FLAT_PATTERN"))
            {
                continue;
            }

            parentCurveTag = currentParent;
            flatPatternFeatureTag = featureTag;
            resolved = true;
        }

        UF_free(parents);
        return resolved;
    }

    std::vector<tag_t> CollectSbFlatSolidFeatures(tag_t flatPatternFeatureTag)
    {
        std::vector<tag_t> flatSolidFeatures;
        if (flatPatternFeatureTag == NULL_TAG)
        {
            return flatSolidFeatures;
        }

        int numParents = 0;
        tag_t* parentFeatures = nullptr;
        int numChildren = 0;
        tag_t* childFeatures = nullptr;
        if (UF_MODL_ask_feat_relatives(flatPatternFeatureTag, &numParents, &parentFeatures, &numChildren, &childFeatures) == 0 &&
            parentFeatures != nullptr)
        {
            for (int index = 0; index < numParents; ++index)
            {
                if (FeatureTypeContains(parentFeatures[index], "SB_FLAT_SOLID"))
                {
                    flatSolidFeatures.push_back(parentFeatures[index]);
                }
            }
        }

        if (parentFeatures != nullptr)
        {
            UF_free(parentFeatures);
        }
        if (childFeatures != nullptr)
        {
            UF_free(childFeatures);
        }

        if (!flatSolidFeatures.empty())
        {
            return flatSolidFeatures;
        }

        NXOpen::Session* session = NXOpen::Session::GetSession();
        NXOpen::Part* workPart = session->Parts()->Work();
        if (workPart == nullptr)
        {
            return flatSolidFeatures;
        }

        std::set<tag_t> visitedFeatures;
        tag_t featureTag = NULL_TAG;
        while (UF_OBJ_cycle_objs_in_part(workPart->Tag(), UF_feature_type, &featureTag) == 0 && featureTag != NULL_TAG)
        {
            if (!visitedFeatures.insert(featureTag).second)
            {
                continue;
            }

            if (FeatureTypeContains(featureTag, "SB_FLAT_SOLID"))
            {
                flatSolidFeatures.push_back(featureTag);
            }
        }

        return flatSolidFeatures;
    }

    void AccumulateFaceSide(tag_t faceTag, int& frontCount, int& backCount, int& unknownCount)
    {
        if (faceTag == NULL_TAG)
        {
            return;
        }

        int faceType = 0;
        double point[3] = {0.0};
        double dir[3] = {0.0};
        double box[6] = {0.0};
        double radius = 0.0;
        double radData = 0.0;
        int normDir = 1;
        if (UF_MODL_ask_face_data(faceTag, &faceType, point, dir, box, &radius, &radData, &normDir) != 0)
        {
            return;
        }

        if (faceType != 22)
        {
            return;
        }

        double uvMinMax[4] = {0.0};
        if (UF_MODL_ask_face_uv_minmax(faceTag, uvMinMax) != 0)
        {
            return;
        }

        double param[2] = {
            (uvMinMax[0] + uvMinMax[1]) * 0.5,
            (uvMinMax[2] + uvMinMax[3]) * 0.5
        };
        double facePoint[3] = {0.0};
        double u1[3] = {0.0};
        double v1[3] = {0.0};
        double u2[3] = {0.0};
        double v2[3] = {0.0};
        double unitNorm[3] = {0.0};
        double radii[2] = {0.0};
        if (UF_MODL_ask_face_props(faceTag, param, facePoint, u1, v1, u2, v2, unitNorm, radii) != 0)
        {
            return;
        }

        std::array<double, 3> faceNormal {unitNorm[0], unitNorm[1], unitNorm[2]};
        if (!Normalize(faceNormal))
        {
            return;
        }

        if (faceNormal[2] > 0.15)
        {
            ++frontCount;
        }
        else if (faceNormal[2] < -0.15)
        {
            ++backCount;
        }
        else
        {
            ++unknownCount;
        }
    }

    void ApplyDebugFaceColor(tag_t faceTag, int color)
    {
        if (faceTag == NULL_TAG)
        {
            return;
        }

        auto& states = GetDebugFaceColorStates();
        const auto existing = std::find_if(states.begin(), states.end(), [faceTag](const DebugFaceColorState& state) {
            return state.first == faceTag;
        });
        if (existing == states.end())
        {
            UF_OBJ_disp_props_t displayProps {};
            if (UF_OBJ_ask_display_properties(faceTag, &displayProps) == 0)
            {
                states.emplace_back(faceTag, displayProps.color);
            }
        }

        UF_OBJ_set_color(faceTag, color);
        NXOpen::DisplayableObject* object =
            dynamic_cast<NXOpen::DisplayableObject*>(NXOpen::NXObjectManager::Get(faceTag));
        if (object != nullptr)
        {
            object->RedisplayObject();
            object->Highlight();
        }
    }

    void RestoreDebugFaceColors()
    {
        auto& states = GetDebugFaceColorStates();
        for (const DebugFaceColorState& state : states)
        {
            if (state.first == NULL_TAG)
            {
                continue;
            }

            UF_OBJ_set_color(state.first, state.second);
            NXOpen::DisplayableObject* object =
                dynamic_cast<NXOpen::DisplayableObject*>(NXOpen::NXObjectManager::Get(state.first));
            if (object != nullptr)
            {
                object->Unhighlight();
                object->RedisplayObject();
            }
        }
        states.clear();
    }
}

namespace KonBiaoZu
{
    double DraftingAnnotationService::ResolveSelectedDiameter(NXOpen::TaggedObject* selectedObject) const
    {
        if (selectedObject == nullptr)
        {
            return 0.0;
        }

        double diameter = 0.0;
        if (TryAskArcDiameter(selectedObject->Tag(), diameter))
        {
            return diameter;
        }

        return 0.0;
    }

    CounterboreCandidate DraftingAnnotationService::FindCounterboreCandidate(
        NXOpen::TaggedObject* selectedObject,
        double centerTolerance,
        double diameterTolerance) const
    {
        CounterboreCandidate result;
        if (selectedObject == nullptr)
        {
            return result;
        }

        double selectedDiameter = 0.0;
        std::array<double, 3> selectedCenter {};
        if (!IsFullCircle(selectedObject->Tag(), selectedDiameter, selectedCenter))
        {
            return result;
        }

        const tag_t viewTag = ResolveViewTagForObject(selectedObject->Tag());
        if (viewTag == NULL_TAG)
        {
            return result;
        }

        const std::vector<DraftingCircleMatch> circles = CollectFullCirclesInView(viewTag);
        DraftingCircleMatch selectedMatch;
        bool selectedMatchFound = false;
        for (const DraftingCircleMatch& circle : circles)
        {
            if (circle.curveTag == selectedObject->Tag())
            {
                selectedMatch = circle;
                selectedMatchFound = true;
                break;
            }
        }

        if (!selectedMatchFound)
        {
            return result;
        }

        std::vector<DraftingCircleMatch> concentricMatches;
        for (const DraftingCircleMatch& circle : circles)
        {
            if (AreDrawingCentersClose(circle, selectedMatch, centerTolerance))
            {
                concentricMatches.push_back(circle);
            }
        }

        if (concentricMatches.size() < 2)
        {
            return result;
        }

        std::sort(concentricMatches.begin(), concentricMatches.end(), [](const DraftingCircleMatch& left, const DraftingCircleMatch& right) {
            return left.diameter < right.diameter;
        });

        const DraftingCircleMatch* inner = &concentricMatches.front();
        const DraftingCircleMatch* outer = &concentricMatches.back();

        if (std::abs(inner->diameter - outer->diameter) <= diameterTolerance)
        {
            return result;
        }

        result.isValid = true;
        result.viewTag = viewTag;
        result.innerCircle = *inner;
        result.outerCircle = *outer;
        return result;
    }

    std::vector<CounterboreCandidate> DraftingAnnotationService::CollectCounterboreCandidatesInCurrentView(
        NXOpen::TaggedObject* selectedObject,
        double centerTolerance,
        double diameterTolerance) const
    {
        std::vector<CounterboreCandidate> results;
        if (selectedObject == nullptr)
        {
            return results;
        }

        const tag_t viewTag = ResolveViewTagForObject(selectedObject->Tag());
        if (viewTag == NULL_TAG)
        {
            return results;
        }

        const std::vector<DraftingCircleMatch> circles = CollectFullCirclesInView(viewTag);
        std::vector<bool> consumed(circles.size(), false);
        for (size_t i = 0; i < circles.size(); ++i)
        {
            if (consumed[i])
            {
                continue;
            }

            std::vector<size_t> concentricIndexes {i};
            for (size_t j = i + 1; j < circles.size(); ++j)
            {
                if (consumed[j])
                {
                    continue;
                }

                if (AreDrawingCentersClose(circles[i], circles[j], centerTolerance))
                {
                    concentricIndexes.push_back(j);
                }
            }

            if (concentricIndexes.size() < 2)
            {
                continue;
            }

            std::sort(concentricIndexes.begin(), concentricIndexes.end(), [&circles](size_t left, size_t right) {
                return circles[left].diameter < circles[right].diameter;
            });

            const size_t innerIndex = concentricIndexes.front();
            const size_t outerIndex = concentricIndexes.back();
            if (std::abs(circles[innerIndex].diameter - circles[outerIndex].diameter) <= diameterTolerance)
            {
                continue;
            }

            CounterboreCandidate candidate;
            candidate.isValid = true;
            candidate.viewTag = viewTag;
            candidate.innerCircle = circles[innerIndex];
            candidate.outerCircle = circles[outerIndex];
            results.push_back(candidate);

            for (const size_t index : concentricIndexes)
            {
                consumed[index] = true;
            }
        }

        return results;
    }

    DraftingCircleGroup DraftingAnnotationService::CollectCounterboreCirclesInCurrentView(
        NXOpen::TaggedObject* selectedObject,
        double innerDiameter,
        double centerTolerance,
        double diameterTolerance) const
    {
        DraftingCircleGroup result;
        if (selectedObject == nullptr || innerDiameter <= 0.0)
        {
            return result;
        }

        const tag_t viewTag = ResolveViewTagForObject(selectedObject->Tag());
        if (viewTag == NULL_TAG)
        {
            return result;
        }
        result.viewTag = viewTag;

        const std::vector<DraftingCircleMatch> circles = CollectFullCirclesInView(viewTag);
        std::vector<bool> consumed(circles.size(), false);
        for (size_t i = 0; i < circles.size(); ++i)
        {
            if (consumed[i])
            {
                continue;
            }

            std::vector<size_t> concentricIndexes {i};

            for (size_t j = i + 1; j < circles.size(); ++j)
            {
                if (consumed[j])
                {
                    continue;
                }

                if (AreDrawingCentersClose(circles[i], circles[j], centerTolerance))
                {
                    concentricIndexes.push_back(j);
                }
            }

            if (concentricIndexes.size() < 2)
            {
                continue;
            }

            std::sort(concentricIndexes.begin(), concentricIndexes.end(), [&circles](size_t left, size_t right) {
                return circles[left].diameter < circles[right].diameter;
            });

            const size_t innerIndex = concentricIndexes.front();
            const size_t outerIndex = concentricIndexes.back();
            if (circles[innerIndex].diameter > circles[outerIndex].diameter)
            {
                continue;
            }

            if (std::abs(circles[innerIndex].diameter - circles[outerIndex].diameter) <= diameterTolerance)
            {
                continue;
            }

            if (std::abs(circles[innerIndex].diameter - innerDiameter) > diameterTolerance)
            {
                continue;
            }

            for (const size_t index : concentricIndexes)
            {
                consumed[index] = true;
            }
            result.matches.push_back(circles[innerIndex]);
        }

        return result;
    }

    int DraftingAnnotationService::CountSameDiameterInCurrentView(
        NXOpen::TaggedObject* selectedObject,
        double diameter,
        double tolerance) const
    {
        return static_cast<int>(CollectSameDiameterCirclesInCurrentView(selectedObject, diameter, tolerance).matches.size());
    }

    bool DraftingAnnotationService::IsFullCircleObject(NXOpen::TaggedObject* selectedObject) const
    {
        if (selectedObject == nullptr)
        {
            return false;
        }

        double diameter = 0.0;
        std::array<double, 3> center {};
        return IsFullCircle(selectedObject->Tag(), diameter, center);
    }

    DraftingCircleGroup DraftingAnnotationService::CollectSameDiameterCirclesInCurrentView(
        NXOpen::TaggedObject* selectedObject,
        double diameter,
        double tolerance) const
    {
        DraftingCircleGroup result;
        if (selectedObject == nullptr || diameter <= 0.0)
        {
            return result;
        }

        const tag_t viewTag = ResolveViewTagForObject(selectedObject->Tag());
        if (viewTag == NULL_TAG)
        {
            return result;
        }
        result.viewTag = viewTag;

        const std::vector<DraftingCircleMatch> circles = CollectFullCirclesInView(viewTag);
        size_t selectedIndex = circles.size();
        for (size_t index = 0; index < circles.size(); ++index)
        {
            if (circles[index].curveTag == selectedObject->Tag())
            {
                selectedIndex = index;
                break;
            }
        }

        const bool filterByConcentricState = selectedIndex < circles.size();
        const bool selectedHasConcentricPartner =
            filterByConcentricState && HasConcentricPartner(circles, selectedIndex, tolerance, tolerance);

        for (size_t index = 0; index < circles.size(); ++index)
        {
            const DraftingCircleMatch& match = circles[index];
            if (std::abs(match.diameter - diameter) <= tolerance)
            {
                if (filterByConcentricState &&
                    HasConcentricPartner(circles, index, tolerance, tolerance) != selectedHasConcentricPartner)
                {
                    continue;
                }

                result.matches.push_back(match);
            }
        }

        return result;
    }

    int DraftingAnnotationService::CountSameDiameterInWorkPart(double diameter, double tolerance) const
    {
        if (diameter <= 0.0)
        {
            return 0;
        }

        NXOpen::Session* session = NXOpen::Session::GetSession();
        NXOpen::Part* workPart = session->Parts()->Work();
        if (workPart == nullptr)
        {
            return 0;
        }

        int count = 0;
        tag_t objectTag = NULL_TAG;
        while (UF_OBJ_cycle_objs_in_part(workPart->Tag(), UF_circle_type, &objectTag) == 0 && objectTag != NULL_TAG)
        {
            double currentDiameter = 0.0;
            if (TryAskArcDiameter(objectTag, currentDiameter) && std::abs(currentDiameter - diameter) <= tolerance)
            {
                ++count;
            }
        }

        return count;
    }

    void DraftingAnnotationService::HighlightCircles(const DraftingCircleGroup& group) const
    {
        for (const DraftingCircleMatch& match : group.matches)
        {
            NXOpen::DisplayableObject* object =
                dynamic_cast<NXOpen::DisplayableObject*>(NXOpen::NXObjectManager::Get(match.curveTag));
            if (object != nullptr)
            {
                object->Highlight();
            }
        }
    }

    tag_t DraftingAnnotationService::CreateDraftingNote(
        tag_t viewTag,
        const NXOpen::Point3d& drawingPoint,
        const std::string& text,
        double textSize) const
    {
        if (viewTag == NULL_TAG || text.empty())
        {
            return NULL_TAG;
        }

        NXOpen::Session* session = NXOpen::Session::GetSession();
        NXOpen::Part* workPart = session->Parts()->Work();
        if (workPart == nullptr)
        {
            return NULL_TAG;
        }

        NXOpen::Drawings::DraftingView* draftingView =
            dynamic_cast<NXOpen::Drawings::DraftingView*>(NXOpen::NXObjectManager::Get(viewTag));
        if (draftingView == nullptr)
        {
            return NULL_TAG;
        }

        NXOpen::Annotations::SimpleDraftingAid* nullAid = nullptr;
        NXOpen::Annotations::DraftingNoteBuilder* noteBuilder =
            workPart->Annotations()->CreateDraftingNoteBuilder(nullAid);
        if (noteBuilder == nullptr)
        {
            return NULL_TAG;
        }

        NXOpen::Annotations::Annotation::AssociativeOriginData originData;
        originData.OriginType = NXOpen::Annotations::AssociativeOriginTypeRelativeToView;
        originData.View = draftingView;
        noteBuilder->Origin()->SetAssociativeOrigin(originData);
        noteBuilder->Origin()->SetInferRelativeToGeometry(true);
        noteBuilder->Style()->LetteringStyle()->SetGeneralTextSize(textSize);
        noteBuilder->Origin()->SetOriginPoint(drawingPoint);

        std::vector<NXOpen::NXString> lines(1);
        const std::string systemText = Utf8ToSystem(text);
        lines[0] = systemText.c_str();
        noteBuilder->Text()->TextBlock()->SetText(lines);

        NXOpen::NXObject* created = noteBuilder->Commit();
        noteBuilder->Destroy();
        return created != nullptr ? created->Tag() : NULL_TAG;
    }

    tag_t DraftingAnnotationService::CreateRadialDimensionLabel(
        NXOpen::DisplayableObject* curveObject,
        tag_t viewTag,
        const NXOpen::Point3d& drawingPoint,
        const std::string& beforeText) const
    {
        if (curveObject == nullptr || viewTag == NULL_TAG)
        {
            return NULL_TAG;
        }

        NXOpen::Session* session = NXOpen::Session::GetSession();
        NXOpen::Part* workPart = session->Parts()->Work();
        if (workPart == nullptr)
        {
            return NULL_TAG;
        }

        NXOpen::View* viewObject = dynamic_cast<NXOpen::View*>(NXOpen::NXObjectManager::Get(viewTag));
        if (viewObject == nullptr)
        {
            return NULL_TAG;
        }

        NXOpen::Annotations::Dimension* nullDimension = nullptr;
        NXOpen::Annotations::RadialDimensionBuilder* radialBuilder =
            workPart->Dimensions()->CreateRadialDimensionBuilder(nullDimension);
        if (radialBuilder == nullptr)
        {
            return NULL_TAG;
        }

        const NXOpen::Point3d assocPoint(0.0, 0.0, 0.0);
        radialBuilder->FirstAssociativity()->SetValue(curveObject, viewObject, assocPoint);
        radialBuilder->Origin()->SetOriginPoint(drawingPoint);

        const std::vector<std::string> rawLines = SplitLines(beforeText);
        std::vector<NXOpen::NXString> beforeLines;
        beforeLines.reserve(rawLines.size());
        for (const std::string& rawLine : rawLines)
        {
            const std::string systemText = Utf8ToSystem(rawLine);
            beforeLines.emplace_back(systemText.c_str());
        }
        radialBuilder->AppendedText()->SetBefore(beforeLines);

        NXOpen::NXObject* created = radialBuilder->Commit();
        radialBuilder->Destroy();
        return created != nullptr ? created->Tag() : NULL_TAG;
    }

    std::string DraftingAnnotationService::BuildCounterboreLabel(double diameter, int sameDiameterCount) const
    {
        std::ostringstream out;
        if (sameDiameterCount > 1)
        {
            out << sameDiameterCount << "-";
        }
        out << "\u6c89\u5b54 \u03a6" << FormatDiameterText(diameter);
        return out.str();
    }

    std::string DraftingAnnotationService::BuildCounterboreComment(double diameter, CounterboreSide side) const
    {
        const int screwSize = static_cast<int>(std::ceil(diameter - 0.001)) - 1;
        if (screwSize < 1)
        {
            return "\u6c89\u5b54";
        }

        std::ostringstream out;
        out << "M" << screwSize << "\u87ba\u4e1d\u6c89\u5b54";
        if (side == CounterboreSide::Front)
        {
            out << "(\u6b63\u9762)";
        }
        else if (side == CounterboreSide::Back)
        {
            out << "(\u53cd\u9762)";
        }
        return out.str();
    }

    std::string DraftingAnnotationService::BuildCounterboreAnnotation(
        double diameter,
        int sameDiameterCount,
        CounterboreSide side) const
    {
        std::ostringstream out;
        if (sameDiameterCount > 1)
        {
            out << sameDiameterCount << "-";
        }
        out << BuildCounterboreComment(diameter, side);
        return out.str();
    }

    CounterboreSide DraftingAnnotationService::ResolveCounterboreSide(const CounterboreCandidate& candidate) const
    {
        if (!candidate.isValid || candidate.viewTag == NULL_TAG || candidate.outerCircle.curveTag == NULL_TAG)
        {
            return CounterboreSide::Unknown;
        }

        tag_t parentCurveTag = NULL_TAG;
        tag_t flatPatternFeatureTag = NULL_TAG;
        std::array<double, 3> flatPatternCenter {};
        double flatPatternDiameter = 0.0;
        if (!TryResolveFlatPatternParentCurve(
                candidate.outerCircle.curveTag,
                parentCurveTag,
                flatPatternFeatureTag,
                flatPatternCenter,
                flatPatternDiameter))
        {
            return CounterboreSide::Unknown;
        }

        const std::vector<tag_t> flatSolidFeatures = CollectSbFlatSolidFeatures(flatPatternFeatureTag);
        if (flatSolidFeatures.empty())
        {
            return CounterboreSide::Unknown;
        }

        int frontCount = 0;
        int backCount = 0;
        int unknownCount = 0;
        constexpr double kCenterTolerance = 0.02;
        constexpr double kDiameterTolerance = 0.02;

        for (const tag_t flatSolidFeatureTag : flatSolidFeatures)
        {
            tag_t bodyTag = NULL_TAG;
            if (UF_MODL_ask_feat_body(flatSolidFeatureTag, &bodyTag) != 0 || bodyTag == NULL_TAG)
            {
                continue;
            }

            uf_list_p_t edgeList = nullptr;
            if (UF_MODL_ask_body_edges(bodyTag, &edgeList) != 0 || edgeList == nullptr)
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

                double edgeDiameter = 0.0;
                std::array<double, 3> edgeCenter {};
                if (!TryAskArcData(edgeTag, edgeDiameter, edgeCenter))
                {
                    continue;
                }

                if (std::abs(edgeDiameter - flatPatternDiameter) > kDiameterTolerance)
                {
                    continue;
                }

                if (std::abs(edgeCenter[0] - flatPatternCenter[0]) > kCenterTolerance ||
                    std::abs(edgeCenter[1] - flatPatternCenter[1]) > kCenterTolerance)
                {
                    continue;
                }

                uf_list_p_t faceList = nullptr;
                if (UF_MODL_ask_edge_faces(edgeTag, &faceList) != 0 || faceList == nullptr)
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
                    double point[3] = {0.0};
                    double dir[3] = {0.0};
                    double box[6] = {0.0};
                    double radius = 0.0;
                    double radData = 0.0;
                    int normDir = 1;
                    if (UF_MODL_ask_face_data(faceTag, &faceType, point, dir, box, &radius, &radData, &normDir) != 0)
                    {
                        continue;
                    }

                    if (faceType != 22)
                    {
                        continue;
                    }

                    AccumulateFaceSide(faceTag, frontCount, backCount, unknownCount);
                    break;
                }

                UF_MODL_delete_list(&faceList);
            }

            UF_MODL_delete_list(&edgeList);
        }

        if (frontCount > 0 && backCount == 0)
        {
            return CounterboreSide::Front;
        }
        if (backCount > 0 && frontCount == 0)
        {
            return CounterboreSide::Back;
        }
        return CounterboreSide::Unknown;
    }

    CounterboreSideSummary DraftingAnnotationService::SummarizeCounterboreSides(
        const std::vector<CounterboreCandidate>& candidates,
        double outerDiameter,
        double tolerance) const
    {
        CounterboreSideSummary summary;
        if (outerDiameter <= 0.0)
        {
            return summary;
        }

        for (const CounterboreCandidate& candidate : candidates)
        {
            if (!candidate.isValid)
            {
                continue;
            }

            if (std::abs(candidate.outerCircle.diameter - outerDiameter) > tolerance)
            {
                continue;
            }

            const CounterboreSide side = ResolveCounterboreSide(candidate);
            if (side == CounterboreSide::Front)
            {
                ++summary.frontCount;
            }
            else if (side == CounterboreSide::Back)
            {
                ++summary.backCount;
            }
            else
            {
                ++summary.unknownCount;
            }
        }

        return summary;
    }

    std::string DraftingAnnotationService::DescribeCounterboreSide(const CounterboreCandidate& candidate) const
    {
        if (!candidate.isValid)
        {
            return "沉孔方向: 未识别到有效沉孔";
        }

        std::ostringstream out;
        out << "沉孔方向: ";
        const CounterboreSide side = ResolveCounterboreSide(candidate);
        if (side == CounterboreSide::Front)
        {
            out << "正面";
        }
        else if (side == CounterboreSide::Back)
        {
            out << "反面";
        }
        else
        {
            out << "未知";
        }

        int parentsCount = 0;
        tag_t* parents = nullptr;
        if (UF_DRAW_ask_drafting_curve_parents(candidate.outerCircle.curveTag, &parentsCount, &parents) == 0 && parents != nullptr)
        {
            out << " | 父对象数: " << parentsCount;
            UF_free(parents);
        }
        else
        {
            out << " | 未取到工程图父对象";
        }

        return out.str();
    }

    std::string DraftingAnnotationService::BuildCounterboreDebugReport(const CounterboreCandidate& candidate) const
    {
        std::ostringstream out;
        out << "沉孔调试信息\n";
        out << "候选有效: " << (candidate.isValid ? "是" : "否") << "\n";
        out << "视图Tag: " << candidate.viewTag << "\n";
        out << "内圆Tag: " << candidate.innerCircle.curveTag << " 直径: " << FormatDiameterText(candidate.innerCircle.diameter) << "\n";
        out << "外圆Tag: " << candidate.outerCircle.curveTag << " 直径: " << FormatDiameterText(candidate.outerCircle.diameter) << "\n";

        if (!candidate.isValid || candidate.viewTag == NULL_TAG || candidate.outerCircle.curveTag == NULL_TAG)
        {
            out << "结果: 沉孔候选无效，未进入方向追溯";
            return out.str();
        }

        std::array<double, 3> viewDirection {};
        if (!TryGetViewDirection(candidate.viewTag, viewDirection))
        {
            out << "结果: 取不到视图方向";
            return out.str();
        }

        out << "视图方向: [" << viewDirection[0] << ", " << viewDirection[1] << ", " << viewDirection[2] << "]\n";

        int parentsCount = 0;
        tag_t* parents = nullptr;
        if (UF_DRAW_ask_drafting_curve_parents(candidate.outerCircle.curveTag, &parentsCount, &parents) != 0 ||
            parents == nullptr)
        {
            out << "结果: 取不到工程图父对象";
            return out.str();
        }

        out << "父对象数: " << parentsCount << "\n";
        for (int parentIndex = 0; parentIndex < parentsCount; ++parentIndex)
        {
            const tag_t parentTag = parents[parentIndex];
            int objectType = 0;
            int objectSubtype = 0;
            if (UF_OBJ_ask_type_and_subtype(parentTag, &objectType, &objectSubtype) != 0)
            {
                out << "父对象[" << parentIndex << "]: 读类型失败\n";
                continue;
            }

            out << "父对象[" << parentIndex << "]: tag=" << parentTag
                << " type=" << objectType
                << " subtype=" << objectSubtype << "\n";

            if (objectType != UF_solid_type || objectSubtype != UF_solid_edge_subtype)
            {
                tag_t featureTag = NULL_TAG;
                if (UF_MODL_ask_object_feat(parentTag, &featureTag) != 0 || featureTag == NULL_TAG)
                {
                    out << "  所属特征: 未找到\n";
                    continue;
                }

                out << "  所属特征Tag: " << featureTag << "\n";

                char* featureType = nullptr;
                if (UF_MODL_ask_feat_type(featureTag, &featureType) == 0 && featureType != nullptr)
                {
                    out << "  特征类型: " << featureType << "\n";
                    UF_free(featureType);
                }
                else
                {
                    out << "  特征类型: 读取失败\n";
                }

                uf_list_p_t featureEdgeList = nullptr;
                if (UF_MODL_ask_feat_edges(featureTag, &featureEdgeList) == 0 && featureEdgeList != nullptr)
                {
                    int featureEdgeCount = 0;
                    UF_MODL_ask_list_count(featureEdgeList, &featureEdgeCount);
                    out << "  特征边数: " << featureEdgeCount << "\n";
                    UF_MODL_delete_list(&featureEdgeList);
                }
                else
                {
                    out << "  特征边数: 获取失败或为0\n";
                }

                int featureObjectCount = 0;
                tag_t* featureObjects = nullptr;
                if (UF_MODL_ask_feat_object(featureTag, &featureObjectCount, &featureObjects) == 0 && featureObjects != nullptr)
                {
                    out << "  特征输出对象数: " << featureObjectCount << "\n";
                    for (int objectIndex = 0; objectIndex < featureObjectCount; ++objectIndex)
                    {
                        int featureObjectType = 0;
                        int featureObjectSubtype = 0;
                        if (UF_OBJ_ask_type_and_subtype(featureObjects[objectIndex], &featureObjectType, &featureObjectSubtype) == 0)
                        {
                            out << "    输出对象[" << objectIndex << "]: tag=" << featureObjects[objectIndex]
                                << " type=" << featureObjectType
                                << " subtype=" << featureObjectSubtype << "\n";
                        }
                    }
                    UF_free(featureObjects);
                }
                else
                {
                    out << "  特征输出对象数: 读取失败或为0\n";
                }

                int numParents = 0;
                tag_t* parentFeatures = nullptr;
                int numChildren = 0;
                tag_t* childFeatures = nullptr;
                if (UF_MODL_ask_feat_relatives(featureTag, &numParents, &parentFeatures, &numChildren, &childFeatures) == 0)
                {
                    out << "  父特征数: " << numParents << "\n";
                    for (int parentFeatureIndex = 0; parentFeatureIndex < numParents; ++parentFeatureIndex)
                    {
                        char* parentFeatureType = nullptr;
                        if (UF_MODL_ask_feat_type(parentFeatures[parentFeatureIndex], &parentFeatureType) == 0 && parentFeatureType != nullptr)
                        {
                            out << "    父特征[" << parentFeatureIndex << "]: tag=" << parentFeatures[parentFeatureIndex]
                                << " type=" << parentFeatureType << "\n";
                            UF_free(parentFeatureType);
                        }
                        else
                        {
                            out << "    父特征[" << parentFeatureIndex << "]: tag=" << parentFeatures[parentFeatureIndex] << "\n";
                        }

                        uf_list_p_t parentFaceList = nullptr;
                        if (UF_MODL_ask_feat_faces(parentFeatures[parentFeatureIndex], &parentFaceList) == 0 && parentFaceList != nullptr)
                        {
                            int parentFaceCount = 0;
                            UF_MODL_ask_list_count(parentFaceList, &parentFaceCount);
                            out << "      父特征面数: " << parentFaceCount << "\n";
                            UF_MODL_delete_list(&parentFaceList);
                        }
                        else
                        {
                            out << "      父特征面数: 获取失败或为0\n";
                        }
                    }

                    tag_t featureArray[1] = {featureTag};
                    tag_t* referenceParents = nullptr;
                    char** parentNames = nullptr;
                    int referenceParentCount = 0;
                    if (UF_MODL_ask_references_of_features(featureArray, 1, &referenceParents, &parentNames, &referenceParentCount) == 0)
                    {
                        out << "  父引用数: " << referenceParentCount << "\n";
                        for (int referenceIndex = 0; referenceIndex < referenceParentCount; ++referenceIndex)
                        {
                            out << "    父引用[" << referenceIndex << "]: tag=" << referenceParents[referenceIndex];
                            if (parentNames != nullptr && parentNames[referenceIndex] != nullptr)
                            {
                                out << " name=" << parentNames[referenceIndex];
                            }
                            out << "\n";
                        }
                        if (referenceParents != nullptr)
                        {
                            UF_free(referenceParents);
                        }
                        if (parentNames != nullptr)
                        {
                            UF_free_string_array(referenceParentCount, parentNames);
                        }
                    }
                    else
                    {
                        out << "  父引用数: 读取失败\n";
                    }
                }
                else
                {
                    out << "  父特征数: 读取失败\n";
                }

                if (parentFeatures != nullptr)
                {
                    UF_free(parentFeatures);
                }
                if (childFeatures != nullptr)
                {
                    UF_free(childFeatures);
                }

                uf_list_p_t featureFaceList = nullptr;
                if (UF_MODL_ask_feat_faces(featureTag, &featureFaceList) != 0 || featureFaceList == nullptr)
                {
                    out << "  特征面: 获取失败\n";
                    continue;
                }

                int featureFaceCount = 0;
                UF_MODL_ask_list_count(featureFaceList, &featureFaceCount);
                out << "  特征面数: " << featureFaceCount << "\n";
                for (int faceIndex = 0; faceIndex < featureFaceCount; ++faceIndex)
                {
                    tag_t faceTag = NULL_TAG;
                    if (UF_MODL_ask_list_item(featureFaceList, faceIndex, &faceTag) != 0 || faceTag == NULL_TAG)
                    {
                        out << "  特征面[" << faceIndex << "]: 读取失败\n";
                        continue;
                    }

                    int faceType = 0;
                    double point[3] = {0.0};
                    double dir[3] = {0.0};
                    double box[6] = {0.0};
                    double radius = 0.0;
                    double radData = 0.0;
                    int normDir = 1;
                    if (UF_MODL_ask_face_data(faceTag, &faceType, point, dir, box, &radius, &radData, &normDir) != 0)
                    {
                        out << "  特征面[" << faceIndex << "]: ask_face_data失败\n";
                        continue;
                    }

                    std::array<double, 3> faceNormal {dir[0], dir[1], dir[2]};
                    const bool normalized = Normalize(faceNormal);
                    out << "  特征面[" << faceIndex << "]: tag=" << faceTag
                        << " type=" << faceType
                        << " normDir=" << normDir;
                    if (normalized)
                    {
                        out << " normal=[" << faceNormal[0] << ", " << faceNormal[1] << ", " << faceNormal[2] << "]";
                        out << " dot=" << DotProduct(faceNormal, viewDirection);
                    }
                    out << "\n";
                }

                UF_MODL_delete_list(&featureFaceList);
                continue;
            }

            uf_list_p_t faceList = nullptr;
            if (UF_MODL_ask_edge_faces(parentTag, &faceList) != 0 || faceList == nullptr)
            {
                out << "  相邻面: 获取失败\n";
                continue;
            }

            int faceCount = 0;
            UF_MODL_ask_list_count(faceList, &faceCount);
            out << "  相邻面数: " << faceCount << "\n";
            for (int faceIndex = 0; faceIndex < faceCount; ++faceIndex)
            {
                tag_t faceTag = NULL_TAG;
                if (UF_MODL_ask_list_item(faceList, faceIndex, &faceTag) != 0 || faceTag == NULL_TAG)
                {
                    out << "  面[" << faceIndex << "]: 读取失败\n";
                    continue;
                }

                int faceType = 0;
                double point[3] = {0.0};
                double dir[3] = {0.0};
                double box[6] = {0.0};
                double radius = 0.0;
                double radData = 0.0;
                int normDir = 1;
                if (UF_MODL_ask_face_data(faceTag, &faceType, point, dir, box, &radius, &radData, &normDir) != 0)
                {
                    out << "  面[" << faceIndex << "]: ask_face_data失败\n";
                    continue;
                }

                std::array<double, 3> faceNormal {dir[0], dir[1], dir[2]};
                if (normDir < 0)
                {
                    faceNormal[0] *= -1.0;
                    faceNormal[1] *= -1.0;
                    faceNormal[2] *= -1.0;
                }

                const bool normalized = Normalize(faceNormal);
                out << "  面[" << faceIndex << "]: tag=" << faceTag
                    << " type=" << faceType
                    << " normDir=" << normDir;
                if (normalized)
                {
                    out << " normal=[" << faceNormal[0] << ", " << faceNormal[1] << ", " << faceNormal[2] << "]";
                    out << " dot=" << DotProduct(faceNormal, viewDirection);
                }
                out << "\n";
            }

            UF_MODL_delete_list(&faceList);
        }

        const CounterboreSide side = ResolveCounterboreSide(candidate);
        out << "最终方向: ";
        if (side == CounterboreSide::Front)
        {
            out << "正面";
        }
        else if (side == CounterboreSide::Back)
        {
            out << "反面";
        }
        else
        {
            out << "未知";
        }

        UF_free(parents);
        return out.str();
    }

    std::string DraftingAnnotationService::BuildCounterboreFlatPatternDebugReport(const CounterboreCandidate& candidate) const
    {
        std::ostringstream out;
        out << "沉孔调试信息\n";
        out << "候选有效: " << (candidate.isValid ? "是" : "否") << "\n";
        out << "视图Tag: " << candidate.viewTag << "\n";
        out << "内圆Tag: " << candidate.innerCircle.curveTag << " 直径: " << FormatDiameterText(candidate.innerCircle.diameter) << "\n";
        out << "外圆Tag: " << candidate.outerCircle.curveTag << " 直径: " << FormatDiameterText(candidate.outerCircle.diameter) << "\n";

        if (!candidate.isValid || candidate.outerCircle.curveTag == NULL_TAG)
        {
            out << "结果: 沉孔候选无效";
            return out.str();
        }

        tag_t parentCurveTag = NULL_TAG;
        tag_t flatPatternFeatureTag = NULL_TAG;
        std::array<double, 3> flatPatternCenter {};
        double flatPatternDiameter = 0.0;
        const bool resolvedFlatPattern = TryResolveFlatPatternParentCurve(
            candidate.outerCircle.curveTag,
            parentCurveTag,
            flatPatternFeatureTag,
            flatPatternCenter,
            flatPatternDiameter);

        out << "Flat Pattern父曲线: " << (resolvedFlatPattern ? "已找到" : "未找到") << "\n";
        if (!resolvedFlatPattern)
        {
            out << "最终方向: 未知";
            return out.str();
        }

        out << "父曲线Tag: " << parentCurveTag << "\n";
        out << "父曲线圆心: [" << flatPatternCenter[0] << ", " << flatPatternCenter[1] << ", " << flatPatternCenter[2] << "]\n";
        out << "父曲线直径: " << FormatDiameterText(flatPatternDiameter) << "\n";
        out << "Flat Pattern特征Tag: " << flatPatternFeatureTag << "\n";

        const std::vector<tag_t> flatSolidFeatures = CollectSbFlatSolidFeatures(flatPatternFeatureTag);
        out << "SB_FLAT_SOLID特征数: " << flatSolidFeatures.size() << "\n";

        int matchedEdgeCount = 0;
        int planeFrontCount = 0;
        int planeBackCount = 0;
        int planeUnknownCount = 0;
        constexpr double kCenterTolerance = 0.02;
        constexpr double kDiameterTolerance = 0.02;

        for (size_t featureIndex = 0; featureIndex < flatSolidFeatures.size(); ++featureIndex)
        {
            const tag_t flatSolidFeatureTag = flatSolidFeatures[featureIndex];
            out << "SB_FLAT_SOLID[" << featureIndex << "]: " << flatSolidFeatureTag << "\n";

            tag_t bodyTag = NULL_TAG;
            if (UF_MODL_ask_feat_body(flatSolidFeatureTag, &bodyTag) != 0 || bodyTag == NULL_TAG)
            {
                out << "  Body: 未找到\n";
                continue;
            }

            out << "  BodyTag: " << bodyTag << "\n";
            uf_list_p_t edgeList = nullptr;
            if (UF_MODL_ask_body_edges(bodyTag, &edgeList) != 0 || edgeList == nullptr)
            {
                out << "  Body边: 读取失败\n";
                continue;
            }

            int edgeCount = 0;
            UF_MODL_ask_list_count(edgeList, &edgeCount);
            out << "  Body边数: " << edgeCount << "\n";
            for (int edgeIndex = 0; edgeIndex < edgeCount; ++edgeIndex)
            {
                tag_t edgeTag = NULL_TAG;
                if (UF_MODL_ask_list_item(edgeList, edgeIndex, &edgeTag) != 0 || edgeTag == NULL_TAG)
                {
                    continue;
                }

                double edgeDiameter = 0.0;
                std::array<double, 3> edgeCenter {};
                if (!TryAskArcData(edgeTag, edgeDiameter, edgeCenter))
                {
                    continue;
                }

                if (std::abs(edgeDiameter - flatPatternDiameter) > kDiameterTolerance ||
                    std::abs(edgeCenter[0] - flatPatternCenter[0]) > kCenterTolerance ||
                    std::abs(edgeCenter[1] - flatPatternCenter[1]) > kCenterTolerance)
                {
                    continue;
                }

                ++matchedEdgeCount;
                out << "  匹配圆边Tag: " << edgeTag
                    << " 圆心=[" << edgeCenter[0] << ", " << edgeCenter[1] << ", " << edgeCenter[2] << "]"
                    << " 直径=" << FormatDiameterText(edgeDiameter) << "\n";

                uf_list_p_t faceList = nullptr;
                if (UF_MODL_ask_edge_faces(edgeTag, &faceList) != 0 || faceList == nullptr)
                {
                    out << "    相邻面: 读取失败\n";
                    continue;
                }

                int faceCount = 0;
                UF_MODL_ask_list_count(faceList, &faceCount);
                out << "    相邻面数: " << faceCount << "\n";
                for (int faceIndex = 0; faceIndex < faceCount; ++faceIndex)
                {
                    tag_t faceTag = NULL_TAG;
                    if (UF_MODL_ask_list_item(faceList, faceIndex, &faceTag) != 0 || faceTag == NULL_TAG)
                    {
                        continue;
                    }

                    int faceType = 0;
                    double point[3] = {0.0};
                    double dir[3] = {0.0};
                    double box[6] = {0.0};
                    double radius = 0.0;
                    double radData = 0.0;
                    int normDir = 1;
                    if (UF_MODL_ask_face_data(faceTag, &faceType, point, dir, box, &radius, &radData, &normDir) != 0)
                    {
                        continue;
                    }

                    if (faceType != 22)
                    {
                        out << "    面[" << faceIndex << "]: tag=" << faceTag
                            << " type=" << faceType
                            << " 跳过=非平面面\n";
                        continue;
                    }

                    double uvMinMax[4] = {0.0};
                    if (UF_MODL_ask_face_uv_minmax(faceTag, uvMinMax) != 0)
                    {
                        out << "    面[" << faceIndex << "]: tag=" << faceTag
                            << " type=" << faceType
                            << " 读取失败=ask_face_uv_minmax\n";
                        continue;
                    }

                    double param[2] = {
                        (uvMinMax[0] + uvMinMax[1]) * 0.5,
                        (uvMinMax[2] + uvMinMax[3]) * 0.5
                    };
                    double facePoint[3] = {0.0};
                    double u1[3] = {0.0};
                    double v1[3] = {0.0};
                    double u2[3] = {0.0};
                    double v2[3] = {0.0};
                    double unitNorm[3] = {0.0};
                    double radii[2] = {0.0};
                    if (UF_MODL_ask_face_props(faceTag, param, facePoint, u1, v1, u2, v2, unitNorm, radii) != 0)
                    {
                        out << "    面[" << faceIndex << "]: tag=" << faceTag
                            << " type=" << faceType
                            << " 读取失败=ask_face_props\n";
                        continue;
                    }

                    std::array<double, 3> faceNormal {unitNorm[0], unitNorm[1], unitNorm[2]};
                    if (!Normalize(faceNormal))
                    {
                        continue;
                    }

                    std::string faceSide = "未知";
                    if (faceNormal[2] > 0.15)
                    {
                        faceSide = "反面(外法向+Z)";
                        ++planeFrontCount;
                    }
                    else if (faceNormal[2] < -0.15)
                    {
                        faceSide = "正面(外法向-Z)";
                        ++planeBackCount;
                    }
                    else
                    {
                        ++planeUnknownCount;
                    }

                    out << "    面[" << faceIndex << "]: tag=" << faceTag
                        << " type=" << faceType
                        << " normal=[" << faceNormal[0] << ", " << faceNormal[1] << ", " << faceNormal[2] << "]"
                        << " 判定=" << faceSide << "\n";
                }

                UF_MODL_delete_list(&faceList);
            }

            UF_MODL_delete_list(&edgeList);
        }

        const CounterboreSide side = ResolveCounterboreSide(candidate);
        out << "匹配圆边数: " << matchedEdgeCount << "\n";
        out << "平面面统计: 正面=" << planeFrontCount
            << " 反面=" << planeBackCount
            << " 未知=" << planeUnknownCount << "\n";
        out << "最终方向: ";
        if (side == CounterboreSide::Front)
        {
            out << "正面";
        }
        else if (side == CounterboreSide::Back)
        {
            out << "反面";
        }
        else
        {
            out << "未知";
        }

        return out.str();
    }

    void DraftingAnnotationService::ColorCounterboreFlatPatternDebugFaces(const CounterboreCandidate& candidate) const
    {
        RestoreDebugFaceColors();

        if (!candidate.isValid || candidate.outerCircle.curveTag == NULL_TAG)
        {
            return;
        }

        tag_t parentCurveTag = NULL_TAG;
        tag_t flatPatternFeatureTag = NULL_TAG;
        std::array<double, 3> flatPatternCenter {};
        double flatPatternDiameter = 0.0;
        if (!TryResolveFlatPatternParentCurve(
                candidate.outerCircle.curveTag,
                parentCurveTag,
                flatPatternFeatureTag,
                flatPatternCenter,
                flatPatternDiameter))
        {
            return;
        }

        const std::vector<tag_t> flatSolidFeatures = CollectSbFlatSolidFeatures(flatPatternFeatureTag);
        constexpr double kCenterTolerance = 0.02;
        constexpr double kDiameterTolerance = 0.02;
        constexpr int kFrontFaceColor = 36;
        constexpr int kBackFaceColor = 186;
        constexpr int kUnknownFaceColor = 25;

        for (const tag_t flatSolidFeatureTag : flatSolidFeatures)
        {
            tag_t bodyTag = NULL_TAG;
            if (UF_MODL_ask_feat_body(flatSolidFeatureTag, &bodyTag) != 0 || bodyTag == NULL_TAG)
            {
                continue;
            }

            uf_list_p_t edgeList = nullptr;
            if (UF_MODL_ask_body_edges(bodyTag, &edgeList) != 0 || edgeList == nullptr)
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

                double edgeDiameter = 0.0;
                std::array<double, 3> edgeCenter {};
                if (!TryAskArcData(edgeTag, edgeDiameter, edgeCenter))
                {
                    continue;
                }

                if (std::abs(edgeDiameter - flatPatternDiameter) > kDiameterTolerance ||
                    std::abs(edgeCenter[0] - flatPatternCenter[0]) > kCenterTolerance ||
                    std::abs(edgeCenter[1] - flatPatternCenter[1]) > kCenterTolerance)
                {
                    continue;
                }

                uf_list_p_t faceList = nullptr;
                if (UF_MODL_ask_edge_faces(edgeTag, &faceList) != 0 || faceList == nullptr)
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
                    double point[3] = {0.0};
                    double dir[3] = {0.0};
                    double box[6] = {0.0};
                    double radius = 0.0;
                    double radData = 0.0;
                    int normDir = 1;
                    if (UF_MODL_ask_face_data(faceTag, &faceType, point, dir, box, &radius, &radData, &normDir) != 0)
                    {
                        continue;
                    }

                    if (faceType != 22)
                    {
                        continue;
                    }

                    std::array<double, 3> faceNormal {dir[0], dir[1], dir[2]};
                    if (normDir < 0)
                    {
                        faceNormal[0] *= -1.0;
                        faceNormal[1] *= -1.0;
                        faceNormal[2] *= -1.0;
                    }

                    if (!Normalize(faceNormal))
                    {
                        continue;
                    }

                    if (faceNormal[2] > 0.15)
                    {
                        ApplyDebugFaceColor(faceTag, kFrontFaceColor);
                    }
                    else if (faceNormal[2] < -0.15)
                    {
                        ApplyDebugFaceColor(faceTag, kBackFaceColor);
                    }
                    else
                    {
                        ApplyDebugFaceColor(faceTag, kUnknownFaceColor);
                    }
                    break;
                }

                UF_MODL_delete_list(&faceList);
            }

            UF_MODL_delete_list(&edgeList);
        }
    }

    std::string DraftingAnnotationService::BuildFallbackHoleLabel(
        const DraftingRequest& request,
        int sameDiameterCount) const
    {
        std::ostringstream out;
        if (sameDiameterCount > 1)
        {
            out << sameDiameterCount << "-";
        }
        if (request.type == AnnotationType::Counterbore)
        {
            return BuildCounterboreAnnotation(request.selectedDiameter, sameDiameterCount, CounterboreSide::Unknown);
        }
        return out.str();
    }

    std::string DraftingAnnotationService::BuildAnnotationLabel(
        const DraftingRequest& request,
        const RuleRecord& rule,
        int sameDiameterCount) const
    {
        std::ostringstream out;
        if (sameDiameterCount > 1)
        {
            out << sameDiameterCount << "-";
        }

        if (!rule.displayText.empty())
        {
            out << rule.displayText;
        }
        else if (!rule.threadSpec.empty())
        {
            out << rule.threadSpec;
        }
        else
        {
            out << rule.size;
        }

        if (request.lengthValue > 0 &&
            (request.type == AnnotationType::PemScrew || request.type == AnnotationType::PemStud))
        {
            out << "x" << request.lengthValue;
        }

        return out.str();
    }

    std::string DraftingAnnotationService::BuildPreviewText(
        const DraftingRequest& request,
        const std::vector<RuleRecord>& rules,
        int sameDiameterCount) const
    {
        std::ostringstream out;
        out << "标注类型: " << ToRuleKey(request.type) << "\n";
        out << "识别孔径: " << std::fixed << std::setprecision(2) << request.selectedDiameter << " mm\n";
        out << "同直径数量: " << sameDiameterCount << "\n";
        out << "孔边标识: " << (request.enableLetterTag ? "开启" : "关闭") << "\n";
        if (request.enableLetterTag && !request.markerCode.empty())
        {
            out << "起始标识: " << request.markerCode << "\n";
        }
        if (!request.subtype.empty())
        {
            out << "子类型: " << request.subtype << "\n";
        }
        if (request.lengthValue > 0)
        {
            out << "输入长度: " << request.lengthValue << " mm\n";
        }

        if (rules.empty())
        {
            out << "查表结果: 未找到匹配规则，已回退为孔标注\n";
            out << "图纸标注: " << BuildFallbackHoleLabel(request, sameDiameterCount);
            return out.str();
        }

        const RuleRecord& primary = rules.front();
        out << "推荐系列: " << primary.series << "\n";
        out << "推荐规格: " << primary.size << "\n";
        out << "螺纹/型号: " << primary.threadSpec << "\n";
        if (!primary.lengthText.empty())
        {
            out << "标准长度: " << primary.lengthText << "\n";
        }
        out << "标准注释: " << primary.standardComment << "\n";
        out << "图纸标注: " << BuildAnnotationLabel(request, primary, sameDiameterCount);

        if (rules.size() > 1)
        {
            out << "\n候选数量: " << rules.size() << "，请继续收紧子类型或规则表。";
        }

        return out.str();
    }

    bool DraftingAnnotationService::IsDraftingMode() const
    {
        NXOpen::Session* session = NXOpen::Session::GetSession();
        NXOpen::BasePart* displayPart = session->Parts()->Display();
        return displayPart != nullptr;
    }
}
