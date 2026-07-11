#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <NXOpen/Annotations_AnnotationManager.hxx>
#include <NXOpen/Annotations_DraftingNoteBuilder.hxx>
#include <NXOpen/Annotations_SimpleDraftingAid.hxx>
#include <NXOpen/Annotations_TextWithSymbolsBuilder.hxx>
#include <NXOpen/AttributeIterator.hxx>
#include <NXOpen/BasePart.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXObject.hxx>
#include <NXOpen/NXString.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/PartLoadStatus.hxx>
#include <NXOpen/PartSaveStatus.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/Update.hxx>
#include <uf.h>
#include <uf_obj.h>
#include <uf_object_types.h>
#include <uf_tabnot.h>
#include <uf_text.h>

#include <algorithm>
#include <cctype>
#include <cwctype>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

static const wchar_t* kNxRoot = L"D:\\Program Files\\Siemens\\NX2412";
static const wchar_t* kNxBin = L"D:\\Program Files\\Siemens\\NX2412\\NXBIN";
static const wchar_t* kNxUgii = L"D:\\Program Files\\Siemens\\NX2412\\UGII";
static const wchar_t* kNxUgopen = L"D:\\Program Files\\Siemens\\NX2412\\UGOPEN";
static const wchar_t* kPluginRoot = L"D:\\UG\x667A\x8F89\x94A3\x91D1\x63D2\x4EF6";
static const char* kQuantityTitleUtf8 = "\xE6\x95\xB0\xE9\x87\x8F";

static std::wstring g_pluginRoot;
static std::wstring g_logPath;

static std::string WideToAcp(const std::wstring& text)
{
	if (text.empty())
	{
		return "";
	}
	const int length = WideCharToMultiByte(CP_ACP, 0, text.c_str(), -1, NULL, 0, NULL, NULL);
	if (length <= 0)
	{
		return "";
	}
	std::string result(static_cast<size_t>(length), '\0');
	if (WideCharToMultiByte(CP_ACP, 0, text.c_str(), -1, &result[0], length, NULL, NULL) <= 0)
	{
		return "";
	}
	if (!result.empty() && result[result.size() - 1] == '\0')
	{
		result.erase(result.size() - 1);
	}
	return result;
}

static std::wstring AcpToWide(const char* text)
{
	if (text == NULL || text[0] == '\0')
	{
		return L"";
	}
	const int length = MultiByteToWideChar(CP_ACP, 0, text, -1, NULL, 0);
	if (length <= 0)
	{
		return L"";
	}
	std::wstring result(static_cast<size_t>(length), L'\0');
	if (MultiByteToWideChar(CP_ACP, 0, text, -1, &result[0], length) <= 0)
	{
		return L"";
	}
	if (!result.empty() && result[result.size() - 1] == L'\0')
	{
		result.erase(result.size() - 1);
	}
	return result;
}

static std::wstring ParentDirectory(const std::wstring& path)
{
	const size_t pos = path.find_last_of(L"\\/");
	if (pos == std::wstring::npos)
	{
		return L"";
	}
	return path.substr(0, pos);
}

static std::wstring FileNameOnly(const std::wstring& path)
{
	const size_t pos = path.find_last_of(L"\\/");
	if (pos == std::wstring::npos)
	{
		return path;
	}
	return path.substr(pos + 1);
}

static bool EqualsIgnoreCase(const std::wstring& a, const std::wstring& b)
{
	if (a.size() != b.size())
	{
		return false;
	}
	for (size_t i = 0; i < a.size(); ++i)
	{
		if (towlower(a[i]) != towlower(b[i]))
		{
			return false;
		}
	}
	return true;
}

static std::wstring DetectPluginRoot()
{
	return kPluginRoot;
}

static void PrepareNxEnvironment()
{
	SetDllDirectoryW(kNxBin);
	SetEnvironmentVariableW(L"UGII_BASE_DIR", kNxRoot);
	SetEnvironmentVariableW(L"UGII_ROOT_DIR", kNxUgii);

	DWORD pathLength = GetEnvironmentVariableW(L"PATH", NULL, 0);
	std::wstring oldPath;
	if (pathLength > 0)
	{
		oldPath.resize(pathLength);
		GetEnvironmentVariableW(L"PATH", &oldPath[0], pathLength);
		if (!oldPath.empty() && oldPath[oldPath.size() - 1] == L'\0')
		{
			oldPath.erase(oldPath.size() - 1);
		}
	}
	std::wstring newPath = std::wstring(kNxBin) + L";" + kNxUgii + L";" + kNxUgopen + L";" + oldPath;
	SetEnvironmentVariableW(L"PATH", newPath.c_str());
}

