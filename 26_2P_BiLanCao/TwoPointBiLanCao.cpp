#include "TwoPointBiLanCao.hpp"

#ifdef CreateDialog
#undef CreateDialog
#endif

#include <NXOpen/BlockStyler_PropertyList.hxx>
#include <NXOpen/BlockStyler_SelectObject.hxx>
#include <NXOpen/Direction.hxx>
#include <NXOpen/DirectionCollection.hxx>
#include <NXOpen/Edge.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/Measurement.hxx>
#include <NXOpen/NXObject.hxx>
#include <NXOpen/BodyCollection.hxx>
#include <NXOpen/Features_BlockFeatureBuilder.hxx>
#include <NXOpen/EdgeDumbRule.hxx>
#include <NXOpen/Features_ExtrudeBuilder.hxx>
#include <NXOpen/Features_Feature.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <NXOpen/GeometricUtilities_BooleanOperation.hxx>
#include <NXOpen/GeometricUtilities_Extend.hxx>
#include <NXOpen/GeometricUtilities_FeatureOffset.hxx>
#include <NXOpen/GeometricUtilities_FeatureOptions.hxx>
#include <NXOpen/GeometricUtilities_Limits.hxx>
#include <NXOpen/GeometricUtilities_SmartVolumeProfileBuilder.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Point.hxx>
#include <NXOpen/ScRuleFactory.hxx>
#include <NXOpen/Section.hxx>
#include <NXOpen/SectionCollection.hxx>
#include <NXOpen/Selection.hxx>
#include <NXOpen/SelectionIntentRule.hxx>
#include <NXOpen/SelectionIntentRuleOptions.hxx>
#include <NXOpen/SmartObject.hxx>

#include <uf.h>
#include <uf_curve.h>
#include <uf_modl.h>
#include <uf_modl_sweep.h>
#include <uf_modl_utilities.h>
#include <uf_obj.h>
#include <uf_object_types.h>
#include <uf_sket.h>
#include <uf_so.h>
#include <uf_undo.h>
#include <uf_ui.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifdef CreateDialog
#undef CreateDialog
#endif

namespace
{
constexpr double kPointTolerance = 1.0e-6;
constexpr double kEndpointTolerance = 1.0e-4;
constexpr double kLooseEndpointTolerance = 5.0e-2;
constexpr double kPlaneTolerance = 1.0e-4;

std::filesystem::path CurrentModuleDirectory()
{
    HMODULE module = nullptr;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&CurrentModuleDirectory),
                           &module))
    {
        wchar_t moduleFileName[MAX_PATH] = {0};
        const DWORD length = GetModuleFileNameW(module, moduleFileName, MAX_PATH);
        if (length > 0 && length < MAX_PATH)
        {
            return std::filesystem::path(moduleFileName).parent_path();
        }
    }

    return std::filesystem::path(__FILE__).parent_path();
}

double PointDistance(const NXOpen::Point3d& a, const NXOpen::Point3d& b)
{
    const double dx = a.X - b.X;
    const double dy = a.Y - b.Y;
    const double dz = a.Z - b.Z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

bool AskObjectType(NXOpen::TaggedObject* object, int& type, int& subtype)
{
    type = 0;
    subtype = 0;
    if (object == nullptr || object->Tag() == NULL_TAG)
    {
        return false;
    }

    return UF_OBJ_ask_type_and_subtype(object->Tag(), &type, &subtype) == 0;
}

NXOpen::Vector3d Subtract(const NXOpen::Point3d& a, const NXOpen::Point3d& b)
{
    return NXOpen::Vector3d(a.X - b.X, a.Y - b.Y, a.Z - b.Z);
}

NXOpen::Vector3d Cross(const NXOpen::Vector3d& a, const NXOpen::Vector3d& b)
{
    return NXOpen::Vector3d(
        a.Y * b.Z - a.Z * b.Y,
        a.Z * b.X - a.X * b.Z,
        a.X * b.Y - a.Y * b.X);
}

double Dot(const NXOpen::Vector3d& a, const NXOpen::Vector3d& b)
{
    return a.X * b.X + a.Y * b.Y + a.Z * b.Z;
}

double Length(const NXOpen::Vector3d& vector)
{
    return std::sqrt(Dot(vector, vector));
}

bool Normalize(NXOpen::Vector3d& vector)
{
    const double length = Length(vector);
    if (length <= kPointTolerance)
    {
        return false;
    }

    vector.X /= length;
    vector.Y /= length;
    vector.Z /= length;
    return true;
}

NXOpen::Point3d Add(const NXOpen::Point3d& point, const NXOpen::Vector3d& vector)
{
    return NXOpen::Point3d(point.X + vector.X, point.Y + vector.Y, point.Z + vector.Z);
}

NXOpen::Vector3d Scale(const NXOpen::Vector3d& vector, double scale)
{
    return NXOpen::Vector3d(vector.X * scale, vector.Y * scale, vector.Z * scale);
}

NXOpen::Vector3d ProjectVectorToPlane(const NXOpen::Vector3d& vector, const NXOpen::Vector3d& normal)
{
    return NXOpen::Vector3d(
        vector.X - normal.X * Dot(vector, normal),
        vector.Y - normal.Y * Dot(vector, normal),
        vector.Z - normal.Z * Dot(vector, normal));
}

double DetInPlane(const NXOpen::Vector3d& a, const NXOpen::Vector3d& b, const NXOpen::Vector3d& planeNormal)
{
    return Dot(Cross(a, b), planeNormal);
}

void CopyPointToArray(const NXOpen::Point3d& point, double arrayPoint[3])
{
    arrayPoint[0] = point.X;
    arrayPoint[1] = point.Y;
    arrayPoint[2] = point.Z;
}

double EdgeChordLength(NXOpen::Edge* edge)
{
    if (edge == nullptr)
    {
        return 0.0;
    }

    try
    {
        NXOpen::Point3d vertex1;
        NXOpen::Point3d vertex2;
        edge->GetVertices(&vertex1, &vertex2);
        return PointDistance(vertex1, vertex2);
    }
    catch (...)
    {
    }

    return 0.0;
}

bool EdgeEndpoints(NXOpen::Edge* edge, NXOpen::Point3d& vertex1, NXOpen::Point3d& vertex2)
{
    if (edge == nullptr)
    {
        return false;
    }

    try
    {
        edge->GetVertices(&vertex1, &vertex2);
        return PointDistance(vertex1, vertex2) > kPointTolerance;
    }
    catch (...)
    {
    }

    return false;
}

bool EdgeMidPoint(NXOpen::Edge* edge, NXOpen::Point3d& midpoint)
{
    NXOpen::Point3d vertex1;
    NXOpen::Point3d vertex2;
    if (!EdgeEndpoints(edge, vertex1, vertex2))
    {
        return false;
    }

    midpoint = NXOpen::Point3d(
        (vertex1.X + vertex2.X) * 0.5,
        (vertex1.Y + vertex2.Y) * 0.5,
        (vertex1.Z + vertex2.Z) * 0.5);
    return true;
}

bool ProjectPointToEdgeChord(NXOpen::Edge* edge, const NXOpen::Point3d& sourcePoint, NXOpen::Point3d& projectedPoint)
{
    NXOpen::Point3d vertex1;
    NXOpen::Point3d vertex2;
    if (!EdgeEndpoints(edge, vertex1, vertex2))
    {
        return false;
    }

    NXOpen::Vector3d edgeVector = Subtract(vertex2, vertex1);
    const double edgeLengthSquared = Dot(edgeVector, edgeVector);
    if (edgeLengthSquared <= kPointTolerance * kPointTolerance)
    {
        return false;
    }

    const double parameter = Dot(Subtract(sourcePoint, vertex1), edgeVector) / edgeLengthSquared;
    projectedPoint = Add(vertex1, Scale(edgeVector, parameter));
    return true;
}

std::string FormatLimit(double value)
{
    std::ostringstream stream;
    stream << std::setprecision(15) << value;
    return stream.str();
}

std::string UfFailureMessage(int code)
{
    if (code == 0)
    {
        return "OK";
    }

    char message[133] = {0};
    if (UF_get_fail_message(code, message) == 0)
    {
        return std::string(message);
    }

    return std::string();
}

std::string TagText(tag_t tag)
{
    std::ostringstream stream;
    stream << tag;
    return stream.str();
}

std::vector<tag_t> AskBodyFeatureTags(tag_t bodyTag)
{
    std::vector<tag_t> featureTags;
    if (bodyTag == NULL_TAG)
    {
        return featureTags;
    }

    uf_list_p_t featureList = NULL;
    if (UF_MODL_ask_body_feats(bodyTag, &featureList) != 0 || featureList == NULL)
    {
        return featureTags;
    }

    int featureCount = 0;
    if (UF_MODL_ask_list_count(featureList, &featureCount) == 0)
    {
        for (int index = 0; index < featureCount; ++index)
        {
            tag_t featureTag = NULL_TAG;
            if (UF_MODL_ask_list_item(featureList, index, &featureTag) == 0 &&
                featureTag != NULL_TAG)
            {
                featureTags.push_back(featureTag);
            }
        }
    }
    UF_MODL_delete_list(&featureList);
    return featureTags;
}

NXOpen::Vector3d InwardOffsetDirectionInPlane(
    const NXOpen::Vector3d& faceDirection,
    const NXOpen::Vector3d& planeNormal,
    const NXOpen::Vector3d& bisectorDirection)
{
    NXOpen::Vector3d offsetDirection = Cross(planeNormal, faceDirection);
    if (!Normalize(offsetDirection))
    {
        return NXOpen::Vector3d(0.0, 0.0, 0.0);
    }
    if (Dot(offsetDirection, bisectorDirection) < 0.0)
    {
        offsetDirection = Scale(offsetDirection, -1.0);
    }
    return offsetDirection;
}

bool IntersectCoplanarLines(
    const NXOpen::Point3d& point1,
    const NXOpen::Vector3d& direction1,
    const NXOpen::Point3d& point2,
    const NXOpen::Vector3d& direction2,
    const NXOpen::Vector3d& planeNormal,
    NXOpen::Point3d& intersection)
{
    const double denominator = DetInPlane(direction1, direction2, planeNormal);
    if (std::fabs(denominator) <= kPointTolerance)
    {
        return false;
    }

    const NXOpen::Vector3d delta = Subtract(point2, point1);
    const double parameter = DetInPlane(delta, direction2, planeNormal) / denominator;
    intersection = Add(point1, Scale(direction1, parameter));
    return true;
}

void AppendDebugLog(const std::string& message)
{
    try
    {
        const std::filesystem::path logPath =
            CurrentModuleDirectory() / "TwoPointBiLanCao_debug.log";
        std::ofstream stream(logPath, std::ios::app);
        if (stream)
        {
            SYSTEMTIME now;
            GetLocalTime(&now);
            stream << std::setfill('0')
                   << now.wYear << "-"
                   << std::setw(2) << now.wMonth << "-"
                   << std::setw(2) << now.wDay << " "
                   << std::setw(2) << now.wHour << ":"
                   << std::setw(2) << now.wMinute << ":"
                   << std::setw(2) << now.wSecond << "."
                   << std::setw(3) << now.wMilliseconds << "  "
                   << message << "\n";
        }
    }
    catch (...)
    {
    }
}
}

TwoPointBiLanCaoDialog::TwoPointBiLanCaoDialog()
    : ui_(NXOpen::UI::GetUI()),
      session_(NXOpen::Session::GetSession()),
      dialog_(nullptr),
      mainGroup_(nullptr),
      startPointBlock_(nullptr),
      endPointBlock_(nullptr),
      slotClearanceBlock_(nullptr),
      bendRadiusBlock_(nullptr),
      chamferEdgeToggleBlock_(nullptr),
      gapOnlyToggleBlock_(nullptr),
      normalDirectionBlock_(nullptr),
      wrapCornerModeBlock_(nullptr),
      cachedStartSelectedObject_(nullptr),
      cachedTargetBody_(nullptr),
      previewProfileCurveTags_(),
      liveSlotUndoMark_(0),
      hasLiveSlot_(false),
      liveSlotRefreshInProgress_(false),
      liveSlotSignature_(),
      liveSlotFeatureTags_(),
      liveSlotCurveTags_(),
      liveSlotToolBodyTags_(),
      liveSlotToolBodyTag_(NULL_TAG),
      slotCreatedOnSelection_(false)
{
    dialog_ = ui_->CreateDialog(GetDialogFilePath().c_str());
    dialog_->AddInitializeHandler(NXOpen::make_callback(this, &TwoPointBiLanCaoDialog::initialize_cb));
    dialog_->AddDialogShownHandler(NXOpen::make_callback(this, &TwoPointBiLanCaoDialog::dialogShown_cb));
    dialog_->AddEnableOKButtonHandler(NXOpen::make_callback(this, &TwoPointBiLanCaoDialog::enable_ok_cb));
    dialog_->AddUpdateHandler(NXOpen::make_callback(this, &TwoPointBiLanCaoDialog::update_cb));
    dialog_->AddApplyHandler(NXOpen::make_callback(this, &TwoPointBiLanCaoDialog::apply_cb));
    dialog_->AddOkHandler(NXOpen::make_callback(this, &TwoPointBiLanCaoDialog::ok_cb));
    dialog_->AddCancelHandler(NXOpen::make_callback(this, &TwoPointBiLanCaoDialog::cancel_cb));
    dialog_->AddFocusNotifyHandler(NXOpen::make_callback(this, &TwoPointBiLanCaoDialog::focus_notify_cb));
    dialog_->AddKeyboardFocusNotifyHandler(
        NXOpen::make_callback(this, &TwoPointBiLanCaoDialog::keyboard_focus_notify_cb));
}

TwoPointBiLanCaoDialog::~TwoPointBiLanCaoDialog()
{
    RevertLiveSlot();
    delete dialog_;
    dialog_ = nullptr;
}

NXOpen::BlockStyler::BlockDialog::DialogResponse TwoPointBiLanCaoDialog::Launch()
{
    return dialog_->Launch();
}

std::string TwoPointBiLanCaoDialog::GetDialogFilePath() const
{
    return (CurrentModuleDirectory() / "TwoPointBiLanCao.dlx").string();
}

void TwoPointBiLanCaoDialog::initialize_cb()
{
    AppendDebugLog("TwoPointBiLanCao build marker: cube-endpoint-effective-angle-20260522-1507");

    mainGroup_ = dialog_->TopBlock()->FindBlock("group0");
    startPointBlock_ = dialog_->TopBlock()->FindBlock("selection0");
    endPointBlock_ = dialog_->TopBlock()->FindBlock("selection01");
    slotClearanceBlock_ = dialog_->TopBlock()->FindBlock("string0");
    bendRadiusBlock_ = dialog_->TopBlock()->FindBlock("string01");
    chamferEdgeToggleBlock_ = dialog_->TopBlock()->FindBlock("toggle0");
    gapOnlyToggleBlock_ = dialog_->TopBlock()->FindBlock("gapOnlyToggle");
    normalDirectionBlock_ = dialog_->TopBlock()->FindBlock("normalDirectionSelection");
    wrapCornerModeBlock_ = dialog_->TopBlock()->FindBlock("wrapCornerMode");

    if (startPointBlock_ == nullptr || endPointBlock_ == nullptr ||
        slotClearanceBlock_ == nullptr || bendRadiusBlock_ == nullptr ||
        chamferEdgeToggleBlock_ == nullptr || gapOnlyToggleBlock_ == nullptr ||
        normalDirectionBlock_ == nullptr || wrapCornerModeBlock_ == nullptr)
    {
        throw NXOpen::NXException::Create(1, "TwoPointBiLanCao dialog block initialization failed.");
    }

    ConfigurePointSelectionsForCurrentMode();

    SetDouble(slotClearanceBlock_, std::max(0.1, ReadDouble(slotClearanceBlock_, 0.5)));
    SetDouble(bendRadiusBlock_, std::max(0.0, ReadDouble(bendRadiusBlock_, 1.0)));
}

void TwoPointBiLanCaoDialog::dialogShown_cb()
{
    ConfigurePointSelectionsForCurrentMode();
}

bool TwoPointBiLanCaoDialog::enable_ok_cb()
{
    try
    {
        if (hasLiveSlot_ || slotCreatedOnSelection_)
        {
            return true;
        }

        DialogValues values;
        const bool canApply = ReadDialogValues(values);
        if (canApply && !hasLiveSlot_ && !liveSlotRefreshInProgress_)
        {
            RefreshLiveSlot();
        }
        return canApply;
    }
    catch (const NXOpen::NXException& ex)
    {
        AppendDebugLog(std::string("enable_ok_cb NXException: ") + ex.Message());
    }
    catch (const std::exception& ex)
    {
        AppendDebugLog(std::string("enable_ok_cb std::exception: ") + ex.what());
    }
    catch (...)
    {
        AppendDebugLog("enable_ok_cb unknown exception");
    }
    return false;
}

int TwoPointBiLanCaoDialog::update_cb(NXOpen::BlockStyler::UIBlock* block)
{
    try
    {
        const bool isEndBlock = block == endPointBlock_;

        if (block == startPointBlock_)
        {
            CacheStartPointOwner();
        }
        else if (block == slotClearanceBlock_)
        {
            SetDouble(slotClearanceBlock_, std::max(0.1, ReadDouble(slotClearanceBlock_, 0.5)));
            if (hasLiveSlot_)
            {
                RevertLiveSlot();
                RefreshLiveSlot();
            }
        }
        else if (block == bendRadiusBlock_)
        {
            SetDouble(bendRadiusBlock_, std::max(0.0, ReadDouble(bendRadiusBlock_, 1.0)));
            if (hasLiveSlot_)
            {
                RevertLiveSlot();
                RefreshLiveSlot();
            }
        }
        else if (block == chamferEdgeToggleBlock_)
        {
            if (ReadLogical(gapOnlyToggleBlock_, false) && ReadLogical(chamferEdgeToggleBlock_, false))
            {
                SetLogical(gapOnlyToggleBlock_, false);
                ConfigurePointSelectionsForCurrentMode();
            }
            if (hasLiveSlot_)
            {
                RevertLiveSlot();
                RefreshLiveSlot();
            }
        }
        else if (block == gapOnlyToggleBlock_)
        {
            if (ReadLogical(gapOnlyToggleBlock_, false))
            {
                SetLogical(chamferEdgeToggleBlock_, false);
            }
            ConfigurePointSelectionsForCurrentMode();
            if (hasLiveSlot_)
            {
                RevertLiveSlot();
                RefreshLiveSlot();
            }
        }
        else if (block == normalDirectionBlock_)
        {
            if (hasLiveSlot_)
            {
                RevertLiveSlot();
                RefreshLiveSlot();
            }
        }
        else if (block == wrapCornerModeBlock_)
        {
            if (hasLiveSlot_)
            {
                RevertLiveSlot();
                RefreshLiveSlot();
            }
        }

        if (isEndBlock && !hasLiveSlot_ && !slotCreatedOnSelection_)
        {
            DialogValues values;
            const bool valuesRead = ReadDialogValues(values);

            if (valuesRead)
            {
                const bool profileCreated = CreateReliefSlotProfile(values, true);
                slotCreatedOnSelection_ = profileCreated;
                if (profileCreated)
                {
                    hasLiveSlot_ = true;
                    liveSlotSignature_ = BuildLiveSlotSignature(values);
                    liveSlotFeatureTags_ = values.profileFeatureTags;
                    liveSlotCurveTags_ = values.profileCurveTags;
                    liveSlotToolBodyTags_ = values.profileToolBodyTags;
                    liveSlotToolBodyTag_ = values.profileToolBodyTag;
                }
            }
        }
    }
    catch (const NXOpen::NXException& ex)
    {
        AppendDebugLog(std::string("update_cb NXException: ") + ex.Message());
    }
    catch (const std::exception& ex)
    {
        AppendDebugLog(std::string("update_cb std::exception: ") + ex.what());
    }
    catch (...)
    {
        AppendDebugLog("update_cb unknown exception");
    }

    return 0;
}

int TwoPointBiLanCaoDialog::apply_cb()
{
    return Execute();
}

int TwoPointBiLanCaoDialog::ok_cb()
{
    return Execute();
}

int TwoPointBiLanCaoDialog::cancel_cb()
{
    RevertLiveSlot();
    return 0;
}

void TwoPointBiLanCaoDialog::focus_notify_cb(NXOpen::BlockStyler::UIBlock* block, bool focus)
{
    if (hasLiveSlot_)
    {
        return;
    }

    if (block == startPointBlock_ || block == endPointBlock_)
    {
        RefreshLiveSlot();
    }
}

