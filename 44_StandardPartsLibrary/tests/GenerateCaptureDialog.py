"""Build the native capture resource from this repository's NX block schemas."""
from pathlib import Path
from copy import deepcopy
import xml.etree.ElementTree as ET

feature = Path(__file__).resolve().parents[1]
features = feature.parent
source = ET.parse(feature / 'StandardPartsLibrary.dlx').getroot()

def template(path, class_name):
    return next(i for i in ET.parse(path).iter('item') if i.get('class') == class_name)

string = template(features / '01_Write_Prat_Attr/Write_Prat_Attr.dlx', 'UICOMP_string')
point = template(features / '08_ZiDonCuTu/ZiDonCuTu.dlx', 'UICOMP_point')
number = template(features / '03_ZiDonFenCen/ZiDonFenCen.dlx', 'UICOMP_double')
label = template(features / '02_PiLian_Write_Prat_Attr/PiLian_Write_Prat_Attr.dlx', 'UICOMP_label')
selection = next(i for i in source.iter('item') if i.get('id') == 'selectionTrim')
button = next(i for i in source.iter('item') if i.get('class') == 'UICOMP_button')
orientation = next(i for i in source.iter('item') if i.get('id') == 'manip0')
dialog = ET.Element('Dialog', dict(source.attrib, title='选择体加入标准件库'))

def block(schema, block_id, title, changes=None):
    item = deepcopy(schema)
    item.attrib.update(id=block_id, name=block_id, hierarchy='', presentation=title)
    props = item.find('PropertyList')
    values = {'BlockID': block_id, 'Label': title, 'LabelString': title, 'Show': 'True', 'Enable': 'True',
              'RetainValue': 'False', 'Value': '', 'WideValue': '', 'Bitmap': '', 'Cue': ''}
    values.update(changes or {})
    for prop in list(props):
        if prop.get('type') == 'attachment':
            # Let NX arrange native blocks in document order.
            props.remove(prop)
        elif prop.get('sname') in values:
            prop.set('selected' if prop.get('type') == 'enum' else 'value', values[prop.get('sname')])
    dialog.append(item)

block(label, 'captureRoot', '标准件库目录')
block(string, 'captureName', '标准件名称 *')
block(string, 'captureCategory', '分类')
block(string, 'captureSpec', '规格')
block(selection, 'captureBodies', '选择入库实体', {'SelectMode':'1', 'MaximumScope':'10', 'StepStatus':'1'})
block(point, 'capturePoint', '拾取基准插入点（可选）', {'StepStatus':'1'})
block(orientation, 'captureOrientation', '指定基准方位', {'IsWCSCoordinates':'False', 'IsOriginSpecified':'True'})
for axis in 'XYZ':
    block(number, 'capture'+axis, '基准点 '+axis+'（绝对坐标）', {'Value':'0'})
block(button, 'captureImage', '抓取图片（当前模型视图）')
block(button, 'clearCaptureImage', '清除图片', {'Enable':'False'})
block(label, 'captureImageStatus', '尚未抓取图片；调整模型视角后点击抓取。')
block(label, 'captureStatus', '应用入库后可继续选择。')
auto = deepcopy(source.find('AutoGenData'))
for prop in auto.iter('Property'):
    if prop.get('id') in ('Cancel', 'Close'):
        prop.set('value', 'True')
dialog.append(auto)
props = deepcopy(source.find('PropertyList'))
for prop in props:
    if prop.get('sname') == 'Label': prop.set('value', '选择体加入标准件库')
dialog.append(props)
ET.indent(dialog, space='  ')
ET.ElementTree(dialog).write(feature / 'StandardPartsCapture.dlx', encoding='utf-8', xml_declaration=True)
