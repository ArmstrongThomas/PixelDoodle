import pathlib, struct, sys
b=pathlib.Path(sys.argv[1]).read_bytes()
assert len(b)>=0x1000
assert b[0x12] in (2,3), 'Expected DS/DSi enhanced header'
for offset in (0x20,0x30):
    start,entry,ram,size=struct.unpack_from('<IIII',b,offset)
    assert size and start+size<=len(b)
assert b'\xed\xa5\x8d\xbf Chishm\0' in b, 'DLDI placeholder missing'
print('NDS/DSi header, ARM bounds and DLDI: PASS')
