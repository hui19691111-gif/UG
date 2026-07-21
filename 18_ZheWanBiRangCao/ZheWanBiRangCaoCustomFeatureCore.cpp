#include "ZheWanBiRangCaoCustomFeatureShared.hpp"
#include "embedded_dialog_resources.h"

#include <NXOpen/Body.hxx>
#include <NXOpen/BodyCollection.hxx>
#include <NXOpen/CartesianCoordinateSystem.hxx>
#include <NXOpen/CoordinateSystemCollection.hxx>
#include <NXOpen/CoordinateSystem.hxx>
#include <NXOpen/Features_BaseFeatureCollection.hxx>
#include <NXOpen/Features_BooleanBuilder.hxx>
#include <NXOpen/Features_ConstructionFeatureData.hxx>
#include <NXOpen/Features_CustomDoubleArrayAttribute.hxx>
#include <NXOpen/Features_CustomDoubleAttribute.hxx>
#include <NXOpen/Features_CustomFeature.hxx>
#include <NXOpen/Features_CustomFeatureClass.hxx>
#include <NXOpen/Features_CustomFeatureClassManager.hxx>
#include <NXOpen/Features_CustomFeatureData.hxx>
#include <NXOpen/Features_CustomFeatureInformationEvent.hxx>
#include <NXOpen/Features_CustomFeatureInternalFeaturePreUpdateEvent.hxx>
#include <NXOpen/Features_CustomFeaturePreUpdateEvent.hxx>
#include <NXOpen/Features_CustomTagAttribute.hxx>
#include <NXOpen/Features_Feature.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <NXOpen/Features_MoveObject.hxx>
#include <NXOpen/Features_MoveObjectBuilder.hxx>
#include <NXOpen/GeometricUtilities_ModlMotion.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Point.hxx>
#include <NXOpen/PointCollection.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/SelectNXObjectList.hxx>

#include <uf.h>
#include <uf_curve.h>
#include <uf_modl.h>
#include <uf_modl_udf.h>
#include <uf_mtx.h>
#include <uf_obj.h>
#include <uf_part.h>
#include <uf_trns.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#ifndef DllExport
#define DllExport __declspec(dllexport)
#endif

namespace
{
using namespace NXOpen;

class ExtractedTemplate
{
public:
    ~ExtractedTemplate()
    {
        std::error_code error;
        if (!file_.empty())
        {
            std::filesystem::remove(file_, error);
        }
        error.clear();
        if (!directory_.empty())
        {
            std::filesystem::remove(directory_, error);
        }
    }

    bool Extract(std::string& path)
    {
        HMODULE module = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                   GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               reinterpret_cast<LPCWSTR>(&CoreModuleAnchor),
                               &module))
        {
            return false;
        }
        HRSRC resource = FindResourceW(module,
                                       MAKEINTRESOURCEW(IDR_ZH_UDF_ZHEWANBIRANGCAO_PRT),
                                       RT_RCDATA);
        if (resource == nullptr)
        {
            return false;
        }
        const DWORD size = SizeofResource(module, resource);
        HGLOBAL loaded = LoadResource(module, resource);
        const void* bytes = loaded != nullptr ? LockResource(loaded) : nullptr;
        if (size == 0 || bytes == nullptr)
        {
            return false;
        }

        directory_ = std::filesystem::temp_directory_path() /
                     (L"ZW_BiLanCaoCore_" + std::to_wstring(GetCurrentProcessId()) + L"_" +
                      std::to_wstring(GetTickCount64()));
        std::filesystem::create_directories(directory_);
        file_ = directory_ / L"ZW_BiLanCao.prt";
        std::ofstream output(file_, std::ios::binary | std::ios::trunc);
        output.write(static_cast<const char*>(bytes), size);
        output.close();
        if (!output)
        {
            return false;
        }
        path = file_.string();
        return true;
    }

private:
    static void CoreModuleAnchor() {}
    std::filesystem::path directory_;
    std::filesystem::path file_;
};

tag_t FindUdfDefinition(tag_t partTag)
{
    tag_t object = NULL_TAG;
    while (UF_OBJ_cycle_objs_in_part(partTag, UF_feature_type, &object) == 0 &&
           object != NULL_TAG)
    {
        char* featureType = nullptr;
        UF_MODL_ask_feat_type(object, &featureType);
        const bool matches = featureType != nullptr && std::string(featureType) == "UDF_DEF";
        if (featureType != nullptr)
        {
            UF_free(featureType);
        }
        if (matches)
        {
            return object;
        }
    }
    return NULL_TAG;
}

