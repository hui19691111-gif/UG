"""Package validated native parts and code-rendered previews, retaining source meshes in build output."""
from pathlib import Path
import sys, csv, shutil, json, hashlib
import numpy as np
from PIL import Image, ImageDraw, ImageFont

source=Path(sys.argv[1]); target=Path(sys.argv[2])
flat='--flat' in sys.argv[3:]
assert not target.exists(), 'Use a new package directory'
target.mkdir(parents=True)
rows=list(csv.reader(source.joinpath('library.tsv').read_text(encoding='utf-16').splitlines(),delimiter='\t'))
output=[]; counts={}
for row in rows:
    if not row or row[0].startswith('#'): continue
    d=row[4].split('x')[0]; row[1]=f'{d} 平头内六角螺钉' if flat else f'{d} 内六角螺钉'
    old=source/Path(row[3].replace('\\','/'))
    # Flat and socket screws have identical display specifications; NX requires
    # distinct physical basenames to load both kinds into the same assembly.
    new=target/'LibParam'/'国标螺钉'/row[1]/(('GB70_3_'+old.name) if flat else old.name)
    new.parent.mkdir(parents=True,exist_ok=True)
    # A closed mesh with positive volume independently validates solid export.
    data=old.with_suffix('.stl').read_bytes()
    faces=np.frombuffer(data,dtype=np.dtype([('n','<f4',(3,)),('v','<f4',(3,3)),('a','<u2')]),offset=84)['v'].astype(float)
    assert np.einsum('ij,ij->i',faces[:,0],np.cross(faces[:,1],faces[:,2])).sum()>0, old
    _,indices=np.unique(np.round(faces.reshape(-1,3),5),axis=0,return_inverse=True)
    triangles=indices.reshape(-1,3)
    edges=np.sort(np.concatenate((triangles[:,[0,1]],triangles[:,[1,2]],triangles[:,[2,0]])),axis=1)
    _,uses=np.unique(edges,axis=0,return_counts=True)
    assert np.all(uses==2), old
    for ext in ['.prt','.png']: shutil.copy2(old.with_suffix(ext),new.with_suffix(ext))
    for name in ['parameters.tsv','parameter-schema.tsv']: shutil.copy2(old.parent/name,new.parent/name)
    row[3]=new.relative_to(target).as_posix(); output.append(row); counts[d]=counts.get(d,0)+1

# A neutral dimension key is used for user-created variants until they capture a preview.
im=Image.new('RGB',(1040,624),'white'); draw=ImageDraw.Draw(im)
font=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',26)
big=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',32)
draw.text((40,30),('GB/T 70.3 · 平头参数示意' if flat else 'GB/T 70.1 · 参数示意'),font=big,fill='#374151')
profile=([(175,220),(195,220),(285,270),(795,270),(810,285),(810,335),(795,350),(285,350),(195,400),(175,400)] if flat else [(175,220),(265,220),(265,270),(795,270),(810,285),(810,335),(795,350),(265,350),(265,400),(175,400)])
draw.polygon(profile,outline='#374151',fill='#dce3eb',width=3)
draw.line([(140,310),(850,310)],fill='#9ca3af',width=2)
draw.line([(180,266),(226,266),(226,354),(180,354)],fill='#596578',width=3)
def dim(x1,y1,x2,y2,label):
    draw.line([(x1,y1),(x2,y2)],fill='#16738b',width=2)
    if y1==y2:
        draw.polygon([(x1,y1),(x1+12,y1-5),(x1+12,y1+5)],fill='#16738b')
        draw.polygon([(x2,y2),(x2-12,y2-5),(x2-12,y2+5)],fill='#16738b')
        draw.text(((x1+x2)/2-12,y1-34),label,font=font,fill='#16738b')
    else:
        draw.polygon([(x1,y1),(x1-5,y1+12),(x1+5,y1+12)],fill='#16738b')
        draw.polygon([(x2,y2),(x2-5,y2-12),(x2+5,y2-12)],fill='#16738b')
        draw.text((x1-48,(y1+y2)/2-18),label,font=font,fill='#16738b')
dim(175 if flat else 265,445,810,445,'L');dim(175,170,285 if flat else 265,170,'K');dim(130,220,130,400,'DK');dim(875,270,875,350,'D')
draw.text((45,520),'S：扳手对边   T：内六角深度   P：螺距   B：螺纹长度',font=font,fill='#374151')
draw.text((45,566),('平头顶面中心为插入点，L 包含头部；单位：mm' if flat else '头下端面中心为插入点，螺杆沿 −Z；尺寸单位：mm'),font=font,fill='#6b7280')
for family in target.joinpath('LibParam','国标螺钉').iterdir():im.save(family/'parameter-preview.png')
text='# ZHIHUI_STANDARD_PARTS_V3\r\n# id\tname\tcategory\trelative_model_path\tspecification\tparameterized\r\n'
text+=''.join('\t'.join(row)+'\r\n' for row in output)
target.joinpath('library.tsv').write_bytes(text.encode('utf-16'))
manifest={'standard':'GB/T 70.3-2023' if flat else 'GB/T 70.1-2008','specifications':len(output),'families':counts,'files':[]}
for path in sorted(target.rglob('*')):
    if path.is_file():manifest['files'].append({'path':path.relative_to(target).as_posix(),'bytes':path.stat().st_size,'sha256':hashlib.sha256(path.read_bytes()).hexdigest().upper()})
target.joinpath('gbt70.3-manifest.json' if flat else 'gbt70.1-manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
print(f'PASS {len(output)} closed positive-volume meshes; package {len(manifest["files"])} files; {counts}')
