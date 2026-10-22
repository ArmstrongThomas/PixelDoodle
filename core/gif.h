#ifndef PD_GIF_H
#define PD_GIF_H
#include "pixel.h"
#include <stddef.h>
/* Visible animation; refuses existing output/temp files. Binary alpha at 128. */
bool pd_gif_write(PdDocument *d,const char *path,size_t limit,char error[64]);
#endif
