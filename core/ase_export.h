#ifndef PD_ASE_EXPORT_H
#define PD_ASE_EXPORT_H
#include "pixel.h"
#include <stdio.h>
/* Stream flat RGBA animation; caller owns atomic replacement and readback. */
bool pd_ase_stream(PdDocument *d,FILE *f,uint32_t *crc,size_t *bytes,char error[64]);
#endif
