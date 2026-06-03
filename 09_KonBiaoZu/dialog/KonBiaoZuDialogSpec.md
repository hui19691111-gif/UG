# KonBiaoZu Dialog Spec

Planned Block Styler dialog file: `KonBiaoZu.dlx`

## Block IDs

- `annotation_type_enum`
  - enumeration list:
    - 孔标注
    - 螺牙标注
    - 焊接螺母标注
    - 压铆螺母标注
    - 压铆螺钉标注
    - 压铆螺柱标注
    - 沉孔标注

- `enable_letter_tag_toggle`
  - logical toggle
  - title: 生成孔边 A/B/C 标识

- `open_excel_button`
  - button title: 打开Excel规则表

- `selected_circle_collector`
  - curve collector
  - single select
  - intent: pick one circular drawing edge / projected circular edge

- `rule_file_label`
  - read-only text
  - displays current workbook path

- `preview_text`
  - optional multiline text
  - displays resolved rule summary

## Callback expectations

- `initialize_cb`
  - bind block IDs
  - preload rule file path
  - default enum to 孔标注

- `update_cb`
  - on `annotation_type_enum` change:
    - refresh preview
  - on `open_excel_button`:
    - launch workbook in Excel
  - on `selected_circle_collector`:
    - extract diameter
    - lookup same-diameter circles later
    - refresh preview

- `apply_cb`
  - validate drafting environment
  - validate one circular edge selected
  - lookup rule
  - group same-diameter circles
  - create quantity annotation
  - optionally create A/B/C edge markers
