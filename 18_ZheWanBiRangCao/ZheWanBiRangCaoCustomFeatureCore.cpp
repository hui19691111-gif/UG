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
#include <NXOpen/Features_DatumCsys.hxx>
#include <NXOpen/Features_DatumCsysBuilder.hxx>
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
#include <uf_modl_datum_features.h>
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
        file_ = directory_ / L"ZW_BiLanCao1.prt";
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

bool AllocateExpressionValues(UF_MODL_udf_exp_data_t& data,
                              double depth,
                              double width,
                              double thickness)
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
        double numericValue = depth;
        if (index == 1)
        {
            numericValue = width;
        }
        else if (index == 2)
        {
            numericValue = thickness;
        }
        const std::string text = Number(numericValue);
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
    tag_t datumCsys = NULL_TAG;
    tag_t udf = NULL_TAG;
    tag_t body = NULL_TAG;
};

bool CreateUdfTool(tag_t definition,
                   double width,
                   double depth,
                   double thickness,
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

    Features::DatumCsysBuilder* datumBuilder = nullptr;
    try
    {
        CartesianCoordinateSystem* placementCsys =
            workPart->CoordinateSystems()->CreateCoordinateSystem(
                Point3d(transformValues[0], transformValues[1], transformValues[2]),
                Vector3d(transformValues[3], transformValues[4], transformValues[5]),
                Vector3d(transformValues[6], transformValues[7], transformValues[8]));
        if (placementCsys == nullptr)
        {
            throw NXException::Create(1, "Failed to create the UDF placement coordinate system.");
        }
        datumBuilder = workPart->Features()->CreateDatumCsysBuilder(nullptr);
        datumBuilder->SetCsys(placementCsys);
        Features::Feature* datumFeature = datumBuilder->CommitFeature();
        datumBuilder->Destroy();
        datumBuilder = nullptr;
        result.datumCsys = datumFeature != nullptr ? datumFeature->Tag() : NULL_TAG;
        if (result.datumCsys == NULL_TAG)
        {
            throw NXException::Create(1, "Datum CSYS returned no feature.");
        }
        static_cast<void>(UF_MODL_set_datum_csys_visibility(result.datumCsys, FALSE));
    }
    catch (...)
    {
        if (datumBuilder != nullptr)
        {
            datumBuilder->Destroy();
        }
        DeleteIfAlive(result.datumCsys);
        return false;
    }

    tag_t smartCsys = NULL_TAG;
    tag_t datumOrigin = NULL_TAG;
    tag_t datumAxes[3] = {NULL_TAG, NULL_TAG, NULL_TAG};
    tag_t datumPlanes[3] = {NULL_TAG, NULL_TAG, NULL_TAG};
    UF_MODL_ask_datum_csys_components(
        result.datumCsys, &smartCsys, &datumOrigin, datumAxes, datumPlanes);
    if (datumOrigin == NULL_TAG || datumAxes[0] == NULL_TAG ||
        datumPlanes[0] == NULL_TAG)
    {
        DeleteIfAlive(result.datumCsys);
        return false;
    }

    UF_MODL_udf_exp_data_t expressionData;
    UF_MODL_udf_ref_data_t referenceData;
    UF_MODL_udf_init_exp_data(&expressionData);
    UF_MODL_udf_init_ref_data(&referenceData);
    int error = UF_MODL_udf_init_insert_data_from_def(definition, &expressionData, &referenceData);
    if (error != 0 || expressionData.num_exps != 3 || referenceData.num_refs != 3 ||
        !AllocateExpressionValues(expressionData, depth, width, thickness))
    {
        UF_MODL_udf_free_exp_data(&expressionData);
        UF_MODL_udf_free_ref_data(&referenceData);
        DeleteIfAlive(result.datumCsys);
        return false;
    }

    int allocationError = 0;
    referenceData.new_refs = static_cast<tag_t*>(UF_allocate_memory(
        static_cast<unsigned int>(sizeof(tag_t) * referenceData.num_refs),
        &allocationError));
    if (referenceData.reverse_refs_dir == nullptr)
    {
        referenceData.reverse_refs_dir = static_cast<UF_MODL_udf_reverse_dir_t*>(
            UF_allocate_memory(
                static_cast<unsigned int>(
                    sizeof(UF_MODL_udf_reverse_dir_t) * referenceData.num_refs),
                &allocationError));
    }
    if (allocationError != 0 || referenceData.new_refs == nullptr ||
        referenceData.reverse_refs_dir == nullptr)
    {
        UF_MODL_udf_free_exp_data(&expressionData);
        UF_MODL_udf_free_ref_data(&referenceData);
        DeleteIfAlive(result.datumCsys);
        return false;
    }
    referenceData.new_refs[0] = datumPlanes[0];
    referenceData.new_refs[1] = datumAxes[0];
    referenceData.new_refs[2] = datumOrigin;
    for (int index = 0; index < referenceData.num_refs; ++index)
    {
        referenceData.reverse_refs_dir[index] = UF_MODL_UDF_KEEP_DIR;
    }

    error = UF_MODL_create_instantiated_udf1(definition,
                                              &expressionData,
                                              &referenceData,
                                              &result.udf);
    UF_MODL_udf_free_exp_data(&expressionData);
    UF_MODL_udf_free_ref_data(&referenceData);
    if (error != 0 || result.udf == NULL_TAG)
    {
        DeleteIfAlive(result.udf);
        DeleteIfAlive(result.datumCsys);
        return false;
    }

    if (UF_MODL_update() != 0)
    {
        DeleteIfAlive(result.udf);
        DeleteIfAlive(result.datumCsys);
        return false;
    }

    for (Body* body : *workPart->Bodies())
    {
        if (body != nullptr && bodiesBefore.find(body->Tag()) == bodiesBefore.end())
        {
            if (result.body != NULL_TAG)
            {
                DeleteIfAlive(result.udf);
                DeleteIfAlive(result.datumCsys);
                return false;
            }
            result.body = body->Tag();
        }
    }
    if (result.body == NULL_TAG)
    {
        DeleteIfAlive(result.udf);
        DeleteIfAlive(result.datumCsys);
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
                          double depth,
                          double thickness)
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
    if (error == 0 && expressionData.num_exps != 2 &&
        expressionData.num_exps != 3)
    {
        error = 1;
    }
    if (error == 0 &&
        !AllocateExpressionValues(expressionData, depth, width, thickness))
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

bool UpdateDatumCsysPlacement(Features::DatumCsys* datumCsys,
                              const std::vector<double>& transforms)
{
    Part* workPart = Session::GetSession()->Parts()->Work();
    if (datumCsys == nullptr || workPart == nullptr || transforms.empty() ||
        transforms.size() % zhihui_zhewan_birangcao::kTransformValueCount != 0)
    {
        return false;
    }

    Features::DatumCsysBuilder* builder = nullptr;
    try
    {
        builder = workPart->Features()->CreateDatumCsysBuilder(datumCsys);
        CartesianCoordinateSystem* csys = builder->Csys();
        if (csys == nullptr)
        {
            builder->Destroy();
            return false;
        }

        const Point3d currentOrigin = csys->Origin();
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

        csys->SetOrigin(Point3d(transforms[bestOffset],
                                transforms[bestOffset + 1],
                                transforms[bestOffset + 2]));
        csys->SetDirections(
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

bool AskDatumCsysOrigin(Features::Feature* feature, Point3d& origin)
{
    Features::DatumCsys* datumCsys = dynamic_cast<Features::DatumCsys*>(feature);
    Part* workPart = Session::GetSession()->Parts()->Work();
    if (datumCsys == nullptr || workPart == nullptr)
    {
        return false;
    }

    Features::DatumCsysBuilder* builder = nullptr;
    try
    {
        builder = workPart->Features()->CreateDatumCsysBuilder(datumCsys);
        CartesianCoordinateSystem* csys = builder->Csys();
        if (csys == nullptr)
        {
            builder->Destroy();
            return false;
        }
        origin = csys->Origin();
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

bool AskMoveOrigin(Features::Feature* feature, Point3d& origin)
{
    Features::MoveObject* move = dynamic_cast<Features::MoveObject*>(feature);
    Part* workPart = Session::GetSession()->Parts()->Work();
    if (move == nullptr || workPart == nullptr)
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
        origin = targetCsys->Origin();
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

    Body* targetBody = dynamic_cast<Body*>(
        data->CustomTagAttributeByName(zhihui_zhewan_birangcao::kAttrTargetBody)->Value());
    const double width = DoubleValue(data, zhihui_zhewan_birangcao::kAttrSlotWidth);
    const double depth = DoubleValue(data, zhihui_zhewan_birangcao::kAttrSlotDepth);
    const double thickness =
        DoubleValue(data, zhihui_zhewan_birangcao::kAttrThickness);
    const std::vector<double> transforms =
        data->CustomDoubleArrayAttributeByName(
                zhihui_zhewan_birangcao::kAttrToolTransforms)
            ->GetValues();
    if (targetBody == nullptr || width <= 0.0 || depth <= 0.0 ||
        thickness <= 0.0 || transforms.empty() ||
        transforms.size() % zhihui_zhewan_birangcao::kTransformValueCount != 0)
    {
        return 1;
    }

    const size_t desiredToolCount = transforms.size() /
                                    zhihui_zhewan_birangcao::kTransformValueCount;
    const std::vector<Features::ConstructionFeatureData*> existingConstruction =
        event->GetConstructionFeatures();
    std::vector<Features::ConstructionFeatureData*> constructionFeatures;
    std::vector<size_t> transformIndicesToCreate;
    size_t retainedToolCount = 0;

    if (!existingConstruction.empty())
    {
        // New relief tools are stored as one Datum CSYS + one UDF + one
        // separate Boolean construction feature.  Legacy groups stored as
        // UDF + Move Object + Boolean remain editable for compatibility.
        // When changing Multi Cut to Single Cut, retaining the earliest group
        // avoids breaking the target-body history; its placement feature is
        // then repositioned to the sole transform by the internal callback.
        if (existingConstruction.size() % 3 != 0)
        {
            event->SetConstructionFeatures(existingConstruction);
            return 0;
        }

        const size_t existingToolCount = existingConstruction.size() / 3;
        if (existingToolCount >= desiredToolCount)
        {
            const size_t keepFeatureCount = desiredToolCount * 3;
            constructionFeatures.assign(existingConstruction.begin(),
                                        existingConstruction.begin() +
                                            keepFeatureCount);
            for (size_t index = 0; index < constructionFeatures.size(); ++index)
            {
                constructionFeatures[index]->SetShowInGraphicView(index % 3 == 2);
            }
            event->SetConstructionFeatures(constructionFeatures);
            return 0;
        }

        constructionFeatures = existingConstruction;
        retainedToolCount = existingToolCount;
        for (size_t index = 0; index < constructionFeatures.size(); ++index)
        {
            constructionFeatures[index]->SetShowInGraphicView(index % 3 == 2);
        }

        // Match each retained placement feature to its nearest new transform.
        // Only create transforms that are not already represented.
        std::vector<bool> covered(desiredToolCount, false);
        for (size_t group = 0; group < existingToolCount; ++group)
        {
            Point3d currentOrigin(0.0, 0.0, 0.0);
            Features::Feature* placementFeature =
                existingConstruction[group * 3]->GetFeature();
            Features::Feature* legacyMoveFeature =
                existingConstruction[group * 3 + 1]->GetFeature();
            size_t bestIndex = desiredToolCount;
            double bestDistanceSquared = std::numeric_limits<double>::max();
            if (AskDatumCsysOrigin(placementFeature, currentOrigin) ||
                AskMoveOrigin(legacyMoveFeature, currentOrigin))
            {
                for (size_t index = 0; index < desiredToolCount; ++index)
                {
                    if (covered[index])
                    {
                        continue;
                    }
                    const size_t offset =
                        index * zhihui_zhewan_birangcao::kTransformValueCount;
                    const double dx = currentOrigin.X - transforms[offset];
                    const double dy = currentOrigin.Y - transforms[offset + 1];
                    const double dz = currentOrigin.Z - transforms[offset + 2];
                    const double distanceSquared = dx * dx + dy * dy + dz * dz;
                    if (distanceSquared < bestDistanceSquared)
                    {
                        bestDistanceSquared = distanceSquared;
                        bestIndex = index;
                    }
                }
            }
            if (bestIndex == desiredToolCount)
            {
                for (size_t index = 0; index < desiredToolCount; ++index)
                {
                    if (!covered[index])
                    {
                        bestIndex = index;
                        break;
                    }
                }
            }
            if (bestIndex < desiredToolCount)
            {
                covered[bestIndex] = true;
            }
        }
        for (size_t index = 0; index < desiredToolCount; ++index)
        {
            if (!covered[index])
            {
                transformIndicesToCreate.push_back(index);
            }
        }
    }
    else
    {
        transformIndicesToCreate.reserve(desiredToolCount);
        for (size_t index = 0; index < desiredToolCount; ++index)
        {
            transformIndicesToCreate.push_back(index);
        }
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

    int createdCuts = static_cast<int>(retainedToolCount);
    for (size_t index : transformIndicesToCreate)
    {
        ToolResult tool;
        if (!CreateUdfTool(definition,
                           width,
                           depth,
                           thickness,
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
            DeleteIfAlive(tool.udf);
            DeleteIfAlive(tool.datumCsys);
            continue;
        }

        Features::Feature* datumFeature = dynamic_cast<Features::Feature*>(
            NXObjectManager::Get(tool.datumCsys));
        Features::Feature* udfFeature = dynamic_cast<Features::Feature*>(
            NXObjectManager::Get(tool.udf));
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

            if (datumFeature != nullptr)
            {
                Features::ConstructionFeatureData* item =
                    event->CreateConstructionFeatureData(datumFeature);
                item->SetShowInGraphicView(false);
                constructionFeatures.push_back(item);
            }
            if (udfFeature != nullptr)
            {
                Features::ConstructionFeatureData* item =
                    event->CreateConstructionFeatureData(udfFeature);
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
            DeleteIfAlive(tool.udf);
            DeleteIfAlive(tool.datumCsys);
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
        const double thickness =
            DoubleValue(data, zhihui_zhewan_birangcao::kAttrThickness);
        const int udfEditError =
            UpdateInstantiatedUdf(internalFeature, width, depth, thickness);
        if (udfEditError != 0)
        {
            customFeature->LogDiagnostic(
                udfEditError,
                "The relief UDF instance parameters could not be edited.",
                Features::Feature::DiagnosticTypeWarning);
            return udfEditError;
        }
    }
    else if (dynamic_cast<Features::DatumCsys*>(internalFeature) != nullptr)
    {
        const std::vector<double> transforms =
            data->CustomDoubleArrayAttributeByName(
                    zhihui_zhewan_birangcao::kAttrToolTransforms)
                ->GetValues();
        if (!UpdateDatumCsysPlacement(
                dynamic_cast<Features::DatumCsys*>(internalFeature), transforms))
        {
            customFeature->LogDiagnostic(
                12,
                "The relief UDF datum coordinate system could not be updated.",
                Features::Feature::DiagnosticTypeWarning);
            return 1;
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
    // downstream from the modified UDF and its placement feature.
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