void TwoPointBiLanCaoDialog::keyboard_focus_notify_cb(NXOpen::BlockStyler::UIBlock* block, bool focus)
{
    if (focus)
    {
        return;
    }

    if (block == slotClearanceBlock_)
    {
        SetDouble(slotClearanceBlock_, std::max(0.1, ReadDouble(slotClearanceBlock_, 0.5)));
        if (hasLiveSlot_)
        {
            RevertLiveSlot();
            RefreshLiveSlot();
        }
    }
    else if (block == bendRadiusBlock_)
    {
        SetDouble(bendRadiusBlock_, std::max(0.0, ReadDouble(bendRadiusBlock_, 1.0)));
        if (hasLiveSlot_)
        {
            RevertLiveSlot();
            RefreshLiveSlot();
        }
    }
    else if (block == chamferEdgeToggleBlock_)
    {
        if (ReadLogical(gapOnlyToggleBlock_, false) && ReadLogical(chamferEdgeToggleBlock_, false))
        {
            SetLogical(gapOnlyToggleBlock_, false);
            ConfigurePointSelectionsForCurrentMode();
        }
        if (hasLiveSlot_)
        {
            RevertLiveSlot();
            RefreshLiveSlot();
        }
    }
    else if (block == gapOnlyToggleBlock_)
    {
        if (ReadLogical(gapOnlyToggleBlock_, false))
        {
            SetLogical(chamferEdgeToggleBlock_, false);
        }
        ConfigurePointSelectionsForCurrentMode();
        if (hasLiveSlot_)
        {
            RevertLiveSlot();
            RefreshLiveSlot();
        }
    }
}

bool TwoPointBiLanCaoDialog::ReadSelectedPoint(NXOpen::BlockStyler::UIBlock* block,
                                               NXOpen::TaggedObject*& selectedObject,
                                               NXOpen::Point3d& point) const
{
    selectedObject = GetFirstSelectedObject(block);
    if (selectedObject == nullptr)
    {
        return false;
    }

    if (NXOpen::Point* selectedPoint = dynamic_cast<NXOpen::Point*>(selectedObject))
    {
        point = selectedPoint->Coordinates();
        return true;
    }

    NXOpen::BlockStyler::PropertyList* properties = block->GetProperties();
    if (properties == nullptr)
    {
        return false;
    }

    try
    {
        point = properties->GetPoint("PickPoint");
        delete properties;

        if (NXOpen::Edge* selectedEdge = dynamic_cast<NXOpen::Edge*>(selectedObject))
        {
            NXOpen::Point3d vertex1;
            NXOpen::Point3d vertex2;
            selectedEdge->GetVertices(&vertex1, &vertex2);
            point = PointDistance(point, vertex1) <= PointDistance(point, vertex2) ? vertex1 : vertex2;
        }

        return true;
    }
    catch (...)
    {
        delete properties;
        return false;
    }
}

NXOpen::TaggedObject* TwoPointBiLanCaoDialog::GetFirstSelectedObject(NXOpen::BlockStyler::UIBlock* block) const
{
    if (block == nullptr)
    {
        return nullptr;
    }

    NXOpen::BlockStyler::PropertyList* properties = block->GetProperties();
    if (properties == nullptr)
    {
        return nullptr;
    }

    try
    {
        const std::vector<NXOpen::TaggedObject*> selectedObjects =
            properties->GetTaggedObjectVector("SelectedObjects");
        delete properties;
        if (!selectedObjects.empty())
        {
            return selectedObjects.front();
        }
    }
    catch (...)
    {
        delete properties;
        return nullptr;
    }

    return nullptr;
}

bool TwoPointBiLanCaoDialog::ReadOptionalVector(NXOpen::BlockStyler::UIBlock* block,
                                                NXOpen::Vector3d& vector) const
{
    vector = NXOpen::Vector3d(0.0, 0.0, 0.0);
    if (block == nullptr)
    {
        return false;
    }

    NXOpen::BlockStyler::PropertyList* properties = block->GetProperties();
    if (properties == nullptr)
    {
        return false;
    }

    bool hasVector = false;
    try
    {
        const std::vector<NXOpen::TaggedObject*> selectedObjects =
            properties->GetTaggedObjectVector("SelectedObjects");
        if (!selectedObjects.empty())
        {
            if (NXOpen::Direction* direction = dynamic_cast<NXOpen::Direction*>(selectedObjects.front()))
            {
                vector = direction->Vector();
                hasVector = Normalize(vector);
            }
        }
    }
    catch (...)
    {
    }

    delete properties;
    return hasVector;
}

NXOpen::Body* TwoPointBiLanCaoDialog::GetOwnerBody(NXOpen::TaggedObject* object) const
{
    if (object == nullptr)
    {
        return nullptr;
    }

    if (NXOpen::Body* body = dynamic_cast<NXOpen::Body*>(object))
    {
        return body;
    }

    if (NXOpen::Face* face = dynamic_cast<NXOpen::Face*>(object))
    {
        return face->GetBody();
    }

    if (NXOpen::Edge* edge = dynamic_cast<NXOpen::Edge*>(object))
    {
        return edge->GetBody();
    }

    return nullptr;
}

NXOpen::Body* TwoPointBiLanCaoDialog::FindBodyBySmartPointParents(NXOpen::TaggedObject* object,
                                                                  NXOpen::Edge** matchedEdge,
                                                                  NXOpen::Face** matchedFace,
                                                                  std::string* parentTrace) const
{
    if (matchedEdge != nullptr)
    {
        *matchedEdge = nullptr;
    }
    if (matchedFace != nullptr)
    {
        *matchedFace = nullptr;
    }
    if (parentTrace != nullptr)
    {
        parentTrace->clear();
    }
    if (object == nullptr || object->Tag() == NULL_TAG)
    {
        return nullptr;
    }

    int nParents = 0;
    tag_t* parents = nullptr;
    const int options = UF_SO_ASK_ALL_PARENTS |
                        UF_SO_ASK_SO_PARENTS |
                        UF_SO_ASK_WIREFRAME_PARENTS |
                        UF_SO_ASK_DUMB_SO_PARENTS |
                        UF_SO_ASK_PARENTS_RECURSIVELY;
    const int errorCode = UF_SO_ask_parents(object->Tag(), options, &nParents, &parents);
    if (errorCode != 0)
    {
        if (parentTrace != nullptr)
        {
            std::ostringstream stream;
            stream << "UF_SO_ask_parents error=" << errorCode;
            *parentTrace = stream.str();
        }
        return nullptr;
    }

    NXOpen::Body* foundBody = nullptr;
    std::ostringstream stream;
    stream << "count=" << nParents;
    for (int index = 0; index < nParents; ++index)
    {
        const tag_t parentTag = parents[index];
        int parentType = 0;
        int parentSubtype = 0;
        UF_OBJ_ask_type_and_subtype(parentTag, &parentType, &parentSubtype);
        stream << "\n    parent[" << index << "] tag=" << parentTag
               << ", type=" << parentType
               << ", subtype=" << parentSubtype;

        NXOpen::TaggedObject* parentObject = nullptr;
        try
        {
            parentObject = NXOpen::NXObjectManager::Get(parentTag);
        }
        catch (...)
        {
            parentObject = nullptr;
        }

        if (parentObject == nullptr)
        {
            continue;
        }

        if (matchedEdge != nullptr && *matchedEdge == nullptr)
        {
            *matchedEdge = dynamic_cast<NXOpen::Edge*>(parentObject);
        }
        if (matchedFace != nullptr && *matchedFace == nullptr)
        {
            *matchedFace = dynamic_cast<NXOpen::Face*>(parentObject);
        }
        if (foundBody == nullptr)
        {
            foundBody = GetOwnerBody(parentObject);
        }
    }

    if (parents != nullptr)
    {
        UF_free(parents);
    }
    if (parentTrace != nullptr)
    {
        *parentTrace = stream.str();
    }
    return foundBody;
}

NXOpen::Body* TwoPointBiLanCaoDialog::FindBodyByEdgeEndpoint(const NXOpen::Point3d& point,
                                                             NXOpen::Edge** matchedEdge) const
{
    if (matchedEdge != nullptr)
    {
        *matchedEdge = nullptr;
    }

    if (session_ == nullptr || session_->Parts() == nullptr)
    {
        return nullptr;
    }

    NXOpen::Part* workPart = session_->Parts()->Work();
    if (workPart == nullptr || workPart->Bodies() == nullptr)
    {
        return nullptr;
    }

    for (NXOpen::BodyCollection::iterator bodyIt = workPart->Bodies()->begin();
         bodyIt != workPart->Bodies()->end();
         ++bodyIt)
    {
        NXOpen::Body* body = *bodyIt;
        if (body == nullptr)
        {
            continue;
        }

        const std::vector<NXOpen::Edge*> edges = body->GetEdges();
        for (NXOpen::Edge* edge : edges)
        {
            if (edge == nullptr)
            {
                continue;
            }

            NXOpen::Point3d vertex1;
            NXOpen::Point3d vertex2;
            try
            {
                edge->GetVertices(&vertex1, &vertex2);
            }
            catch (...)
            {
                continue;
            }

            if (PointDistance(point, vertex1) <= kEndpointTolerance ||
                PointDistance(point, vertex2) <= kEndpointTolerance)
            {
                if (matchedEdge != nullptr)
                {
                    *matchedEdge = edge;
                }
                return body;
            }
        }
    }

    return nullptr;
}

bool TwoPointBiLanCaoDialog::GetPlanarFacePlane(NXOpen::Face* face,
                                                NXOpen::Point3d& origin,
                                                NXOpen::Vector3d& normal) const
{
    if (face == nullptr)
    {
        return false;
    }

    try
    {
        if (face->SolidFaceType() != NXOpen::Face::FaceTypePlanar)
        {
            return false;
        }

        const std::vector<NXOpen::Edge*> faceEdges = face->GetEdges();
        for (NXOpen::Edge* edgeA : faceEdges)
        {
            if (edgeA == nullptr)
            {
                continue;
            }

            NXOpen::Point3d a1;
            NXOpen::Point3d a2;
            edgeA->GetVertices(&a1, &a2);
            const NXOpen::Vector3d directionA = Subtract(a2, a1);
            if (Length(directionA) <= kPointTolerance)
            {
                continue;
            }

            for (NXOpen::Edge* edgeB : faceEdges)
            {
                if (edgeB == nullptr || edgeB == edgeA)
                {
                    continue;
                }

                NXOpen::Point3d b1;
                NXOpen::Point3d b2;
                edgeB->GetVertices(&b1, &b2);
                NXOpen::Vector3d candidateNormal = Cross(directionA, Subtract(b2, b1));
                const double normalLength = Length(candidateNormal);
                if (normalLength <= kPointTolerance)
                {
                    continue;
                }

                candidateNormal.X /= normalLength;
                candidateNormal.Y /= normalLength;
                candidateNormal.Z /= normalLength;
                origin = a1;
                normal = candidateNormal;
                return true;
            }
        }
    }
    catch (...)
    {
    }

    return false;
}

double TwoPointBiLanCaoDialog::MeasureFaceArea(NXOpen::Face* face) const
{
    if (face == nullptr || session_ == nullptr || session_->Measurement() == nullptr)
    {
        return 0.0;
    }

    try
    {
        std::vector<NXOpen::ISurface*> faces;
        faces.push_back(face);

        double area = 0.0;
        double perimeter = 0.0;
        double radiusDiameter = 0.0;
        NXOpen::Point3d cog;
        double minRadiusOfCurvature = 0.0;
        double areaErrorEstimate = 0.0;
        NXOpen::Point3d anchorPoint;
        bool isApproximate = false;

        session_->Measurement()->GetFaceProperties(
            faces,
            0.99,
            NXOpen::Measurement::AlternateFaceRadius,
            true,
            &area,
            &perimeter,
            &radiusDiameter,
            &cog,
            &minRadiusOfCurvature,
            &areaErrorEstimate,
            &anchorPoint,
            &isApproximate);
        return area;
    }
    catch (...)
    {
    }

    return 0.0;
}

bool TwoPointBiLanCaoDialog::FindSheetThicknessByLargestParallelFaces(NXOpen::Body* body,
                                                                      NXOpen::Face** largestFace,
                                                                      NXOpen::Face** oppositeFace,
                                                                      double& largestArea,
                                                                      double& oppositeArea,
                                                                      double& thickness) const
{
    if (largestFace != nullptr)
    {
        *largestFace = nullptr;
    }
    if (oppositeFace != nullptr)
    {
        *oppositeFace = nullptr;
    }
    largestArea = 0.0;
    oppositeArea = 0.0;
    thickness = 0.0;

    if (body == nullptr)
    {
        return false;
    }

    NXOpen::Face* maxFace = nullptr;
    NXOpen::Point3d maxOrigin;
    NXOpen::Vector3d maxNormal;

    const std::vector<NXOpen::Face*> faces = body->GetFaces();
    for (NXOpen::Face* face : faces)
    {
        NXOpen::Point3d origin;
        NXOpen::Vector3d normal;
        if (!GetPlanarFacePlane(face, origin, normal))
        {
            continue;
        }

        const double area = MeasureFaceArea(face);
        if (area > largestArea)
        {
            largestArea = area;
            maxFace = face;
            maxOrigin = origin;
            maxNormal = normal;
        }
    }

    if (maxFace == nullptr || largestArea <= kPointTolerance)
    {
        return false;
    }

    const double minRequiredArea = largestArea * 0.5;
    double bestDistance = std::numeric_limits<double>::max();
    NXOpen::Face* bestFace = nullptr;
    double bestArea = 0.0;

    for (NXOpen::Face* face : faces)
    {
        if (face == nullptr || face == maxFace)
        {
            continue;
        }

        NXOpen::Point3d origin;
        NXOpen::Vector3d normal;
        if (!GetPlanarFacePlane(face, origin, normal))
        {
            continue;
        }

        const double parallel = std::fabs(Dot(maxNormal, normal));
        if (parallel < 1.0 - 1.0e-4)
        {
            continue;
        }

        const double area = MeasureFaceArea(face);
        if (area < minRequiredArea)
        {
            continue;
        }

        const double distance = std::fabs(Dot(Subtract(origin, maxOrigin), maxNormal));
        if (distance <= kPlaneTolerance)
        {
            continue;
        }

        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestFace = face;
            bestArea = area;
        }
    }

    if (bestFace == nullptr)
    {
        return false;
    }

    if (largestFace != nullptr)
    {
        *largestFace = maxFace;
    }
    if (oppositeFace != nullptr)
    {
        *oppositeFace = bestFace;
    }
    oppositeArea = bestArea;
    thickness = bestDistance;
    return true;
}

bool TwoPointBiLanCaoDialog::FindTwoPlanarEdgesAtPoint(NXOpen::Face* planeFace,
                                                       const NXOpen::Point3d& point,
                                                       NXOpen::Edge** edge1,
                                                       NXOpen::Edge** edge2) const
{
    if (edge1 != nullptr)
    {
        *edge1 = nullptr;
    }
    if (edge2 != nullptr)
    {
        *edge2 = nullptr;
    }
    if (planeFace == nullptr)
    {
        return false;
    }

    struct Candidate
    {
        NXOpen::Edge* edge;
        NXOpen::Vector3d direction;
        double distance;
    };
    std::vector<Candidate> candidates;
    double nearestEndpointDistance = std::numeric_limits<double>::max();
    const std::vector<NXOpen::Edge*> faceEdges = planeFace->GetEdges();
    for (NXOpen::Edge* edge : faceEdges)
    {
        NXOpen::Point3d vertex1;
        NXOpen::Point3d vertex2;
        try
        {
            edge->GetVertices(&vertex1, &vertex2);
        }
        catch (...)
        {
            continue;
        }

        const double distance1 = PointDistance(point, vertex1);
        const double distance2 = PointDistance(point, vertex2);
        const double endpointDistance = std::min(distance1, distance2);
        nearestEndpointDistance = std::min(nearestEndpointDistance, endpointDistance);
        if (endpointDistance > kLooseEndpointTolerance)
        {
            continue;
        }

        NXOpen::Vector3d direction;
        if (!EdgeDirectionFromPoint(edge, point, direction))
        {
            continue;
        }
        candidates.push_back({edge, direction, endpointDistance});
    }

    if (candidates.size() < 2)
    {
        return false;
    }

    double bestScore = -1.0;
    std::size_t bestA = 0;
    std::size_t bestB = 1;
    for (std::size_t i = 0; i < candidates.size(); ++i)
    {
        for (std::size_t j = i + 1; j < candidates.size(); ++j)
        {
            const double score = 1.0 - std::fabs(Dot(candidates[i].direction, candidates[j].direction));
            if (score > bestScore)
            {
                bestScore = score;
                bestA = i;
                bestB = j;
            }
        }
    }

    if (edge1 != nullptr)
    {
        *edge1 = candidates[bestA].edge;
    }
    if (edge2 != nullptr)
    {
        *edge2 = candidates[bestB].edge;
    }
    return true;
}

bool TwoPointBiLanCaoDialog::EdgeDirectionFromPoint(NXOpen::Edge* edge,
                                                    const NXOpen::Point3d& point,
                                                    NXOpen::Vector3d& direction) const
{
    if (edge == nullptr)
    {
        return false;
    }

    try
    {
        NXOpen::Point3d vertex1;
        NXOpen::Point3d vertex2;
        edge->GetVertices(&vertex1, &vertex2);
        if (PointDistance(point, vertex1) <= PointDistance(point, vertex2))
        {
            direction = Subtract(vertex2, point);
        }
        else
        {
            direction = Subtract(vertex1, point);
        }

        return Normalize(direction);
    }
    catch (...)
    {
    }

    return false;
}

std::vector<NXOpen::Edge*> TwoPointBiLanCaoDialog::FindEdgesConnectedToPointOnBody(
    NXOpen::Body* body,
    const NXOpen::Point3d& point) const
{
    std::vector<NXOpen::Edge*> result;
    std::set<tag_t> edgeTags;
    if (body == nullptr)
    {
        return result;
    }

    const std::vector<NXOpen::Edge*> bodyEdges = body->GetEdges();
    for (NXOpen::Edge* edge : bodyEdges)
    {
        if (edge == nullptr)
        {
            continue;
        }

        try
        {
            NXOpen::Point3d vertex1;
            NXOpen::Point3d vertex2;
            edge->GetVertices(&vertex1, &vertex2);
            if (PointDistance(point, vertex1) <= kLooseEndpointTolerance ||
                PointDistance(point, vertex2) <= kLooseEndpointTolerance)
            {
                if (edgeTags.insert(edge->Tag()).second)
                {
                    result.push_back(edge);
                }
            }
        }
        catch (...)
        {
        }
    }

    return result;
}

NXOpen::Edge* TwoPointBiLanCaoDialog::FindCornerEdgeAtPoint(
    NXOpen::Body* body,
    const NXOpen::Point3d& point,
    NXOpen::Edge* planeEdge1,
    NXOpen::Edge* planeEdge2) const
{
    const tag_t planeEdgeTag1 = planeEdge1 != nullptr ? planeEdge1->Tag() : NULL_TAG;
    const tag_t planeEdgeTag2 = planeEdge2 != nullptr ? planeEdge2->Tag() : NULL_TAG;
    const std::vector<NXOpen::Edge*> connectedEdges = FindEdgesConnectedToPointOnBody(body, point);
    {
        std::ostringstream log;
        log << "FindCornerEdgeAtPoint body="
            << (body != nullptr ? body->Tag() : NULL_TAG)
            << ", point=(" << point.X << "," << point.Y << "," << point.Z << ")"
            << ", planeEdge1=" << planeEdgeTag1
            << ", planeEdge2=" << planeEdgeTag2
            << ", connectedCount=" << connectedEdges.size();
        AppendDebugLog(log.str());
    }

    NXOpen::Edge* bestEdge = nullptr;
    double bestLength = -1.0;
    for (NXOpen::Edge* edge : connectedEdges)
    {
        if (edge == nullptr)
        {
            continue;
        }

        const tag_t edgeTag = edge->Tag();
        if (edgeTag == planeEdgeTag1 || edgeTag == planeEdgeTag2)
        {
            AppendDebugLog("  skip plane edge tag=" + TagText(edgeTag));
            continue;
        }

        NXOpen::Vector3d direction;
        if (!EdgeDirectionFromPoint(edge, point, direction))
        {
            AppendDebugLog("  skip edge no direction tag=" + TagText(edgeTag));
            continue;
        }

        const double length = EdgeChordLength(edge);
        {
            std::ostringstream log;
            log << "  candidate corner edge tag=" << edgeTag
                << ", length=" << length
                << ", direction=(" << direction.X << "," << direction.Y << "," << direction.Z << ")";
            AppendDebugLog(log.str());
        }
        if (length > bestLength)
        {
            bestLength = length;
            bestEdge = edge;
        }
    }

    AppendDebugLog("FindCornerEdgeAtPoint result tag=" +
                   TagText(bestEdge != nullptr ? bestEdge->Tag() : NULL_TAG));
    return bestEdge;
}

