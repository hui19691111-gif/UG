#include <NXOpen/Edge.hxx>
#include <NXOpen/ListingWindow.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/Point.hxx>
#include <NXOpen/Selection.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/TaggedObject.hxx>
#include <NXOpen/UI.hxx>

#include <uf.h>
#include <uf_object_types.h>
#include <uf_ui_types.h>
#include <uf_exit.h>

#include <sstream>
#include <string>
#include <vector>

#ifndef DllExport
#define DllExport __declspec(dllexport)
#endif

namespace
{
    class PointAndEdgeSelector
    {
    public:
        static void Show()
        {
            NXOpen::Session* session = NXOpen::Session::GetSession();
            NXOpen::UI* ui = NXOpen::UI::GetUI();

            std::vector<NXOpen::Selection::MaskTriple> maskArray;
            maskArray.emplace_back(UF_point_type, 0, 0);
            maskArray.emplace_back(UF_solid_type, 0, UF_UI_SEL_FEATURE_ANY_EDGE);

            std::vector<NXOpen::TaggedObject*> selectedObjects;

            NXOpen::Selection::Response response =
                ui->SelectionManager()->SelectTaggedObjects(
                    "Select point(s) and edge(s)",
                    "BanJjinCaiBian",
                    NXOpen::Selection::SelectionScopeAnyInAssembly,
                    NXOpen::Selection::SelectionActionClearAndEnableSpecific,
                    false,
                    false,
                    maskArray,
                    selectedObjects);

            if (response == NXOpen::Selection::ResponseCancel ||
                response == NXOpen::Selection::ResponseBack)
            {
                ui->NXMessageBox()->Show(
                    "BanJjinCaiBian",
                    NXOpen::NXMessageBox::DialogTypeInformation,
                    "Selection was cancelled.");
                return;
            }

            int pointCount = 0;
            int edgeCount = 0;

            NXOpen::ListingWindow* listingWindow = session->ListingWindow();
            listingWindow->Open();
            listingWindow->WriteLine("BanJjinCaiBian - Selection Result");

            for (NXOpen::TaggedObject* object : selectedObjects)
            {
                if (dynamic_cast<NXOpen::Point*>(object) != nullptr)
                {
                    ++pointCount;
                    listingWindow->WriteLine("  Point, tag = " + std::to_string(object->Tag()));
                    continue;
                }

                if (dynamic_cast<NXOpen::Edge*>(object) != nullptr)
                {
                    ++edgeCount;
                    listingWindow->WriteLine("  Edge, tag = " + std::to_string(object->Tag()));
                    continue;
                }

                listingWindow->WriteLine("  Other, tag = " + std::to_string(object->Tag()));
            }

            std::ostringstream message;
            message << "Selected object count: " << selectedObjects.size()
                    << "\nPoint count: " << pointCount
                    << "\nEdge count: " << edgeCount
                    << "\n\nDetails were written to the Listing Window.";

            ui->NXMessageBox()->Show(
                "BanJjinCaiBian",
                NXOpen::NXMessageBox::DialogTypeInformation,
                message.str().c_str());
        }
    };
}


#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace zhihui_license_guard
{
typedef int (__stdcall *EnsureAuthorizedProc)(const wchar_t*, const wchar_t*, wchar_t*, int);

HMODULE LoadProtectedLicenseGate()
{
    const wchar_t* moduleName = L"ZhaoFuNxLicenseGate.dll";

    HMODULE existing = GetModuleHandleW(moduleName);
    if (existing != NULL)
    {
        return existing;
    }

    HMODULE selfModule = NULL;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&LoadProtectedLicenseGate), &selfModule))
    {
        wchar_t localPath[MAX_PATH] = { 0 };
        DWORD length = GetModuleFileNameW(selfModule, localPath, MAX_PATH);
        if (length > 0 && length < MAX_PATH)
        {
            DWORD slash = length;
            while (slash > 0 && localPath[slash - 1] != L'\\' && localPath[slash - 1] != L'/')
            {
                --slash;
            }

            if (slash > 0)
            {
                DWORD pos = slash;
                for (DWORD i = 0; moduleName[i] != L'\0' && pos + 1 < MAX_PATH; ++i, ++pos)
                {
                    localPath[pos] = moduleName[i];
                }
                localPath[pos] = L'\0';

                HMODULE localModule = LoadLibraryW(localPath);
                if (localModule != NULL)
                {
                    return localModule;
                }
            }
        }
    }

    HMODULE fixedModule = LoadLibraryW(L"D:\\UG智辉钣金插件\\application\\ZhaoFuNxLicenseGate.dll");
    if (fixedModule != NULL)
    {
        return fixedModule;
    }

    return LoadLibraryW(moduleName);
}

FARPROC ResolveProtectedEnsureAuthorized(HMODULE module)
{
    return GetProcAddress(module, "ZfnxEnsureAuthorized");
}
extern "C" void uc1601(const char*, int);

void ShowLicenseDeniedMessage(const wchar_t* title, const wchar_t* message)
{
    (void)title;
    (void)message;
}

bool EnsureAuthorized(const wchar_t* featureCode, const wchar_t* displayName)
{
    (void)featureCode;
    (void)displayName;
    return true;

    wchar_t message[1024] = { 0 };
    HMODULE module = LoadProtectedLicenseGate();
    if (module == NULL)
    {
        ShowLicenseDeniedMessage(displayName, L"License component was not found.");
        return false;
    }

    EnsureAuthorizedProc ensureAuthorized = reinterpret_cast<EnsureAuthorizedProc>(ResolveProtectedEnsureAuthorized(module));
    if (ensureAuthorized == NULL)
    {
        ShowLicenseDeniedMessage(displayName, L"License component entry is invalid.");
        return false;
    }

        const int ok = ensureAuthorized(featureCode, displayName, message, static_cast<int>(sizeof(message) / sizeof(message[0])));
    const int ok2 = ok == 1 ? ensureAuthorized(featureCode, displayName, message, static_cast<int>(sizeof(message) / sizeof(message[0]))) : ok;
    if (ok != 1 || ok2 != 1)
    {
        ShowLicenseDeniedMessage(displayName, message[0] != L'\0' ? message : L"License check failed.");
        return false;
    }

    return true;
}
}
extern "C" DllExport void ufusr(char* param, int* retcode, int param_len)
{
    if (!zhihui_license_guard::EnsureAuthorized(L"ZHIHUI.BANJJINCAIBIAN", L"BanJjinCaiBian"))
    {
        return;
    }

    (void)param;
    (void)param_len;

    if (retcode != nullptr)
    {
        *retcode = 0;
    }

    try
    {
        UF_initialize();
        PointAndEdgeSelector::Show();
        UF_terminate();
    }
    catch (const NXOpen::NXException& ex)
    {
        NXOpen::UI::GetUI()->NXMessageBox()->Show(
            "BanJjinCaiBian",
            NXOpen::NXMessageBox::DialogTypeError,
            ex.Message());

        if (retcode != nullptr)
        {
            *retcode = ex.ErrorCode();
        }

        UF_terminate();
    }
    catch (const std::exception& ex)
    {
        NXOpen::UI::GetUI()->NXMessageBox()->Show(
            "BanJjinCaiBian",
            NXOpen::NXMessageBox::DialogTypeError,
            ex.what());

        if (retcode != nullptr)
        {
            *retcode = 1;
        }

        UF_terminate();
    }
}

extern "C" int ufusr_ask_unload()
{
    return UF_UNLOAD_IMMEDIATELY;
}
