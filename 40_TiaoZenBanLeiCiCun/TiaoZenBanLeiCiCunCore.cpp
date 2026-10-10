#include "TiaoZenBanLeiCiCun.hpp"

#include <NXOpen/Body.hxx>
#include <NXOpen/Callback.hxx>
#include <NXOpen/Expression.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/Features_CustomFeature.hxx>
#include <NXOpen/Features_CustomFeatureClass.hxx>
#include <NXOpen/Features_CustomFeatureClassManager.hxx>
#include <NXOpen/Features_CustomFeatureInformationEvent.hxx>
#include <NXOpen/Features_CustomFeatureInternalFeaturePreUpdateEvent.hxx>
#include <NXOpen/Features_CustomFeaturePreUpdateEvent.hxx>
#include <NXOpen/Features_Feature.hxx>
#include <NXOpen/Features_MoveFaceBuilder.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/NXString.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Session.hxx>

#include <uf_obj.h>

#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>

#ifndef DllExport
#define DllExport __declspec(dllexport)
#endif

namespace
{
const char* kSideNames[4] = {"TZBL-L", "TZBL-R", "TZBL-B", "TZBL-T"};

int SideIndex(tag_t featureTag)
{
    char name[256] = {};
    if (featureTag == NULL_TAG ||
        UF_OBJ_ask_name(featureTag, name) != 0)
    {
        return -1;
    }
    for (int index = 0; index < 4; ++index)
    {
        if (std::strcmp(name, kSideNames[index]) == 0)
        {
            return index;
        }
    }
    return -1;
}

int UpdateSideDistance(NXOpen::Features::Feature* feature, double distance)
{
    if (feature == nullptr)
    {
        return 1;
    }
    NXOpen::Part* part = NXOpen::Session::GetSession()->Parts()->Work();
    if (part == nullptr)
    {
        return 1;
    }
    NXOpen::Features::MoveFaceBuilder* builder = nullptr;
    try
    {
        builder = part->Features()->CreateMoveFaceBuilder(feature);
        std::ostringstream formula;
        formula << std::fixed << std::setprecision(9) << distance;
        builder->Distance()->SetFormula(formula.str().c_str());
        builder->Commit();
        builder->Destroy();
        return 0;
    }
    catch (const NXOpen::NXException& exception)
    {
        if (builder != nullptr)
        {
            builder->Destroy();
        }
        TiaoZenWriteLog(
            "ERROR",
            std::string("编辑板件调尺内部移动面失败：") +
                (exception.Message() != nullptr ? exception.Message() : ""));
        return exception.ErrorCode() != 0 ? exception.ErrorCode() : 1;
    }
    catch (...)
    {
        if (builder != nullptr)
        {
            builder->Destroy();
        }
        return 1;
    }
}
}

class PanelResizeFeatureCore
{
public:
    static int PreUpdate(
        NXOpen::Features::CustomFeaturePreUpdateEvent* event);
    static int InternalPreUpdate(
        NXOpen::Features::CustomFeatureInternalFeaturePreUpdateEvent* event);
    static int Information(
        NXOpen::Features::CustomFeatureInformationEvent* event);
    static void Register();
};

int PanelResizeFeatureCore::PreUpdate(
    NXOpen::Features::CustomFeaturePreUpdateEvent* event)
{
    try
    {
        PanelResizeParameters parameters;
        std::string error;
        if (!ReadPanelResizeParameters(
                event->GetCustomFeature(), parameters, error))
        {
            TiaoZenWriteLog("ERROR", error);
            return 1;
        }
        auto* face = dynamic_cast<NXOpen::Face*>(
            NXOpen::NXObjectManager::Get(parameters.faceTag));
        std::array<tag_t, 4> tags{};
        for (NXOpen::Features::ConstructionFeatureData* data :
             event->GetConstructionFeatures())
        {
            NXOpen::Features::Feature* child =
                data != nullptr ? data->GetFeature() : nullptr;
            const int index = child != nullptr ? SideIndex(child->Tag()) : -1;
            if (index >= 0)
            {
                tags[static_cast<std::size_t>(index)] = child->Tag();
            }
        }
        PanelSizeDialog worker(false);
        if (!worker.EnsureSideMoves(
                face, parameters.swapped, parameters.left, parameters.right,
                parameters.bottom, parameters.top, tags, error))
        {
            TiaoZenWriteLog("ERROR", error);
            return 1;
        }
        std::vector<NXOpen::Features::ConstructionFeatureData*> kept;
        for (tag_t tag : tags)
        {
            if (tag == NULL_TAG || UF_OBJ_ask_status(tag) != UF_OBJ_ALIVE)
            {
                continue;
            }
            auto* child = dynamic_cast<NXOpen::Features::Feature*>(
                NXOpen::NXObjectManager::Get(tag));
            if (child == nullptr)
            {
                continue;
            }
            NXOpen::Features::ConstructionFeatureData* data =
                event->CreateConstructionFeatureData(child);
            data->SetShowInGraphicView(true);
            kept.push_back(data);
        }
        event->SetConstructionFeatures(kept);
        if (face != nullptr && face->GetBody() != nullptr)
        {
            UF_OBJ_set_blank_status(
                face->GetBody()->Tag(), UF_OBJ_NOT_BLANKED);
        }
        return 0;
    }
    catch (const NXOpen::NXException& exception)
    {
        TiaoZenWriteLog(
            "ERROR",
            std::string("板件调尺特征更新失败：") +
                (exception.Message() != nullptr ? exception.Message() : ""));
        return 1;
    }
    catch (...)
    {
        TiaoZenWriteLog("ERROR", "板件调尺特征更新时发生未知错误。");
        return 1;
    }
}

