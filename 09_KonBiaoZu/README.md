# KonBiaoZu

NX2412 C++ drafting plugin scaffold for hole-style annotation workflows.

## First-stage scope

- Target platform: Siemens NX 2412
- Language: C++
- Project type: NX Open DLL plugin
- Rule data: built-in defaults plus editable INI config
- Dialog: planned as Block Styler dialog

## Planned dialog controls

- Enumeration: annotation type
- Toggle: enable A/B/C edge marker
- Button: open INI rule config
- Select curve block: pick one circular edge in drafting

## Annotation types

- Hole annotation
- Thread annotation
- Weld nut annotation
- PEM nut annotation
- PEM stud annotation
- PEM standoff annotation
- Counterbore annotation

## Current scaffold behavior

- Launches inside NX with a startup summary
- Creates `config/KonBiaoZuRules.ini` from built-in defaults when missing
- Loads INI rules directly for lookup
- Exposes service classes for future circle grouping and annotation placement
- Opens the INI rule config via the built-in table editor

## Files

- `src/`: plugin source
- `dialog/KonBiaoZuDialogSpec.md`: dialog block IDs and intended behavior

## Next implementation steps

1. Build the real Block Styler `.dlx` dialog from the spec.
2. Add drafting environment validation and circular edge selection.
3. Group equal-diameter circles inside the active drawing view.
4. Add A/B/C marker placement and grouped annotation text placement.
5. Keep the shared rule table in `config/KonBiaoZuRules.ini`.
