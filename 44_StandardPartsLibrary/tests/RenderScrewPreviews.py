"""Render our exported NX mesh; no third-party CAD assets or screenshots."""
from pathlib import Path
import sys, struct, csv
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection

def render(path,standard='GB/T 70.1'):
    content=path.read_bytes()
    count=struct.unpack_from('<I',content,80)[0]
    assert len(content)==84+count*50 and count>0
    data=np.frombuffer(content, dtype=np.dtype([('n','<f4',(3,)),('v','<f4',(3,3)),('a','<u2')]),offset=84)
    v=data['v'].astype(float)
    n=np.cross(v[:,1]-v[:,0],v[:,2]-v[:,0]); n/=np.maximum(np.linalg.norm(n,axis=1,keepdims=True),1e-12)
    light=np.array([-.4,-.7,1]); light/=np.linalg.norm(light)
    shade=.48+.45*np.maximum(n@light,0)
    colors=np.column_stack((shade*.86,shade*.92,shade,np.ones(count)))
    # Rotate long parts into a horizontal diagonal for the wide library preview.
    v=v[:,:,[2,0,1]]
    fig=plt.figure(figsize=(8,4.8),dpi=130,facecolor='white')
    ax=fig.add_axes([0,.06,1,.9],projection='3d',computed_zorder=False)
    ax.add_collection3d(Poly3DCollection(v,facecolors=colors,edgecolors='none',linewidths=0,antialiased=False))
    lo=v.min(axis=(0,1)); hi=v.max(axis=(0,1)); center=(lo+hi)/2; span=hi-lo
    ax.set(xlim=(lo[0]-.04*span[0],hi[0]+.04*span[0]),ylim=(lo[1]*1.15,hi[1]*1.15),zlim=(lo[2]*1.15,hi[2]*1.15))
    ax.set_box_aspect(span); ax.view_init(elev=24,azim=-44); ax.set_proj_type('ortho'); ax.set_axis_off()
    fig.text(.05,.92,standard,fontsize=17,color='#374151',weight='bold')
    fig.text(.95,.06,path.stem,fontsize=15,color='#374151',ha='right')
    fig.savefig(path.with_suffix('.png'),facecolor='white'); plt.close(fig)

if __name__=='__main__':
    paths=list(Path(sys.argv[1]).rglob('*.stl'))
    for path in paths: render(path,sys.argv[2] if len(sys.argv)>2 else 'GB/T 70.1')
    print(f'PASS {len(paths)} mesh previews')
