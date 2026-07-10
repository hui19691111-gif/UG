import sys, os, math
sys.path.insert(0, r'.\douyin_promo\pydeps')

import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter
from moviepy import VideoFileClip, ImageClip, CompositeVideoClip, ColorClip, AudioArrayClip, CompositeAudioClip

SRC = r'C:\Users\Administrator\Desktop\302aeb45b1a3df08a553fd1876e47616.mp4'
OUT_DIR = r'C:\Users\Administrator\Desktop'
OUT = os.path.join(OUT_DIR, 'zhihui_douyin_ready.mp4')
ASSET_DIR = r'G:\UG智辉钣金插件_源码精简_20260605_最终精简\features\26_2P_BiLanCao\douyin_promo'
os.makedirs(ASSET_DIR, exist_ok=True)

W, H = 1080, 1920
FONT_BOLD = r'C:\Windows\Fonts\msyhbd.ttc'
FONT_REG = r'C:\Windows\Fonts\msyh.ttc'
if not os.path.exists(FONT_BOLD):
    FONT_BOLD = r'C:\Windows\Fonts\simhei.ttf'
if not os.path.exists(FONT_REG):
    FONT_REG = FONT_BOLD

def text_img(size, lines, font_sizes, fills, aligns=None, bg=None, radius=0, strokes=None, line_gap=10, pad=(0,0)):
    img = Image.new('RGBA', size, (0,0,0,0))
    d = ImageDraw.Draw(img)
    if bg:
        x0,y0,x1,y1 = 0,0,size[0],size[1]
        if radius:
            d.rounded_rectangle([x0,y0,x1-1,y1-1], radius=radius, fill=bg)
        else:
            d.rectangle([x0,y0,x1,y1], fill=bg)
    fonts=[ImageFont.truetype(FONT_BOLD if i==0 else FONT_REG, fs) for i,fs in enumerate(font_sizes)]
    bboxes=[d.textbbox((0,0), line, font=fonts[i], stroke_width=(strokes[i][0] if strokes and strokes[i] else 0)) for i,line in enumerate(lines)]
    heights=[b[3]-b[1] for b in bboxes]
    total=sum(heights)+line_gap*(len(lines)-1)
    y=(size[1]-total)//2 + pad[1]
    for i,line in enumerate(lines):
        bbox=bboxes[i]
        tw=bbox[2]-bbox[0]
        align = aligns[i] if aligns else 'center'
        if align == 'left': x=pad[0]
        elif align == 'right': x=size[0]-tw-pad[0]
        else: x=(size[0]-tw)//2
        stroke_width=0
        stroke_fill=(0,0,0,0)
        if strokes and strokes[i]:
            stroke_width, stroke_fill = strokes[i]
        d.text((x,y), line, font=fonts[i], fill=fills[i], stroke_width=stroke_width, stroke_fill=stroke_fill)
        y += heights[i] + line_gap
    return np.array(img)

def make_caption(text):
    return text_img(
        (980, 190),
        [text], [48], [(255,255,255,255)],
        bg=(0,0,0,175), radius=26,
        strokes=[(2,(0,0,0,210))], line_gap=0
    )

def make_label(text, w=640, h=72):
    return text_img((w,h), [text], [34], [(255,255,255,255)], bg=(0,117,148,225), radius=20)

def make_outline(size, color=(0,210,255,255), width=6):
    img=Image.new('RGBA', size, (0,0,0,0))
    d=ImageDraw.Draw(img)
    for i in range(width):
        d.rounded_rectangle([i,i,size[0]-1-i,size[1]-1-i], radius=16, outline=color, width=1)
    return np.array(img)

def make_arrow(size=(220,90)):
    img=Image.new('RGBA', size, (0,0,0,0))
    d=ImageDraw.Draw(img)
    c=(255,213,74,255)
    d.line([(10,70),(155,25)], fill=c, width=10)
    d.polygon([(155,25),(130,10),(135,45)], fill=c)
    d.line([(10,70),(155,25)], fill=(0,0,0,120), width=2)
    return np.array(img)

def make_bgm(duration, sr=44100):
    t=np.linspace(0, duration, int(sr*duration), endpoint=False)
    # light tech pad + soft pulse, normalized. No copyrighted melody.
    pad=0.08*np.sin(2*np.pi*220*t)+0.05*np.sin(2*np.pi*330*t)+0.035*np.sin(2*np.pi*440*t)
    pulse=((np.sin(2*np.pi*2.0*t)>0.82).astype(float))*0.10*np.sin(2*np.pi*95*t)
    tick=((np.sin(2*np.pi*4.0*t)>0.92).astype(float))*0.045*np.sin(2*np.pi*900*t)
    beat=pad+pulse+tick
    fade=np.minimum(1, t/1.5)*np.minimum(1, (duration-t)/2.0)
    beat=beat*fade
    audio=np.stack([beat, beat], axis=1).astype('float32')
    return AudioArrayClip(audio, fps=sr)