std::string Number(double value)
{
    std::ostringstream stream;
    stream.precision(15);
    stream << value;
    return stream.str();
}

bool AllocateExpressionValues(UF_MODL_udf_exp_data_t& data, double depth, double width)
{
    int error = 0;
    data.new_exp_values = static_cast<char**>(UF_allocate_memory(
        static_cast<unsigned int>(sizeof(char*) * data.num_exps), &error));
    if (error != 0 || data.new_exp_values == nullptr)
    {
        return false;
    }
    std::memset(data.new_exp_values, 0, sizeof(char*) * data.num_exps);
    for (int index = 0; index < data.num_exps; ++index)
    {
        const std::string text = Number(index == 0 ? depth : width);
        char* value = static_cast<char*>(UF_allocate_memory(
            static_cast<unsigned int>(text.size() + 1), &error));
        if (error != 0 || value == nullptr)
        {
            return false;
        }
        std::memcpy(value, text.c_str(), text.size() + 1);
        data.new_exp_values[index] = value;
    }
    return true;
}

void DeleteIfAlive(tag_t tag)
{
    if (tag == NULL_TAG)
    {
        return;
    }
    if (UF_OBJ_ask_status(tag) == UF_OBJ_ALIVE)
    {
        static_cast<void>(UF_OBJ_delete_object(tag));
    }
}

struct ToolResult
{
    tag_t udf = NULL_TAG;
    tag_t move = NULL_TAG;
    tag_t point = NULL_TAG;
    tag_t body = NULL_TAG;
};

