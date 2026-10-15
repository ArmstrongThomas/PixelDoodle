"""Rasterize the website's five-square SVG mark on an integer pixel grid.
Run manually with Pillow when changing the logo; generated assets are build inputs.
"""
from pathlib import Path
from PIL import Image, ImageDraw
out=Path(__file__).resolve().parents[1]/'assets'
colors=['#211039','#ff84c5','#d9ff57']
def icon(size):
    im=Image.new('RGB',(size,size),colors[0]);d=ImageDraw.Draw(im)
    unit=size//4;origin=(size-3*unit-unit//3)//2;shadow=max(1,unit//3)
    for offset,color in [(shadow,colors[1]),(0,colors[2])]:
        for x,y in [(0,0),(2,0),(1,1),(0,2),(2,2)]:
            left=origin+x*unit+offset;top=origin+y*unit+offset
            d.rectangle((left,top,left+unit-1,top+unit-1),fill=color)
    return im
icon(32).save(out/'icon-nds.gif')
icon(48).save(out/'icon-3ds.png')
icon(48).resize((384,384),Image.Resampling.NEAREST).save(out/'pixel-doodle-logo.png')
# Original SVG geometry retained alongside the raster build inputs.
(out/'pixel-doodle-logo.svg').write_text('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 48 48"><rect width="48" height="48" fill="#211039"/><path fill="#ff84c5" transform="translate(8 8) scale(1.2)" d="M0 0h10v10H0zm10 10h10v10H10zM20 0h10v10H20zM0 20h10v10H0zm20 0h10v10H20z"/><path fill="#d9ff57" transform="translate(4 4) scale(1.2)" d="M0 0h10v10H0zm10 10h10v10H10zM20 0h10v10H20zM0 20h10v10H0zm20 0h10v10H20z"/></svg>',encoding='utf-8')
print('Generated 32px NDS GIF, 48px 3DS PNG and 384px logo preview')
