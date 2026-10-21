#ifndef PD_DROPDOWN_H
#define PD_DROPDOWN_H
#include <stdbool.h>

typedef enum {
    PD_DROPDOWN_NONE,
    PD_DROPDOWN_CLOSED,
    PD_DROPDOWN_SELECTED,
    PD_DROPDOWN_SCROLLED
} PdDropdownEvent;

typedef struct {
    bool open, ignore_open_touch, tracking, dragging, press_inside;
    int x, y, w, header_h, row_h, visible_rows;
    int down_x, down_y;
    unsigned count, selected, first, down_first;
} PdDropdown;

void pd_dropdown_open(PdDropdown *menu, int x, int y, int w, int header_h,
                      int row_h, int visible_rows, unsigned count,
                      unsigned selected, bool ignore_open_touch);
void pd_dropdown_close(PdDropdown *menu);
/* Called once per input batch while open. It consumes the complete touch,
   including outside taps, so an underlying gallery control cannot fire. */
PdDropdownEvent pd_dropdown_input(PdDropdown *menu, bool touch, bool pressed,
                                  int x, int y, unsigned *selected);
void pd_dropdown_scroll(PdDropdown *menu, int rows);
#endif
