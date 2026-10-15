import pathlib, struct, sys, re
b=pathlib.Path(sys.argv[1]).read_bytes()
align=lambda x:(x+63)&~63
assert len(b)>=32
header,cert,ticket,tmd=struct.unpack_from('<I4xIII',b)
offset=align(align(align(header)+cert)+ticket)
assert offset+tmd<=len(b)
sig=struct.unpack_from('>I',b,offset)[0]
body=offset+{0x10000:0x240,0x10001:0x140,0x10002:0x80,0x10003:0x240,0x10004:0x140,0x10005:0x80}[sig]
assert body+0xa0<=offset+tmd
title=struct.unpack_from('>Q',b,body+0x4c)[0]
version=struct.unpack_from('>H',b,body+0x9c)[0]
assert title==0x000400000CE47800,hex(title)
semver=re.search(r'PD_VERSION "(\d+)\.(\d+)\.(\d+)"', (pathlib.Path(__file__).resolve().parents[1]/'core/version.h').read_text()).groups()
major,minor,patch=map(int,semver)
assert version==(major<<10 | minor<<4 | patch),version
assert 'Pixel Doodle'.encode('utf-16le') in b
print(f'CIA title 000400000CE47800 / version {version}: PASS')