std::vector<NXOpen::Face*> TwoPointBiLanCaoDialog::FindPlanarFacesOfEdge(NXOpen::Edge* edge) const
{
    std::vector<NXOpen::Face*> planarFaces;
    std::set<tag_t> faceTags;
    if (edge == nullptr)
    {
        return planarFaces;
    }

    const std::vector<NXOpen::Face*> faces = edge->GetFaces();
    for (NXOpen::Face* face : faces)
    {
        if (face == nullptr)
        {
            continue;
        }

        try
        {
            if (face->SolidFaceType() != NXOpen::Face::FaceTypePlanar)
            {
                continue;
            }
        }
        catch (...)
        {
            continue;
        }

        if (faceTags.insert(face->Tag()).second)
        {
            planarFaces.push_back(face);
        }
    }

    return planarFaces;
}

NXOpen::Face* TwoPointBiLanCaoDialog::FindLargerBoundingFace(NXOpen::Edge* edge) const
{
    const std::vector<NXOpen::Face*> planarFaces = FindPlanarFacesOfEdge(edge);
    NXOpen::Face* bestFace = nullptr;
    double bestArea = -1.0;
    for (NXOpen::Face* face : planarFaces)
    {
        const double area = MeasureFaceArea(face);
        if (area > bestArea)
        {
            bestArea = area;
            bestFace = face;
        }
    }

    return bestFace;
}

NXOpen::Face* TwoPointBiLanCaoDialog::FindSmallerBoundingFace(NXOpen::Edge* edge) const
{
    const std::vector<NXOpen::Face*> planarFaces = FindPlanarFacesOfEdge(edge);
    NXOpen::Face* bestFace = nullptr;
    double bestArea = std::numeric_limits<double>::max();
    for (NXOpen::Face* face : planarFaces)
    {
        const double area = MeasureFaceArea(face);
        if (area < bestArea)
        {
            bestArea = area;
            bestFace = face;
        }
    }

    return bestFace;
}

