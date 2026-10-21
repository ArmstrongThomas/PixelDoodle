import hashlib
from pathlib import Path

root = Path(__file__).resolve().parent.parent
pem = (root / 'data/cacert.pem').read_bytes()
embedded = (root / 'data/cacert.bin').read_bytes()
assert hashlib.sha256(pem).hexdigest() == '3ff344e30b9b1ed2971044eabb438a08f2e2245ddb5f8ab1a3ad8b63ab4eaf91'
assert embedded == pem + b'\0'
assert pem.count(b'-----BEGIN CERTIFICATE-----') >= 100
header = root / 'data/cacert_bin.h'
with header.open('w', newline='\n') as f:
    f.write('#ifndef PD_CACERT_BIN_H\n#define PD_CACERT_BIN_H\n#include <stddef.h>\n')
    f.write('static const unsigned char cacert_bin[] = {\n')
    for offset in range(0, len(embedded), 16):
        f.write('    ' + ','.join(f'0x{x:02x}' for x in embedded[offset:offset + 16]) + ',\n')
    f.write('};\nstatic const size_t cacert_bin_size = sizeof(cacert_bin);\n#endif\n')
print('Pinned Mozilla CA bundle and generated embedded copy: PASS')
