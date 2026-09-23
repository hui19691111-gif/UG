"""Create native DLX from existing repository control definitions and a vector-style icon."""
from pathlib import Path
import copy
import xml.etree.ElementTree as ET
from PIL import Image, ImageDraw

root = Path(__file__).resolve().parents[1]
tree = ET.parse(root.parent/'45_ZeWanFuZu/ZeWanFuZu.dlx')
dialog = tree.getroot()
group = dialog.find('item')
members = group.find('./PropertyList/Property/PropertyList')
templates = {p.get('id'): copy.deepcopy(p) for p in members}
enum_tree = ET.parse(root.parent/'43_BendMarkNotch/BendMarkNotch.dlx')
enum_template = next(p for p in enum_tree.iter('Property') if p.get('type')=='uicomp' and p.get('class')=='UICOMP_enum')
button_tree = ET.parse(root.parent/'44_StandardPartsLibrary/StandardPartsLibrary.dlx')
button_template = next(p for p in button_tree.iter('Property') if p.get('type')=='uicomp' and p.get('class')=='UICOMP_button')
list_tree = ET.parse(root.parent/'23_DuoSiTiZuanZuanPei/DuoSiTiZuanZuanPei.dlx')
list_template = next(p for p in list_tree.iter('Property') if p.get('type')=='uicomp' and p.get('class')=='UICOMP_tree_control')
for parent in list_template.iter():
    for child in list(parent):
        if child.get('type')=='attachment': parent.remove(child)
for p in list_template.iter('Property'):
    sizes={'Height':210,'Width':360,'MinimumHeight':180,'MinimumWidth':340}
    if p.get('sname') in sizes: p.set('value',str(sizes[p.get('sname')]))
    if p.get('sname')=='CanStretchHeight': p.set('value','False')
image_tree = ET.parse(Path(r'D:\Program Files\Siemens\NX2412\UGOPEN\SNAP\Dialog\Drawing Area.dlx'))
image_item = image_tree.getroot().find('item')
image_template = ET.Element('Property',{'class':image_item.get('class'),'hierarchy':'UGS::UICOMP_group','id':image_item.get('id'),'name':image_item.get('id'),'type':'uicomp','presentation':'Drawing Area','mask':'256'})
image_template.append(copy.deepcopy(image_item))
for parent in image_template.iter():
    for child in list(parent):
        if child.get('type')=='attachment': parent.remove(child)
for p in image_template.iter('Property'):
    if p.get('sname')=='Height':p.set('value','128')
    if p.get('sname')=='Width':p.set('value','288')
for parent in button_template.iter():
    for child in list(parent):
        if child.get('type')=='attachment': parent.remove(child)
for p in list(members): members.remove(p)

def block(template, name, label, value=None, minimum=None, maximum=None, options=None):
    p = copy.deepcopy(template)
    old = p.get('id')
    for e in p.iter():
        for key in ('id','name','value'):
            if e.get(key)==old: e.set(key,name)
    for e in p.iter('Property'):
        key=e.get('sname')
        if key=='Bitmap' and template is button_template: e.set('value','')
        if key in ('Label','LabelString','Cue'): e.set('value',label)
        if key=='Value' and value is not None: e.set('value',str(value))
        if key=='MinimumValue' and minimum is not None: e.set('value',str(minimum))
        if key=='MaximumValue' and maximum is not None: e.set('value',str(maximum))
        if key=='SelectMode': e.set('selected','0')
        if key=='Value' and options is not None:
            for child in list(e): e.remove(child)
            e.set('selected','0')
            for i,label in enumerate(options): ET.SubElement(e,'Option',name=label,value=str(i))
    members.append(p)

block(templates['thickness_faces'],'bend_selection','选择内圆柱面或内侧锐边')
block(list_template,'tool_choice','折弯刀具列表')
block(templates['result_status'],'tool_profile_info','截面预览')
block(image_template,'tool_profile_image','刀具截面预览')
block(templates['result_status'],'bend_detail','自动识别内圆柱面或内侧锐边。')
block(templates['reverse_end'],'reverse_tool','反转刀具方向','False')
block(button_template,'check_button','重新检查当前姿态')
block(button_template,'reload_tools','重新加载自定义刀具')
block(button_template,'import_dwg','选择 DWG 并预览轮廓')
block(button_template,'save_dwg','保存当前预览刀具')
block(button_template,'tool_folder','打开智辉刀图目录')
block(templates['result_status'],'result_status','请选择折弯位置，自动检查干涉。')
block(templates['result_status'],'color_notice','黄色：刀具轮廓；红色：实际重叠边界。')
block(templates['result_status'],'scope_notice','仅当前姿态；未检查运动路径、下模及整机。')
block(templates['result_status'],'sample_notice','自定义刀具选中后按 F2 改名；内置刀具仅为示例。')
dialog.set('title','折弯模拟 · 单刀干涉')
dialog.set('icon','ZeWanMoNi.bmp')
for e in group.find('PropertyList'):
    if e.get('sname')=='Label': e.set('value','折弯位置与刀具')
for e in dialog.find('PropertyList'):
    if e.get('sname')=='Label': e.set('value','折弯模拟 · 单刀干涉')
    if e.get('sname')=='HelpTag': e.set('value','Zhihui_ZeWanMoNi')
ET.indent(tree,space='  ')
tree.write(root/'ZeWanMoNi.dlx',encoding='utf-8',xml_declaration=True)

im=Image.new('RGB',(32,32),'white')
d=ImageDraw.Draw(im)
d.line([(4,10),(4,27),(28,27)],fill=(40,110,170),width=4)
d.polygon([(14,4),(24,4),(24,13),(11,24),(9,22),(14,15)],fill=(70,78,88))
d.line([(17,19),(22,24)],fill=(226,86,34),width=2)
d.line([(22,19),(17,24)],fill=(226,86,34),width=2)
im.save(root/'ZeWanMoNi.bmp')
