// TODO: clean this garbage up.

#ifndef STATES
#define STATES

extern bool held_w;
extern bool held_a;
extern bool held_s;
extern bool held_d;
extern bool held_space;
extern bool held_ctrl;
extern bool held_q;
extern bool held_e;
extern bool held_x;

extern bool toggle_r;
extern bool toggle_t;

extern bool toggle_c;

extern glm::vec2 current_cursor_position;
extern glm::vec2 cursor_clicked_at;
extern bool held_click;
extern bool moving_cursor;

extern glm::vec2 right_click_lock_at;
extern bool rightClickLock;

#endif