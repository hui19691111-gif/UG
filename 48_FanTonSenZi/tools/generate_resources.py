"""Build DLX using the project's established native Block Styler definitions."""
from pathlib import Path
import copy
import xml.etree.ElementTree as ET
from PIL import Image, ImageDraw
root=Path(__file__).resolve().parents[1]
tree=ET.parse(root.parent/'45_ZeWanFuZu/ZeWanFuZu.dlx')
dialog=tree.getroot(); group=dialog.find('item')
members=group.find('./PropertyList/Property/PropertyList')
templates={p.get('id'):copy.deepcopy(p) for p in members}
for p in list(members):members.remove(p)
def block(template,name,label,value=None,minimum=None,maximum=None):
    p=copy.deepcopy(templates[template]);old=p.get('id')
    for e in p.iter():
        for key in ('id','name','value'):
            if e.get(key)==old:e.set(key,name)
    for e in p.iter('Property'):
        key=e.get('sname')
        if key in ('Label','LabelString','Cue'):e.set('value',label)
        if key=='Value' and value is not None:e.set('value',str(value))
        if key=='MinimumValue' and minimum is not None:e.set('value',str(minimum))
        if key=='MaximumValue' and maximum is not None:e.set('value',str(maximum))
        if key=='SelectMode':e.set('selected','0')
        if key=='Increment' and name=='divisions':e.set('value','1')
        if key=='Increment' and name in ('kfactor','tube_kfactor'):e.set('value','0.01')
    members.append(p)
block('thickness_faces','path_edges','选择方通侧平面 / 圆管面')
block('reverse_end','segment_arcs','圆弧多段伸直','True')
block('plate_width','divisions','每条圆弧切口数',12,2,180)
block('plate_width','tube_kfactor','弯管 K 因子（0.5 为中心线）',.5,.01,1)
block('plate_width','radius','连接壁内弯半径 (mm)',1,0,10000)
block('plate_width','kfactor','连接壁 K 系数',.4,.01,1)
block('plate_width','gap','切口间隙 (mm)',.2,0,100)
block('plate_width','bridge_width','圆管连接带弧宽 (mm)',6,.1,10000)
block('reverse_end','cut_source','在原管件上切间隙槽（同时伸直）','False')
block('reverse_end','hide_source','完成后隐藏原管','False')
block('result_status','result_status','方通选完整侧平面；圆管选管面或圆环端口。')
block('result_status','result_detail','开槽开关默认关闭；开启后原管同步切槽。')
block('result_status','notice','关闭多段伸直：圆弧不开槽；真实转角仍开槽。')
block('result_status','scope','平面、同向、等厚；圆管暂不支持孔槽。')
block('result_status','forming','弯管 K 按管高/外径计；连接壁 K 按壁厚计。')
dialog.set('title','方通 / 圆管伸直');dialog.set('icon','FanTonSenZi.bmp')
for e in group.find('PropertyList'):
    if e.get('sname')=='Label':e.set('value','方通 / 圆管切口下料')
for e in dialog.find('PropertyList'):
    if e.get('sname')=='Label':e.set('value','方通 / 圆管伸直')
    if e.get('sname')=='HelpTag':e.set('value','Zhihui_FanTonSenZi')
ET.indent(tree,space='  ');tree.write(root/'FanTonSenZi.dlx',encoding='utf-8',xml_declaration=True)
im=Image.new('RGB',(32,32),'white');d=ImageDraw.Draw(im)
d.polygon([(2,12),(10,4),(27,4),(27,10),(13,10),(8,15)],fill='#317caa')
d.rectangle((2,21,29,29),fill='#317caa')
for x in (8,17):d.polygon([(x,21),(x+5,21),(x+2,27)],fill='white')
d.line((2,29,29,29),fill='#48a25e',width=2)
d.line((21,12,21,17),fill='#50565d',width=2)
d.polygon([(17,15),(25,15),(21,19)],fill='#50565d')
im.save(root/'FanTonSenZi.bmp')