static bool FileExistsA2(const char* path)
{
	if (path == NULL || path[0] == '\0')
	{
		return false;
	}
	const DWORD attributes = GetFileAttributesA(path);
	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static std::string LocaleTextToUtf8(const char* text)
{
	if (text == NULL || text[0] == '\0')
	{
		return "";
	}
	const int wideLength = MultiByteToWideChar(CP_ACP, 0, text, -1, NULL, 0);
	if (wideLength <= 0)
	{
		return text;
	}
	std::wstring wideText(static_cast<size_t>(wideLength), L'\0');
	if (MultiByteToWideChar(CP_ACP, 0, text, -1, &wideText[0], wideLength) <= 0)
	{
		return text;
	}
	const int utf8Length = WideCharToMultiByte(CP_UTF8, 0, wideText.c_str(), -1, NULL, 0, NULL, NULL);
	if (utf8Length <= 0)
	{
		return text;
	}
	std::string utf8Text(static_cast<size_t>(utf8Length), '\0');
	if (WideCharToMultiByte(CP_UTF8, 0, wideText.c_str(), -1, &utf8Text[0], utf8Length, NULL, NULL) <= 0)
	{
		return text;
	}
	if (!utf8Text.empty() && utf8Text[utf8Text.size() - 1] == '\0')
	{
		utf8Text.erase(utf8Text.size() - 1);
	}
	return utf8Text;
}

static void LogLine(const std::string& message)
{
	try
	{
		if (g_logPath.empty())
		{
			g_pluginRoot = DetectPluginRoot();
			g_logPath = g_pluginRoot + L"\\logs\\template_quantity_repair_exe.log";
		}
		CreateDirectoryW((g_pluginRoot + L"\\logs").c_str(), NULL);
		FILE* file = NULL;
		if (_wfopen_s(&file, g_logPath.c_str(), L"ab") == 0 && file != NULL)
		{
			SYSTEMTIME now{};
			GetLocalTime(&now);
			std::ostringstream line;
			line << "["
				<< now.wYear << "-"
				<< now.wMonth << "-"
				<< now.wDay << " "
				<< now.wHour << ":"
				<< now.wMinute << ":"
				<< now.wSecond << "."
				<< now.wMilliseconds
				<< "] " << message << "\r\n";
			const std::string bytes = line.str();
			fwrite(bytes.data(), 1, bytes.size(), file);
			fclose(file);
		}
	}
	catch (...)
	{
	}
	std::cout << message << std::endl;
}

static std::string AskCellText(tag_t cell, bool evaluated)
{
	if (cell == NULL_TAG)
	{
		return "";
	}
	char* text = NULL;
	const int status = evaluated ?
		UF_TABNOT_ask_evaluated_cell_text(cell, &text) :
		UF_TABNOT_ask_cell_text(cell, &text);
	std::string value;
	if (status == 0 && text != NULL)
	{
		value = text;
	}
	if (text != NULL)
	{
		UF_free(text);
	}
	return value;
}

static bool IsQuantityLabel(const std::string& text)
{
	return !text.empty() &&
		text.find(kQuantityTitleUtf8) != std::string::npos &&
		text.find('@') == std::string::npos;
}

static bool IsQuantityReference(const std::string& text)
{
	static const std::string quantityReference = std::string("@") + kQuantityTitleUtf8;
	return !text.empty() && text.find(quantityReference) != std::string::npos;
}

static tag_t ResolveTabularNote(tag_t objectTag)
{
	if (objectTag == NULL_TAG)
	{
		return NULL_TAG;
	}
	int type = 0;
	int subtype = 0;
	if (UF_OBJ_ask_type_and_subtype(objectTag, &type, &subtype) != 0 ||
		type != UF_tabular_note_type)
	{
		return NULL_TAG;
	}
	if (subtype == UF_tabular_note_subtype)
	{
		return objectTag;
	}
	if (subtype == UF_tabular_note_section_subtype)
	{
		tag_t tabnote = NULL_TAG;
		if (UF_TABNOT_ask_tabular_note_of_section(objectTag, &tabnote) == 0)
		{
			return tabnote;
		}
	}
	return NULL_TAG;
}

static tag_t AskCellAt(NXOpen::Part* part, tag_t tabnote, tag_t row, int columnIndex)
{
	(void)part;
	tag_t column = NULL_TAG;
	tag_t cell = NULL_TAG;
	if (tabnote == NULL_TAG || row == NULL_TAG || columnIndex < 0)
	{
		return NULL_TAG;
	}
	if (UF_TABNOT_ask_nth_column(tabnote, columnIndex, &column) != 0 || column == NULL_TAG)
	{
		return NULL_TAG;
	}
	if (UF_TABNOT_ask_cell_at_row_col(row, column, &cell) != 0)
	{
		return NULL_TAG;
	}
	return cell;
}

static std::vector<tag_t> FindQuantityValueCells(NXOpen::Part* part, std::vector<tag_t>& tabnotes)
{
	std::vector<tag_t> cells;
	if (part == NULL)
	{
		return cells;
	}
	std::set<tag_t> processedTabnotes;
	std::set<tag_t> processedCells;
	tag_t objectTag = NULL_TAG;
	while (UF_OBJ_cycle_objs_in_part(part->Tag(), UF_tabular_note_type, &objectTag) == 0 &&
		objectTag != NULL_TAG)
	{
		tag_t tabnote = ResolveTabularNote(objectTag);
		if (tabnote == NULL_TAG || processedTabnotes.find(tabnote) != processedTabnotes.end())
		{
			continue;
		}
		processedTabnotes.insert(tabnote);
		tabnotes.push_back(tabnote);

		int rows = 0;
		int columns = 0;
		if (UF_TABNOT_ask_nm_rows(tabnote, &rows) != 0 ||
			UF_TABNOT_ask_nm_columns(tabnote, &columns) != 0 ||
			rows <= 0 || columns <= 1)
		{
			continue;
		}

		for (int r = 0; r < rows; ++r)
		{
			tag_t row = NULL_TAG;
			if (UF_TABNOT_ask_nth_row(tabnote, r, &row) != 0 || row == NULL_TAG)
			{
				continue;
			}
			for (int c = 0; c < columns; ++c)
			{
				tag_t cell = AskCellAt(part, tabnote, row, c);
				if (cell == NULL_TAG)
				{
					continue;
				}
				const std::string raw = AskCellText(cell, false);
				const std::string evaluated = AskCellText(cell, true);
				if (IsQuantityReference(raw) || IsQuantityReference(evaluated))
				{
					if (processedCells.insert(cell).second)
					{
						cells.push_back(cell);
					}
				}
				if (c < columns - 1 && (IsQuantityLabel(raw) || IsQuantityLabel(evaluated)))
				{
					tag_t valueCell = AskCellAt(part, tabnote, row, c + 1);
					if (valueCell != NULL_TAG && processedCells.insert(valueCell).second)
					{
						cells.push_back(valueCell);
					}
				}
			}
		}
	}
	return cells;
}

static bool HasAnyUserAttribute(NXOpen::NXObject* object, const char* title)
{
	if (object == NULL || title == NULL)
	{
		return false;
	}
	try { if (object->HasUserAttribute(title, NXOpen::NXObject::AttributeTypeInteger, -1)) return true; } catch (...) {}
	try { if (object->HasUserAttribute(title, NXOpen::NXObject::AttributeTypeReal, -1)) return true; } catch (...) {}
	try { if (object->HasUserAttribute(title, NXOpen::NXObject::AttributeTypeString, -1)) return true; } catch (...) {}
	return false;
}

static bool DraftAttributeReferenceText(
	NXOpen::Annotations::TextWithSymbolsBuilder* textBlock,
	NXOpen::NXObject* owner,
	const char* title,
	std::string& referenceText)
{
	referenceText.clear();
	if (textBlock == NULL || owner == NULL || title == NULL || !HasAnyUserAttribute(owner, title))
	{
		return false;
	}
	try
	{
		std::vector<NXOpen::NXString> emptyText(1);
		emptyText[0] = "";
		textBlock->SetText(emptyText);
		textBlock->AddAttributeReference(owner, title, false, 1, 1);
		std::vector<NXOpen::NXString> text = textBlock->GetText();
		textBlock->SetText(emptyText);
		if (text.empty())
		{
			return false;
		}
		const char* utf8 = text[0].GetUTF8Text();
		if (utf8 != NULL && utf8[0] != '\0')
		{
			referenceText = utf8;
		}
		else
		{
			referenceText = LocaleTextToUtf8(text[0].GetLocaleText());
		}
		return !referenceText.empty();
	}
	catch (...)
	{
		return false;
	}
}

static void DeleteQuantityAttributeType(NXOpen::Part* part, NXOpen::NXObject::AttributeType type)
{
	if (part == NULL)
	{
		return;
	}
	try
	{
		NXOpen::AttributeIterator* iterator = part->CreateAttributeIterator();
		if (iterator != NULL)
		{
			iterator->SetIncludeOnlyTitle(kQuantityTitleUtf8);
			iterator->SetIncludeOnlyType(type);
			part->DeleteUserAttributes(iterator, NXOpen::Update::OptionNow);
			delete iterator;
		}
	}
	catch (...)
	{
	}
	for (int guard = 0; guard < 8; ++guard)
	{
		try
		{
			part->DeleteUserAttribute(type, kQuantityTitleUtf8, true, NXOpen::Update::OptionNow);
		}
		catch (...)
		{
			break;
		}
	}
}

static bool RepairTemplate(NXOpen::Session* session, const std::string& templatePath)
{
	if (session == NULL || !FileExistsA2(templatePath.c_str()))
	{
		LogLine("missing template: " + templatePath);
		return false;
	}

	DWORD attributes = GetFileAttributesA(templatePath.c_str());
	if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_READONLY) != 0)
	{
		SetFileAttributesA(templatePath.c_str(), attributes & ~FILE_ATTRIBUTE_READONLY);
		LogLine("clear readonly: " + templatePath);
	}

	NXOpen::PartLoadStatus* loadStatus = NULL;
	NXOpen::BasePart* basePart = NULL;
	bool ok = false;
	try
	{
		LogLine("open: " + templatePath);
		basePart = session->Parts()->OpenBaseDisplay(templatePath.c_str(), &loadStatus);
		NXOpen::Part* part = dynamic_cast<NXOpen::Part*>(basePart);
		if (part == NULL)
		{
			throw std::runtime_error("opened object is not a part");
		}

		bool hasInteger = false;
		bool hasString = false;
		bool hasReal = false;
		try { hasInteger = part->HasUserAttribute(kQuantityTitleUtf8, NXOpen::NXObject::AttributeTypeInteger, -1); } catch (...) {}
		try { hasString = part->HasUserAttribute(kQuantityTitleUtf8, NXOpen::NXObject::AttributeTypeString, -1); } catch (...) {}
		try { hasReal = part->HasUserAttribute(kQuantityTitleUtf8, NXOpen::NXObject::AttributeTypeReal, -1); } catch (...) {}
		{
			std::ostringstream line;
			line << "attribute state integer=" << (hasInteger ? 1 : 0)
				<< " string=" << (hasString ? 1 : 0)
				<< " real=" << (hasReal ? 1 : 0);
			LogLine(line.str());
		}
		if (hasInteger && !hasString && !hasReal)
		{
			LogLine("skip already integer: " + templatePath);
			ok = true;
			goto cleanup;
		}

		std::vector<tag_t> tabnotes;
		std::vector<tag_t> valueCells = FindQuantityValueCells(part, tabnotes);
		{
			std::ostringstream line;
			line << "quantity cells=" << valueCells.size() << " tabnotes=" << tabnotes.size();
			LogLine(line.str());
		}
		if (valueCells.empty())
		{
			throw std::runtime_error("quantity value cell not found");
		}

		for (size_t i = 0; i < valueCells.size(); ++i)
		{
			UF_TABNOT_set_cell_text(valueCells[i], "0");
		}
		for (size_t i = 0; i < tabnotes.size(); ++i)
		{
			UF_TABNOT_update(tabnotes[i]);
		}

		DeleteQuantityAttributeType(part, NXOpen::NXObject::AttributeTypeString);
		DeleteQuantityAttributeType(part, NXOpen::NXObject::AttributeTypeReal);
		part->SetUserAttribute(kQuantityTitleUtf8, -1, 0, NXOpen::Update::OptionNow);

		NXOpen::Annotations::SimpleDraftingAid* nullAid(NULL);
		NXOpen::Annotations::DraftingNoteBuilder* noteBuilder =
			part->Annotations()->CreateDraftingNoteBuilder(nullAid);
		std::string referenceText;
		if (noteBuilder != NULL)
		{
			DraftAttributeReferenceText(noteBuilder->Text()->TextBlock(), part, kQuantityTitleUtf8, referenceText);
			noteBuilder->Destroy();
		}
		LogLine("reference: " + referenceText);
		if (referenceText.empty() || referenceText.find("@") == std::string::npos)
		{
			throw std::runtime_error("failed to create quantity reference text");
		}

		for (size_t i = 0; i < valueCells.size(); ++i)
		{
			UF_TABNOT_set_cell_text(valueCells[i], referenceText.c_str());
		}
		for (size_t i = 0; i < tabnotes.size(); ++i)
		{
			UF_TABNOT_update(tabnotes[i]);
		}

		NXOpen::PartSaveStatus* saveStatus = part->Save(
			NXOpen::BasePart::SaveComponentsFalse,
			NXOpen::BasePart::CloseAfterSaveTrue);
		if (saveStatus != NULL)
		{
			delete saveStatus;
		}
		basePart = NULL;
		LogLine("saved: " + templatePath);
		ok = true;
	}
	catch (const NXOpen::NXException& ex)
	{
		LogLine(std::string("NXException: ") + ex.Message());
	}
	catch (const std::exception& ex)
	{
		LogLine(std::string("Exception: ") + ex.what());
	}
	catch (...)
	{
		LogLine("Unknown exception");
	}

