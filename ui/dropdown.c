#include "dropdown.h"
#include <string.h>

static unsigned max_first(const PdDropdown *m) {
    return m->count > (unsigned)m->visible_rows
        ? m->count - (unsigned)m->visible_rows : 0;
}

static void clamp(PdDropdown *m) {
    unsigned last = max_first(m);
    if (m->first > last) m->first = last;
}

void pd_dropdown_open(PdDropdown *m, int x, int y, int w, int header_h,
                      int row_h, int visible_rows, unsigned count,
                      unsigned selected, bool ignore_open_touch) {
    memset(m, 0, sizeof(*m));
    m->open = true;
    m->ignore_open_touch = ignore_open_touch;
    m->x = x; m->y = y; m->w = w;
    m->header_h = header_h > 0 ? header_h : 1;
    m->row_h = row_h > 0 ? row_h : 1;
    m->visible_rows = visible_rows > 0 ? visible_rows : 1;
    m->count = count;
    m->selected = selected < count ? selected : 0;
    if (m->selected >= (unsigned)m->visible_rows)
        m->first = m->selected - (unsigned)m->visible_rows + 1;
    clamp(m);
}

void pd_dropdown_close(PdDropdown *m) {
    m->open = false;
    m->ignore_open_touch = false;
    m->tracking = false;
    m->dragging = false;
    m->press_inside = false;
}

void pd_dropdown_scroll(PdDropdown *m, int rows) {
    if (!m->open || !rows) return;
    int next = (int)m->first + rows;
    if (next < 0) next = 0;
    m->first = (unsigned)next;
    clamp(m);
}

PdDropdownEvent pd_dropdown_input(PdDropdown *m, bool touch, bool pressed,
                                  int x, int y, unsigned *selected) {
    if (!m->open) return PD_DROPDOWN_NONE;
    if (m->ignore_open_touch) {
        if (!touch) m->ignore_open_touch = false;
        return PD_DROPDOWN_NONE;
    }
    if (pressed) {
        m->tracking = true;
        m->dragging = false;
        m->press_inside = x >= m->x && x < m->x + m->w &&
            y >= m->y && y < m->y + m->header_h +
                m->row_h * m->visible_rows;
        m->down_x = x;
        m->down_y = y;
        m->down_first = m->first;
        return PD_DROPDOWN_NONE;
    }
    if (touch && m->tracking) {
        int dy = y - m->down_y;
        if (!m->dragging && (dy > 7 || dy < -7) && m->press_inside)
            m->dragging = true;
        if (m->dragging) {
            int rows = (m->down_y - y) / m->row_h;
            int next = (int)m->down_first + rows;
            if (next < 0) next = 0;
            unsigned before = m->first;
            m->first = (unsigned)next;
            clamp(m);
            return before == m->first ? PD_DROPDOWN_NONE : PD_DROPDOWN_SCROLLED;
        }
        return PD_DROPDOWN_NONE;
    }
    if (!touch && m->tracking) {
        PdDropdownEvent event = PD_DROPDOWN_CLOSED;
        int release_x=m->down_x,release_y=m->down_y;
        if (m->press_inside && !m->dragging && release_x >= m->x && release_x < m->x + m->w &&
            release_y >= m->y + m->header_h && release_y < m->y + m->header_h +
                m->row_h * m->visible_rows) {
            unsigned row = (unsigned)(release_y - m->y - m->header_h) /
                (unsigned)m->row_h;
            unsigned item = m->first + row;
            if (item < m->count) {
                m->selected = item;
                if (selected) *selected = item;
                event = PD_DROPDOWN_SELECTED;
            }
        } else if (m->dragging) {
            event = PD_DROPDOWN_SCROLLED;
        } else if (m->press_inside && release_x >= m->x && release_x < m->x + m->w &&
                   release_y >= m->y && release_y < m->y + m->header_h) {
            m->tracking = false;
            m->press_inside = false;
            return PD_DROPDOWN_NONE;
        }
        m->tracking = false;
        m->dragging = false;
        m->press_inside = false;
        if (event != PD_DROPDOWN_SCROLLED) pd_dropdown_close(m);
        return event;
    }
    return PD_DROPDOWN_NONE;
}
