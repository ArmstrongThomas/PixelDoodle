from pathlib import Path
import struct
from PIL import Image
root=Path('.')
b=(root/'PixelDoodle.nds').read_bytes();start=struct.unpack_from('<I',b,0x68)[0];assert start>0
pal=struct.unpack_from('<16H',b,start+0x220)
im=Image.new('RGB',(32,32))
for y in range(32):
 for x in range(32):
  index=((y//8)*4+x//8)*64+(y%8)*8+x%8
  v=b[start+0x20+index//2];c=pal[(v>>(4*(index%2)))&15]
  im.putpixel((x,y),((c&31)<<3,((c>>5)&31)<<3,((c>>10)&31)<<3))
source=Image.open('assets/icon-nds.gif').convert('RGB')
assert all(tuple(v>>3 for v in im.getpixel((x,y)))==tuple(v>>3 for v in source.getpixel((x,y))) for y in range(32) for x in range(32)), 'NDS logo mismatch'
s=(root/'build/3ds/PixelDoodle.smdh').read_bytes();assert s[:4]==b'SMDH'
def morton(x,y):
 return sum(((x>>i)&1)<<(2*i) | ((y>>i)&1)<<(2*i+1) for i in range(3))
im3=Image.new('RGB',(48,48));source3=Image.open('assets/icon-3ds.png').convert('RGB')
for y in range(48):
 for x in range(48):
  at=0x24c0+2*(((y//8)*6+x//8)*64+morton(x%8,y%8));v=struct.unpack_from('<H',s,at)[0]
  r,g,bl=(v>>11)&31,(v>>5)&63,v&31
  expected=source3.getpixel((x,y));assert (r,g,bl)==(expected[0]>>3,expected[1]>>2,expected[2]>>3),'3DS logo mismatch'
  im3.putpixel((x,y),(r<<3,g<<2,bl<<3))
x=(root/'build/3ds/PixelDoodle.3dsx').read_bytes();assert s in x,'SMDH not embedded in 3DSX'
im.resize((256,256),Image.Resampling.NEAREST).save('build/icon-nds-embedded.png')
im3.resize((288,288),Image.Resampling.NEAREST).save('build/icon-3ds-embedded.png')
print('Embedded NDS RGB555 and 3DS RGB565 pixels match source; exact SMDH embedded in 3DSX: PASS')
