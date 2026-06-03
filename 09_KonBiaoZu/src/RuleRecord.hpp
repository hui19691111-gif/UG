#pragma once

#include <string>

namespace KonBiaoZu
{
    struct RuleRecord
    {
        std::string annotationType;
        std::string series;
        std::string size;
        std::string threadSpec;
        std::string lengthText;
        double bottomHole {};
        std::string displayText;
        std::string standardComment;
        std::string note;
    };
}
