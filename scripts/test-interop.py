"""Optional desktop interop check; synthetic fixtures only. Run host tests first."""
from pathlib import Path
import struct, subprocess, sys, zlib
from PIL import Image
repo=Path(__file__).resolve().parents[1]
root=max((repo/'build/host').glob('run-*'),key=lambda p:p.stat().st_mtime)
exe=repo/'build/host/test.exe'
ase=Path(sys.argv[1]) if len(sys.argv)>1 else Path('C:/Program Files (x86)/Steam/steamapps/common/Aseprite/Aseprite.exe')
subprocess.run([str(ase),'--batch',str(root/'roundtrip.aseprite'),'--save-as',str(root/'aseprite-saved.aseprite')],check=True)
subprocess.run([str(exe),'compare',str(root/'roundtrip.pdd'),str(root/'aseprite-saved.aseprite')],check=True)
for name in ['one-layer','layer-set','ordered-blend','project-palette','eight-layers','protected']:
 source=root/(name+'.aseprite'); dest=root/(name+'-desktop.aseprite')
 subprocess.run([str(ase),'--batch',str(source),'--save-as',str(dest)],check=True)
 subprocess.run([str(exe),'compare',str(source),str(dest)],check=True)
 if name=='ordered-blend': subprocess.run([str(exe),'compare',str(root/'ordered-blend.pdd'),str(dest)],check=True)
# PDD2 compatibility uses the exact old layout with its former reserved field.
current=(root/'roundtrip.pdd').read_bytes();p=current[32:]
base=bytearray(p[:8]+p[16:16+4*64]+p[16+8*64:16+8*64+4*128*128*4])
for version in range(1,7):
 payload=bytearray(base[:8]+base[8+4*64:]) if version==1 else base.copy()
 if version>=4: payload+=bytes(4)+bytes(range(4))
 if version>=5: payload+=bytes(1028)
 header=bytearray(32);header[:4]=f'PDD{version}'.encode();struct.pack_into('<6I',header,4,version,128,128,4,15 if version>=3 else 1,len(payload));struct.pack_into('<I',header,28,zlib.crc32(payload,zlib.crc32(header[:28])))
 legacy_path=root/f'legacy-pdd{version}.pdd';legacy_path.write_bytes(header+payload)
 subprocess.run([str(exe),'compare-pixels' if version==1 else 'compare',str(root/'roundtrip.pdd'),str(legacy_path)],check=True)
b=bytearray((root/'roundtrip.aseprite').read_bytes()); cases={}
for name,offset,fmt,value in [('animation',6,'H',2),('oversize',8,'H',256),('chunk-overflow',144,'I',0xffffffff),('blend-mode',160,'H',4),('nested-layer',154,'H',1)]:
 x=b.copy();struct.pack_into('<'+fmt,x,offset,value);cases[name]=x
x=bytearray((root/'roundtrip.pdd').read_bytes());x[-1]^=1;cases['bad-crc']=x
# This installed Aseprite writes raw cels. Independently encode standard zlib
# cels, verify our reader, and make desktop Aseprite consume that variant too.
c=bytearray((root/'aseprite-saved.aseprite').read_bytes());at=144;chunks=[]
while at<len(c):
 size,kind=struct.unpack_from('<IH',c,at);chunk=bytearray(c[at:at+size])
 if kind==0x2005 and struct.unpack_from('<H',chunk,13)[0]==0:
  chunk=chunk[:26]+zlib.compress(chunk[26:]);struct.pack_into('<I',chunk,0,len(chunk));struct.pack_into('<H',chunk,13,2)
 chunks.append(chunk);at+=size
c=c[:144]+b''.join(chunks);struct.pack_into('<I',c,0,len(c));struct.pack_into('<I',c,128,len(c)-128)
compressed_path=root/'compressed.aseprite';compressed_path.write_bytes(c)
subprocess.run([str(exe),'compare',str(root/'roundtrip.pdd'),str(compressed_path)],check=True)
subprocess.run([str(ase),'--batch',str(compressed_path),'--save-as',str(root/'compressed-desktop.aseprite')],check=True)
subprocess.run([str(exe),'compare',str(root/'roundtrip.pdd'),str(root/'compressed-desktop.aseprite')],check=True)
at=144
while at<len(c):
 size,kind=struct.unpack_from('<IH',c,at)
 if kind==0x2005 and struct.unpack_from('<H',c,at+13)[0]==2:
  c[at+26]^=255;break
 at+=size
