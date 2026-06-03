#pragma once

#include "AnnotationTypes.hpp"
#include "RuleRecord.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace KonBiaoZu
{
    class ExcelRuleManager
    {
    public:
        ExcelRuleManager();
        explicit ExcelRuleManager(const std::string& configFileName);
        ~ExcelRuleManager();

        [[nodiscard]] const std::string& WorkbookPath() const;
        [[nodiscard]] const std::vector<RuleRecord>& Rules() const;

        bool LoadRules(std::string& errorMessage);
        bool RefreshRules(std::string& errorMessage);
        [[nodiscard]] std::vector<RuleRecord> FindRules(
            AnnotationType type,
            const std::string& subtype,
            double bottomHole,
            int lengthValue,
            double tolerance) const;
        [[nodiscard]] bool HasRules() const;
        bool OpenWorkbook(std::string& errorMessage);
        void CloseEditor();

    private:
        bool SaveRules(const std::vector<RuleRecord>& rules, std::string& errorMessage) const;
        std::string configPath_;
        void* editorWindow_ {};
        std::vector<RuleRecord> rules_;
        std::unordered_map<std::string, std::vector<RuleRecord>> rulesByType_;
    };
}