bool CreateUdfTool(tag_t definition,
                   double width,
                   double depth,
                   const double* transformValues,
                   ToolResult& result)
{
    Part* workPart = Session::GetSession()->Parts()->Work();
    if (workPart == nullptr)
    {
        return false;
    }

    std::set<tag_t> bodiesBefore;
    for (Body* body : *workPart->Bodies())
    {
        if (body != nullptr)
        {
            bodiesBefore.insert(body->Tag());
        }
    }

    Point* placementPoint = workPart->Points()->CreatePoint(Point3d(0.0, 0.0, 0.0));
    placementPoint->Blank();
    result.point = placementPoint->Tag();

    UF_MODL_udf_exp_data_t expressionData;
    UF_MODL_udf_ref_data_t referenceData;
    UF_MODL_udf_init_exp_data(&expressionData);
    UF_MODL_udf_init_ref_data(&referenceData);
    int error = UF_MODL_udf_init_insert_data_from_def(definition, &expressionData, &referenceData);
    if (error != 0 || expressionData.num_exps != 2 || referenceData.num_refs != 1 ||
        !AllocateExpressionValues(expressionData, depth, width))
    {
        UF_MODL_udf_free_exp_data(&expressionData);
        UF_MODL_udf_free_ref_data(&referenceData);
        DeleteIfAlive(result.point);
        return false;
    }

    int allocationError = 0;
    referenceData.new_refs = static_cast<tag_t*>(UF_allocate_memory(sizeof(tag_t), &allocationError));
    if (referenceData.reverse_refs_dir == nullptr)
    {
        referenceData.reverse_refs_dir = static_cast<UF_MODL_udf_reverse_dir_t*>(
            UF_allocate_memory(sizeof(UF_MODL_udf_reverse_dir_t), &allocationError));
    }
    if (allocationError != 0 || referenceData.new_refs == nullptr ||
        referenceData.reverse_refs_dir == nullptr)
    {
        UF_MODL_udf_free_exp_data(&expressionData);
        UF_MODL_udf_free_ref_data(&referenceData);
        DeleteIfAlive(result.point);
        return false;
    }
    referenceData.new_refs[0] = result.point;
    referenceData.reverse_refs_dir[0] = UF_MODL_UDF_KEEP_DIR;

    error = UF_MODL_create_instantiated_udf1(definition,
                                              &expressionData,
                                              &referenceData,
                                              &result.udf);
    UF_MODL_udf_free_exp_data(&expressionData);
    UF_MODL_udf_free_ref_data(&referenceData);
    if (error != 0 || result.udf == NULL_TAG)
    {
        DeleteIfAlive(result.point);
        return false;
    }

    for (Body* body : *workPart->Bodies())
    {
        if (body != nullptr && bodiesBefore.find(body->Tag()) == bodiesBefore.end())
        {
            if (result.body != NULL_TAG)
            {
                DeleteIfAlive(result.udf);
                DeleteIfAlive(result.point);
                return false;
            }
            result.body = body->Tag();
        }
    }
    if (result.body == NULL_TAG)
    {
        DeleteIfAlive(result.udf);
        DeleteIfAlive(result.point);
        return false;
    }

    Features::MoveObjectBuilder* moveBuilder = nullptr;
    try
    {
        Body* sourceBody = dynamic_cast<Body*>(NXObjectManager::Get(result.body));
        const Matrix3x3 sourceOrientation(1.0, 0.0, 0.0,
                                          0.0, 1.0, 0.0,
                                          0.0, 0.0, 1.0);
        const Matrix3x3 targetOrientation(
            transformValues[3], transformValues[4], transformValues[5],
            transformValues[6], transformValues[7], transformValues[8],
            transformValues[9], transformValues[10], transformValues[11]);
        CartesianCoordinateSystem* sourceCsys =
            workPart->CoordinateSystems()->CreateCoordinateSystem(
                Point3d(0.0, 0.0, 0.0), sourceOrientation, true);
        CartesianCoordinateSystem* targetCsys =
            workPart->CoordinateSystems()->CreateCoordinateSystem(
                Point3d(transformValues[0], transformValues[1], transformValues[2]),
                targetOrientation,
                true);
        if (sourceBody == nullptr || sourceCsys == nullptr || targetCsys == nullptr)
        {
            throw NXException::Create(1, "Invalid UDF placement inputs.");
        }
        moveBuilder = workPart->BaseFeatures()->CreateMoveObjectBuilder(nullptr);
        moveBuilder->SetMoveObjectResult(
            Features::MoveObjectBuilder::MoveObjectResultOptionsMoveOriginal);
        moveBuilder->SetMoveParents(false);
        moveBuilder->SetAssociative(true);
        moveBuilder->ObjectToMoveObject()->Add(sourceBody);
        moveBuilder->TransformMotion()->SetOption(
            GeometricUtilities::ModlMotion::OptionsCsysToCsys);
        moveBuilder->TransformMotion()->SetFromCsys(sourceCsys);
        moveBuilder->TransformMotion()->SetToCsys(targetCsys);
        NXObject* moveResult = moveBuilder->Commit();
        result.move = moveResult != nullptr ? moveResult->Tag() : NULL_TAG;
        moveBuilder->Destroy();
        moveBuilder = nullptr;
        if (result.move == NULL_TAG || UF_MODL_update() != 0)
        {
            throw NXException::Create(1, "Failed to create the UDF placement feature.");
        }
    }
    catch (...)
    {
        if (moveBuilder != nullptr)
        {
            moveBuilder->Destroy();
        }
        DeleteIfAlive(result.move);
        DeleteIfAlive(result.udf);
        DeleteIfAlive(result.point);
        return false;
    }
    return true;
}

double DoubleValue(Features::CustomFeatureData* data, const char* name)
{
    return data->CustomDoubleAttributeByName(name)->Value();
}

std::string FeatureType(Features::Feature* feature)
{
    if (feature == nullptr)
    {
        return {};
    }
    char* type = nullptr;
    if (UF_MODL_ask_feat_type(feature->Tag(), &type) != 0 || type == nullptr)
    {
        return {};
    }
    const std::string result(type);
    UF_free(type);
    return result;
}

int UpdateInstantiatedUdf(Features::Feature* feature,
                          double width,
                          double depth)
{
    if (feature == nullptr)
    {
        return 1;
    }

    UF_MODL_udf_exp_data_t expressionData;
    UF_MODL_udf_ref_data_t referenceData;
    UF_MODL_udf_init_exp_data(&expressionData);
    UF_MODL_udf_init_ref_data(&referenceData);

    int error = UF_MODL_ask_instantiated_udf(
        feature->Tag(), &expressionData, &referenceData);
    if (error == 0 && expressionData.num_exps != 2)
    {
        error = 1;
    }
    if (error == 0 && !AllocateExpressionValues(expressionData, depth, width))
    {
        error = 1;
    }

    if (error == 0 && referenceData.num_refs > 0)
    {
        int allocationError = 0;
        referenceData.new_refs = static_cast<tag_t*>(UF_allocate_memory(
            static_cast<unsigned int>(sizeof(tag_t) * referenceData.num_refs),
            &allocationError));
        if (allocationError != 0 || referenceData.new_refs == nullptr ||
            referenceData.old_refs == nullptr)
        {
            error = allocationError != 0 ? allocationError : 1;
        }
        else
        {
            std::memcpy(referenceData.new_refs,
                        referenceData.old_refs,
                        sizeof(tag_t) * referenceData.num_refs);
        }
    }

    if (error == 0)
    {
        // This edits only the current instantiated UDF's parameters.  Do not
        // call UF_MODL_update here: NX updates the internal feature after this
        // callback returns.
        error = UF_MODL_edit_instantiated_udf(
            feature->Tag(), false, &expressionData, &referenceData);
    }

    UF_MODL_udf_free_exp_data(&expressionData);
    UF_MODL_udf_free_ref_data(&referenceData);
    return error;
}