int PanelResizeFeatureCore::InternalPreUpdate(
    NXOpen::Features::CustomFeatureInternalFeaturePreUpdateEvent* event)
{
    try
    {
        NXOpen::Features::Feature* child = event->GetFeature();
        const int index = child != nullptr ? SideIndex(child->Tag()) : -1;
        if (index < 0)
        {
            return 0;
        }
        PanelResizeParameters parameters;
        std::string error;
        if (!ReadPanelResizeParameters(
                event->GetCustomFeature(), parameters, error))
        {
            TiaoZenWriteLog("ERROR", error);
            return 1;
        }
        const double distances[4] = {
            parameters.left, parameters.right,
            parameters.bottom, parameters.top};
        return UpdateSideDistance(child, distances[index]);
    }
    catch (const NXOpen::NXException& exception)
    {
        TiaoZenWriteLog(
            "ERROR",
            std::string("板件调尺内部特征更新失败：") +
                (exception.Message() != nullptr ? exception.Message() : ""));
        return 1;
    }
    catch (...)
    {
        return 1;
    }
}

int PanelResizeFeatureCore::Information(
    NXOpen::Features::CustomFeatureInformationEvent* event)
{
    PanelResizeParameters parameters;
    std::string error;
    if (!ReadPanelResizeParameters(
            event->GetCustomFeature(), parameters, error))
    {
        event->SetInformation(NXOpen::NXString(
            "板件调尺", NXOpen::NXString::UTF8));
        return 0;
    }
    std::ostringstream text;
    text << std::fixed << std::setprecision(3)
         << "板件调尺\n"
         << "左 " << parameters.left << " mm，右 " << parameters.right
         << " mm\n下 " << parameters.bottom << " mm，上 "
         << parameters.top << " mm";
    event->SetInformation(NXOpen::NXString(
        text.str(), NXOpen::NXString::UTF8));
    return 0;
}

void PanelResizeFeatureCore::Register()
{
    static bool registered = false;
    if (registered)
    {
        return;
    }
    auto* featureClass =
        NXOpen::Session::GetSession()->CustomFeatureClassManager()->GetClassFromName(
            "NXOpen::CustomFeature::TiaoZenBanLeiCiCun");
    if (featureClass == nullptr)
    {
        TiaoZenWriteLog(
            "ERROR",
            "未找到板件调尺自定义特征类，双击编辑不会生效。");
        return;
    }
    featureClass->AddPreUpdateHandler(
        NXOpen::make_callback(&PanelResizeFeatureCore::PreUpdate));
    featureClass->AddInternalFeaturePreUpdateHandler(
        NXOpen::make_callback(&PanelResizeFeatureCore::InternalPreUpdate));
    featureClass->AddInformationHandler(
        NXOpen::make_callback(&PanelResizeFeatureCore::Information));
    registered = true;
    TiaoZenWriteLog("INFO", "板件调尺自定义特征回调已注册。");
}

extern "C" DllExport void ufusr(char*, int* returnCode, int)
{
    if (returnCode != nullptr)
    {
        *returnCode = 0;
    }
    try
    {
        PanelResizeFeatureCore::Register();
    }
    catch (const NXOpen::NXException& exception)
    {
        TiaoZenWriteLog(
            "ERROR",
            std::string("注册板件调尺自定义特征失败：") +
                (exception.Message() != nullptr ? exception.Message() : ""));
        if (returnCode != nullptr)
        {
            *returnCode = 1;
        }
    }
    catch (...)
    {
        TiaoZenWriteLog("ERROR", "注册板件调尺自定义特征时发生未知错误。");
        if (returnCode != nullptr)
        {
            *returnCode = 1;
        }
    }
}

extern "C" DllExport int ufusr_ask_unload()
{
    return static_cast<int>(
        NXOpen::Session::LibraryUnloadOptionAtTermination);
}
