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
        if key=='Increment' and name=='petals':e.set('value','1')
    members.append(p)
block('thickness_faces','cylinder','选择圆柱面')
block('thickness_faces','sphere','选择相接球面或环面')
block('plate_width','petals','分瓣数量',12,2,180)
block('plate_width','gap','瓣间隙 (mm)',.5,.05,1000)
block('plate_width','relief','根部避让深度 (mm)',3,.05,10000)
block('reverse_end','create_flat','同时生成展开实体','False')
block('reverse_end','hide_source','完成后隐藏参考体','True')
block('result_status','result_status','选择圆柱面及球面或环面，自动识别板厚。')
block('result_status','result_detail',' ')
block('result_status','notice','分瓣近似球面或环面；生成可展开钣金。')
block('result_status','scope','折弯补偿采用当前零件的钣金设置。')
dialog.set('title','球面展开');dialog.set('icon','QiuMianZanKai.bmp')
for e in group.find('PropertyList'):
    if e.get('sname')=='Label':e.set('value','分瓣钣金')
for e in dialog.find('PropertyList'):
    if e.get('sname')=='Label':e.set('value','球面展开')
    if e.get('sname')=='HelpTag':e.set('value','Zhihui_QiuMianZanKai')
ET.indent(tree,space='  ');tree.write(root/'QiuMianZanKai.dlx',encoding='utf-8',xml_declaration=True)
im=Image.new('RGB',(32,32),'white');d=ImageDraw.Draw(im)
d.rectangle((3,20,29,28),fill='#287baa')
for i in range(4):
    x=3+i*7;d.polygon([(x,20),(x+6,20),(x+3,4)],fill='#60ad59',outline='#286a39')
im.save(root/'QiuMianZanKai.bmp')
