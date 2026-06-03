#include <NXOpen/BlockStyler_BlockDialog.hxx>
#include <NXOpen/BlockStyler_PropertyList.hxx>
#include <NXOpen/BlockStyler_SelectObject.hxx>
#include <NXOpen/BlockStyler_UIBlock.hxx>
#include <NXOpen/Callback.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/Selection.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/TaggedObject.hxx>
#include <NXOpen/UI.hxx>

#include <uf.h>
#include <uf_exit.h>
#include <uf_object_types.h>
#include <uf_ui_types.h>

#include <exception>
#include <string>
#include <vector>

#include "../../../protection/native/ZhihuiLicenseGuard.hpp"

#ifndef DllExport
#define DllExport __declspec(dllexport)
#endif

namespace
{
class ChaiJiJiaDialog
{
public:
    ChaiJiJiaDialog()
        : ui(NXOpen::UI::GetUI()),
          dialog(NULL),
          faceSelection(NULL)
    {
        dialog = ui->CreateDialog("ChaiJiJia.dlx");
        dialog->AddInitializeHandler(NXOpen::make_callback(this, &ChaiJiJiaDialog::Initialize));
        dialog->AddOkHandler(NXOpen::make_callback(this, &ChaiJiJiaDialog::Ok));
        dialog->AddApplyHandler(NXOpen::make_callback(this, &ChaiJiJiaDialog::Apply));
    }

    ~ChaiJiJiaDialog()
    {
        if (dialog != NULL)
        {
            delete dialog;
            dialog = NULL;
        }
    }

    int Show()
    {
        return dialog->Launch();
    }

private:
    NXOpen::UI* ui;
    NXOpen::BlockStyler::BlockDialog* dialog;
    NXOpen::BlockStyler::SelectObject* faceSelection;

    void Initialize()
    {
        faceSelection = dynamic_cast<NXOpen::BlockStyler::SelectObject*>(
            dialog->TopBlock()->FindBlock("faceSelection"));

        if (faceSelection == NULL)
        {
            return;
        }

        NXOpen::Selection::SelectionAction action = NXOpen::Selection::SelectionActionClearAndEnableSpecific;
        std::vector<NXOpen::Selection::MaskTriple> masks;
        masks.push_back(NXOpen::Selection::MaskTriple(
            UF_solid_type,
            UF_solid_body_subtype,
            UF_UI_SEL_FEATURE_ANY_FACE));

        NXOpen::BlockStyler::PropertyList* properties = faceSelection->GetProperties();
        properties->SetSelectionFilter("SelectionFilter", action, masks);
        delete properties;
    }

    int Ok()
    {
        try
        {
            std::vector<NXOpen::TaggedObject*> selectedFaces;
            if (faceSelection != NULL)
            {
                NXOpen::BlockStyler::PropertyList* properties = faceSelection->GetProperties();
                selectedFaces = properties->GetTaggedObjectVector("SelectedObjects");
                delete properties;
            }

            if (selectedFaces.empty())
            {
                ui->NXMessageBox()->Show(
                    "ChaiJiJia",
                    NXOpen::NXMessageBox::DialogTypeInformation,
                    "Please select one face.");
                return 1;
            }

            return 0;
        }
        catch (const NXOpen::NXException& ex)
        {
            ui->NXMessageBox()->Show("ChaiJiJia", NXOpen::NXMessageBox::DialogTypeError, ex.Message());
            return 1;
        }
        catch (const std::exception& ex)
        {
            ui->NXMessageBox()->Show("ChaiJiJia", NXOpen::NXMessageBox::DialogTypeError, ex.what());
            return 1;
        }
    }

    int Apply()
    {
        return Ok();
    }
};
}

extern "C" DllExport void ufusr(char* param, int* retcode, int param_len)
{
    (void)param;
    (void)param_len;

    if (retcode != NULL)
    {
        *retcode = 0;
    }

    if (!zhihui_license_guard::EnsureAuthorized(L"ZHIHUI.CHAIJIJIA", L"ChaiJiJia"))
    {
        if (retcode != NULL)
        {
            *retcode = 1;
        }
        return;
    }

    try
    {
        UF_initialize();
        ChaiJiJiaDialog commandDialog;
        commandDialog.Show();
        UF_terminate();
    }
    catch (const NXOpen::NXException& ex)
    {
        NXOpen::UI::GetUI()->NXMessageBox()->Show(
            "ChaiJiJia",
            NXOpen::NXMessageBox::DialogTypeError,
            ex.Message());
        if (retcode != NULL)
        {
            *retcode = ex.ErrorCode();
        }
        UF_terminate();
    }
    catch (const std::exception& ex)
    {
        NXOpen::UI::GetUI()->NXMessageBox()->Show(
            "ChaiJiJia",
            NXOpen::NXMessageBox::DialogTypeError,
            ex.what());
        if (retcode != NULL)
        {
            *retcode = 1;
        }
        UF_terminate();
    }
}

extern "C" DllExport int ufusr_ask_unload()
{
    return UF_UNLOAD_IMMEDIATELY;
}