bool UpdateMovePlacement(Features::MoveObject* move,
                         const std::vector<double>& transforms)
{
    Part* workPart = Session::GetSession()->Parts()->Work();
    if (move == nullptr || workPart == nullptr || transforms.empty() ||
        transforms.size() % zhihui_zhewan_birangcao::kTransformValueCount != 0)
    {
        return false;
    }

    Features::MoveObjectBuilder* builder = nullptr;
    try
    {
        builder = workPart->BaseFeatures()->CreateMoveObjectBuilder(move);
        CoordinateSystem* targetCsys = builder->TransformMotion()->ToCsys();
        if (targetCsys == nullptr)
        {
            builder->Destroy();
            return false;
        }

        const Point3d currentOrigin = targetCsys->Origin();
        size_t bestOffset = 0;
        double bestDistanceSquared = std::numeric_limits<double>::max();
        for (size_t offset = 0; offset < transforms.size();
             offset += zhihui_zhewan_birangcao::kTransformValueCount)
        {
            const double dx = currentOrigin.X - transforms[offset];
            const double dy = currentOrigin.Y - transforms[offset + 1];
            const double dz = currentOrigin.Z - transforms[offset + 2];
            const double distanceSquared = dx * dx + dy * dy + dz * dz;
            if (distanceSquared < bestDistanceSquared)
            {
                bestDistanceSquared = distanceSquared;
                bestOffset = offset;
            }
        }

        targetCsys->SetOrigin(Point3d(transforms[bestOffset],
                                           transforms[bestOffset + 1],
                                           transforms[bestOffset + 2]));
        targetCsys->SetDirections(
            Vector3d(transforms[bestOffset + 3],
                     transforms[bestOffset + 4],
                     transforms[bestOffset + 5]),
            Vector3d(transforms[bestOffset + 6],
                     transforms[bestOffset + 7],
                     transforms[bestOffset + 8]));
        builder->Commit();
        builder->Destroy();
        return true;
    }
    catch (...)
    {
        if (builder != nullptr)
        {
            builder->Destroy();
        }
        return false;
    }
}

