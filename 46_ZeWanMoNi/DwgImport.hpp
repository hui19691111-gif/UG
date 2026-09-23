#pragma once

#include "BendSimulation.hpp"
#include <filesystem>
#include <string>
#include <vector>

namespace bend_sim {
struct DwgCandidate {
    Tool tool;
    std::wstring block;
};

// Reads a temporary copy through AutoCAD ObjectDBX. Never saves the DWG or
// changes the current NX part. Closed, unambiguous outlines are offered for
// preview; the user chooses the upper tool to persist.
std::vector<DwgCandidate> ReadDwgCandidates(const std::filesystem::path& dwg);
}
