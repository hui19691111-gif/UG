#include "ZiDonCuTuSupport.hpp"

#include <cstdlib>
#include <filesystem>
#include <unordered_map>
#include <utility>

namespace
{
struct TagPairHash
{
	size_t operator()(const std::pair<tag_t, tag_t>& key) const
	{
		return std::hash<tag_t>()(key.first) ^ (std::hash<tag_t>()(key.second) << 1);
	}
};

static std::unordered_map<std::pair<tag_t, tag_t>, double, TagPairHash> g_supportMinDistanceCache;
}

void LogPluginMessage(const std::string& message)
{
	(void)message;
}

void ClearSupportPerformanceCaches()
{
	g_supportMinDistanceCache.clear();
}

void RefreshNxParts()
{
	::theSession = NXOpen::Session::GetSession();
	if (::theSession != NULL)
	{
		::workPart = ::theSession->Parts()->Work();
		::displayPart = ::theSession->Parts()->Display();
	}
}

std::string GetPluginRoot()
{
	const char* configuredRoot = std::getenv("UG_ZH_BANJIN_PLUGIN_DIR");
	if (configuredRoot != NULL && configuredRoot[0] != '\0')
	{
		return configuredRoot;
	}
	std::string defaultRoot = "D:\\UG";
	defaultRoot += "\xD6\xC7"; // Zhi, CP936
	defaultRoot += "\xBB\xD4"; // Hui, CP936
	defaultRoot += "\xEE\xD3"; // Ban, CP936
	defaultRoot += "\xBD\xF0"; // Jin, CP936
	defaultRoot += "\xB2\xE5"; // Cha, CP936
	defaultRoot += "\xBC\xFE"; // Jian, CP936
	return defaultRoot;
}

std::string PluginPath(const char* first, const char* second)
{
	return (std::filesystem::path(GetPluginRoot()) / first / second).string();
}

bool RunUserExitLibrary(const char* dllName)
{
	const std::string libraryPath = PluginPath("application", dllName);
	char symbolName[] = "ufusr";
	UF_load_f_p_t functionPtr = NULL;
	const int loadStatus = UF_load_library(const_cast<char*>(libraryPath.c_str()), symbolName, &functionPtr);
	if (loadStatus != 0 || functionPtr == NULL)
	{
		std::string msg = std::string("Failed to load external user exit: ") + libraryPath;
		ZiDonCuTu::theUI->NXMessageBox()->Show("Block Styler", NXOpen::NXMessageBox::DialogTypeError, msg);
		return false;
	}

	functionPtr();
	UF_unload_library(const_cast<char*>(libraryPath.c_str()));
	return true;
}

double MeasureMinimumDistance(NXOpen::Part* part, NXOpen::NXObject* first, NXOpen::NXObject* second)
{
	if (part == NULL || first == NULL || second == NULL)
	{
		return 0.0;
	}

	tag_t firstTag = first->Tag();
	tag_t secondTag = second->Tag();
	if (firstTag == secondTag)
	{
		return 0.0;
	}
	if (secondTag < firstTag)
	{
		std::swap(firstTag, secondTag);
	}

	const std::pair<tag_t, tag_t> cacheKey(firstTag, secondTag);
	std::unordered_map<std::pair<tag_t, tag_t>, double, TagPairHash>::const_iterator cached = g_supportMinDistanceCache.find(cacheKey);
	if (cached != g_supportMinDistanceCache.end())
	{
		return cached->second;
	}

	NXOpen::MeasureDistance* measureDistance = part->MeasureManager()->NewDistance(NULL, NXOpen::MeasureManager::MeasureTypeMinimum, first, second);
	if (measureDistance == NULL)
	{
		return 0.0;
	}
	const double value = measureDistance->Value();
	g_supportMinDistanceCache[cacheKey] = value;
	return value;
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

	return fabs(firstDrawingPoint[0] - secondDrawingPoint[0]) < tolerance &&
		fabs(firstDrawingPoint[1] - secondDrawingPoint[1]) < tolerance;
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

NXOpen::Face* FindFaceAtDistance(
	NXOpen::Part* part,
	NXOpen::NXObject* reference,
	const std::vector<NXOpen::Face*>& candidates,
	double targetDistance,
	double tolerance)
{
	if (part == NULL || reference == NULL)
	{
		return NULL;
	}

	for (size_t i = 0; i < candidates.size(); i++)
	{
		if (candidates[i] == NULL)
		{
			continue;
		}

		double distance = MeasureMinimumDistance(part, reference, candidates[i]);
		if (fabs(distance - targetDistance) < tolerance)
		{
			return candidates[i];
		}
	}

	return NULL;
}
