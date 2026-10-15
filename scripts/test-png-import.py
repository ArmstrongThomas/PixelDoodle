"""Independent synthetic PNG import oracles; no user artwork."""
from pathlib import Path
import struct, subprocess, zlib
from PIL import Image
repo=Path(__file__).resolve().parents[1]
root=max((repo/'build/host').glob('run-*'),key=lambda p:p.stat().st_mtime)
exe=repo/'build/host/test.exe'
def chunk(t,p):
 return struct.pack('>I',len(p))+t+p+struct.pack('>I',zlib.crc32(t+p))
def png(w,h,depth,kind,raw,extra=b'',split=True):
 z=zlib.compress(raw);n=len(z)//2
 data=chunk(b'IDAT',z[:n])+chunk(b'IDAT',z[n:]) if split else chunk(b'IDAT',z)
 return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,depth,kind,0,0,0))+extra+data+chunk(b'IEND',b'')
def check(name,data,rgba=None):
 path=root/(name+'.png');path.write_bytes(data)
 im=Image.open(path).convert('RGBA');expected=root/(name+'.rgba');expected.write_bytes(struct.pack('<II',*im.size)+(im.tobytes() if rgba is None else rgba))
 subprocess.run([str(exe),'png-check',str(path),str(expected)],check=True)
def reject(name,data):
 path=root/(name+'.png');path.write_bytes(data);subprocess.run([str(exe),'reject',str(path)],check=True)
def paeth(a,b,c):
 p=a+b-c;return min((abs(p-a),0,a),(abs(p-b),1,b),(abs(p-c),2,c))[2]
for kind,channels in [(0,1),(2,3),(4,2),(6,4)]:
 for filt in range(5):
  raw=bytearray();prev=bytes(7*channels)
  for y in range(5):
   row=bytes((x*17+y*31)%256 for x in range(7*channels));raw.append(filt)
   for x,v in enumerate(row):
    left=row[x-channels] if x>=channels else 0;up=prev[x];corner=prev[x-channels] if x>=channels else 0
    prediction=[0,left,up,(left+up)//2,paeth(left,up,corner)][filt];raw.append((v-prediction)&255)
   prev=row
  check(f'png-type{kind}-filter{filt}',png(7,5,8,kind,raw))
for depth in [1,2,4,8]:
 count=1<<depth;w,h=13,3;rows=bytearray()
 for y in range(h):
  packed=bytearray((w*depth+7)//8)
  for x in range(w):packed[x*depth//8]|=((x+y)%count)<<(8-depth-x*depth%8)
  rows+=bytes([0])+packed
 palette=bytes(v for i in range(count) for v in (i,255-i,i*7%256));alpha=bytes(i*255//max(1,count-1) for i in range(count))
 check(f'png-indexed{depth}',png(w,h,depth,3,rows,chunk(b'PLTE',palette)+chunk(b'tRNS',alpha)))
 # Match tRNS against the original sample, before expanding packed gray to 8 bits.
 expected=bytes(v for y in range(h) for x in range(w) for v in (((x+y)%count*255//(count-1),)*3+(0 if (x+y)%count==1 else 255,)))
 check(f'png-gray{depth}',png(w,h,depth,0,rows,chunk(b'tRNS',struct.pack('>H',1))),expected)
check('png-rgb-key',png(2,1,8,2,b'\0\x12\x34\x56\xaa\xbb\xcc',chunk(b'tRNS',struct.pack('>HHH',18,52,86))))
for size in [128,129,256,512]:
 image=Image.new('RGBA',(size,size));image.putpixel((size-1,size-1),(11,22,33,77));path=root/f'png-pillow-{size}.png';image.save(path);check(f'png-pillow-check-{size}',path.read_bytes())
base=png(2,2,8,6,b'\0'+bytes(8)+b'\0'+bytes(8));reject('png-crc',base[:40]+bytes([base[40]^1])+base[41:]);reject('png-truncated',base[:-1]);reject('png-extra',base+b'x')
reject('png-filter',png(2,2,8,6,b'\5'+bytes(8)+b'\0'+bytes(8)))
reject('png-short-data',png(2,2,8,6,b'\0'+bytes(8)))
reject('png-long-data',png(2,2,8,6,b'\0'+bytes(8)+b'\0'+bytes(9)))
reject('png-apng',png(2,2,8,6,b'\0'+bytes(8)+b'\0'+bytes(8),chunk(b'acTL',struct.pack('>II',1,0))))
reject('png-invalid-index',png(1,1,8,3,b'\0\2',chunk(b'PLTE',b'\0\0\0')))
reject('png-no-palette',png(1,1,8,3,b'\0\0'))
reject('png-16-bit',png(1,1,16,6,bytes(9)))
reject('png-huge',png(513,1,8,6,b'\0'+bytes(513*4)))
z=zlib.compress(b'\0'+bytes(8)+b'\0'+bytes(8));n=len(z)//2
reject('png-separated-idat',base[:33]+chunk(b'IDAT',z[:n])+chunk(b'tEXt',b'Comment\0test')+chunk(b'IDAT',z[n:])+chunk(b'IEND',b''))
print('Independent PNG import: five filters, RGB/RGBA/grayscale/alpha, packed indexed palettes/transparency, 512 bounds, malformed preservation: PASS')
