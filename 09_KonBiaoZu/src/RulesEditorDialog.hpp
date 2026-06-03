#pragma once

#include "RuleRecord.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace KonBiaoZu
{
    bool ShowNxRulesEditor(
        const std::string& configPath,
        std::vector<RuleRecord> rules,
        std::vector<RuleRecord>& liveRules,
        std::unordered_map<std::string, std::vector<RuleRecord>>& liveRulesByType,
        std::string& errorMessage);
}