cleanup:
	if (loadStatus != NULL)
	{
		delete loadStatus;
	}
	if (basePart != NULL)
	{
		try
		{
			basePart->Close(
				NXOpen::BasePart::CloseWholeTreeFalse,
				NXOpen::BasePart::CloseModifiedDontCloseModified,
				NULL);
		}
		catch (...)
		{
		}
	}
	return ok;
}

static std::vector<std::string> DefaultTemplatePaths()
{
	std::vector<std::string> paths;
	if (g_pluginRoot.empty())
	{
		g_pluginRoot = DetectPluginRoot();
	}
	const std::wstring dataDir = g_pluginRoot + L"\\DATA\\";
	const std::wstring pattern = dataDir + L"A4-noviews-template*.prt";
	WIN32_FIND_DATAW data{};
	HANDLE find = FindFirstFileW(pattern.c_str(), &data);
	if (find == INVALID_HANDLE_VALUE)
	{
		return paths;
	}
	do
	{
		if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
		{
			paths.push_back(WideToAcp(dataDir + data.cFileName));
		}
	} while (FindNextFileW(find, &data));
	FindClose(find);
	std::sort(paths.begin(), paths.end());
	return paths;
}

int main(int argc, char* argv[])
{
	PrepareNxEnvironment();
	g_pluginRoot = DetectPluginRoot();
	g_logPath = g_pluginRoot + L"\\logs\\template_quantity_repair_exe.log";

	std::vector<std::string> paths;
	for (int i = 1; i < argc; ++i)
	{
		if (argv[i] != NULL && argv[i][0] != '\0')
		{
			paths.push_back(argv[i]);
		}
	}
	if (paths.empty())
	{
		paths = DefaultTemplatePaths();
	}
	if (paths.empty())
	{
		LogLine("no template found");
		return 2;
	}

	const int initStatus = UF_initialize();
	if (initStatus != 0)
	{
		std::ostringstream line;
		line << "UF_initialize failed status=" << initStatus;
		LogLine(line.str());
		return 3;
	}
	UF_TEXT_set_text_mode(UF_TEXT_ALL_UTF8);

	NXOpen::Session* session = NXOpen::Session::GetSession();
	int failed = 0;
	for (size_t i = 0; i < paths.size(); ++i)
	{
		LogLine("==== repair begin ====");
		if (!RepairTemplate(session, paths[i]))
		{
			++failed;
		}
		LogLine("==== repair end ====");
	}

	UF_terminate();
	return failed == 0 ? 0 : 1;
}
