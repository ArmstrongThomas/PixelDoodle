#ifndef PD_FILES_H
#define PD_FILES_H
#include "pixel.h"
#define PD_PATH 512
#define PD_NAME 64
#define PD_ENTRIES 128
typedef struct { char name[PD_NAME]; bool directory; } PdEntry;
typedef struct { PdEntry entries[PD_ENTRIES]; unsigned count; bool truncated; } PdListing;
bool pd_list(const char *path,PdListing *out,char error[64]);
bool pd_join(char out[PD_PATH],const char *dir,const char *name);
bool pd_exists(const char *path);
bool pd_mkdir(const char *path);
/* Stage imports before touching live artwork. Error text is bounded. */
bool pd_preview_file_profile(const char *path,uint32_t preview[64*64],unsigned profile,char error[64]);
/* As pd_preview_file_profile, also reporting the decoded size, enabled layers and frames. */
typedef struct { unsigned width,height,layers,frames; } PdPreviewInfo;
bool pd_preview_file_info(const char *path,uint32_t preview[64*64],unsigned profile,PdPreviewInfo *info,char error[64]);
bool pd_preview_file(const char *path,uint32_t preview[64*64],char error[64]);
bool pd_write_recovery(PdDocument *d,const char *path,char error[64]);
bool pd_open_file(PdDocument *d,const char *path,char error[64]);
/* kind: 0 native PDD8/PDD9/PDDA, 1 Aseprite, 2 transparent PNG. Explicit overwrite only. */
bool pd_write_file(PdDocument *d,const char *path,unsigned kind,bool overwrite,char error[64]);
bool pd_color_parse(const char *text,uint32_t *color);
bool pd_palette_read(const char *path,uint32_t colors[256],unsigned *count,char error[64]);
/* kind: 0 GPL (opaque only), 1 HEX RRGGBBAA, 2 PNG strip */
bool pd_palette_write(const char *path,const uint32_t colors[256],unsigned count,unsigned kind,bool overwrite,char error[64]);
/* operation: 0 rename, 1 verified duplicate, 2 delete regular file */
bool pd_file_manage(const char *source,const char *destination,unsigned operation,char error[64]);
#endif
