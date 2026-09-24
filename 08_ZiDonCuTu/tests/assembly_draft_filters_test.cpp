#define NOMINMAX
#include "../ZiDonCuTu/AssemblyDraftFilters.hpp"
#include <cassert>
#include <iostream>

using namespace assembly_draft_filters;
int main()
{
    Metadata plate;
    plate.sheetMetal = true;
    plate.partName = L" Panel_A ";
    plate.attributes[L" Material "] = L"SUS304";
    plate.attributes[L"Count"] = L"2";
    Metadata bolt;
    bolt.partName = L"Bolt_M6";
    bolt.hasDrawing = true;
    bolt.hidden = true;

    Rules rules;
    assert(!Remove(plate, rules) && !Remove(bolt, rules));
    rules.enabled[NonSheetMetal] = true;
    assert(!Remove(plate, rules) && Remove(bolt, rules));
    rules.enabled[SheetMetal] = true;
    assert(Remove(plate, rules) && Remove(bolt, rules));
    for (int rule : { WithDrawing, Hidden })
    {
        rules = Rules(); rules.enabled[rule] = true;
        assert(!Remove(plate, rules) && Remove(bolt, rules));
    }
    rules = Rules(); rules.enabled[WithoutDrawing] = true;
    assert(Remove(plate, rules) && !Remove(bolt, rules));
    rules = Rules(); rules.enabled[KeywordMatches] = true;
    rules.text[KeywordMatches] = L"panel";
    assert(Remove(plate, rules) && !Remove(bolt, rules));
    rules.text[KeywordMatches] = L"SUS304";
    assert(!Remove(plate, rules)); // keywords only search the displayed name
    rules = Rules(); rules.enabled[KeywordNonMatches] = true;
    rules.text[KeywordNonMatches] = L" PANEL ";
    assert(!Remove(plate, rules) && Remove(bolt, rules));
    rules = Rules(); rules.enabled[HasAttribute] = true;
    rules.text[HasAttribute] = L"material";
    assert(Remove(plate, rules) && !Remove(bolt, rules));
    rules = Rules(); rules.enabled[MissingAttribute] = true;
    rules.text[MissingAttribute] = L" MATERIAL ";
    assert(!Remove(plate, rules) && Remove(bolt, rules));
    rules = Rules(); rules.enabled[AttributeEquals] = true;
    rules.text[AttributeEquals] = L"material";
    rules.equalsValue = L" sus304 ";
    assert(Remove(plate, rules) && !Remove(bolt, rules));
    rules.equalsValue = L"SUS";
    assert(!Remove(plate, rules)); // exact value, not substring
    rules = Rules(); rules.enabled[WithoutAttributeValue] = true;
    rules.text[WithoutAttributeValue] = L"sus304";
    assert(!Remove(plate, rules) && Remove(bolt, rules));
    rules.enabled[SheetMetal] = true;
    assert(Remove(plate, rules)); // any matching removal rule wins
    for (int i = KeywordMatches; i < RuleCount; ++i)
    {
        rules = Rules(); rules.enabled[i] = true; rules.text[i] = L"  ";
        assert(InvalidRule(rules) == i);
    }
    rules = Rules(); rules.enabled[AttributeEquals] = true;
    rules.text[AttributeEquals] = L"material";
    assert(InvalidRule(rules) == AttributeEquals);
    rules.equalsValue = L"sus304";
    assert(InvalidRule(rules) == -1);
    rules = Rules(); // clear resets selection calculation, no accumulated state
    assert(InvalidRule(rules) == -1 && !Remove(plate, rules) && !Remove(bolt, rules));
    std::cout << "PASS: all 11 filters, OR combinations, validation and reset\n";
}