clip=VideoFileClip(SRC)
duration=clip.duration

# Background: dark gradient, avoids cropping software details.
bg_img=Image.new('RGB',(W,H),(9,14,18))
d=ImageDraw.Draw(bg_img)
for y in range(H):
    r=int(8+20*y/H); g=int(14+15*y/H); b=int(18+18*y/H)
    d.line([(0,y),(W,y)], fill=(r,g,b))
bg=np.array(bg_img)
background=ImageClip(bg).with_duration(duration)

# Main video centered.
main_w=1040
main_h=int(main_w*clip.h/clip.w)
main=clip.resized(width=main_w).with_position(((W-main_w)//2, 570))
main_y=570
main_x=(W-main_w)//2

# Header.
header_img=text_img(
    (1040, 210),
    ['智辉钣金插件', 'UG/NX 钣金拆件提效工具'],
    [78, 38],
    [(255,255,255,255), (169,232,255,255)],
    strokes=[(3,(0,0,0,180)), (2,(0,0,0,170))],
    line_gap=18
)
header=ImageClip(header_img).with_duration(duration).with_position((20,38))

# Toolbar close-up from the first frame.
first=Image.fromarray(clip.get_frame(0)).convert('RGB')
toolbar_crop=first.crop((0,0,clip.w,132))
toolbar_crop=toolbar_crop.resize((1000,103), Image.LANCZOS)
frame=Image.new('RGBA',(1040,155),(0,0,0,0))
fd=ImageDraw.Draw(frame)
fd.rounded_rectangle([0,0,1039,154], radius=22, fill=(255,255,255,235))
frame.paste(toolbar_crop.convert('RGBA'), (20,32))
fd.rounded_rectangle([20,32,1019,135], radius=10, outline=(0,160,210,255), width=5)
closeup=ImageClip(np.array(frame)).with_duration(duration).with_position((20,260))
close_label=ImageClip(make_label('工具栏集成常用拆件功能', 520, 66)).with_duration(duration).with_position((280,225))

# Main toolbar outline on the embedded screen, for first half.
outline=ImageClip(make_outline((main_w, 110))).with_start(0).with_duration(min(duration, 28)).with_position((main_x, main_y))
arrow=ImageClip(make_arrow()).with_start(0).with_duration(8).with_position((760,455))

# Captions.
captions=[
    (0, 6, '钣金拆件，最耗时间的是重复操作'),
    (6, 14, '属性、分层、编号、出图，每个项目都要来一遍'),
    (14, 24, '智辉钣金插件，把常用流程放进 UG 工具栏'),
    (24, 40, '打开功能，按流程批量处理，减少重复点击'),
    (40, 58, '模型、图纸、明细结果更直观'),
    (58, duration, 'UG/NX 钣金用户，需要的留言'),
]
caption_clips=[]
for s,e,text in captions:
    caption_clips.append(ImageClip(make_caption(text)).with_start(s).with_duration(max(0.1,e-s)).with_position((50, 1290)))

cta_img=text_img((760,88), ['需要的在评论区留言'], [44], [(255,255,255,255)], bg=(238,68,45,235), radius=28)
cta=ImageClip(cta_img).with_start(max(0,duration-10)).with_duration(min(10,duration)).with_position((160, 1512))

footer_img=text_img((980,80), ['#UG钣金 #NX钣金 #钣金拆件 #智辉钣金插件'], [30], [(220,244,255,255)], bg=(0,0,0,90), radius=18)
footer=ImageClip(footer_img).with_duration(duration).with_position((50, 1748))

layers=[background, main, header, closeup, close_label, outline, arrow, footer, cta] + caption_clips
final=CompositeVideoClip(layers, size=(W,H)).with_duration(duration)

music=make_bgm(duration).with_duration(duration)
audios=[music]
if clip.audio is not None:
    try:
        audios.append(clip.audio.with_volume_scaled(0.12))
    except Exception:
        pass
final=final.with_audio(CompositeAudioClip(audios).with_duration(duration))

final.write_videofile(OUT, fps=30, codec='libx264', audio_codec='aac', bitrate='4500k', preset='medium', threads=4)
print(OUT)
clip.close()
final.close()
