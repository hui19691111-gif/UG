import sys, os
sys.path.insert(0, r'.\douyin_promo\pydeps')
from moviepy import VideoFileClip
from PIL import Image, ImageDraw
video = r'C:\Users\Administrator\Desktop\zhihui_douyin_ready.mp4'
out = r'G:\UG智辉钣金插件_源码精简_20260605_最终精简\features\26_2P_BiLanCao\douyin_promo\ready_preview_grid.jpg'
clip = VideoFileClip(video)
print(f'duration={clip.duration:.2f} size={clip.w}x{clip.h} fps={clip.fps} audio={clip.audio is not None}')
times=[1,8,18,32,55,clip.duration-3]
frames=[]
for t in times:
    img=Image.fromarray(clip.get_frame(max(0,min(t,clip.duration-0.1)))).convert('RGB')
    img.thumbnail((270,480))
    tile=Image.new('RGB',(300,530),(15,15,15))
    tile.paste(img,((300-img.width)//2,0))
    d=ImageDraw.Draw(tile)
    d.text((10,496), f'{t:.1f}s', fill=(255,255,255))
    frames.append(tile)
canvas=Image.new('RGB',(900,1060),(0,0,0))
for i,img in enumerate(frames):
    canvas.paste(img,((i%3)*300,(i//3)*530))
canvas.save(out, quality=92)
print(out)
clip.close()