int PreUpdateImpl(Features::CustomFeaturePreUpdateEvent* event)
{
    Features::CustomFeature* customFeature = event->GetCustomFeature();
    Features::CustomFeatureData* data = customFeature != nullptr ? customFeature->FeatureData() : nullptr;
    Part* workPart = Session::GetSession()->Parts()->Work();
    if (customFeature == nullptr || data == nullptr || workPart == nullptr)
    {
        return 1;
    }

    const std::vector<Features::ConstructionFeatureData*> existingConstruction =
        event->GetConstructionFeatures();
    if (!existingConstruction.empty())
    {
        // Siemens CustomFeature contract: construction features are created
        // once. Existing members are retained here and their parameters are
        // edited only by InternalFeaturePreUpdateCallback.
        for (Features::ConstructionFeatureData* constructionData : existingConstruction)
        {
            Features::Feature* constructionFeature =
                constructionData != nullptr ? constructionData->GetFeature() : nullptr;
            if (constructionFeature == nullptr)
            {
                continue;
            }
            const bool isUdf = FeatureType(constructionFeature).find("UDF") !=
                               std::string::npos;
            constructionData->SetShowInGraphicView(!isUdf);
        }
        event->SetConstructionFeatures(existingConstruction);
        return 0;
    }

    Body* targetBody = dynamic_cast<Body*>(
        data->CustomTagAttributeByName(zhihui_zhewan_birangcao::kAttrTargetBody)->Value());
    const double width = DoubleValue(data, zhihui_zhewan_birangcao::kAttrSlotWidth);
    const double depth = DoubleValue(data, zhihui_zhewan_birangcao::kAttrSlotDepth);
    const std::vector<double> transforms =
        data->CustomDoubleArrayAttributeByName(
                zhihui_zhewan_birangcao::kAttrToolTransforms)
            ->GetValues();
    if (targetBody == nullptr || width <= 0.0 || depth <= 0.0 || transforms.empty() ||
        transforms.size() % zhihui_zhewan_birangcao::kTransformValueCount != 0)
    {
        return 1;
    }

    ExtractedTemplate extracted;
    std::string templatePath;
    if (!extracted.Extract(templatePath))
    {
        return 1;
    }
    UF_PART_load_status_t loadStatus;
    std::memset(&loadStatus, 0, sizeof(loadStatus));
    tag_t templatePart = NULL_TAG;
    const int openError = UF_PART_open_quiet(templatePath.c_str(), &templatePart, &loadStatus);
    UF_PART_free_load_status(&loadStatus);
    if (openError != 0 || templatePart == NULL_TAG)
    {
        return openError != 0 ? openError : 1;
    }
    const tag_t definition = FindUdfDefinition(templatePart);
    if (definition == NULL_TAG)
    {
        UF_PART_close(templatePart, 0, 1);
        return 1;
    }

    std::vector<Features::ConstructionFeatureData*> constructionFeatures;
    int createdCuts = 0;
    const size_t toolCount = transforms.size() /
                             zhihui_zhewan_birangcao::kTransformValueCount;
    for (size_t index = 0; index < toolCount; ++index)
    {
        ToolResult tool;
        if (!CreateUdfTool(definition,
                           width,
                           depth,
                           transforms.data() + index *
                               zhihui_zhewan_birangcao::kTransformValueCount,
                           tool))
        {
            continue;
        }

        int interferenceStatus = 0;
        tag_t toolBodyTag = tool.body;
        const int checkError = UF_MODL_check_interference(
            targetBody->Tag(), 1, &toolBodyTag, &interferenceStatus);
        static_cast<void>(UF_MODL_update());
        if (checkError != 0 || interferenceStatus != 1)
        {
            DeleteIfAlive(tool.move);
            DeleteIfAlive(tool.udf);
            DeleteIfAlive(tool.point);
            continue;
        }

        Features::Feature* udfFeature = dynamic_cast<Features::Feature*>(
            NXObjectManager::Get(tool.udf));
        Features::Feature* moveFeature = dynamic_cast<Features::Feature*>(
            NXObjectManager::Get(tool.move));
        Features::BooleanBuilder* booleanBuilder = nullptr;
        try
        {
            Body* toolBody = dynamic_cast<Body*>(NXObjectManager::Get(tool.body));
            booleanBuilder = workPart->Features()->CreateBooleanBuilder(nullptr);
            booleanBuilder->SetOperation(Features::Feature::BooleanTypeSubtract);
            booleanBuilder->SetTarget(targetBody);
#pragma warning(push)
#pragma warning(disable : 4996)
            booleanBuilder->SetTool(toolBody);
#pragma warning(pop)
            booleanBuilder->SetRetainTarget(false);
            booleanBuilder->SetRetainTool(false);
            Features::Feature* booleanFeature = booleanBuilder->CommitFeature();
            booleanBuilder->Destroy();
            booleanBuilder = nullptr;

            if (udfFeature != nullptr)
            {
                Features::ConstructionFeatureData* item =
                    event->CreateConstructionFeatureData(udfFeature);
                item->SetShowInGraphicView(false);
                constructionFeatures.push_back(item);
            }
            if (moveFeature != nullptr)
            {
                Features::ConstructionFeatureData* item =
                    event->CreateConstructionFeatureData(moveFeature);
                item->SetShowInGraphicView(false);
                constructionFeatures.push_back(item);
            }
            if (booleanFeature != nullptr)
            {
                Features::ConstructionFeatureData* item =
                    event->CreateConstructionFeatureData(booleanFeature);
                item->SetShowInGraphicView(true);
                constructionFeatures.push_back(item);
                ++createdCuts;
            }
        }
        catch (...)
        {
            if (booleanBuilder != nullptr)
            {
                booleanBuilder->Destroy();
            }
            DeleteIfAlive(tool.move);
            DeleteIfAlive(tool.udf);
            DeleteIfAlive(tool.point);
        }
    }
    UF_PART_close(templatePart, 0, 1);

    event->SetConstructionFeatures(constructionFeatures);
    if (createdCuts == 0)
    {
        customFeature->LogDiagnostic(
            670030,
            "No relief tool body intersects the target body.",
            Features::Feature::DiagnosticTypeWarning);
        return 670030;
    }
    return 0;
}

