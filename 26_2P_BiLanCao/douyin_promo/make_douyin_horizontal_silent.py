import sys, os
sys.path.insert(0, r'.\douyin_promo\pydeps')

import numpy as np
from PIL import Image, ImageDraw, ImageFont
from moviepy import VideoFileClip, ImageClip, CompositeVideoClip, ColorClip

SRC = r'C:\Users\Administrator\Desktop\302aeb45b1a3df08a553fd1876e47616.mp4'
OUT = r'C:\Users\Administrator\Desktop\zhihui_douyin_horizontal_key_features_no_audio.mp4'
W, H = 1920, 1080
FONT_BOLD = r'C:\Windows\Fonts\msyhbd.ttc'
FONT_REG = r'C:\Windows\Fonts\msyh.ttc'
if not os.path.exists(FONT_BOLD):
    FONT_BOLD = r'C:\Windows\Fonts\simhei.ttf'
if not os.path.exists(FONT_REG):
    FONT_REG = FONT_BOLD

def text_box(size, lines, sizes, colors, bg=(0,0,0,0), radius=0, strokes=None, line_gap=8, align='center', pad_x=0):
    img = Image.new('RGBA', size, (0,0,0,0))
    d = ImageDraw.Draw(img)
    if bg[3] > 0:
        if radius:
            d.rounded_rectangle([0,0,size[0]-1,size[1]-1], radius=radius, fill=bg)
        else:
            d.rectangle([0,0,size[0],size[1]], fill=bg)
    fonts = [ImageFont.truetype(FONT_BOLD if i == 0 else FONT_REG, sizes[i]) for i in range(len(lines))]
    bbs = [d.textbbox((0,0), lines[i], font=fonts[i], stroke_width=(strokes[i][0] if strokes and strokes[i] else 0)) for i in range(len(lines))]
    hs = [bb[3]-bb[1] for bb in bbs]
    total = sum(hs) + line_gap * (len(lines)-1)
    y = (size[1]-total)//2
    for i,line in enumerate(lines):
        tw = bbs[i][2]-bbs[i][0]
        if align == 'left':
            x = pad_x
        elif align == 'right':
            x = size[0]-tw-pad_x
        else:
            x = (size[0]-tw)//2
        sw, sf = (0, (0,0,0,0))
        if strokes and strokes[i]:
            sw, sf = strokes[i]
        d.text((x,y), line, font=fonts[i], fill=colors[i], stroke_width=sw, stroke_fill=sf)
        y += hs[i] + line_gap
    return np.array(img)

def caption_img(text):
    return text_box(
        (1500, 96), [text], [44], [(255,255,255,255)],
        bg=(0,0,0,165), radius=22, strokes=[(2,(0,0,0,230))]
    )

def pill(text, color=(0,132,172,220), w=470):
    return text_box((w,64), [text], [32], [(255,255,255,255)], bg=color, radius=24)

clip = VideoFileClip(SRC)
dur = clip.duration

# 16:9 horizontal canvas. Fit source by width, slight letterbox to preserve UI details.
main_w = W
main_h = int(W * clip.h / clip.w)
main_y = (H - main_h) // 2
bg = ColorClip((W,H), color=(12,14,18)).with_duration(dur)
main = clip.resized(width=main_w).with_position((0, main_y))

# Text stays inside the actual video frame area.
title = ImageClip(text_box(
    (620,110), ['智辉钣金插件', '一键出图 · 自动展开 · 装配拆件'],
    [44,24], [(255,255,255,255),(178,236,255,255)],
    bg=(0,0,0,115), radius=18, strokes=[(2,(0,0,0,190)),(1,(0,0,0,160))]
)).with_duration(dur).with_position((28, main_y + 22))

hint = ImageClip(pill('多实体 / 装配 / 钣金展开', w=560)).with_duration(min(22,dur)).with_position((690, main_y + 30))

# Cyan toolbar frame in source area.
outline_img = Image.new('RGBA', (W-26, 118), (0,0,0,0))
od = ImageDraw.Draw(outline_img)
for i in range(5):
    od.rounded_rectangle([i,i,W-27-i,117-i], radius=14, outline=(0,210,255,230), width=1)
outline = ImageClip(np.array(outline_img)).with_duration(min(20,dur)).with_position((13, main_y + 2))

captions = [
    (0, 7, '智辉钣金插件：钣金拆件一键出图'),
    (7, 17, '多实体模型也能处理，减少反复拆分操作'),
    (17, 29, '装配件同样支持，批量流程更适合实际项目'),
    (29, 43, '自动转钣金，自动展开，少走手工步骤'),
    (43, 60, '展开、出图、明细流程集中处理，结果更直观'),
    (60, dur, 'UG/NX 钣金用户，需要的评论区留言'),
]
cap_clips=[]
cap_y = main_y + main_h - 132
for s,e,t in captions:
    cap_clips.append(ImageClip(caption_img(t)).with_start(s).with_duration(max(0.1,e-s)).with_position(((W-1500)//2, cap_y)))

footer = ImageClip(text_box((720,54), ['#UG钣金 #NX钣金 #钣金拆件 #智辉钣金插件'], [25], [(230,246,255,230)], bg=(0,0,0,95), radius=15)).with_duration(dur).with_position((W-750, main_y + main_h - 62))
cta = ImageClip(text_box((560,64), ['需要的评论区留言'], [34], [(255,255,255,255)], bg=(238,68,45,230), radius=22)).with_start(max(0,dur-12)).with_duration(min(12,dur)).with_position((W-600, main_y + main_h - 144))

final = CompositeVideoClip([bg, main, outline, title, hint, footer, cta] + cap_clips, size=(W,H)).with_duration(dur)

# No audio track. Add upbeat trending music on Douyin publish page.
final.write_videofile(OUT, fps=30, codec='libx264', audio=False, bitrate='6500k', preset='medium', threads=4)
print(OUT)
clip.close(); final.close()

