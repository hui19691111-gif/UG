#pragma once
#include <uf_defs.h>
#include <vector>

namespace tianyuan {

struct Input {
    double diameterMm = 0;
    double lengthMm = 0;
    double widthMm = 0;
    double heightMm = 0;
    double thicknessMm = 0;
    double seamMm = 0;
    double cornerRadiusMm = 10;
    bool segmentedBends = true;
    int bendSegments = 15;
    double indexMarkMm = 5;
    int split = 0;
};

struct Result {
    tag_t loft = 0;
    tag_t flat = 0;
    tag_t body = 0;
    tag_t flatBody = 0;
    std::vector<tag_t> formedBodies;
    std::vector<tag_t> flatBodies;
};

void Validate(const Input& input);
Result Create(const Input& input);

}
