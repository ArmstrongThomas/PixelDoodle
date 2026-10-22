#ifndef PD_SHARE_H
#define PD_SHARE_H
#include <stdbool.h>
#include <stdio.h>
static inline int pd_share_prefix(char *out,size_t size,bool gif){
    return snprintf(out,size,"--PDShareBoundary7db\r\nContent-Disposition: form-data; name=\"payload_json\"\r\nContent-Type: application/json\r\n\r\n{\"content\":\"Pixel Doodle %s\",\"allowed_mentions\":{\"parse\":[]}}\r\n--PDShareBoundary7db\r\nContent-Disposition: form-data; name=\"files[0]\"; filename=\"PixelDoodle.%s\"\r\nContent-Type: image/%s\r\n\r\n",gif?"animation":"sprite",gif?"gif":"png",gif?"gif":"png");
}
#endif
