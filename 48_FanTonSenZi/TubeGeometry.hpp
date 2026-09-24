#pragma once
#include "TubePlan.hpp"
#include <uf_defs.h>
namespace tube_straighten {
void Check(int);
Source Inspect(const std::vector<tag_t>& edges);
Source InspectFace(tag_t face);
Source InspectRoundFace(tag_t face);
bool IsRoundCap(tag_t face);
// Caller owns the NX undo transaction, including failure rollback.
tag_t Create(const Plan&,std::vector<tag_t>* construction=nullptr);
double Volume(tag_t body);
}
