#pragma once
#include "BendSimulation.hpp"

// Rasterized from the same polygon used for interference checks.
std::filesystem::path ToolThumbnail(const bend_sim::Tool&,const std::filesystem::path& cache,bool large=false);
