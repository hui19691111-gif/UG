#pragma once

#include <string>

namespace KonBiaoZu
{
    enum class AnnotationType
    {
        Hole,
        Thread,
        WeldNut,
        PemNut,
        PemScrew,
        PemStud,
        Counterbore
    };

    inline std::string ToRuleKey(AnnotationType type)
    {
        switch (type)
        {
        case AnnotationType::Hole:
            return "\u5b54\u6807\u6ce8";
        case AnnotationType::Thread:
            return "\u87ba\u7259\u6807\u6ce8";
        case AnnotationType::WeldNut:
            return "\u710a\u63a5\u87ba\u6bcd\u6807\u6ce8";
        case AnnotationType::PemNut:
            return "\u538b\u94c6\u87ba\u6bcd\u6807\u6ce8";
        case AnnotationType::PemScrew:
            return "\u538b\u94c6\u87ba\u9489\u6807\u6ce8";
        case AnnotationType::PemStud:
            return "\u538b\u94c6\u87ba\u6bcd\u67f1\u6807\u6ce8";
        case AnnotationType::Counterbore:
            return "\u6c89\u5b54\u6807\u6ce8";
        }
        return "\u5b54\u6807\u6ce8";
    }
}