NXOpen::Point3d TwoPointBiLanCaoDialog::ComputeBodyBoundsCenter(NXOpen::Body* body) const
{
    if (body == nullptr)
    {
        return NXOpen::Point3d(0.0, 0.0, 0.0);
    }

    double box[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    if (UF_MODL_ask_bounding_box(body->Tag(), box) != 0)
    {
        return NXOpen::Point3d(0.0, 0.0, 0.0);
    }

    return NXOpen::Point3d(
        (box[0] + box[3]) * 0.5,
        (box[1] + box[4]) * 0.5,
        (box[2] + box[5]) * 0.5);
}

bool TwoPointBiLanCaoDialog::ComputeInwardNormal(
    NXOpen::Body* body,
    NXOpen::Face* face,
    NXOpen::Vector3d& inwardNormal) const
{
    inwardNormal = NXOpen::Vector3d(0.0, 0.0, 0.0);
    if (body == nullptr || face == nullptr)
    {
        return false;
    }

    NXOpen::Point3d planeOrigin;
    NXOpen::Vector3d planeNormal;
    if (!GetPlanarFacePlane(face, planeOrigin, planeNormal))
    {
        return false;
    }

    const NXOpen::Point3d bodyCenter = ComputeBodyBoundsCenter(body);
    const NXOpen::Vector3d toCenter = Subtract(bodyCenter, planeOrigin);
    if (Dot(planeNormal, toCenter) < 0.0)
    {
        planeNormal = Scale(planeNormal, -1.0);
    }

    inwardNormal = planeNormal;
    return Normalize(inwardNormal);
}

bool TwoPointBiLanCaoDialog::FindNearestEndpointFromBodyEdges(
    NXOpen::Body* body,
    const NXOpen::Point3d& basePoint,
    NXOpen::Point3d& nearestPoint,
    double& nearestDistance) const
{
    nearestPoint = NXOpen::Point3d(0.0, 0.0, 0.0);
    nearestDistance = 0.0;
    if (body == nullptr)
    {
        return false;
    }

    bool found = false;
    for (NXOpen::Edge* edge : body->GetEdges())
    {
        if (edge == nullptr)
        {
            continue;
        }

        NXOpen::Point3d vertex1;
        NXOpen::Point3d vertex2;
        try
        {
            edge->GetVertices(&vertex1, &vertex2);
        }
        catch (...)
        {
            continue;
        }

        for (const NXOpen::Point3d& candidate : {vertex1, vertex2})
        {
            const double distance = PointDistance(basePoint, candidate);
            if (distance <= kLooseEndpointTolerance)
            {
                continue;
            }
            if (!found || distance < nearestDistance)
            {
                found = true;
                nearestDistance = distance;
                nearestPoint = candidate;
            }
        }
    }

    return found;
}

bool TwoPointBiLanCaoDialog::ShouldCreateCubeForEdgeEndpoint(
    NXOpen::Body* body,
    const NXOpen::Point3d& point,
    double thickness,
    double faceAngleDegrees) const
{
    if (body == nullptr || thickness <= kPointTolerance ||
        faceAngleDegrees <= kPointTolerance ||
        faceAngleDegrees >= 180.0 - kPointTolerance)
    {
        return false;
    }

    NXOpen::Point3d nearestPoint;
    double nearestDistance = 0.0;
    if (!FindNearestEndpointFromBodyEdges(body, point, nearestPoint, nearestDistance))
    {
        return false;
    }

    struct CornerDistanceRule
    {
        double angleDegrees = 0.0;
        double planarInnerDistance = 0.0;
        double topCornerDistance = 0.0;
        double planarDelta = std::numeric_limits<double>::max();
        double topDelta = std::numeric_limits<double>::max();
        double score = std::numeric_limits<double>::max();
        bool valid = false;
    };

    auto makeDistanceRule = [&](double angleDegrees) -> CornerDistanceRule
    {
        CornerDistanceRule rule;
        rule.angleDegrees = angleDegrees;
        if (angleDegrees <= kPointTolerance || angleDegrees >= 180.0 - kPointTolerance)
        {
            return rule;
        }

        const double halfAngleRadians = angleDegrees * 0.5 * 3.14159265358979323846 / 180.0;
        const double sineHalfAngle = std::sin(halfAngleRadians);
        if (std::fabs(sineHalfAngle) <= 1.0e-8)
        {
            return rule;
        }

        rule.planarInnerDistance = thickness / sineHalfAngle;
        rule.topCornerDistance = std::sqrt(rule.planarInnerDistance * rule.planarInnerDistance +
                                          thickness * thickness);
        rule.planarDelta = std::fabs(nearestDistance - rule.planarInnerDistance);
        rule.topDelta = std::fabs(nearestDistance - rule.topCornerDistance);
        rule.score = std::min(rule.planarDelta, rule.topDelta);
        rule.valid = true;
        return rule;
    };

    CornerDistanceRule rule = makeDistanceRule(faceAngleDegrees);
    CornerDistanceRule supplementRule = makeDistanceRule(180.0 - faceAngleDegrees);
    if (supplementRule.valid && (!rule.valid || supplementRule.score < rule.score))
    {
        rule = supplementRule;
    }

    if (!rule.valid)
    {
        return false;
    }

    const double tolerance = std::max(0.05, thickness * 0.15);
    bool createCube = false;
    if (rule.planarDelta <= tolerance)
    {
        createCube = false;
    }
    else if (rule.topDelta <= tolerance)
    {
        createCube = true;
    }
    else
    {
        createCube = rule.topDelta < rule.planarDelta;
    }
    {
        std::ostringstream log;
        log << "ShouldCreateCubeForEdgeEndpoint point=("
            << point.X << "," << point.Y << "," << point.Z << ")"
            << ", nearestDistance=" << nearestDistance
            << ", thickness=" << thickness
            << ", faceAngle=" << faceAngleDegrees
            << ", effectiveAngle=" << rule.angleDegrees
            << ", L2D=" << rule.planarInnerDistance
            << ", L3D=" << rule.topCornerDistance
            << ", planarDelta=" << rule.planarDelta
            << ", topDelta=" << rule.topDelta
            << ", create=" << (createCube ? 1 : 0);
        AppendDebugLog(log.str());
    }
    return createCube;
}

bool TwoPointBiLanCaoDialog::CreateCubeToolAtPoint(
    DialogValues& values,
    const NXOpen::Point3d& point,
    const std::vector<NXOpen::Edge*>& connectedEdges) const
{
    NXOpen::Part* workPart = session_ != nullptr && session_->Parts() != nullptr ? session_->Parts()->Work() : nullptr;
    if (workPart == nullptr || values.targetBody == nullptr || connectedEdges.size() < 3)
    {
        return false;
    }

    NXOpen::Vector3d xDirection;
    NXOpen::Vector3d yDirection;
    NXOpen::Vector3d zDirection;
    if (!EdgeDirectionFromPoint(connectedEdges[0], point, xDirection) ||
        !EdgeDirectionFromPoint(connectedEdges[1], point, yDirection) ||
        !EdgeDirectionFromPoint(connectedEdges[2], point, zDirection))
    {
        return false;
    }

    if (Dot(Cross(xDirection, yDirection), zDirection) < 0.0)
    {
        std::swap(xDirection, yDirection);
    }

    return CreateCubeToolAtPointWithDirections(values, point, xDirection, yDirection, zDirection);
}

bool TwoPointBiLanCaoDialog::CreateCubeToolAtPointWithDirections(
    DialogValues& values,
    const NXOpen::Point3d& point,
    NXOpen::Vector3d xDirection,
    NXOpen::Vector3d yDirection,
    NXOpen::Vector3d zDirection) const
{
    NXOpen::Part* workPart = session_ != nullptr && session_->Parts() != nullptr ? session_->Parts()->Work() : nullptr;
    if (workPart == nullptr || values.targetBody == nullptr ||
        !Normalize(xDirection) || !Normalize(yDirection) || !Normalize(zDirection))
    {
        return false;
    }

    if (Dot(Cross(xDirection, yDirection), zDirection) < 0.0)
    {
        std::swap(xDirection, yDirection);
    }

    const double cubeEdgeLength = values.sheetThickness + values.bendRadius;
    NXOpen::Features::BlockFeatureBuilder* blockBuilder = nullptr;
    try
    {
        blockBuilder = workPart->Features()->CreateBlockFeatureBuilder(nullptr);
        blockBuilder->SetType(NXOpen::Features::BlockFeatureBuilder::TypesOriginAndEdgeLengths);
        blockBuilder->SetOrigin(point);
        blockBuilder->SetOrientation(xDirection, yDirection);
        blockBuilder->SetLength(FormatLimit(cubeEdgeLength).c_str());
        blockBuilder->SetWidth(FormatLimit(cubeEdgeLength).c_str());
        blockBuilder->SetHeight(FormatLimit(cubeEdgeLength).c_str());
        blockBuilder->SetBooleanOperationAndTarget(NXOpen::Features::Feature::BooleanTypeCreate, nullptr);
        NXOpen::Features::Feature* feature = blockBuilder->CommitFeature();
        blockBuilder->Destroy();
        if (feature == nullptr || feature->Tag() == NULL_TAG)
        {
            return false;
        }

        tag_t toolBody = NULL_TAG;
        if (UF_MODL_ask_feat_body(feature->Tag(), &toolBody) != 0 || toolBody == NULL_TAG)
        {
            values.profileFeatureTags.push_back(feature->Tag());
            return false;
        }
        values.profileToolBodyTags.push_back(toolBody);

        uf_list_p_t bodyList = NULL;
        int removeParmsResult = UF_MODL_create_list(&bodyList);
        if (removeParmsResult == 0 && bodyList != NULL)
        {
            removeParmsResult = UF_MODL_put_list_item(bodyList, toolBody);
            if (removeParmsResult == 0)
            {
                removeParmsResult = UF_MODL_delete_body_parms(bodyList);
            }
            UF_MODL_delete_list(&bodyList);
        }
        if (removeParmsResult != 0)
        {
            values.profileFeatureTags.push_back(feature->Tag());
            return false;
        }

        const std::vector<tag_t> targetFeatureTagsBefore =
            AskBodyFeatureTags(values.targetBody->Tag());
        int resultCount = 0;
        tag_t* resultingBodies = NULL;
        const int subtractResult = UF_MODL_subtract_bodies(
            values.targetBody->Tag(),
            toolBody,
            &resultCount,
            &resultingBodies);
        if (resultingBodies != NULL)
        {
            UF_free(resultingBodies);
        }
        if (subtractResult != 0)
        {
            return false;
        }

        const std::vector<tag_t> targetFeatureTagsAfter =
            AskBodyFeatureTags(values.targetBody->Tag());
        for (tag_t featureTag : targetFeatureTagsAfter)
        {
            if (std::find(targetFeatureTagsBefore.begin(), targetFeatureTagsBefore.end(), featureTag) ==
                targetFeatureTagsBefore.end())
            {
                values.profileFeatureTags.push_back(featureTag);
            }
        }
        AppendDebugLog("CreateCubeToolAtPoint success, boolean feature count=" +
                       std::to_string(values.profileFeatureTags.size()));
        return true;
    }
    catch (const NXOpen::NXException& ex)
    {
        AppendDebugLog(std::string("CreateCubeToolAtPoint NXException: ") + ex.Message());
    }
    catch (...)
    {
        AppendDebugLog("CreateCubeToolAtPoint unknown exception");
    }

    if (blockBuilder != nullptr)
    {
        try
        {
            blockBuilder->Destroy();
        }
        catch (...)
        {
        }
    }
    return false;
}

bool TwoPointBiLanCaoDialog::CreateTopCornerEndpointCutWithDirections(
    DialogValues& values,
    const NXOpen::Point3d& point,
    NXOpen::Vector3d cornerEdgeDirection,
    NXOpen::Vector3d edge1Direction,
    NXOpen::Vector3d edge2Direction) const
{
    if (values.targetBody == nullptr ||
        !Normalize(cornerEdgeDirection) ||
        !Normalize(edge1Direction) ||
        !Normalize(edge2Direction))
    {
        return false;
    }

    const double cutLength = values.sheetThickness + values.bendRadius;
    if (cutLength <= kPointTolerance)
    {
        return false;
    }

    NXOpen::Vector3d centerDirection =
        ProjectVectorToPlane(Subtract(values.endPoint, values.startPoint), cornerEdgeDirection);
    if (!Normalize(centerDirection))
    {
        return false;
    }
    if (Dot(centerDirection, edge1Direction) < 0.0)
    {
        edge1Direction = Scale(edge1Direction, -1.0);
    }
    if (Dot(centerDirection, edge2Direction) < 0.0)
    {
        edge2Direction = Scale(edge2Direction, -1.0);
    }

    const double sideLength = values.sheetThickness + values.bendRadius;
    std::vector<NXOpen::Point3d> profilePoints;
    auto evaluateProfile = [&](double centerDistance,
                               std::vector<NXOpen::Point3d>* points,
                               double* gap) -> bool
    {
        const NXOpen::Point3d cPoint = Add(point, Scale(centerDirection, centerDistance));
        const double edgeParameterA = Dot(Subtract(cPoint, point), edge1Direction);
        const double edgeParameterE = Dot(Subtract(cPoint, point), edge2Direction);
        if (edgeParameterA <= kPointTolerance || edgeParameterE <= kPointTolerance)
        {
            return false;
        }

        const NXOpen::Point3d aPoint = Add(point, Scale(edge1Direction, edgeParameterA));
        const NXOpen::Point3d ePoint = Add(point, Scale(edge2Direction, edgeParameterE));
        NXOpen::Vector3d aToC = Subtract(cPoint, aPoint);
        NXOpen::Vector3d eToC = Subtract(cPoint, ePoint);
        const double acLength = Length(aToC);
        const double ecLength = Length(eToC);
        if (acLength <= sideLength + kPointTolerance ||
            ecLength <= sideLength + kPointTolerance ||
            !Normalize(aToC) ||
            !Normalize(eToC))
        {
            return false;
        }

        const NXOpen::Point3d bPoint = Add(aPoint, Scale(aToC, sideLength));
        const NXOpen::Point3d dPoint = Add(ePoint, Scale(eToC, sideLength));
        const double bdGap = PointDistance(bPoint, dPoint);
        if (gap != nullptr)
        {
            *gap = bdGap;
        }
        if (points != nullptr)
        {
            *points = {point, aPoint, bPoint, cPoint, dPoint, ePoint, point};
        }
        return true;
    };

    double lowDistance = sideLength + kPointTolerance;
    double lowGap = 0.0;
    for (int index = 0; index < 80; ++index)
    {
        if (evaluateProfile(lowDistance, nullptr, &lowGap))
        {
            break;
        }
        lowDistance *= 1.25;
    }

    double highDistance = lowDistance;
    double highGap = lowGap;
    for (int index = 0; index < 80 && highGap < values.slotClearance; ++index)
    {
        highDistance *= 1.5;
        if (!evaluateProfile(highDistance, nullptr, &highGap))
        {
            highGap = 0.0;
        }
    }

    if (highGap < values.slotClearance || values.slotClearance <= kPointTolerance)
    {
        AppendDebugLog("CreateTopCornerEndpointCut failed: no valid first-end corner profile");
        return false;
    }

    for (int index = 0; index < 80; ++index)
    {
        const double midDistance = (lowDistance + highDistance) * 0.5;
        double midGap = 0.0;
        if (evaluateProfile(midDistance, nullptr, &midGap) &&
            midGap >= values.slotClearance)
        {
            highDistance = midDistance;
            highGap = midGap;
        }
        else
        {
            lowDistance = midDistance;
        }
    }

    double finalGap = 0.0;
    if (!evaluateProfile(highDistance, &profilePoints, &finalGap))
    {
        AppendDebugLog("CreateTopCornerEndpointCut failed: evaluate first-end corner profile");
        return false;
    }

    {
        std::ostringstream log;
        log << "CreateTopCornerEndpointCut profile=OABCDEO"
            << ", cutLength=" << cutLength
            << ", sideLength=" << sideLength
            << ", C distance=" << highDistance
            << ", BD gap=" << finalGap
            << ", pO=(" << profilePoints[0].X << "," << profilePoints[0].Y << "," << profilePoints[0].Z << ")"
            << ", pA=(" << profilePoints[1].X << "," << profilePoints[1].Y << "," << profilePoints[1].Z << ")"
            << ", pB=(" << profilePoints[2].X << "," << profilePoints[2].Y << "," << profilePoints[2].Z << ")"
            << ", pC=(" << profilePoints[3].X << "," << profilePoints[3].Y << "," << profilePoints[3].Z << ")"
            << ", pD=(" << profilePoints[4].X << "," << profilePoints[4].Y << "," << profilePoints[4].Z << ")"
            << ", pE=(" << profilePoints[5].X << "," << profilePoints[5].Y << "," << profilePoints[5].Z << ")";
        AppendDebugLog(log.str());
    }

    std::vector<tag_t> curveTags;
    auto deleteSectionCurves = [&curveTags]() -> std::pair<int, int>
    {
        int deleteFailures = 0;
        int blankFailures = 0;
        for (tag_t curveTag : curveTags)
        {
            if (curveTag == NULL_TAG)
            {
                continue;
            }
            const int deleteResult = UF_OBJ_delete_object(curveTag);
            if (deleteResult != 0)
            {
                ++deleteFailures;
                const int blankResult = UF_OBJ_set_blank_status(curveTag, UF_OBJ_BLANKED);
                if (blankResult != 0)
                {
                    ++blankFailures;
                }
            }
        }
        curveTags.clear();
        return std::make_pair(deleteFailures, blankFailures);
    };
    for (std::size_t index = 0; index + 1 < profilePoints.size(); ++index)
    {
        UF_CURVE_line_t lineData;
        CopyPointToArray(profilePoints[index], lineData.start_point);
        CopyPointToArray(profilePoints[index + 1], lineData.end_point);
        tag_t lineTag = NULL_TAG;
        if (UF_CURVE_create_line(&lineData, &lineTag) != 0 || lineTag == NULL_TAG)
        {
            deleteSectionCurves();
            return false;
        }
        curveTags.push_back(lineTag);
    }

    uf_list_p_t curveList = NULL;
    if (UF_MODL_create_list(&curveList) != 0 || curveList == NULL)
    {
        deleteSectionCurves();
        return false;
    }
    for (tag_t curveTag : curveTags)
    {
        UF_MODL_put_list_item(curveList, curveTag);
    }

    std::string startLimit = "0.0";
    std::string endLimit = FormatLimit(cutLength);
    char taperAngle[] = "0.0";
    char* limits[2] = {const_cast<char*>(startLimit.c_str()), const_cast<char*>(endLimit.c_str())};
    double origin[3] = {point.X, point.Y, point.Z};
    double direction[3] = {cornerEdgeDirection.X, cornerEdgeDirection.Y, cornerEdgeDirection.Z};
    uf_list_p_t featureList = NULL;
    const std::vector<tag_t> targetFeatureTagsBefore = AskBodyFeatureTags(values.targetBody->Tag());
    const int extrudeResult = UF_MODL_create_extruded(
        curveList,
        taperAngle,
        limits,
        origin,
        direction,
        UF_NULLSIGN,
        &featureList);
    UF_MODL_delete_list(&curveList);
    tag_t toolFeatureTag = NULL_TAG;
    tag_t toolBody = NULL_TAG;
    int featureCount = 0;
    if (extrudeResult == 0 && featureList != NULL &&
        UF_MODL_ask_list_count(featureList, &featureCount) == 0 &&
        featureCount > 0)
    {
        UF_MODL_ask_list_item(featureList, 0, &toolFeatureTag);
    }
    if (featureList != NULL)
    {
        UF_MODL_delete_list(&featureList);
    }

    int askBodyResult = 1;
    int deleteParmsResult = 1;
    int subtractResult = 1;
    int resultCount = 0;
    if (toolFeatureTag != NULL_TAG)
    {
        askBodyResult = UF_MODL_ask_feat_body(toolFeatureTag, &toolBody);
    }
    if (askBodyResult == 0 && toolBody != NULL_TAG)
    {
        values.profileToolBodyTags.push_back(toolBody);
        uf_list_p_t bodyList = NULL;
        deleteParmsResult = UF_MODL_create_list(&bodyList);
        if (deleteParmsResult == 0 && bodyList != NULL)
        {
            deleteParmsResult = UF_MODL_put_list_item(bodyList, toolBody);
            if (deleteParmsResult == 0)
            {
                deleteParmsResult = UF_MODL_delete_body_parms(bodyList);
            }
            UF_MODL_delete_list(&bodyList);
        }
        tag_t* resultingBodies = NULL;
        if (deleteParmsResult == 0)
        {
            subtractResult = UF_MODL_subtract_bodies(
                values.targetBody->Tag(),
                toolBody,
                &resultCount,
                &resultingBodies);
        }
        if (resultingBodies != NULL)
        {
            UF_free(resultingBodies);
        }
    }

    int deleteCurveFailures = 0;
    int blankCurveFailures = 0;
    for (tag_t curveTag : curveTags)
    {
        if (curveTag == NULL_TAG)
        {
            continue;
        }
        const int deleteResult = UF_OBJ_delete_object(curveTag);
        if (deleteResult != 0)
        {
            ++deleteCurveFailures;
            const int blankResult = UF_OBJ_set_blank_status(curveTag, UF_OBJ_BLANKED);
            if (blankResult != 0)
            {
                ++blankCurveFailures;
            }
        }
    }
    AppendDebugLog("CreateTopCornerEndpointCut extrude rc=" +
                   std::to_string(extrudeResult) +
                   " (" + UfFailureMessage(extrudeResult) + ")" +
                   ", ask body rc=" + std::to_string(askBodyResult) +
                   " (" + UfFailureMessage(askBodyResult) + ")" +
                   ", delete parms rc=" + std::to_string(deleteParmsResult) +
                   " (" + UfFailureMessage(deleteParmsResult) + ")" +
                   ", subtract rc=" + std::to_string(subtractResult) +
                   " (" + UfFailureMessage(subtractResult) + ")" +
                   ", delete curve failures=" + std::to_string(deleteCurveFailures) +
                   ", blank curve failures=" + std::to_string(blankCurveFailures));
    if (extrudeResult != 0 || toolFeatureTag == NULL_TAG || askBodyResult != 0 ||
        toolBody == NULL_TAG || deleteParmsResult != 0 || subtractResult != 0)
    {
        return false;
    }

    const std::vector<tag_t> targetFeatureTagsAfter = AskBodyFeatureTags(values.targetBody->Tag());
    for (tag_t newFeatureTag : targetFeatureTagsAfter)
    {
        if (std::find(targetFeatureTagsBefore.begin(), targetFeatureTagsBefore.end(), newFeatureTag) ==
            targetFeatureTagsBefore.end())
        {
            values.profileFeatureTags.push_back(newFeatureTag);
        }
    }
    return true;
}

bool TwoPointBiLanCaoDialog::GetSelectedEdgeFaceAngle(
    NXOpen::Body* body,
    NXOpen::Edge* edge,
    double& angleDegrees,
    std::vector<NXOpen::Face*>& principalFaces,
    std::vector<NXOpen::Vector3d>& inwardNormals) const
{
    angleDegrees = 0.0;
    principalFaces.clear();
    inwardNormals.clear();
    if (body == nullptr || edge == nullptr)
    {
        return false;
    }

    principalFaces = FindPlanarFacesOfEdge(edge);
    std::sort(
        principalFaces.begin(),
        principalFaces.end(),
        [this](NXOpen::Face* leftFace, NXOpen::Face* rightFace)
        {
            return MeasureFaceArea(leftFace) > MeasureFaceArea(rightFace);
        });
    if (principalFaces.size() > 2)
    {
        principalFaces.resize(2);
    }
    if (principalFaces.size() < 2)
    {
        return false;
    }

    for (NXOpen::Face* face : principalFaces)
    {
        NXOpen::Vector3d normal;
        if (!ComputeInwardNormal(body, face, normal))
        {
            return false;
        }
        inwardNormals.push_back(normal);
    }

    double cosine = Dot(inwardNormals[0], inwardNormals[1]);
    cosine = std::max(-1.0, std::min(1.0, cosine));
    angleDegrees = std::acos(cosine) * 180.0 / 3.14159265358979323846;
    {
        std::ostringstream log;
        log << "GetSelectedEdgeFaceAngle edge=" << edge->Tag()
            << ", face1=" << principalFaces[0]->Tag()
            << ", face2=" << principalFaces[1]->Tag()
            << ", angle=" << angleDegrees
            << ", n1=(" << inwardNormals[0].X << "," << inwardNormals[0].Y << "," << inwardNormals[0].Z << ")"
            << ", n2=(" << inwardNormals[1].X << "," << inwardNormals[1].Y << "," << inwardNormals[1].Z << ")";
        AppendDebugLog(log.str());
    }
    return true;
}

bool TwoPointBiLanCaoDialog::CreateNonRightCornerEdgeCut(
    DialogValues& values,
    NXOpen::Edge* cornerEdge,
    double angleDegrees,
    const std::vector<NXOpen::Face*>& principalFaces,
    const std::vector<NXOpen::Vector3d>& inwardNormals) const
{
    if (values.targetBody == nullptr || cornerEdge == nullptr ||
        principalFaces.size() < 2 || inwardNormals.size() < 2)
    {
        return false;
    }

    NXOpen::Point3d edgeVertex1;
    NXOpen::Point3d edgeVertex2;
    try
    {
        cornerEdge->GetVertices(&edgeVertex1, &edgeVertex2);
    }
    catch (...)
    {
        return false;
    }

    NXOpen::Point3d profileOrigin = edgeVertex1;
    NXOpen::Point3d profileOtherEndpoint = edgeVertex2;
    if (PointDistance(edgeVertex2, values.startPoint) < PointDistance(edgeVertex1, values.startPoint))
    {
        profileOrigin = edgeVertex2;
        profileOtherEndpoint = edgeVertex1;
    }

    NXOpen::Vector3d edgeDirection = Subtract(profileOtherEndpoint, profileOrigin);
    const double edgeLength = Length(edgeDirection);
    if (!Normalize(edgeDirection) || edgeLength <= kPointTolerance)
    {
        return false;
    }

    NXOpen::Point3d facePlanePoint1;
    NXOpen::Point3d facePlanePoint2;
    NXOpen::Vector3d facePlaneNormal1;
    NXOpen::Vector3d facePlaneNormal2;
    if (!GetPlanarFacePlane(principalFaces[0], facePlanePoint1, facePlaneNormal1) ||
        !GetPlanarFacePlane(principalFaces[1], facePlanePoint2, facePlaneNormal2))
    {
        return false;
    }

    NXOpen::Vector3d faceDirection1 = ProjectVectorToPlane(Subtract(facePlanePoint1, profileOrigin), edgeDirection);
    NXOpen::Vector3d faceDirection2 = ProjectVectorToPlane(Subtract(facePlanePoint2, profileOrigin), edgeDirection);
    if (!Normalize(faceDirection1))
    {
        faceDirection1 = Cross(edgeDirection, facePlaneNormal1);
        if (!Normalize(faceDirection1))
        {
            faceDirection1 = Cross(facePlaneNormal1, edgeDirection);
        }
    }
    if (!Normalize(faceDirection2))
    {
        faceDirection2 = Cross(edgeDirection, facePlaneNormal2);
        if (!Normalize(faceDirection2))
        {
            faceDirection2 = Cross(facePlaneNormal2, edgeDirection);
        }
    }
    if (!Normalize(faceDirection1) || !Normalize(faceDirection2))
    {
        AppendDebugLog("CreateNonRightCornerEdgeCut failed: face direction normalize");
        return false;
    }
    {
        std::ostringstream log;
        log << "CreateNonRightCornerEdgeCut face directions d1=("
            << faceDirection1.X << "," << faceDirection1.Y << "," << faceDirection1.Z << ") d2=("
            << faceDirection2.X << "," << faceDirection2.Y << "," << faceDirection2.Z << ")";
        AppendDebugLog(log.str());
    }

    std::vector<NXOpen::Point3d> profilePoints;
    const double cornerExtra = values.bendRadius + values.slotClearance * 0.5;
    const double offsetDistance = values.sheetThickness + cornerExtra;
    bool explicitGapProfileCreated = false;
    {
        NXOpen::Vector3d centerDirection =
            ProjectVectorToPlane(Subtract(values.endPoint, values.startPoint), edgeDirection);
        if (Normalize(centerDirection))
        {
            if (Dot(centerDirection, faceDirection1) < 0.0)
            {
                faceDirection1 = Scale(faceDirection1, -1.0);
            }
            if (Dot(centerDirection, faceDirection2) < 0.0)
            {
                faceDirection2 = Scale(faceDirection2, -1.0);
            }

            auto evaluateGapProfile = [&](double centerDistance,
                                          std::vector<NXOpen::Point3d>* points,
                                          double* gap) -> bool
            {
                const NXOpen::Point3d cPoint = Add(profileOrigin, Scale(centerDirection, centerDistance));
                const double edgeParameterA = Dot(Subtract(cPoint, profileOrigin), faceDirection1);
                const double edgeParameterE = Dot(Subtract(cPoint, profileOrigin), faceDirection2);
                if (edgeParameterA <= kPointTolerance || edgeParameterE <= kPointTolerance)
                {
                    return false;
                }

                const NXOpen::Point3d aPoint = Add(profileOrigin, Scale(faceDirection1, edgeParameterA));
                const NXOpen::Point3d ePoint = Add(profileOrigin, Scale(faceDirection2, edgeParameterE));
                NXOpen::Vector3d aToC = Subtract(cPoint, aPoint);
                NXOpen::Vector3d eToC = Subtract(cPoint, ePoint);
                const double acLength = Length(aToC);
                const double ecLength = Length(eToC);
                if (acLength <= values.sheetThickness + kPointTolerance ||
                    ecLength <= values.sheetThickness + kPointTolerance ||
                    !Normalize(aToC) ||
                    !Normalize(eToC))
                {
                    return false;
                }

                const NXOpen::Point3d bPoint = Add(aPoint, Scale(aToC, values.sheetThickness));
                const NXOpen::Point3d dPoint = Add(ePoint, Scale(eToC, values.sheetThickness));
                const double bdGap = PointDistance(bPoint, dPoint);
                if (gap != nullptr)
                {
                    *gap = bdGap;
                }
                if (points != nullptr)
                {
                    *points = {profileOrigin, aPoint, cPoint, ePoint, profileOrigin};
                }
                return true;
            };

            double lowDistance = std::max(values.sheetThickness, values.slotClearance) + kPointTolerance;
            double lowGap = 0.0;
            for (int index = 0; index < 80; ++index)
            {
                if (evaluateGapProfile(lowDistance, nullptr, &lowGap))
                {
                    break;
                }
                lowDistance *= 1.25;
            }

            double highDistance = lowDistance;
            double highGap = lowGap;
            for (int index = 0; index < 80 && highGap < values.slotClearance; ++index)
            {
                highDistance *= 1.5;
                if (!evaluateGapProfile(highDistance, nullptr, &highGap))
                {
                    highGap = 0.0;
                }
            }

            if (highGap >= values.slotClearance && values.slotClearance > kPointTolerance)
            {
                for (int index = 0; index < 80; ++index)
                {
                    const double midDistance = (lowDistance + highDistance) * 0.5;
                    double midGap = 0.0;
                    if (evaluateGapProfile(midDistance, nullptr, &midGap) &&
                        midGap >= values.slotClearance)
                    {
                        highDistance = midDistance;
                        highGap = midGap;
                    }
                    else
                    {
                        lowDistance = midDistance;
                    }
                }

                double finalGap = 0.0;
                explicitGapProfileCreated =
                    evaluateGapProfile(highDistance, &profilePoints, &finalGap);
                if (explicitGapProfileCreated)
                {
                    std::ostringstream log;
                    log << "CreateNonRightCornerEdgeCut profile=explicit-OACEO"
                        << ", C distance=" << highDistance
                        << ", BD gap=" << finalGap
                        << ", target=" << values.slotClearance;
                    AppendDebugLog(log.str());
                }
            }
        }
    }

    if (!explicitGapProfileCreated && angleDegrees < 180.0 - 1.0)
    {
        NXOpen::Vector3d bisectorDirection = ProjectVectorToPlane(
            NXOpen::Vector3d(
                inwardNormals[0].X + inwardNormals[1].X,
                inwardNormals[0].Y + inwardNormals[1].Y,
                inwardNormals[0].Z + inwardNormals[1].Z),
            edgeDirection);
        if (!Normalize(bisectorDirection))
        {
            bisectorDirection = NXOpen::Vector3d(
                faceDirection1.X + faceDirection2.X,
                faceDirection1.Y + faceDirection2.Y,
                faceDirection1.Z + faceDirection2.Z);
            if (!Normalize(bisectorDirection))
            {
                return false;
            }
        }

        const NXOpen::Vector3d offsetDirection1 =
            InwardOffsetDirectionInPlane(faceDirection1, edgeDirection, bisectorDirection);
        const NXOpen::Vector3d offsetDirection2 =
            InwardOffsetDirectionInPlane(faceDirection2, edgeDirection, bisectorDirection);
        if (Length(offsetDirection1) <= kPointTolerance || Length(offsetDirection2) <= kPointTolerance)
        {
            return false;
        }

        const NXOpen::Point3d face1OffsetLineOrigin =
            Add(profileOrigin, Scale(offsetDirection1, offsetDistance));
        const NXOpen::Point3d face2OffsetLineOrigin =
            Add(profileOrigin, Scale(offsetDirection2, offsetDistance));
        NXOpen::Point3d topPoint(0.0, 0.0, 0.0);
        if (!IntersectCoplanarLines(
                face1OffsetLineOrigin,
                faceDirection1,
                face2OffsetLineOrigin,
                faceDirection2,
                edgeDirection,
                topPoint))
        {
            return false;
        }

        const NXOpen::Point3d bottomLeftPoint =
            Add(topPoint, Scale(offsetDirection1, -offsetDistance));
        const NXOpen::Point3d rightPoint =
            Add(topPoint, Scale(offsetDirection2, -offsetDistance));
        const double currentGap = std::min(
            PointDistance(profileOrigin, bottomLeftPoint),
            PointDistance(profileOrigin, rightPoint));
        double gapScale = 1.0;
        if (values.slotClearance > kPointTolerance && currentGap > kPointTolerance)
        {
            gapScale = values.slotClearance / currentGap;
        }
        const NXOpen::Point3d scaledBottomLeftPoint =
            Add(profileOrigin, Scale(Subtract(bottomLeftPoint, profileOrigin), gapScale));
        const NXOpen::Point3d scaledTopPoint =
            Add(profileOrigin, Scale(Subtract(topPoint, profileOrigin), gapScale));
        const NXOpen::Point3d scaledRightPoint =
            Add(profileOrigin, Scale(Subtract(rightPoint, profileOrigin), gapScale));
        {
            std::ostringstream log;
            log << "CreateNonRightCornerEdgeCut acute gap current=" << currentGap
                << ", target=" << values.slotClearance
                << ", scale=" << gapScale;
            AppendDebugLog(log.str());
        }
        profilePoints = {profileOrigin, scaledBottomLeftPoint, scaledTopPoint, scaledRightPoint, profileOrigin};
        AppendDebugLog("CreateNonRightCornerEdgeCut profile=acute-source-4-lines");
    }
    else if (!explicitGapProfileCreated)
    {
        const double angleRadians = angleDegrees * 3.14159265358979323846 / 180.0;
        const double denominator = 2.0 * std::sin(angleRadians * 0.5);
        if (std::fabs(denominator) <= 1.0e-8)
        {
            return false;
        }

        const double endpointSpacing = std::max(0.001, values.slotClearance * 0.5);
        const double oaLength = endpointSpacing / denominator;
        if (oaLength <= kPointTolerance)
        {
            return false;
        }

        NXOpen::Vector3d bisectorDirection = Scale(
            NXOpen::Vector3d(
                faceDirection1.X + faceDirection2.X,
                faceDirection1.Y + faceDirection2.Y,
                faceDirection1.Z + faceDirection2.Z),
            -1.0);
        if (!Normalize(bisectorDirection))
        {
            bisectorDirection = ProjectVectorToPlane(
                Scale(
                    NXOpen::Vector3d(
                        inwardNormals[0].X + inwardNormals[1].X,
                        inwardNormals[0].Y + inwardNormals[1].Y,
                        inwardNormals[0].Z + inwardNormals[1].Z),
                    -1.0),
                edgeDirection);
            if (!Normalize(bisectorDirection))
            {
                return false;
            }
        }

        const NXOpen::Vector3d perpendicularDirection1 =
            InwardOffsetDirectionInPlane(faceDirection1, edgeDirection, bisectorDirection);
        const NXOpen::Vector3d perpendicularDirection2 =
            InwardOffsetDirectionInPlane(faceDirection2, edgeDirection, bisectorDirection);
        if (Length(perpendicularDirection1) <= kPointTolerance ||
            Length(perpendicularDirection2) <= kPointTolerance)
        {
            return false;
        }

        const NXOpen::Point3d pointA = Add(profileOrigin, Scale(faceDirection1, oaLength));
        const NXOpen::Point3d pointB = Add(profileOrigin, Scale(faceDirection2, oaLength));
        const NXOpen::Point3d pointC = Add(pointA, Scale(perpendicularDirection1, offsetDistance));
        const NXOpen::Point3d pointE = Add(pointB, Scale(perpendicularDirection2, offsetDistance));
        NXOpen::Point3d pointD(0.0, 0.0, 0.0);
        if (!IntersectCoplanarLines(pointC, faceDirection1, pointE, faceDirection2, edgeDirection, pointD))
        {
            return false;
        }
        profilePoints = {profileOrigin, pointA, pointC, pointD, pointE, pointB, profileOrigin};
        AppendDebugLog("CreateNonRightCornerEdgeCut profile=obtuse-source-6-lines");
    }

    {
        std::ostringstream log;
        log << "CreateNonRightCornerEdgeCut profile points";
        for (std::size_t i = 0; i < profilePoints.size(); ++i)
        {
            log << " p" << i << "=("
                << profilePoints[i].X << "," << profilePoints[i].Y << "," << profilePoints[i].Z << ")";
        }
        AppendDebugLog(log.str());
    }

    std::vector<tag_t> curveTags;
    auto deleteNonRightSectionCurves = [&curveTags]() -> std::pair<int, int>
    {
        int deleteFailures = 0;
        int blankFailures = 0;
        for (tag_t curveTag : curveTags)
        {
            if (curveTag == NULL_TAG)
            {
                continue;
            }
            const int deleteResult = UF_OBJ_delete_object(curveTag);
            if (deleteResult != 0)
            {
                ++deleteFailures;
                const int blankResult = UF_OBJ_set_blank_status(curveTag, UF_OBJ_BLANKED);
                if (blankResult != 0)
                {
                    ++blankFailures;
                }
            }
        }
        curveTags.clear();
        return std::make_pair(deleteFailures, blankFailures);
    };
    for (std::size_t index = 0; index + 1 < profilePoints.size(); ++index)
    {
        UF_CURVE_line_t lineData;
        CopyPointToArray(profilePoints[index], lineData.start_point);
        CopyPointToArray(profilePoints[index + 1], lineData.end_point);
        tag_t lineTag = NULL_TAG;
        if (UF_CURVE_create_line(&lineData, &lineTag) != 0 || lineTag == NULL_TAG)
        {
            deleteNonRightSectionCurves();
            return false;
        }
        curveTags.push_back(lineTag);
    }

    uf_list_p_t curveList = NULL;
    if (UF_MODL_create_list(&curveList) != 0 || curveList == NULL)
    {
        deleteNonRightSectionCurves();
        return false;
    }
    for (tag_t curveTag : curveTags)
    {
        UF_MODL_put_list_item(curveList, curveTag);
    }

    std::string startLimit = FormatLimit(-values.sheetThickness);
    std::string endLimit = FormatLimit(edgeLength + values.sheetThickness);
    char taperAngle[] = "0.0";
    char* limits[2] = {const_cast<char*>(startLimit.c_str()), const_cast<char*>(endLimit.c_str())};
    double origin[3] = {profileOrigin.X, profileOrigin.Y, profileOrigin.Z};
    double direction[3] = {edgeDirection.X, edgeDirection.Y, edgeDirection.Z};
    uf_list_p_t featureList = NULL;
    const int extrudeResult = UF_MODL_create_extruded(
        curveList,
        taperAngle,
        limits,
        origin,
        direction,
        UF_NULLSIGN,
        &featureList);
    UF_MODL_delete_list(&curveList);
    AppendDebugLog("CreateNonRightCornerEdgeCut extrude rc=" +
                   std::to_string(extrudeResult) +
                   " (" + UfFailureMessage(extrudeResult) + ")");
    if (extrudeResult != 0 || featureList == NULL)
    {
        if (featureList != NULL)
        {
            UF_MODL_delete_list(&featureList);
        }
        deleteNonRightSectionCurves();
        return false;
    }

    tag_t featureTag = NULL_TAG;
    int featureCount = 0;
    if (UF_MODL_ask_list_count(featureList, &featureCount) == 0 && featureCount > 0)
    {
        UF_MODL_ask_list_item(featureList, 0, &featureTag);
    }
    UF_MODL_delete_list(&featureList);
    if (featureTag == NULL_TAG)
    {
        deleteNonRightSectionCurves();
        return false;
    }

    tag_t toolBody = NULL_TAG;
    if (UF_MODL_ask_feat_body(featureTag, &toolBody) != 0 || toolBody == NULL_TAG)
    {
        values.profileFeatureTags.push_back(featureTag);
        deleteNonRightSectionCurves();
        return false;
    }
    values.profileToolBodyTags.push_back(toolBody);

    uf_list_p_t bodyList = NULL;
    int removeParmsResult = UF_MODL_create_list(&bodyList);
    if (removeParmsResult == 0 && bodyList != NULL)
    {
        removeParmsResult = UF_MODL_put_list_item(bodyList, toolBody);
        if (removeParmsResult == 0)
        {
            removeParmsResult = UF_MODL_delete_body_parms(bodyList);
        }
        UF_MODL_delete_list(&bodyList);
    }
    if (removeParmsResult != 0)
    {
        values.profileFeatureTags.push_back(featureTag);
        deleteNonRightSectionCurves();
        return false;
    }

    const std::vector<tag_t> targetFeatureTagsBefore = AskBodyFeatureTags(values.targetBody->Tag());
    int resultCount = 0;
    tag_t* resultingBodies = NULL;
    const int subtractResult = UF_MODL_subtract_bodies(
        values.targetBody->Tag(),
        toolBody,
        &resultCount,
        &resultingBodies);
    if (resultingBodies != NULL)
    {
        UF_free(resultingBodies);
    }
    AppendDebugLog("CreateNonRightCornerEdgeCut subtract rc=" +
                   std::to_string(subtractResult) +
                   " (" + UfFailureMessage(subtractResult) + ")");
    const std::pair<int, int> deleteCurveResults = deleteNonRightSectionCurves();
    AppendDebugLog("CreateNonRightCornerEdgeCut delete curve failures=" +
                   std::to_string(deleteCurveResults.first) +
                   ", blank curve failures=" + std::to_string(deleteCurveResults.second));
    if (subtractResult != 0)
    {
        return false;
    }

    const std::vector<tag_t> targetFeatureTagsAfter = AskBodyFeatureTags(values.targetBody->Tag());
    for (tag_t newFeatureTag : targetFeatureTagsAfter)
    {
        if (std::find(targetFeatureTagsBefore.begin(), targetFeatureTagsBefore.end(), newFeatureTag) ==
            targetFeatureTagsBefore.end())
        {
            values.profileFeatureTags.push_back(newFeatureTag);
        }
    }
    return true;
}

bool TwoPointBiLanCaoDialog::CreateCornerEdgeCutWithFace(
    DialogValues& values,
    NXOpen::Edge* cornerEdge,
    NXOpen::Face* referenceFace) const
{
    NXOpen::Part* workPart = session_ != nullptr && session_->Parts() != nullptr ? session_->Parts()->Work() : nullptr;
    AppendDebugLog("CreateCornerEdgeCutWithFace begin cornerEdge=" +
                   TagText(cornerEdge != nullptr ? cornerEdge->Tag() : NULL_TAG) +
                   ", targetBody=" +
                   TagText(values.targetBody != nullptr ? values.targetBody->Tag() : NULL_TAG) +
                   ", referenceFace=" +
                   TagText(referenceFace != nullptr ? referenceFace->Tag() : NULL_TAG));
    if (workPart == nullptr || values.targetBody == nullptr || cornerEdge == nullptr || referenceFace == nullptr)
    {
        AppendDebugLog("CreateCornerEdgeCutWithFace failed: null workPart/body/edge/face");
        return false;
    }

    NXOpen::Vector3d inwardNormal;
    if (!ComputeInwardNormal(values.targetBody, referenceFace, inwardNormal))
    {
        AppendDebugLog("CreateCornerEdgeCutWithFace failed: ComputeInwardNormal referenceFace=" +
                       TagText(referenceFace != nullptr ? referenceFace->Tag() : NULL_TAG));
        return false;
    }
    {
        std::ostringstream log;
        log << "CreateCornerEdgeCutWithFace referenceFace="
            << (referenceFace != nullptr ? referenceFace->Tag() : NULL_TAG)
            << ", inwardNormal=(" << inwardNormal.X << "," << inwardNormal.Y << "," << inwardNormal.Z << ")";
        AppendDebugLog(log.str());
    }

    NXOpen::Point3d edgeVertex1;
    NXOpen::Point3d edgeVertex2;
    try
    {
        cornerEdge->GetVertices(&edgeVertex1, &edgeVertex2);
    }
    catch (...)
    {
        AppendDebugLog("CreateCornerEdgeCutWithFace failed: GetVertices exception");
        return false;
    }
    {
        std::ostringstream log;
        log << "CreateCornerEdgeCutWithFace vertices p1=("
            << edgeVertex1.X << "," << edgeVertex1.Y << "," << edgeVertex1.Z << ") p2=("
            << edgeVertex2.X << "," << edgeVertex2.Y << "," << edgeVertex2.Z << ")";
        AppendDebugLog(log.str());
    }

    const NXOpen::Point3d helpPoint(
        (edgeVertex1.X + edgeVertex2.X) * 0.5,
        (edgeVertex1.Y + edgeVertex2.Y) * 0.5,
        (edgeVertex1.Z + edgeVertex2.Z) * 0.5);

    NXOpen::Features::ExtrudeBuilder* extrudeBuilder = nullptr;
    try
    {
        extrudeBuilder = workPart->Features()->CreateExtrudeBuilder(nullptr);
        NXOpen::Section* section = workPart->Sections()->CreateSection(9.5e-05, 0.0001, 0.5);
        extrudeBuilder->SetSection(section);
        extrudeBuilder->AllowSelfIntersectingSection(true);
        extrudeBuilder->SetDistanceTolerance(0.0001);
        extrudeBuilder->BooleanOperation()->SetType(
            NXOpen::GeometricUtilities::BooleanOperation::BooleanTypeCreate);
        extrudeBuilder->SmartVolumeProfile()->SetOpenProfileSmartVolumeOption(false);
        extrudeBuilder->SmartVolumeProfile()->SetCloseProfileRule(
            NXOpen::GeometricUtilities::SmartVolumeProfileBuilder::CloseProfileRuleTypeFci);

        section->SetDistanceTolerance(0.0001);
        section->SetChainingTolerance(9.5e-05);
        section->SetAllowedEntityTypes(NXOpen::Section::AllowTypesOnlyCurves);
        section->AllowSelfIntersection(true);
        section->AllowDegenerateCurves(false);

        NXOpen::SelectionIntentRuleOptions* ruleOptions = workPart->ScRuleFactory()->CreateRuleOptions();
        ruleOptions->SetSelectedFromInactive(false);
        std::vector<NXOpen::Edge*> edgeSeed(1, cornerEdge);
        NXOpen::EdgeDumbRule* edgeRule = workPart->ScRuleFactory()->CreateRuleEdgeDumb(edgeSeed, ruleOptions);
        delete ruleOptions;

        std::vector<NXOpen::SelectionIntentRule*> rules(1, edgeRule);
        section->AddToSection(
            rules,
            cornerEdge,
            nullptr,
            nullptr,
            helpPoint,
            NXOpen::Section::ModeCreate,
            false);

        NXOpen::Direction* direction = workPart->Directions()->CreateDirection(
            helpPoint,
            inwardNormal,
            NXOpen::SmartObject::UpdateOptionWithinModeling);
        extrudeBuilder->SetDirection(direction);

        const double startDistance = values.sheetThickness;
        const double endDistance = values.sheetThickness + std::max(values.slotClearance, kPointTolerance);
        {
            std::ostringstream log;
            log << "CreateCornerEdgeCutWithFace limits start=" << startDistance
                << ", end=" << endDistance
                << ", offsetEnd=" << values.sheetThickness;
            AppendDebugLog(log.str());
        }
        extrudeBuilder->Limits()->StartExtend()->Value()->SetFormula(FormatLimit(startDistance).c_str());
        extrudeBuilder->Limits()->EndExtend()->Value()->SetFormula(FormatLimit(endDistance).c_str());
        extrudeBuilder->Limits()->StartExtend()->SetTrimType(NXOpen::GeometricUtilities::Extend::ExtendTypeValue);
        extrudeBuilder->Limits()->EndExtend()->SetTrimType(NXOpen::GeometricUtilities::Extend::ExtendTypeValue);
        extrudeBuilder->Offset()->SetOption(NXOpen::GeometricUtilities::TypeSymmetricOffset);
        extrudeBuilder->Offset()->StartOffset()->SetFormula("0.0");
        extrudeBuilder->Offset()->EndOffset()->SetFormula(FormatLimit(values.sheetThickness).c_str());
        extrudeBuilder->FeatureOptions()->SetBodyType(NXOpen::GeometricUtilities::FeatureOptions::BodyStyleSolid);

        NXOpen::Features::Feature* feature = extrudeBuilder->CommitFeature();
        AppendDebugLog("CreateCornerEdgeCutWithFace commit feature=" +
                       TagText(feature != nullptr ? feature->Tag() : NULL_TAG));

        extrudeBuilder->Destroy();
        if (feature == nullptr || feature->Tag() == NULL_TAG)
        {
            return false;
        }

        tag_t toolBody = NULL_TAG;
        const int askBodyResult = UF_MODL_ask_feat_body(feature->Tag(), &toolBody);
        AppendDebugLog("CreateCornerEdgeCutWithFace ask tool body rc=" +
                       std::to_string(askBodyResult) +
                       ", body=" + TagText(toolBody));
        if (askBodyResult != 0 || toolBody == NULL_TAG)
        {
            values.profileFeatureTags.push_back(feature->Tag());
            return false;
        }
        values.profileToolBodyTags.push_back(toolBody);

        uf_list_p_t bodyList = NULL;
        int removeParmsResult = UF_MODL_create_list(&bodyList);
        if (removeParmsResult == 0 && bodyList != NULL)
        {
            removeParmsResult = UF_MODL_put_list_item(bodyList, toolBody);
            if (removeParmsResult == 0)
            {
                removeParmsResult = UF_MODL_delete_body_parms(bodyList);
            }
            UF_MODL_delete_list(&bodyList);
        }
        AppendDebugLog("CreateCornerEdgeCutWithFace delete tool body parms rc=" +
                       std::to_string(removeParmsResult) +
                       " (" + UfFailureMessage(removeParmsResult) + ")");
        if (removeParmsResult != 0)
        {
            values.profileFeatureTags.push_back(feature->Tag());
            return false;
        }

        const std::vector<tag_t> targetFeatureTagsBefore =
            AskBodyFeatureTags(values.targetBody->Tag());

        int resultCount = 0;
        tag_t* resultingBodies = NULL;
        const int subtractResult = UF_MODL_subtract_bodies(
            values.targetBody->Tag(),
            toolBody,
            &resultCount,
            &resultingBodies);
        if (resultingBodies != NULL)
        {
            UF_free(resultingBodies);
        }
        AppendDebugLog("CreateCornerEdgeCutWithFace subtract rc=" +
                       std::to_string(subtractResult) +
                       " (" + UfFailureMessage(subtractResult) + ")" +
                       ", resultCount=" + std::to_string(resultCount));
        if (subtractResult != 0)
        {
            return false;
        }

        const std::vector<tag_t> targetFeatureTagsAfter =
            AskBodyFeatureTags(values.targetBody->Tag());
        for (tag_t featureTag : targetFeatureTagsAfter)
        {
            if (std::find(targetFeatureTagsBefore.begin(), targetFeatureTagsBefore.end(), featureTag) ==
                targetFeatureTagsBefore.end())
            {
                values.profileFeatureTags.push_back(featureTag);
            }
        }
        AppendDebugLog("CreateCornerEdgeCutWithFace new boolean feature count=" +
                       std::to_string(values.profileFeatureTags.size()));
        return true;
    }
    catch (const NXOpen::NXException& ex)
    {
        AppendDebugLog(std::string("CreateCornerEdgeCutWithFace NXException: ") + ex.Message());
        if (extrudeBuilder != nullptr)
        {
            try
            {
                extrudeBuilder->Destroy();
            }
            catch (...)
            {
            }
        }
    }
    catch (const std::exception& ex)
    {
        AppendDebugLog(std::string("CreateCornerEdgeCutWithFace std::exception: ") + ex.what());
        if (extrudeBuilder != nullptr)
        {
            try
            {
                extrudeBuilder->Destroy();
            }
            catch (...)
            {
            }
        }
    }
    catch (...)
    {
        AppendDebugLog("CreateCornerEdgeCutWithFace unknown exception");
        if (extrudeBuilder != nullptr)
        {
            try
            {
                extrudeBuilder->Destroy();
            }
            catch (...)
            {
            }
        }
    }

    return false;
}

bool TwoPointBiLanCaoDialog::CreateCornerEdgeCut(DialogValues& values, NXOpen::Edge* cornerEdge) const
{
    if (values.targetBody == nullptr || cornerEdge == nullptr)
    {
        return false;
    }

    NXOpen::Point3d edgeVertex1;
    NXOpen::Point3d edgeVertex2;
    try
    {
        cornerEdge->GetVertices(&edgeVertex1, &edgeVertex2);
    }
    catch (...)
    {
        return false;
    }

    bool created = false;
    double angleDegrees = 0.0;
    std::vector<NXOpen::Face*> principalFaces;
    std::vector<NXOpen::Vector3d> inwardNormals;
    const bool hasAngle = GetSelectedEdgeFaceAngle(
        values.targetBody,
        cornerEdge,
        angleDegrees,
        principalFaces,
        inwardNormals);
    const bool isRightAngle = hasAngle && std::fabs(angleDegrees - 90.0) <= 1.0;
    AppendDebugLog("CreateCornerEdgeCut angle route hasAngle=" +
                   std::to_string(hasAngle ? 1 : 0) +
                   ", angle=" + FormatLimit(angleDegrees) +
                   ", right=" + std::to_string(isRightAngle ? 1 : 0) +
                   ", wrapCornerMode=" + std::to_string(values.wrapCornerMode));

    NXOpen::Face* largerFace = FindLargerBoundingFace(cornerEdge);
    NXOpen::Face* smallerFace = FindSmallerBoundingFace(cornerEdge);
    NXOpen::Face* preferredFace = values.wrapCornerMode == 0 ? largerFace : smallerFace;
    if (preferredFace == nullptr)
    {
        preferredFace = values.wrapCornerMode == 0 ? smallerFace : largerFace;
    }
    AppendDebugLog("CreateCornerEdgeCut wrap reference largerFace=" +
                   TagText(largerFace != nullptr ? largerFace->Tag() : NULL_TAG) +
                   ", smallerFace=" +
                   TagText(smallerFace != nullptr ? smallerFace->Tag() : NULL_TAG) +
                   ", preferredFace=" +
                   TagText(preferredFace != nullptr ? preferredFace->Tag() : NULL_TAG));

    struct PendingTopCornerCut
    {
        bool create = false;
        NXOpen::Point3d point;
        NXOpen::Vector3d cornerEdgeDirection;
        NXOpen::Vector3d edge1Direction;
        NXOpen::Vector3d edge2Direction;
    };

    auto prepareTopCornerCut = [&](const NXOpen::Point3d& point) -> PendingTopCornerCut
    {
        PendingTopCornerCut pending;
        pending.point = point;
        if (!ShouldCreateCubeForEdgeEndpoint(values.targetBody, point, values.sheetThickness, angleDegrees))
        {
            return pending;
        }

        const std::vector<NXOpen::Edge*> connectedEdges =
            FindEdgesConnectedToPointOnBody(values.targetBody, point);
        std::vector<NXOpen::Edge*> planeEdges;
        for (NXOpen::Edge* connectedEdge : connectedEdges)
        {
            if (connectedEdge != nullptr && connectedEdge->Tag() != cornerEdge->Tag())
            {
                planeEdges.push_back(connectedEdge);
            }
        }
        if (planeEdges.size() < 2)
        {
            AppendDebugLog("PrepareTopCornerCut failed: plane edge count=" +
                           std::to_string(planeEdges.size()) +
                           ", connected edge count=" +
                           std::to_string(connectedEdges.size()));
            return pending;
        }

        if (!EdgeDirectionFromPoint(cornerEdge, point, pending.cornerEdgeDirection) ||
            !EdgeDirectionFromPoint(planeEdges[0], point, pending.edge1Direction) ||
            !EdgeDirectionFromPoint(planeEdges[1], point, pending.edge2Direction))
        {
            AppendDebugLog("PrepareTopCornerCut failed: edge direction");
            return pending;
        }

        if (!Normalize(pending.cornerEdgeDirection) ||
            !Normalize(pending.edge1Direction) ||
            !Normalize(pending.edge2Direction))
        {
            AppendDebugLog("PrepareTopCornerCut failed: normalize direction");
            return pending;
        }

        if (Dot(Cross(pending.edge1Direction, pending.edge2Direction), pending.cornerEdgeDirection) < 0.0)
        {
            std::swap(pending.edge1Direction, pending.edge2Direction);
        }
        pending.create = true;
        AppendDebugLog("PrepareTopCornerCut success point=(" +
                       FormatLimit(point.X) + "," +
                       FormatLimit(point.Y) + "," +
                       FormatLimit(point.Z) + ")");
        return pending;
    };

    const double vertex1ToSlotEnd = PointDistance(edgeVertex1, values.endPoint);
    const double vertex2ToSlotEnd = PointDistance(edgeVertex2, values.endPoint);
    PendingTopCornerCut topCornerCut1;
    PendingTopCornerCut topCornerCut2;
    if (vertex1ToSlotEnd > vertex2ToSlotEnd + kLooseEndpointTolerance)
    {
        topCornerCut1 = prepareTopCornerCut(edgeVertex1);
        AppendDebugLog("CreateCornerEdgeCut skip vertex2 top-corner cut: near slot end");
    }
    else if (vertex2ToSlotEnd > vertex1ToSlotEnd + kLooseEndpointTolerance)
    {
        topCornerCut2 = prepareTopCornerCut(edgeVertex2);
        AppendDebugLog("CreateCornerEdgeCut skip vertex1 top-corner cut: near slot end");
    }
    else
    {
        AppendDebugLog("CreateCornerEdgeCut skip top-corner cut: endpoint distance tie");
    }

    if (hasAngle && !isRightAngle)
    {
        std::vector<NXOpen::Face*> orderedPrincipalFaces = principalFaces;
        std::vector<NXOpen::Vector3d> orderedInwardNormals = inwardNormals;
        if (preferredFace != nullptr && orderedPrincipalFaces.size() >= 2 &&
            orderedPrincipalFaces[1] != nullptr &&
            orderedPrincipalFaces[1]->Tag() == preferredFace->Tag())
        {
            std::swap(orderedPrincipalFaces[0], orderedPrincipalFaces[1]);
            std::swap(orderedInwardNormals[0], orderedInwardNormals[1]);
        }
        created = CreateNonRightCornerEdgeCut(
            values,
            cornerEdge,
            angleDegrees,
            orderedPrincipalFaces,
            orderedInwardNormals);
    }
    else
    {
        NXOpen::Face* referenceFace = preferredFace != nullptr
            ? preferredFace
            : (!principalFaces.empty() ? principalFaces.front() : FindLargerBoundingFace(cornerEdge));
        created = CreateCornerEdgeCutWithFace(values, cornerEdge, referenceFace);
    }

    if (created)
    {
        if (topCornerCut1.create)
        {
            (void)CreateTopCornerEndpointCutWithDirections(
                values,
                topCornerCut1.point,
                topCornerCut1.cornerEdgeDirection,
                topCornerCut1.edge1Direction,
                topCornerCut1.edge2Direction);
        }
        if (topCornerCut2.create)
        {
            (void)CreateTopCornerEndpointCutWithDirections(
                values,
                topCornerCut2.point,
                topCornerCut2.cornerEdgeDirection,
                topCornerCut2.edge1Direction,
                topCornerCut2.edge2Direction);
        }
    }

    return created;
}

bool TwoPointBiLanCaoDialog::CreateReliefSlotProfile(DialogValues& values, bool commitCut) const
{
    values.profileCurveTags.clear();
    values.profileFeatureTags.clear();
    values.profileToolBodyTags.clear();
    values.profilePointTrace.clear();
    values.profileOperationTrace.clear();
    values.profileEdge1Tag = NULL_TAG;
    values.profileEdge2Tag = NULL_TAG;
    values.profileSketchTag = NULL_TAG;
    values.profileExtrudeFeatureTag = NULL_TAG;
    values.profileToolBodyTag = NULL_TAG;
    values.profileEdgeAngleDeg = 0.0;
    values.profileABLength = 0.0;
    values.profileGALength = 0.0;
    values.profileExtrudeHeight = 0.0;
    values.slotSidePoint1 = NXOpen::Point3d(0.0, 0.0, 0.0);
    values.slotSidePoint2 = NXOpen::Point3d(0.0, 0.0, 0.0);
    values.slotSideDirection1 = NXOpen::Vector3d(0.0, 0.0, 0.0);
    values.slotSideDirection2 = NXOpen::Vector3d(0.0, 0.0, 0.0);
    values.hasSlotSideDirections = false;
    values.profileExtrudeDirectionTrace.clear();
    if (values.targetBody == nullptr || values.containingPlaneFace == nullptr || values.sheetThickness <= 0.0)
    {
        values.profileOperationTrace += "\n    invalid base data: body=" +
                                        TagText(values.targetBody != nullptr ? values.targetBody->Tag() : NULL_TAG) +
                                        ", face=" +
                                        TagText(values.containingPlaneFace != nullptr ? values.containingPlaneFace->Tag() : NULL_TAG) +
                                        ", thickness=" + FormatLimit(values.sheetThickness);
        return false;
    }

    NXOpen::Point3d planeOrigin;
    NXOpen::Vector3d planeNormal;
    if (!GetPlanarFacePlane(values.containingPlaneFace, planeOrigin, planeNormal))
    {
        values.profileOperationTrace += "\n    GetPlanarFacePlane failed";
        return false;
    }

    NXOpen::Vector3d centerDirection = ProjectVectorToPlane(Subtract(values.endPoint, values.startPoint), planeNormal);
    if (!Normalize(centerDirection))
    {
        values.profileOperationTrace += "\n    center direction normalize failed";
        return false;
    }

    NXOpen::Vector3d slotNormal = Cross(planeNormal, centerDirection);
    if (!Normalize(slotNormal))
    {
        values.profileOperationTrace += "\n    slot normal normalize failed";
        return false;
    }

    NXOpen::Edge* cornerEdge = nullptr;
    const double halfClearance = values.slotClearance * 0.5;
    bool foundProfile = false;
    std::vector<NXOpen::Point3d> profilePoints;
    double bestABLength = 0.0;
    double bestGALength = 0.0;

    if (values.gapOnlyMode)
    {
        const double extendBothEnds = 5.0;
        const NXOpen::Point3d slotStart = Add(values.startPoint, Scale(centerDirection, -extendBothEnds));
        const NXOpen::Point3d slotEnd = Add(values.endPoint, Scale(centerDirection, extendBothEnds));
        if (values.gapOnlyHasOffsetDirection)
        {
            values.gapOnlyOffsetDirection =
                ProjectVectorToPlane(values.gapOnlyOffsetDirection, planeNormal);
            values.gapOnlyOffsetDirection =
                ProjectVectorToPlane(values.gapOnlyOffsetDirection, centerDirection);
            if (!Normalize(values.gapOnlyOffsetDirection) ||
                std::fabs(Dot(values.gapOnlyOffsetDirection, slotNormal)) <= kPointTolerance)
            {
                values.gapOnlyHasOffsetDirection = false;
            }
            else if (Dot(values.gapOnlyOffsetDirection, slotNormal) < 0.0)
            {
                slotNormal = Scale(slotNormal, -1.0);
            }
        }

        NXOpen::Point3d startLeft;
        NXOpen::Point3d endLeft;
        NXOpen::Point3d endRight;
        NXOpen::Point3d startRight;
        if (values.gapOnlyHasOffsetDirection)
        {
            startLeft = slotStart;
            endLeft = slotEnd;
            endRight = Add(slotEnd, Scale(slotNormal, values.slotClearance));
            startRight = Add(slotStart, Scale(slotNormal, values.slotClearance));
        }
        else
        {
            startLeft = Add(slotStart, Scale(slotNormal, halfClearance));
            endLeft = Add(slotEnd, Scale(slotNormal, halfClearance));
            endRight = Add(slotEnd, Scale(slotNormal, -halfClearance));
            startRight = Add(slotStart, Scale(slotNormal, -halfClearance));
        }

        profilePoints = {startLeft, endLeft, endRight, startRight, startLeft};
        values.slotSidePoint1 = startLeft;
        values.slotSidePoint2 = startRight;
        values.slotSideDirection1 = Subtract(endLeft, startLeft);
        values.slotSideDirection2 = Subtract(endRight, startRight);
        values.hasSlotSideDirections =
            Normalize(values.slotSideDirection1) &&
            Normalize(values.slotSideDirection2);
        foundProfile = true;
        values.profileExtrudeHeight = values.sheetThickness + values.bendRadius;
        AppendDebugLog("CreateReliefSlotProfile gap only enabled");
    }

    if (!foundProfile)
    {
    NXOpen::Edge* edge1 = nullptr;
    NXOpen::Edge* edge2 = nullptr;
    if (!FindTwoPlanarEdgesAtPoint(values.containingPlaneFace, values.startPoint, &edge1, &edge2))
    {
        NXOpen::Face* edgePlaneFace = nullptr;
        if (values.targetBody != nullptr)
        {
            const std::vector<NXOpen::Face*> bodyFaces = values.targetBody->GetFaces();
            for (NXOpen::Face* candidateFace : bodyFaces)
            {
                if (candidateFace == nullptr || candidateFace == values.containingPlaneFace)
                {
                    continue;
                }

                try
                {
                    if (candidateFace->SolidFaceType() != NXOpen::Face::FaceTypePlanar)
                    {
                        continue;
                    }
                }
                catch (...)
                {
                    continue;
                }

                NXOpen::Point3d candidateOrigin;
                NXOpen::Vector3d candidateNormal;
                if (!GetPlanarFacePlane(candidateFace, candidateOrigin, candidateNormal))
                {
                    continue;
                }

                if (std::fabs(Dot(candidateNormal, planeNormal)) < 1.0 - 1.0e-4)
                {
                    continue;
                }

                const double startDistance = std::fabs(Dot(Subtract(values.startPoint, candidateOrigin), candidateNormal));
                const double endDistance = std::fabs(Dot(Subtract(values.endPoint, candidateOrigin), candidateNormal));
                if (startDistance > kPlaneTolerance || endDistance > kPlaneTolerance)
                {
                    continue;
                }

                NXOpen::Edge* candidateEdge1 = nullptr;
                NXOpen::Edge* candidateEdge2 = nullptr;
                if (FindTwoPlanarEdgesAtPoint(candidateFace, values.startPoint, &candidateEdge1, &candidateEdge2))
                {
                    edgePlaneFace = candidateFace;
                    edge1 = candidateEdge1;
                    edge2 = candidateEdge2;
                    values.containingPlaneFace = candidateFace;
                    planeOrigin = candidateOrigin;
                    planeNormal = candidateNormal;
                    break;
                }
            }
        }

        if (edgePlaneFace == nullptr)
        {
            values.profileOperationTrace += "\n    FindTwoPlanarEdgesAtPoint failed";
            return false;
        }

        values.profileOperationTrace += "\n    switched containing face for corner edges: face=" +
                                        TagText(edgePlaneFace->Tag());
    }
    values.profileEdge1Tag = edge1 != nullptr ? edge1->Tag() : NULL_TAG;
    values.profileEdge2Tag = edge2 != nullptr ? edge2->Tag() : NULL_TAG;
    if (values.chamferEdgeMode)
    {
        AppendDebugLog("CreateReliefSlotProfile chamfer enabled: planeEdge1=" +
                       TagText(values.profileEdge1Tag) +
                       ", planeEdge2=" +
                       TagText(values.profileEdge2Tag));
        cornerEdge = FindCornerEdgeAtPoint(values.targetBody, values.startPoint, edge1, edge2);
    }
    else
    {
        AppendDebugLog("CreateReliefSlotProfile chamfer disabled");
    }

    const double edge1Length = EdgeChordLength(edge1);
    const double edge2Length = EdgeChordLength(edge2);
    const double shortEdgeLimit = values.sheetThickness * 2.5;
    if (edge1Length > kPointTolerance &&
        edge2Length > kPointTolerance &&
        edge1Length <= shortEdgeLimit &&
        edge2Length <= shortEdgeLimit)
    {
        const double extendBothEnds = 5.0;
        const NXOpen::Point3d slotStart = Add(values.startPoint, Scale(centerDirection, -extendBothEnds));
        const NXOpen::Point3d slotEnd = Add(values.endPoint, Scale(centerDirection, extendBothEnds));
        const NXOpen::Point3d startLeft = Add(slotStart, Scale(slotNormal, halfClearance));
        const NXOpen::Point3d endLeft = Add(slotEnd, Scale(slotNormal, halfClearance));
        const NXOpen::Point3d endRight = Add(slotEnd, Scale(slotNormal, -halfClearance));
        const NXOpen::Point3d startRight = Add(slotStart, Scale(slotNormal, -halfClearance));
        profilePoints = {startLeft, endLeft, endRight, startRight, startLeft};
        values.slotSidePoint1 = startLeft;
        values.slotSidePoint2 = startRight;
        values.slotSideDirection1 = Subtract(endLeft, startLeft);
        values.slotSideDirection2 = Subtract(endRight, startRight);
        values.hasSlotSideDirections =
            Normalize(values.slotSideDirection1) &&
            Normalize(values.slotSideDirection2);
        foundProfile = true;
        bestABLength = edge1Length;
        bestGALength = edge2Length;
        values.profileExtrudeHeight = values.sheetThickness + 0.1;
    }

    if (!foundProfile)
    {
        NXOpen::Vector3d edge1Direction;
        if (!EdgeDirectionFromPoint(edge1, values.startPoint, edge1Direction))
        {
            values.profileOperationTrace += "\n    EdgeDirectionFromPoint edge1 failed, edge1=" +
                                            TagText(edge1 != nullptr ? edge1->Tag() : NULL_TAG);
            return false;
        }
        edge1Direction = ProjectVectorToPlane(edge1Direction, planeNormal);
        if (!Normalize(edge1Direction))
        {
            values.profileOperationTrace += "\n    projected edge1 direction normalize failed";
            return false;
        }

        NXOpen::Vector3d edge2Direction;
        if (!EdgeDirectionFromPoint(edge2, values.startPoint, edge2Direction))
        {
            values.profileOperationTrace += "\n    EdgeDirectionFromPoint edge2 failed, edge2=" +
                                            TagText(edge2 != nullptr ? edge2->Tag() : NULL_TAG);
            return false;
        }
        edge2Direction = ProjectVectorToPlane(edge2Direction, planeNormal);
        if (!Normalize(edge2Direction))
        {
            values.profileOperationTrace += "\n    projected edge2 direction normalize failed";
            return false;
        }

        const double sideLength = values.sheetThickness + values.bendRadius;
        const double extendPastEnd = 5.0;
        const NXOpen::Point3d endMid = Add(values.endPoint, Scale(centerDirection, extendPastEnd));
        const double edgeAngleCos = std::max(-1.0, std::min(1.0, Dot(edge1Direction, edge2Direction)));
        values.profileEdgeAngleDeg = std::acos(edgeAngleCos) * 180.0 / 3.14159265358979323846;

        struct CornerSolution
        {
            bool valid;
            NXOpen::Point3d edgePoint;
            NXOpen::Point3d offsetPoint;
            double edgeParameter;
            double centerParameter;
            double sideSign;
        };

        auto calculateCornerByFormula = [&](const NXOpen::Vector3d& edgeDirection,
                                            double sideSign,
                                            double farParameter) -> CornerSolution
        {
            CornerSolution best;
            best.valid = false;
            best.edgeParameter = 0.0;
            best.centerParameter = 0.0;
            best.sideSign = sideSign;

            NXOpen::Vector3d perpendicular = Cross(planeNormal, edgeDirection);
            if (!Normalize(perpendicular))
            {
                return best;
            }

            for (double perpendicularSign : {-1.0, 1.0})
            {
                const NXOpen::Vector3d sideOffset = Scale(slotNormal, sideSign * halfClearance);
                const NXOpen::Vector3d perpendicularOffset = Scale(perpendicular, perpendicularSign * sideLength);
                const NXOpen::Vector3d rhs = Subtract(Add(values.startPoint, sideOffset), Add(values.startPoint, perpendicularOffset));
                const NXOpen::Vector3d negCenter = Scale(centerDirection, -1.0);
                const double denominator = DetInPlane(edgeDirection, negCenter, planeNormal);
                if (std::fabs(denominator) <= kPointTolerance)
                {
                    continue;
                }

                // A + edgeDirection * edgeParameter + perpendicular * sideLength =
                // A + slotNormal * halfClearance + centerDirection * centerParameter.
                const double edgeParameter = DetInPlane(rhs, negCenter, planeNormal) / denominator;
                const double centerParameter = DetInPlane(edgeDirection, rhs, planeNormal) / denominator;
                if (edgeParameter <= kEndpointTolerance ||
                    centerParameter < -kEndpointTolerance ||
                    centerParameter > farParameter + kEndpointTolerance)
                {
                    continue;
                }

                if (!best.valid || edgeParameter > best.edgeParameter)
                {
                    best.valid = true;
                    best.edgeParameter = edgeParameter;
                    best.centerParameter = centerParameter;
                    best.edgePoint = Add(values.startPoint, Scale(edgeDirection, edgeParameter));
                    best.offsetPoint = Add(Add(values.startPoint, sideOffset), Scale(centerDirection, centerParameter));
                }
            }

            return best;
        };

        const double farParameter = Dot(Subtract(endMid, values.startPoint), centerDirection);
        double bestScore = -std::numeric_limits<double>::max();

        for (double side1Sign : {-1.0, 1.0})
        {
            const double side2Sign = -side1Sign;
            for (int swapEdges = 0; swapEdges < 2; ++swapEdges)
            {
                const NXOpen::Vector3d firstEdgeDirection = swapEdges == 0 ? edge1Direction : edge2Direction;
                const NXOpen::Vector3d secondEdgeDirection = swapEdges == 0 ? edge2Direction : edge1Direction;

                const CornerSolution firstCorner = calculateCornerByFormula(firstEdgeDirection, side1Sign, farParameter);
                const CornerSolution secondCorner = calculateCornerByFormula(secondEdgeDirection, side2Sign, farParameter);
                if (!firstCorner.valid || !secondCorner.valid)
                {
                    continue;
                }

                const double score = std::min(firstCorner.edgeParameter, secondCorner.edgeParameter);
                if (score <= bestScore)
                {
                    continue;
                }

                bestScore = score;
                bestABLength = firstCorner.edgeParameter;
                bestGALength = secondCorner.edgeParameter;
                foundProfile = true;
                const NXOpen::Point3d a = values.startPoint;
                const NXOpen::Point3d b = firstCorner.edgePoint;
                const NXOpen::Point3d c = firstCorner.offsetPoint;
                const NXOpen::Point3d d = Add(endMid, Scale(slotNormal, side1Sign * halfClearance));
                const NXOpen::Point3d e = Add(endMid, Scale(slotNormal, side2Sign * halfClearance));
                const NXOpen::Point3d f = secondCorner.offsetPoint;
                const NXOpen::Point3d g = secondCorner.edgePoint;
                profilePoints = {a, b, c, d, e, f, g, a};
                values.slotSidePoint1 = c;
                values.slotSidePoint2 = f;
                values.slotSideDirection1 = Subtract(d, c);
                values.slotSideDirection2 = Subtract(e, f);
                values.hasSlotSideDirections =
                    Normalize(values.slotSideDirection1) &&
                    Normalize(values.slotSideDirection2);
                values.profileExtrudeHeight = values.sheetThickness + values.bendRadius;
            }
        }
    }
    }

    if (!foundProfile)
    {
        values.profileOperationTrace += "\n    profile formula failed: no valid A-B-C-D-E-F-G";
        return false;
    }
    values.profileABLength = bestABLength;
    values.profileGALength = bestGALength;

    {
        std::ostringstream trace;
        const char* names[] = {"A", "B", "C", "D", "E", "F", "G", "A2"};
        for (std::size_t index = 0; index < profilePoints.size(); ++index)
        {
            trace << "\n    " << names[index] << "=("
                  << profilePoints[index].X << ", "
                  << profilePoints[index].Y << ", "
                  << profilePoints[index].Z << ")";
        }
        values.profilePointTrace = trace.str();
    }

    for (std::size_t index = 0; index + 1 < profilePoints.size(); ++index)
    {
        UF_CURVE_line_t lineData;
        CopyPointToArray(profilePoints[index], lineData.start_point);
        CopyPointToArray(profilePoints[index + 1], lineData.end_point);

        tag_t lineTag = NULL_TAG;
        const int createLineResult = UF_CURVE_create_line(&lineData, &lineTag);
        if (createLineResult != 0 || lineTag == NULL_TAG)
        {
            values.profileOperationTrace += "\n    create profile line failed rc=" +
                                            std::to_string(createLineResult) +
                                            " (" + UfFailureMessage(createLineResult) + ")";
            return false;
        }
        values.profileCurveTags.push_back(lineTag);
    }

    if (values.profileCurveTags.size() < 4)
    {
        values.profileOperationTrace += "\n    profile curve count invalid=" +
                                        std::to_string(values.profileCurveTags.size());
        return false;
    }

    if (!commitCut)
    {
        return true;
    }

    values.profileOperationTrace += "\n    sketch skipped; using profile curves directly";
    if (!ExtrudeReliefSlot(values))
    {
        return false;
    }

    std::vector<tag_t> cornerFeatureTags;
    std::vector<tag_t> cornerToolBodyTags;
    if (cornerEdge != nullptr && CreateCornerEdgeCut(values, cornerEdge))
    {
        cornerFeatureTags = values.profileFeatureTags;
        cornerToolBodyTags = values.profileToolBodyTags;
        values.profileFeatureTags.clear();
        values.profileToolBodyTags.clear();
        AppendDebugLog("CreateReliefSlotProfile corner cut success featureCount=" +
                       std::to_string(cornerFeatureTags.size()));
    }
    else if (values.chamferEdgeMode)
    {
        AppendDebugLog("CreateReliefSlotProfile corner cut skipped/failed cornerEdge=" +
                       TagText(cornerEdge != nullptr ? cornerEdge->Tag() : NULL_TAG));
    }

    values.profileFeatureTags.insert(
        values.profileFeatureTags.end(),
        cornerFeatureTags.begin(),
        cornerFeatureTags.end());
    values.profileToolBodyTags.insert(
        values.profileToolBodyTags.end(),
        cornerToolBodyTags.begin(),
        cornerToolBodyTags.end());
    return true;
}

bool TwoPointBiLanCaoDialog::GetFaceInnerNormalDirection(DialogValues& values, NXOpen::Vector3d& direction) const
{
    direction = NXOpen::Vector3d(0.0, 0.0, 0.0);
    NXOpen::Part* workPart = session_ != nullptr && session_->Parts() != nullptr ? session_->Parts()->Work() : nullptr;
    NXOpen::Point* startPointObject = dynamic_cast<NXOpen::Point*>(values.startSelectedObject);
    if (workPart != nullptr && workPart->Directions() != nullptr &&
        values.containingPlaneFace != nullptr && startPointObject != nullptr)
    {
        try
        {
            NXOpen::Direction* faceDirection = workPart->Directions()->CreateDirection(
                startPointObject,
                values.containingPlaneFace,
                NXOpen::Direction::OnFaceOptionNormal,
                nullptr,
                NXOpen::SenseReverse,
                NXOpen::SmartObject::UpdateOptionWithinModeling);
            if (faceDirection != nullptr)
            {
                direction = faceDirection->Vector();
                if (Normalize(direction))
                {
                    return true;
                }
            }
        }
        catch (...)
        {
        }
    }

    if (values.containingPlaneFace == nullptr)
    {
        return false;
    }

    int faceType = 0;
    double point[3] = {0.0, 0.0, 0.0};
    double faceDirection[3] = {0.0, 0.0, 0.0};
    double box[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    double radius = 0.0;
    double radiusData = 0.0;
    int normalDirection = 1;
    if (UF_MODL_ask_face_data(
            values.containingPlaneFace->Tag(),
            &faceType,
            point,
            faceDirection,
            box,
            &radius,
            &radiusData,
            &normalDirection) != 0)
    {
        return false;
    }

    direction = NXOpen::Vector3d(
        -faceDirection[0] * normalDirection,
        -faceDirection[1] * normalDirection,
        -faceDirection[2] * normalDirection);
    return Normalize(direction);
}

bool TwoPointBiLanCaoDialog::CreateProfileSketch(DialogValues& values) const
{
    if (values.containingPlaneFace == nullptr ||
        values.profileEdge1Tag == NULL_TAG ||
        values.profileCurveTags.empty())
    {
        return false;
    }

    char sketchName[UF_OBJ_NAME_BUFSIZE] = "2P_BiLanCao_Slot";
    double sketchMatrix[9] = {0.0};
    tag_t sketchObjects[2] = {values.containingPlaneFace->Tag(), values.profileEdge1Tag};
    int reference[2] = {UF_SKET_HORIZONTAL, UF_SKET_ALONG_CURVE};

    tag_t activeSketch = NULL_TAG;
    const int initResult = UF_SKET_initialize_sketch(sketchName, &activeSketch);
    {
        std::ostringstream trace;
        trace << values.profileOperationTrace
              << "\n    sketch initialize rc=" << initResult
              << ", active sketch tag=" << activeSketch;
        values.profileOperationTrace = trace.str();
    }
    if (initResult != 0)
    {
        return false;
    }

    tag_t sketchTag = NULL_TAG;
    const int createResult = UF_SKET_create_sketch(
        sketchName,
        1,
        sketchMatrix,
        sketchObjects,
        reference,
        UF_SKET_WITH_NORMAL,
        &sketchTag);
    {
        std::ostringstream trace;
        trace << values.profileOperationTrace
              << "\n    create sketch rc=" << createResult
              << ", sketch tag=" << sketchTag;
        values.profileOperationTrace = trace.str();
    }
    if (createResult != 0 || sketchTag == NULL_TAG)
    {
        UF_SKET_terminate_sketch();
        return false;
    }

    std::vector<tag_t> curveTags = values.profileCurveTags;
    const int addResult = UF_SKET_add_objects(sketchTag, static_cast<int>(curveTags.size()), curveTags.data());
    {
        std::ostringstream trace;
        trace << values.profileOperationTrace
              << "\n    add sketch objects rc=" << addResult
              << ", object count=" << curveTags.size();
        values.profileOperationTrace = trace.str();
    }
    UF_SKET_terminate_sketch();
    if (addResult != 0)
    {
        return false;
    }

    values.profileSketchTag = sketchTag;
    return true;
}

bool TwoPointBiLanCaoDialog::ExtrudeReliefSlot(DialogValues& values) const
{
    if (values.targetBody == nullptr || values.profileCurveTags.empty())
    {
        return false;
    }

    NXOpen::Vector3d innerNormal;
    if (!GetFaceInnerNormalDirection(values, innerNormal))
    {
        values.profileOperationTrace += "\n    inner normal failed";
        return false;
    }

    if (values.profileExtrudeHeight <= kPointTolerance)
    {
        values.profileExtrudeHeight = values.sheetThickness + values.bendRadius;
    }
    if (values.profileExtrudeHeight <= kPointTolerance)
    {
        return false;
    }

    {
        std::ostringstream directionTrace;
        directionTrace << "(" << innerNormal.X << ", " << innerNormal.Y << ", " << innerNormal.Z << ")";
        values.profileExtrudeDirectionTrace = directionTrace.str();
    }

    uf_list_p_t curveList = NULL;
    const int createListResult = UF_MODL_create_list(&curveList);
    {
        std::ostringstream trace;
        trace << values.profileOperationTrace
              << "\n    create curve list rc=" << createListResult;
        values.profileOperationTrace = trace.str();
    }
    if (createListResult != 0 || curveList == NULL)
    {
        return false;
    }

    for (tag_t curveTag : values.profileCurveTags)
    {
        const int putResult = UF_MODL_put_list_item(curveList, curveTag);
        if (putResult != 0)
        {
            std::ostringstream trace;
            trace << values.profileOperationTrace
                  << "\n    put curve list item rc=" << putResult
                  << ", curve tag=" << curveTag;
            values.profileOperationTrace = trace.str();
            UF_MODL_delete_list(&curveList);
            return false;
        }
    }

    std::string startLimit = "0.0";
    std::string endLimit = FormatLimit(values.profileExtrudeHeight);
    char taperAngle[] = "0.0";
    char* limits[2] = {const_cast<char*>(startLimit.c_str()), const_cast<char*>(endLimit.c_str())};
    double unusedPoint[3] = {0.0, 0.0, 0.0};
    double direction[3] = {innerNormal.X, innerNormal.Y, innerNormal.Z};

    uf_list_p_t featureList = NULL;
    const int extrudeResult = UF_MODL_create_extruded(
        curveList,
        taperAngle,
        limits,
        unusedPoint,
        direction,
        UF_NULLSIGN,
        &featureList);
    {
        std::ostringstream trace;
        trace << values.profileOperationTrace
              << "\n    extrude rc=" << extrudeResult
              << " (" << UfFailureMessage(extrudeResult) << ")"
              << ", feature list=" << (featureList != NULL ? 1 : 0);
        values.profileOperationTrace = trace.str();
    }
    UF_MODL_delete_list(&curveList);
    if (extrudeResult != 0 || featureList == NULL)
    {
        if (featureList != NULL)
        {
            UF_MODL_delete_list(&featureList);
        }
        return false;
    }

    int featureCount = 0;
    tag_t featureTag = NULL_TAG;
    if (UF_MODL_ask_list_count(featureList, &featureCount) == 0 && featureCount > 0)
    {
        UF_MODL_ask_list_item(featureList, 0, &featureTag);
    }
    UF_MODL_delete_list(&featureList);

    if (featureTag == NULL_TAG)
    {
        return false;
    }

    values.profileExtrudeFeatureTag = featureTag;

    tag_t toolBody = NULL_TAG;
    const int askBodyResult = UF_MODL_ask_feat_body(featureTag, &toolBody);
    {
        std::ostringstream trace;
        trace << values.profileOperationTrace
              << "\n    ask tool body rc=" << askBodyResult
              << " (" << UfFailureMessage(askBodyResult) << ")"
              << ", tool body tag=" << toolBody;
        values.profileOperationTrace = trace.str();
    }
    if (askBodyResult != 0 || toolBody == NULL_TAG)
    {
        return false;
    }
    values.profileToolBodyTag = toolBody;
    values.profileToolBodyTags.push_back(toolBody);

    std::vector<tag_t> targetFeatureTagsBefore;
    if (values.targetBody != nullptr)
    {
        uf_list_p_t beforeFeatureList = NULL;
        if (UF_MODL_ask_body_feats(values.targetBody->Tag(), &beforeFeatureList) == 0 && beforeFeatureList != NULL)
        {
            int beforeFeatureCount = 0;
            if (UF_MODL_ask_list_count(beforeFeatureList, &beforeFeatureCount) == 0)
            {
                for (int index = 0; index < beforeFeatureCount; ++index)
                {
                    tag_t beforeFeatureTag = NULL_TAG;
                    if (UF_MODL_ask_list_item(beforeFeatureList, index, &beforeFeatureTag) == 0 &&
                        beforeFeatureTag != NULL_TAG)
                    {
                        targetFeatureTagsBefore.push_back(beforeFeatureTag);
                    }
                }
            }
            UF_MODL_delete_list(&beforeFeatureList);
        }
    }

    uf_list_p_t bodyList = NULL;
    int bodyListResult = UF_MODL_create_list(&bodyList);
    if (bodyListResult == 0 && bodyList != NULL)
    {
        bodyListResult = UF_MODL_put_list_item(bodyList, toolBody);
        if (bodyListResult == 0)
        {
            bodyListResult = UF_MODL_delete_body_parms(bodyList);
        }
        UF_MODL_delete_list(&bodyList);
    }
    {
        std::ostringstream trace;
        trace << values.profileOperationTrace
              << "\n    delete tool body parms rc=" << bodyListResult
              << " (" << UfFailureMessage(bodyListResult) << ")";
        values.profileOperationTrace = trace.str();
    }

    int resultCount = 0;
    tag_t* resultingBodies = NULL;
    const int subtractResult = UF_MODL_subtract_bodies(
        values.targetBody->Tag(),
        toolBody,
        &resultCount,
        &resultingBodies);
    if (resultingBodies != NULL)
    {
        UF_free(resultingBodies);
    }
    {
        std::ostringstream trace;
        trace << values.profileOperationTrace
              << "\n    subtract rc=" << subtractResult
              << " (" << UfFailureMessage(subtractResult) << ")"
              << ", result count=" << resultCount;
        values.profileOperationTrace = trace.str();
    }

    if (subtractResult != 0)
    {
        return false;
    }

    values.profileFeatureTags.clear();
    if (values.targetBody != nullptr)
    {
        uf_list_p_t afterFeatureList = NULL;
        if (UF_MODL_ask_body_feats(values.targetBody->Tag(), &afterFeatureList) == 0 && afterFeatureList != NULL)
        {
            int afterFeatureCount = 0;
            if (UF_MODL_ask_list_count(afterFeatureList, &afterFeatureCount) == 0)
            {
                for (int index = 0; index < afterFeatureCount; ++index)
                {
                    tag_t afterFeatureTag = NULL_TAG;
                    if (UF_MODL_ask_list_item(afterFeatureList, index, &afterFeatureTag) == 0 &&
                        afterFeatureTag != NULL_TAG &&
                        std::find(targetFeatureTagsBefore.begin(), targetFeatureTagsBefore.end(), afterFeatureTag) ==
                            targetFeatureTagsBefore.end())
                    {
                        values.profileFeatureTags.push_back(afterFeatureTag);
                    }
                }
            }
            UF_MODL_delete_list(&afterFeatureList);
        }
    }
    {
        std::ostringstream trace;
        trace << values.profileOperationTrace
              << "\n    new boolean feature count=" << values.profileFeatureTags.size();
        values.profileOperationTrace = trace.str();
    }

    int deleteCurveFailures = 0;
    for (tag_t curveTag : values.profileCurveTags)
    {
        if (curveTag != NULL_TAG && UF_OBJ_delete_object(curveTag) != 0)
        {
            ++deleteCurveFailures;
        }
    }
    {
        std::ostringstream trace;
        trace << values.profileOperationTrace
              << "\n    delete profile curves failures=" << deleteCurveFailures;
        values.profileOperationTrace = trace.str();
    }
    values.profileCurveTags.clear();

    return true;
}

bool TwoPointBiLanCaoDialog::ExtrudeReliefSlotFeature(DialogValues& values) const
{
    if (values.targetBody == nullptr || values.profileCurveTags.empty())
    {
        return false;
    }

    NXOpen::Vector3d innerNormal;
    if (!GetFaceInnerNormalDirection(values, innerNormal))
    {
        values.profileOperationTrace += "\n    live inner normal failed";
        return false;
    }

    if (values.profileExtrudeHeight <= kPointTolerance)
    {
        values.profileExtrudeHeight = values.sheetThickness + values.bendRadius;
    }
    if (values.profileExtrudeHeight <= kPointTolerance)
    {
        return false;
    }

    {
        std::ostringstream directionTrace;
        directionTrace << "(" << innerNormal.X << ", " << innerNormal.Y << ", " << innerNormal.Z << ")";
        values.profileExtrudeDirectionTrace = directionTrace.str();
    }

    uf_list_p_t curveList = NULL;
    const int createListResult = UF_MODL_create_list(&curveList);
    if (createListResult != 0 || curveList == NULL)
    {
        return false;
    }

    for (tag_t curveTag : values.profileCurveTags)
    {
        const int putResult = UF_MODL_put_list_item(curveList, curveTag);
        if (putResult != 0)
        {
            UF_MODL_delete_list(&curveList);
            return false;
        }
    }

    std::string startLimit = "0.0";
    std::string endLimit = FormatLimit(values.profileExtrudeHeight);
    char taperAngle[] = "0.0";
    char* limits[2] = {const_cast<char*>(startLimit.c_str()), const_cast<char*>(endLimit.c_str())};
    double unusedPoint[3] = {0.0, 0.0, 0.0};
    double direction[3] = {innerNormal.X, innerNormal.Y, innerNormal.Z};

    uf_list_p_t featureList = NULL;
    const int extrudeResult = UF_MODL_create_extruded1(
        curveList,
        taperAngle,
        limits,
        unusedPoint,
        direction,
        UF_NEGATIVE,
        values.targetBody->Tag(),
        &featureList);
    {
        std::ostringstream trace;
        trace << values.profileOperationTrace
              << "\n    live extrusion cut rc=" << extrudeResult
              << " (" << UfFailureMessage(extrudeResult) << ")"
              << ", feature list=" << (featureList != NULL ? 1 : 0);
        values.profileOperationTrace = trace.str();
    }
    UF_MODL_delete_list(&curveList);

    if (extrudeResult != 0 || featureList == NULL)
    {
        if (featureList != NULL)
        {
            UF_MODL_delete_list(&featureList);
        }
        return false;
    }

    int featureCount = 0;
    if (UF_MODL_ask_list_count(featureList, &featureCount) == 0)
    {
        for (int index = 0; index < featureCount; ++index)
        {
            tag_t featureTag = NULL_TAG;
            if (UF_MODL_ask_list_item(featureList, index, &featureTag) == 0 && featureTag != NULL_TAG)
            {
                values.profileFeatureTags.push_back(featureTag);
            }
        }
    }
    UF_MODL_delete_list(&featureList);

    if (values.profileFeatureTags.empty())
    {
        return false;
    }

    values.profileExtrudeFeatureTag = values.profileFeatureTags.front();
    return true;
}

void TwoPointBiLanCaoDialog::ClearPreviewProfile()
{
    for (tag_t curveTag : previewProfileCurveTags_)
    {
        if (curveTag != NULL_TAG)
        {
            UF_OBJ_delete_object(curveTag);
        }
    }
    previewProfileCurveTags_.clear();
}

std::string TwoPointBiLanCaoDialog::BuildLiveSlotSignature(const DialogValues& values) const
{
    std::ostringstream stream;
    stream << std::setprecision(15)
           << "startObj=" << (values.startSelectedObject != nullptr ? values.startSelectedObject->Tag() : NULL_TAG)
           << ";endObj=" << (values.endSelectedObject != nullptr ? values.endSelectedObject->Tag() : NULL_TAG)
           << ";body=" << (values.targetBody != nullptr ? values.targetBody->Tag() : NULL_TAG)
           << ";face=" << (values.containingPlaneFace != nullptr ? values.containingPlaneFace->Tag() : NULL_TAG)
           << ";sx=" << values.startPoint.X
           << ";sy=" << values.startPoint.Y
           << ";sz=" << values.startPoint.Z
           << ";ex=" << values.endPoint.X
           << ";ey=" << values.endPoint.Y
           << ";ez=" << values.endPoint.Z
           << ";clearance=" << values.slotClearance
           << ";bendR=" << values.bendRadius
           << ";chamferEdge=" << (values.chamferEdgeMode ? 1 : 0)
           << ";gapOnly=" << (values.gapOnlyMode ? 1 : 0)
           << ";gapOnlyEdge=" << (values.gapOnlyEdgeSelection ? 1 : 0)
           << ";gapOnlyDir=" << (values.gapOnlyHasOffsetDirection ? 1 : 0)
           << ";gapDirX=" << values.gapOnlyOffsetDirection.X
           << ";gapDirY=" << values.gapOnlyOffsetDirection.Y
           << ";gapDirZ=" << values.gapOnlyOffsetDirection.Z
           << ";wrapCornerMode=" << values.wrapCornerMode
           << ";thickness=" << values.sheetThickness;
    return stream.str();
}

void TwoPointBiLanCaoDialog::RevertLiveSlot()
{
    AppendDebugLog("RevertLiveSlot begin hasLiveSlot=" +
                   std::to_string(hasLiveSlot_ ? 1 : 0) +
                   ", featureCount=" + std::to_string(liveSlotFeatureTags_.size()) +
                   ", toolBodyCount=" + std::to_string(liveSlotToolBodyTags_.size()));
    ClearPreviewProfile();

    if (!hasLiveSlot_)
    {
        AppendDebugLog("RevertLiveSlot skipped: no live slot");
        return;
    }

    int deleteFeatureResult = 0;
    if (!liveSlotFeatureTags_.empty())
    {
        uf_list_p_t featureList = NULL;
        deleteFeatureResult = UF_MODL_create_list(&featureList);
        if (deleteFeatureResult == 0 && featureList != NULL)
        {
            for (tag_t featureTag : liveSlotFeatureTags_)
            {
                if (featureTag != NULL_TAG)
                {
                    deleteFeatureResult = UF_MODL_put_list_item(featureList, featureTag);
                    if (deleteFeatureResult != 0)
                    {
                        break;
                    }
                }
            }
            if (deleteFeatureResult == 0)
            {
                deleteFeatureResult = UF_MODL_delete_feature(featureList);
            }
            UF_MODL_delete_list(&featureList);
        }
    }
    AppendDebugLog("RevertLiveSlot delete feature rc=" +
                   std::to_string(deleteFeatureResult) +
                   " (" + UfFailureMessage(deleteFeatureResult) + ")");

    if (liveSlotToolBodyTag_ != NULL_TAG)
    {
        const int deleteBodyResult = UF_OBJ_delete_object(liveSlotToolBodyTag_);
        AppendDebugLog("RevertLiveSlot delete main tool body tag=" +
                       TagText(liveSlotToolBodyTag_) +
                       ", rc=" + std::to_string(deleteBodyResult) +
                       " (" + UfFailureMessage(deleteBodyResult) + ")");
    }
    for (tag_t toolBodyTag : liveSlotToolBodyTags_)
    {
        if (toolBodyTag != NULL_TAG && toolBodyTag != liveSlotToolBodyTag_)
        {
            const int deleteBodyResult = UF_OBJ_delete_object(toolBodyTag);
            AppendDebugLog("RevertLiveSlot delete extra tool body tag=" +
                           TagText(toolBodyTag) +
                           ", rc=" + std::to_string(deleteBodyResult) +
                           " (" + UfFailureMessage(deleteBodyResult) + ")");
        }
    }

    liveSlotUndoMark_ = 0;
    hasLiveSlot_ = false;
    liveSlotSignature_.clear();
    liveSlotFeatureTags_.clear();
    liveSlotCurveTags_.clear();
    liveSlotToolBodyTags_.clear();
    liveSlotToolBodyTag_ = NULL_TAG;
    slotCreatedOnSelection_ = false;
    AppendDebugLog("RevertLiveSlot end");
}

void TwoPointBiLanCaoDialog::CommitLiveSlot()
{
    AppendDebugLog("CommitLiveSlot begin hasLiveSlot=" +
                   std::to_string(hasLiveSlot_ ? 1 : 0) +
                   ", featureCount=" + std::to_string(liveSlotFeatureTags_.size()) +
                   ", toolBodyCount=" + std::to_string(liveSlotToolBodyTags_.size()));
    ClearPreviewProfile();

    liveSlotUndoMark_ = 0;
    hasLiveSlot_ = false;
    liveSlotSignature_.clear();
    liveSlotFeatureTags_.clear();
    liveSlotCurveTags_.clear();
    liveSlotToolBodyTags_.clear();
    liveSlotToolBodyTag_ = NULL_TAG;
    slotCreatedOnSelection_ = false;
    AppendDebugLog("CommitLiveSlot end");
}

void TwoPointBiLanCaoDialog::ClearPointSelections()
{
    AppendDebugLog("ClearPointSelections begin");
    ClearPreviewProfile();

    for (NXOpen::BlockStyler::UIBlock* block : {startPointBlock_, endPointBlock_})
    {
        if (block == nullptr)
        {
            continue;
        }

        NXOpen::BlockStyler::PropertyList* properties = block->GetProperties();
        if (properties == nullptr)
        {
            continue;
        }

        try
        {
            std::vector<NXOpen::TaggedObject*> emptySelection;
            properties->SetTaggedObjectVector("SelectedObjects", emptySelection);
        }
        catch (...)
        {
        }

        delete properties;
    }

    cachedStartSelectedObject_ = nullptr;
    cachedTargetBody_ = nullptr;
    liveSlotUndoMark_ = 0;
    hasLiveSlot_ = false;
    liveSlotRefreshInProgress_ = false;
    liveSlotSignature_.clear();
    liveSlotFeatureTags_.clear();
    liveSlotCurveTags_.clear();
    liveSlotToolBodyTags_.clear();
    liveSlotToolBodyTag_ = NULL_TAG;
    slotCreatedOnSelection_ = false;

    try
    {
        if (NXOpen::BlockStyler::SelectObject* startSelection =
                dynamic_cast<NXOpen::BlockStyler::SelectObject*>(startPointBlock_))
        {
            startSelection->Focus();
        }
    }
    catch (...)
    {
    }
    AppendDebugLog("ClearPointSelections end");
}

void TwoPointBiLanCaoDialog::RefreshLiveSlot()
{
    AppendDebugLog("RefreshLiveSlot begin hasLiveSlot=" +
                   std::to_string(hasLiveSlot_ ? 1 : 0) +
                   ", inProgress=" + std::to_string(liveSlotRefreshInProgress_ ? 1 : 0));
    if (liveSlotRefreshInProgress_)
    {
        AppendDebugLog("RefreshLiveSlot skipped: already in progress");
        return;
    }

    if (hasLiveSlot_)
    {
        AppendDebugLog("RefreshLiveSlot skipped: already has live slot");
        return;
    }

    DialogValues values;
    if (!ReadDialogValues(values))
    {
        AppendDebugLog("RefreshLiveSlot skipped: ReadDialogValues failed");
        return;
    }

    const std::string newSignature = BuildLiveSlotSignature(values);

    liveSlotRefreshInProgress_ = true;

    if (!CreateReliefSlotProfile(values, true))
    {
        AppendDebugLog("RefreshLiveSlot CreateReliefSlotProfile failed");
        for (tag_t curveTag : values.profileCurveTags)
        {
            if (curveTag != NULL_TAG)
            {
                UF_OBJ_delete_object(curveTag);
            }
        }
        ClearPreviewProfile();
        RevertLiveSlot();
        liveSlotRefreshInProgress_ = false;
        return;
    }

    hasLiveSlot_ = true;
    liveSlotSignature_ = newSignature;
    liveSlotFeatureTags_ = values.profileFeatureTags;
    liveSlotCurveTags_ = values.profileCurveTags;
    liveSlotToolBodyTags_ = values.profileToolBodyTags;
    liveSlotToolBodyTag_ = values.profileToolBodyTag;
    liveSlotRefreshInProgress_ = false;
    AppendDebugLog("RefreshLiveSlot success featureCount=" +
                   std::to_string(liveSlotFeatureTags_.size()) +
                   ", toolBodyCount=" + std::to_string(liveSlotToolBodyTags_.size()));
}

NXOpen::Face* TwoPointBiLanCaoDialog::FindPlanarFaceContainingPoints(NXOpen::Body* body,
                                                                     const NXOpen::Point3d& firstPoint,
                                                                     const NXOpen::Point3d& secondPoint) const
{
    if (body == nullptr)
    {
        return nullptr;
    }

    const std::vector<NXOpen::Face*> faces = body->GetFaces();
    NXOpen::Face* fallbackFace = nullptr;
    for (NXOpen::Face* face : faces)
    {
        if (face == nullptr)
        {
            continue;
        }

        try
        {
            if (face->SolidFaceType() != NXOpen::Face::FaceTypePlanar)
            {
                continue;
            }

            NXOpen::Point3d origin;
            NXOpen::Vector3d normal;
            if (!GetPlanarFacePlane(face, origin, normal))
            {
                continue;
            }

            const double firstDistance = std::fabs(Dot(Subtract(firstPoint, origin), normal));
            const double secondDistance = std::fabs(Dot(Subtract(secondPoint, origin), normal));
            if (firstDistance > kPlaneTolerance || secondDistance > kPlaneTolerance)
            {
                continue;
            }

            if (fallbackFace == nullptr)
            {
                fallbackFace = face;
            }

            int endpointEdgeCount = 0;
            const std::vector<NXOpen::Edge*> faceEdges = face->GetEdges();
            for (NXOpen::Edge* edge : faceEdges)
            {
                if (edge == nullptr)
                {
                    continue;
                }

                NXOpen::Point3d vertex1;
                NXOpen::Point3d vertex2;
                edge->GetVertices(&vertex1, &vertex2);
                if (std::min(PointDistance(firstPoint, vertex1), PointDistance(firstPoint, vertex2)) <=
                    kLooseEndpointTolerance)
                {
                    ++endpointEdgeCount;
                }
            }

            if (endpointEdgeCount >= 2)
            {
                return face;
            }
        }
        catch (...)
        {
        }
    }

    return fallbackFace;
}

NXOpen::Face* TwoPointBiLanCaoDialog::FindPlanarFaceForSelectedEdgeGap(
    NXOpen::Edge* edge,
    const NXOpen::Point3d& referencePoint) const
{
    if (edge == nullptr)
    {
        return nullptr;
    }

    NXOpen::Face* bestFace = nullptr;
    double bestDistance = std::numeric_limits<double>::max();
    const std::vector<NXOpen::Face*> faces = edge->GetFaces();
    for (NXOpen::Face* face : faces)
    {
        if (face == nullptr)
        {
            continue;
        }

        try
        {
            if (face->SolidFaceType() != NXOpen::Face::FaceTypePlanar)
            {
                continue;
            }

            NXOpen::Point3d origin;
            NXOpen::Vector3d normal;
            if (!GetPlanarFacePlane(face, origin, normal))
            {
                continue;
            }

            const double distance = std::fabs(Dot(Subtract(referencePoint, origin), normal));
            if (distance < bestDistance)
            {
                bestDistance = distance;
                bestFace = face;
            }
            if (distance <= kPlaneTolerance)
            {
                return face;
            }
        }
        catch (...)
        {
        }
    }

    return bestFace;
}

void TwoPointBiLanCaoDialog::ConfigurePointSelection(NXOpen::BlockStyler::UIBlock* block, bool allowEdges) const
{
    if (block == nullptr)
    {
        return;
    }

    NXOpen::BlockStyler::PropertyList* properties = block->GetProperties();
    if (properties == nullptr)
    {
        return;
    }

    try
    {
        std::vector<NXOpen::Selection::MaskTriple> selectionMaskArray;
        selectionMaskArray.emplace_back(UF_point_type, UF_point_subtype, 0);
        if (allowEdges)
        {
            selectionMaskArray.emplace_back(UF_solid_type, UF_solid_body_subtype, UF_UI_SEL_FEATURE_ANY_EDGE);
        }
        properties->SetSelectionFilter(
            "SelectionFilter",
            NXOpen::Selection::SelectionActionClearAndEnableSpecific,
            selectionMaskArray);
    }
    catch (...)
    {
    }

    delete properties;
}

void TwoPointBiLanCaoDialog::SetBlockLogicalProperty(NXOpen::BlockStyler::UIBlock* block,
                                                     const char* propertyName,
                                                     bool value) const
{
    if (block == nullptr || propertyName == nullptr)
    {
        return;
    }

    NXOpen::BlockStyler::PropertyList* properties = block->GetProperties();
    if (properties == nullptr)
    {
        return;
    }

    try
    {
        properties->SetLogical(propertyName, value);
    }
    catch (...)
    {
        try
        {
            properties->SetString(propertyName, value ? "True" : "False");
        }
        catch (...)
        {
        }
    }

    delete properties;
}

void TwoPointBiLanCaoDialog::ConfigurePointSelectionsForCurrentMode() const
{
    const bool allowEdges = ReadLogical(gapOnlyToggleBlock_, false);
    ConfigurePointSelection(startPointBlock_, allowEdges);
    ConfigurePointSelection(endPointBlock_, allowEdges);
    SetBlockLogicalProperty(normalDirectionBlock_, "Show", allowEdges);
    SetBlockLogicalProperty(normalDirectionBlock_, "Enable", allowEdges);
}

void TwoPointBiLanCaoDialog::CacheStartPointOwner()
{
    cachedStartSelectedObject_ = GetFirstSelectedObject(startPointBlock_);
    cachedTargetBody_ = GetOwnerBody(cachedStartSelectedObject_);
    if (cachedTargetBody_ == nullptr)
    {
        cachedTargetBody_ = FindBodyBySmartPointParents(cachedStartSelectedObject_, nullptr, nullptr, nullptr);
    }
}

double TwoPointBiLanCaoDialog::ReadDouble(NXOpen::BlockStyler::UIBlock* block, double fallback) const
{
    if (block == nullptr)
    {
        return fallback;
    }

    NXOpen::BlockStyler::PropertyList* properties = block->GetProperties();
    if (properties == nullptr)
    {
        return fallback;
    }

    double value = fallback;
    bool hasValue = false;
    try
    {
        const NXOpen::NXString uncommittedText = properties->GetString("UncommittedValue");
        const std::string text = uncommittedText.GetLocaleText();
        if (!text.empty())
        {
            value = std::stod(text);
            hasValue = true;
        }
    }
    catch (...)
    {
    }

    if (!hasValue)
    {
        try
        {
            value = properties->GetDouble("Value");
            hasValue = true;
        }
        catch (...)
        {
            try
            {
                const NXOpen::NXString text = properties->GetString("Value");
                value = std::stod(text.GetLocaleText());
                hasValue = true;
            }
            catch (...)
            {
                value = fallback;
            }
        }
    }
    delete properties;
    return value;
}

bool TwoPointBiLanCaoDialog::ReadLogical(NXOpen::BlockStyler::UIBlock* block, bool fallback) const
{
    if (block == nullptr)
    {
        return fallback;
    }

    NXOpen::BlockStyler::PropertyList* properties = block->GetProperties();
    if (properties == nullptr)
    {
        return fallback;
    }

    bool value = fallback;
    try
    {
        value = properties->GetLogical("Value");
    }
    catch (...)
    {
        try
        {
            const NXOpen::NXString text = properties->GetString("Value");
            const std::string localeText = text.GetLocaleText();
            value = localeText == "True" || localeText == "true" || localeText == "1";
        }
        catch (...)
        {
            value = fallback;
        }
    }

    delete properties;
    return value;
}

int TwoPointBiLanCaoDialog::ReadEnumValue(NXOpen::BlockStyler::UIBlock* block, int fallback) const
{
    if (block == nullptr)
    {
        return fallback;
    }

    NXOpen::BlockStyler::PropertyList* properties = block->GetProperties();
    if (properties == nullptr)
    {
        return fallback;
    }

    int value = fallback;
    try
    {
        value = properties->GetEnum("Value");
    }
    catch (...)
    {
        try
        {
            value = properties->GetInteger("Value");
        }
        catch (...)
        {
            value = fallback;
        }
    }

    delete properties;
    return value;
}

void TwoPointBiLanCaoDialog::SetDouble(NXOpen::BlockStyler::UIBlock* block, double value) const
{
    if (block == nullptr)
    {
        return;
    }

    NXOpen::BlockStyler::PropertyList* properties = block->GetProperties();
    if (properties == nullptr)
    {
        return;
    }

    try
    {
        properties->SetDouble("Value", value);
    }
    catch (...)
    {
        std::ostringstream stream;
        stream << value;
        try
        {
            properties->SetString("Value", stream.str().c_str());
        }
        catch (...)
        {
        }
    }
    delete properties;
}

void TwoPointBiLanCaoDialog::SetLogical(NXOpen::BlockStyler::UIBlock* block, bool value) const
{
    if (block == nullptr)
    {
        return;
    }

    NXOpen::BlockStyler::PropertyList* properties = block->GetProperties();
    if (properties == nullptr)
    {
        return;
    }

    try
    {
        properties->SetLogical("Value", value);
    }
    catch (...)
    {
        try
        {
            properties->SetString("Value", value ? "True" : "False");
        }
        catch (...)
        {
        }
    }
    delete properties;
}

bool TwoPointBiLanCaoDialog::ReadDialogValues(DialogValues& values) const
{
    values.startSelectedObject = nullptr;
    values.endSelectedObject = nullptr;
    values.startMatchedEdge = nullptr;
    values.containingPlaneFace = nullptr;
    values.largestPlanarFace = nullptr;
    values.oppositePlanarFace = nullptr;
    values.startParentTrace.clear();
    values.profilePointTrace.clear();
    values.profileOperationTrace.clear();
    values.profileCurveTags.clear();
    values.profileFeatureTags.clear();
    values.largestPlanarFaceArea = 0.0;
    values.oppositePlanarFaceArea = 0.0;
    values.sheetThickness = 0.0;
    values.profileEdge1Tag = NULL_TAG;
    values.profileEdge2Tag = NULL_TAG;
    values.profileSketchTag = NULL_TAG;
    values.profileExtrudeFeatureTag = NULL_TAG;
    values.profileToolBodyTag = NULL_TAG;
    values.profileEdgeAngleDeg = 0.0;
    values.profileABLength = 0.0;
    values.profileGALength = 0.0;
    values.profileExtrudeHeight = 0.0;
    values.profileExtrudeDirectionTrace.clear();
    values.gapOnlyMode = ReadLogical(gapOnlyToggleBlock_, false);
    values.gapOnlyEdgeSelection = false;
    values.gapOnlyHasOffsetDirection = false;
    values.gapOnlyOffsetDirection = NXOpen::Vector3d(0.0, 0.0, 0.0);
    values.chamferEdgeMode = ReadLogical(chamferEdgeToggleBlock_, true);
    if (values.gapOnlyMode)
    {
        values.chamferEdgeMode = false;
    }
    values.wrapCornerMode = ReadEnumValue(wrapCornerModeBlock_, 0);

    if (!ReadSelectedPoint(startPointBlock_, values.startSelectedObject, values.startPoint) ||
        !ReadSelectedPoint(endPointBlock_, values.endSelectedObject, values.endPoint))
    {
        return false;
    }

    values.slotClearance = ReadDouble(slotClearanceBlock_, 0.5);
    values.bendRadius = ReadDouble(bendRadiusBlock_, 1.0);
    values.startMatchedEdge = dynamic_cast<NXOpen::Edge*>(values.startSelectedObject);
    const NXOpen::Point3d originalStartPoint = values.startPoint;
    const NXOpen::Point3d originalEndPoint = values.endPoint;
    NXOpen::Edge* gapStartSelectedEdge = nullptr;
    NXOpen::Edge* gapEndSelectedEdge = nullptr;

    if (values.gapOnlyMode)
    {
        gapStartSelectedEdge = dynamic_cast<NXOpen::Edge*>(values.startSelectedObject);
        gapEndSelectedEdge = dynamic_cast<NXOpen::Edge*>(values.endSelectedObject);
        values.gapOnlyEdgeSelection = gapStartSelectedEdge != nullptr || gapEndSelectedEdge != nullptr;
        if (gapStartSelectedEdge != nullptr && gapEndSelectedEdge != nullptr)
        {
            if (!EdgeMidPoint(gapStartSelectedEdge, values.startPoint) ||
                !EdgeMidPoint(gapEndSelectedEdge, values.endPoint))
            {
                return false;
            }
        }
        else if (gapStartSelectedEdge != nullptr)
        {
            if (!ProjectPointToEdgeChord(gapStartSelectedEdge, values.endPoint, values.startPoint))
            {
                return false;
            }
        }
        else if (gapEndSelectedEdge != nullptr)
        {
            if (!ProjectPointToEdgeChord(gapEndSelectedEdge, values.startPoint, values.endPoint))
            {
                return false;
            }
        }
        values.gapOnlyHasOffsetDirection =
            ReadOptionalVector(normalDirectionBlock_, values.gapOnlyOffsetDirection);
    }

    values.targetBody = GetOwnerBody(values.startSelectedObject);
    NXOpen::Face* startParentFace = nullptr;
    if (values.startSelectedObject == nullptr)
    {
        values.startSelectedObject = cachedStartSelectedObject_;
    }
    if (values.targetBody == nullptr)
    {
        values.targetBody = FindBodyBySmartPointParents(
            values.startSelectedObject,
            &values.startMatchedEdge,
            &startParentFace,
            &values.startParentTrace);
    }
    if (values.targetBody == nullptr)
    {
        values.targetBody = cachedTargetBody_;
    }
    if (values.targetBody == nullptr && values.endSelectedObject != nullptr)
    {
        values.targetBody = GetOwnerBody(values.endSelectedObject);
        if (values.targetBody == nullptr)
        {
            NXOpen::Face* endParentFace = nullptr;
            NXOpen::Edge* endMatchedEdge = nullptr;
            std::string endParentTrace;
            values.targetBody = FindBodyBySmartPointParents(
                values.endSelectedObject,
                &endMatchedEdge,
                &endParentFace,
                &endParentTrace);
        }
    }
    values.containingPlaneFace =
        FindPlanarFaceContainingPoints(values.targetBody, values.startPoint, values.endPoint);
    if (values.gapOnlyMode && values.gapOnlyEdgeSelection)
    {
        NXOpen::Face* edgePlaneFace = nullptr;
        if (gapStartSelectedEdge != nullptr && gapEndSelectedEdge != nullptr)
        {
            edgePlaneFace = FindPlanarFaceForSelectedEdgeGap(gapStartSelectedEdge, values.endPoint);
            if (edgePlaneFace == nullptr)
            {
                edgePlaneFace = FindPlanarFaceForSelectedEdgeGap(gapEndSelectedEdge, values.startPoint);
            }
        }
        else if (gapStartSelectedEdge != nullptr)
        {
            edgePlaneFace = FindPlanarFaceForSelectedEdgeGap(gapStartSelectedEdge, originalEndPoint);
        }
        else if (gapEndSelectedEdge != nullptr)
        {
            edgePlaneFace = FindPlanarFaceForSelectedEdgeGap(gapEndSelectedEdge, originalStartPoint);
        }
        if (edgePlaneFace != nullptr)
        {
            values.containingPlaneFace = edgePlaneFace;
        }
    }
    if (values.gapOnlyMode && values.gapOnlyHasOffsetDirection)
    {
        NXOpen::Point3d directionFaceOrigin;
        NXOpen::Vector3d directionFaceNormal;
        if (values.containingPlaneFace != nullptr &&
            GetPlanarFacePlane(values.containingPlaneFace, directionFaceOrigin, directionFaceNormal))
        {
            values.gapOnlyOffsetDirection =
                ProjectVectorToPlane(values.gapOnlyOffsetDirection, directionFaceNormal);
            NXOpen::Vector3d centerDirection =
                ProjectVectorToPlane(Subtract(values.endPoint, values.startPoint), directionFaceNormal);
            if (Normalize(centerDirection))
            {
                values.gapOnlyOffsetDirection =
                    ProjectVectorToPlane(values.gapOnlyOffsetDirection, centerDirection);
            }
            values.gapOnlyHasOffsetDirection = Normalize(values.gapOnlyOffsetDirection);
        }
        else
        {
            values.gapOnlyHasOffsetDirection = false;
        }
    }
    FindSheetThicknessByLargestParallelFaces(
        values.targetBody,
        &values.largestPlanarFace,
        &values.oppositePlanarFace,
        values.largestPlanarFaceArea,
        values.oppositePlanarFaceArea,
        values.sheetThickness);

    if (PointDistance(values.startPoint, values.endPoint) <= kPointTolerance)
    {
        return false;
    }

    return values.slotClearance > 0.0 && values.bendRadius >= 0.0;
}

void TwoPointBiLanCaoDialog::ShowError(const std::string& message) const
{
    if (ui_ != nullptr && ui_->NXMessageBox() != nullptr)
    {
        ui_->NXMessageBox()->Show("2P_BiLanCao", NXOpen::NXMessageBox::DialogTypeError, message.c_str());
    }
}

int TwoPointBiLanCaoDialog::Execute()
{
    if (hasLiveSlot_ || slotCreatedOnSelection_)
    {
        CommitLiveSlot();
        ClearPointSelections();
        return 0;
    }

    DialogValues values;
    if (!ReadDialogValues(values))
    {
        ShowError("请选择不同的起点和终点；槽间隙大于 0，折弯R不小于 0。");
        return 1;
    }

    const bool profileCreated = CreateReliefSlotProfile(values, true);

    if (values.targetBody == nullptr)
    {
        ShowError("起点坐标已读取，但点控件没有返回可追溯到实体的面、边或体。下一步需要加坐标反查体的兜底逻辑。");
        return 1;
    }
    if (!profileCreated)
    {
        ShowError("Failed to create 2P relief slot.");
        return 1;
    }

    ClearPointSelections();
    return 0;
}
