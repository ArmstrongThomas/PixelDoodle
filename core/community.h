#ifndef PD_COMMUNITY_H
#define PD_COMMUNITY_H
#include <stdbool.h>
#include <stddef.h>
#define PD_COMMUNITY_PAGE 6
typedef struct {
    char id[65];
    char title[65];
    char artist[49];
    char status[17];
    char reason[97];
    unsigned width, height;
    bool favorite;
} PdCommunityArt;
typedef struct {
    PdCommunityArt items[PD_COMMUNITY_PAGE];
    unsigned count, total, next_offset;
    bool has_total, has_next, profile_required;
} PdCommunityPage;
/* Parse only the bounded public gallery/favorites page contract. Reject malformed,
   escaped-control, overlong, or structurally unexpected fields. */
bool pd_community_parse_page(const char *json, size_t size, PdCommunityPage *page);
#endif
