/* J Login >> screen.h */
/* John Lindgren */
/* July 12, 2010 */

#include <X11/Xlib.h>

struct console
{
   int vt, display, process;
   Display * handle;
};

void init_vt (void);
void close_vt (void);
int get_vt (void);
int get_open_vt (void);
void set_vt (int vt);
void lock_vt (void);
void unlock_vt (void);

struct console * start_x (void);
void popup_x (const struct console * console);
void close_x (struct console * console);
void set_display (int display);
char block_x (Display * handle, Window window);
void unblock_x (Display * handle);