int InternalFeaturePreUpdateCallback(
    Features::CustomFeatureInternalFeaturePreUpdateEvent* event)
{
    if (event == nullptr)
    {
        return 1;
    }
    Features::CustomFeature* customFeature = event->GetCustomFeature();
    Features::Feature* internalFeature = event->GetFeature();
    Features::CustomFeatureData* data =
        customFeature != nullptr ? customFeature->FeatureData() : nullptr;
    if (customFeature == nullptr || internalFeature == nullptr || data == nullptr)
    {
        return 1;
    }

    const std::string type = FeatureType(internalFeature);
    if (type.find("UDF") != std::string::npos)
    {
        const double width =
            DoubleValue(data, zhihui_zhewan_birangcao::kAttrSlotWidth);
        const double depth =
            DoubleValue(data, zhihui_zhewan_birangcao::kAttrSlotDepth);
        const int udfEditError =
            UpdateInstantiatedUdf(internalFeature, width, depth);
        if (udfEditError != 0)
        {
            customFeature->LogDiagnostic(
                udfEditError,
                "The relief UDF instance parameters could not be edited.",
                Features::Feature::DiagnosticTypeWarning);
            return udfEditError;
        }
    }
    else if (dynamic_cast<Features::MoveObject*>(internalFeature) != nullptr)
    {
        const std::vector<double> transforms =
            data->CustomDoubleArrayAttributeByName(
                    zhihui_zhewan_birangcao::kAttrToolTransforms)
                ->GetValues();
        if (!UpdateMovePlacement(
                dynamic_cast<Features::MoveObject*>(internalFeature), transforms))
        {
            customFeature->LogDiagnostic(
                12,
                "The relief UDF placement could not be updated.",
                Features::Feature::DiagnosticTypeWarning);
            return 1;
        }
    }
    // Boolean construction features have no editable input here; they update
    // downstream from the modified UDF and Move Object features.
    return 0;
}

int PreUpdateCallback(Features::CustomFeaturePreUpdateEvent* event)
{
    try
    {
        return PreUpdateImpl(event);
    }
    catch (const NXException& ex)
    {
        Features::CustomFeature* feature = event != nullptr ? event->GetCustomFeature() : nullptr;
        if (feature != nullptr)
        {
            feature->LogDiagnostic(3,
                                   ex.Message(),
                                   Features::Feature::DiagnosticTypeWarning);
        }
        return ex.ErrorCode() != 0 ? ex.ErrorCode() : 1;
    }
    catch (...)
    {
        return 1;
    }
}

int InformationCallback(Features::CustomFeatureInformationEvent* event)
{
    event->SetInformation(
        "ZheWanBiRangCao: editable relief-slot feature. Double-click to reopen the original dialog.");
    return 0;
}
}

extern "C" DllExport void ufusr(char* param, int* returnCode, int rlen)
{
    (void)param;
    (void)rlen;
    if (returnCode != nullptr)
    {
        *returnCode = 0;
    }
    try
    {
        NXOpen::Features::CustomFeatureClassManager* manager =
            NXOpen::Session::GetSession()->CustomFeatureClassManager();
        NXOpen::Features::CustomFeatureClass* featureClass =
            manager->GetClassFromName(zhihui_zhewan_birangcao::kFeatureClassName);
        featureClass->AddInternalFeaturePreUpdateHandler(
            NXOpen::make_callback(&InternalFeaturePreUpdateCallback));
        featureClass->AddPreUpdateHandler(NXOpen::make_callback(&PreUpdateCallback));
        featureClass->AddInformationHandler(NXOpen::make_callback(&InformationCallback));
    }
    catch (const NXOpen::NXException& ex)
    {
        if (returnCode != nullptr)
        {
            *returnCode = ex.ErrorCode();
        }
    }
    catch (...)
    {
        if (returnCode != nullptr)
        {
            *returnCode = -1;
        }
    }
}

extern "C" DllExport int ufusr_ask_unload(void)
{
    return NXOpen::Session::LibraryUnloadOptionAtTermination;
}
