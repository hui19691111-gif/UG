import sys, os, math
sys.path.insert(0, r'.\douyin_promo\pydeps')
from moviepy import VideoFileClip
from PIL import Image, ImageDraw, ImageFont

video_path = r'C:\Users\Administrator\Desktop\302aeb45b1a3df08a553fd1876e47616.mp4'
out_dir = r'G:\UG智辉钣金插件_源码精简_20260605_最终精简\features\26_2P_BiLanCao\douyin_promo'
os.makedirs(out_dir, exist_ok=True)
clip = VideoFileClip(video_path)
print(f'duration={clip.duration:.3f}')
print(f'size={clip.w}x{clip.h}')
print(f'fps={clip.fps}')
print(f'audio={clip.audio is not None}')
# sample 6 frames
n=6
times=[clip.duration*(i+0.5)/n for i in range(n)]
frames=[]
for idx,t in enumerate(times):
    frame=clip.get_frame(t)
    img=Image.fromarray(frame).convert('RGB')
    img.thumbnail((480,270))
    tile=Image.new('RGB',(480,310),(20,20,20))
    tile.paste(img,((480-img.width)//2,0))
    d=ImageDraw.Draw(tile)
    d.text((10,280),f'{t:.1f}s',fill=(255,255,255))
    frames.append(tile)
canvas=Image.new('RGB',(960,620),(0,0,0))
for i,img in enumerate(frames):
    canvas.paste(img,((i%2)*480,(i//2)*310))
preview=os.path.join(out_dir,'source_preview_grid.jpg')
canvas.save(preview,quality=92)
print(preview)
clip.close()