cases['bad-zlib']=c
for name,data in cases.items():
 path=root/(name+'.aseprite');path.write_bytes(data);subprocess.run([str(exe),'reject',str(path)],check=True)
png=Image.open(root/'roundtrip.png');png.load()
assert png.mode=='RGBA' and png.size==(128,128)
assert png.getpixel((80,40))==(0,0,0,0) and png.getpixel((30,60))==(68,85,102,96)
print('Desktop Aseprite roundtrip and independent zlib-cel interop, malformed file preservation, independent PNG: PASS')

# Independently construct vertical RGBA strips using each PNG filter.
colors=[(i,255-i,(i*7)%256,i) for i in range(32)]
(root/'independent.hex').write_text(''.join('%02X%02X%02X%02X\n'%c for c in colors))
def chunk(kind,data):
 return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data))
def paeth(a,b,c):
 p=a+b-c; return min([(abs(p-a),0,a),(abs(p-b),1,b),(abs(p-c),2,c)])[2]
for filt in range(5):
 raw=bytearray();prev=bytes(4)
 for color in colors:
  row=bytes(color);raw.append(filt)
  for i,v in enumerate(row):
   up=prev[i]; prediction=0 if filt in (0,1) else up if filt==2 else up//2 if filt==3 else paeth(0,up,0)
   raw.append((v-prediction)&255)
  prev=row
 data=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',1,32,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b'')
 path=root/('filter-%d.png'%filt);path.write_bytes(data)
 subprocess.run([str(exe),'palette-compare',str(root/'independent.hex'),str(path)],check=True)
img=Image.open(root/'palette.png').convert('RGBA');assert img.size==(256,1)
assert list(img.getdata())==[(255-i,i,i,i) for i in range(256)]
print('Independent palette PNG decoding and all five filter imports: PASS')

for w,h in [(1,1),(16,16),(32,32),(64,64),(16,32),(37,19),(128,1),(1,128)]:
 name=f'size-{w}x{h}'
 im=Image.open(root/(name+'.png'));im.load();assert im.size==(w,h) and im.getpixel((w-1,h-1))==(18,52,86,255)
 source=root/(name+'.aseprite');dest=root/(name+'-desktop.aseprite')
 subprocess.run([str(ase),'--batch',str(source),'--save-as',str(dest)],check=True)
 subprocess.run([str(exe),'compare',str(root/(name+'.pdd')),str(dest)],check=True)
print('Variable-size desktop Aseprite round trips and independent PNG dimensions/pixels: PASS')

for w,h in [(0,32),(129,32),(16,0),(16,129)]:
 data=bytearray((root/'size-16x32.pdd').read_bytes());struct.pack_into('<II',data,8,w,h);struct.pack_into('<I',data,28,zlib.crc32(data[32:],zlib.crc32(data[:28])))
 path=root/f'invalid-{w}x{h}.pdd';path.write_bytes(data);subprocess.run([str(exe),'reject',str(path)],check=True)

# Ninth layer is rejected before changing the open document.
data=bytearray((root/'eight-layers.aseprite').read_bytes());at=144
while struct.unpack_from('<H',data,at+4)[0]!=0x2004: at+=struct.unpack_from('<I',data,at)[0]
size=struct.unpack_from('<I',data,at)[0];data+=data[at:at+size];struct.pack_into('<I',data,0,len(data));struct.pack_into('<I',data,128,len(data)-128);struct.pack_into('<H',data,134,struct.unpack_from('<H',data,134)[0]+1)
path=root/'nine-layers.aseprite';path.write_bytes(data);subprocess.run([str(exe),'reject',str(path)],check=True)

for k,(w,h) in enumerate([(129,129),(256,256),(512,512),(1,512),(512,1),(137,319)]):
 im=Image.open(root/f'large-{k}.png');im.load();assert im.size==(w,h) and im.getpixel((w-1,h-1))==(18,52,86,128)
 source=root/f'large-{k}.aseprite';dest=root/f'large-{k}-desktop.aseprite'
 subprocess.run([str(ase),'--batch',str(source),'--save-as',str(dest)],check=True)
 subprocess.run([str(exe),'compare-large',str(root/f'large-{k}.pdd'),str(dest)],check=True)
print('Large native/Aseprite desktop round trips and independent PNG pixels: PASS')
