# PILianDaoCuZKT

NX C++ command project for inserting flat pattern views of assembly child components into the current drawing sheet.

## Build

- Visual Studio 2022
- Platform: x64
- Configuration: Release
- NX path assumed by the project: `D:\Program Files\Siemens\NX2412`

If NX is installed elsewhere, update `AdditionalIncludeDirectories` and `AdditionalLibraryDirectories` in `src\PILianDaoCuZKT\PILianDaoCuZKT.vcxproj`.

## Current Command Flow

The command currently provides a safe loadable skeleton:

1. Initializes UFUN.
2. Reads the current NX session and work part.
3. Checks whether a drawing sheet is active.
4. Recursively scans the displayed assembly's leaf components.
5. Groups leaf components by prototype part path and counts repeated occurrences.
6. Checks each unique part's modeling views for flat-pattern style names such as `FLAT-PATTERN`, `ZKT`, `展开`, and `展平`.
7. Logs the unique part list, counts, detected flat-pattern view names, skipped suppressed components, and skipped invalid/unloaded components to the Listing Window.

The implementation is intentionally split so the next step can add:

- sheet-metal/flat-pattern detection
- drafting view creation
- automatic drawing-sheet layout

## Deployment

Current local deployment target:

- `D:\UG智辉钣金插件\application\PILianDaoCuZKT.dll`
- `D:\UG智辉钣金插件\application\PILianDaoCuZKT.bmp`
- menu/toolbars updated in `D:\UG智辉钣金插件\startup\UGZH_design.men`, `.tbr`, and `.rtb`

Release packaging adds the shared Zhihui native license guard and package hash verification.
