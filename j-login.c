/* J Login >> j-login.c */
/* John Lindgren */
/* May 15, 2010 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <locale.h>
#include <pthread.h>
#include <pwd.h>
#include <shadow.h>
#include <signal.h>
#include <unistd.h>

char * crypt (const char * key, const char * salt);

#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h>
#include <glib.h>
#include <gtk/gtk.h>

#include "screen.h"
#include "utils.h"

struct session {
   char * name;
   struct console * console;
   int process;
   struct session * prev, * next;
};

static struct console * first_console;
static GtkWidget * window, * frame, * icon, * pages, * log_in_page, * fail_page,
 * prompt_box, * prompt, * name_entry, * password_entry, * log_in_button_box,
 * log_in_button, * fail_message_box, * fail_message, * ok_button_box,
 * ok_button, * tool_box, * status_bar, * sleep_button, * shut_down_button,
 * reboot_button;
static struct session * sessions = 0;
static char sessions_flag = 1;
static int runlevel = 0;

static void run_setup (void) {
  char * args [3];
  int process;
   args [0] = "/bin/sh";
   args [1] = "/usr/bin/j-login-setup";
   args [2] = 0;
   process = launch (args);
   wait_for_exit (process);
}

static void make_window (void) {
  GdkScreen * screen;
  int monitor;
  GdkRectangle rect;
   window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
   gtk_window_set_decorated ((GtkWindow *) window, 0);
   gtk_window_set_keep_above ((GtkWindow *) window, 1);
   screen = gtk_window_get_screen ((GtkWindow *) window);
#if GTK_CHECK_VERSION (2, 20, 0)
   monitor = gdk_screen_get_primary_monitor (screen);
#else
   monitor = 0;
#endif
   gdk_screen_get_monitor_geometry (screen, monitor, & rect);
   gtk_window_move ((GtkWindow *) window, rect.x, rect.y);
   gtk_window_set_default_size ((GtkWindow *) window, rect.width, rect.height);
   gtk_container_set_border_width ((GtkContainer *) window, 6);
   frame = gtk_vbox_new (0, 6);
   icon = gtk_image_new_from_file ("/usr/share/pixmaps/j-login.png");
   gtk_box_pack_start ((GtkBox *) frame, icon, 0, 0, 0);
   pages = gtk_hbox_new (0, 6);
   gtk_box_pack_start ((GtkBox *) frame, pages, 1, 0, 0);
   gtk_container_add ((GtkContainer *) window, frame);
   gtk_widget_show_all (frame);
}

static void make_log_in_page (void) {
   log_in_page = gtk_vbox_new (0, 6);
   gtk_box_pack_start ((GtkBox *) pages, log_in_page, 1, 0, 0);
   prompt_box = gtk_hbox_new (0, 6);
   prompt = gtk_label_new ("Name and password:");
   gtk_box_pack_start ((GtkBox *) prompt_box, prompt, 0, 0, 0);
   gtk_box_pack_start ((GtkBox *) log_in_page, prompt_box, 0, 0, 0);
   name_entry = gtk_entry_new ();
   gtk_box_pack_start ((GtkBox *) log_in_page, name_entry, 0, 0, 0);
   password_entry = gtk_entry_new ();
   gtk_entry_set_visibility ((GtkEntry *) password_entry, 0);
   gtk_entry_set_activates_default ((GtkEntry *) password_entry, 1);
   gtk_box_pack_start ((GtkBox *) log_in_page, password_entry, 0, 0, 0);
   log_in_button_box = gtk_hbox_new (0, 6);
   log_in_button = gtk_button_new_with_label ("Log in");
   GTK_WIDGET_UNSET_FLAGS (log_in_button, GTK_CAN_FOCUS);
   GTK_WIDGET_SET_FLAGS (log_in_button, GTK_CAN_DEFAULT);
   gtk_box_pack_end ((GtkBox *) log_in_button_box, log_in_button, 0, 0, 0);
   gtk_box_pack_start ((GtkBox *) log_in_page, log_in_button_box, 0, 0, 0);
   gtk_widget_show_all (log_in_page);
}

static void make_fail_page (void) {
   fail_page = gtk_vbox_new (0, 6);
   gtk_widget_set_no_show_all (fail_page, 1);
   gtk_box_pack_start ((GtkBox *) pages, fail_page, 1, 0, 0);
   fail_message_box = gtk_hbox_new (0, 6);
   fail_message = gtk_label_new ("");
   gtk_box_pack_start ((GtkBox *) fail_message_box, fail_message, 0, 0, 0);
   gtk_widget_show_all (fail_message_box);
   gtk_box_pack_start ((GtkBox *) fail_page, fail_message_box, 0, 0, 0);
   ok_button_box = gtk_hbox_new (0, 6);
   ok_button = gtk_button_new_with_label ("O.K.");
   GTK_WIDGET_SET_FLAGS (ok_button, GTK_CAN_DEFAULT);
   gtk_box_pack_end ((GtkBox *) ok_button_box, ok_button, 0, 0, 0);
   gtk_widget_show_all (ok_button_box);
   gtk_box_pack_start ((GtkBox *) fail_page, ok_button_box, 0, 0, 0);
}

static void make_tool_box (void) {
   tool_box = gtk_hbox_new (0, 6);
   gtk_box_pack_start ((GtkBox *) frame, tool_box, 0, 0, 0);
   status_bar = gtk_label_new ("");
   gtk_box_pack_start ((GtkBox *) tool_box, status_bar, 0, 0, 0);
   reboot_button = gtk_button_new_with_mnemonic ("_Reboot");
   GTK_WIDGET_UNSET_FLAGS (reboot_button, GTK_CAN_FOCUS);
   gtk_box_pack_end ((GtkBox *) tool_box, reboot_button, 0, 0, 0);
   shut_down_button = gtk_button_new_with_mnemonic ("Sh_ut down");
   GTK_WIDGET_UNSET_FLAGS (shut_down_button, GTK_CAN_FOCUS);
   gtk_box_pack_end ((GtkBox *) tool_box, shut_down_button, 0, 0, 0);
   sleep_button = gtk_button_new_with_mnemonic ("_Sleep");
   GTK_WIDGET_UNSET_FLAGS (sleep_button, GTK_CAN_FOCUS);
   gtk_box_pack_end ((GtkBox *) tool_box, sleep_button, 0, 0, 0);
   gtk_widget_show_all (tool_box);
}

static void reset (void) {
   gtk_widget_hide (fail_page);
   gtk_entry_set_text ((GtkEntry *) name_entry, "");
   gtk_entry_set_text ((GtkEntry *) password_entry, "");
   gtk_widget_show (log_in_page);
   gtk_widget_grab_focus (name_entry);
   gtk_widget_grab_default (log_in_button);
}

static char is_right_password (char * name, char * password) {
  char * should_be, * compare;
  struct passwd * user_data;
  struct spwd * hidden_data;
   user_data = getpwnam (name);
   if (! user_data)
      return 0;
   should_be = user_data->pw_passwd;
   if (! strcmp (should_be, "x")) {
      hidden_data = getspnam (name);
      if (! hidden_data)
         return 0;
      should_be = hidden_data->sp_pwdp;
   }
   compare = crypt (password, should_be);
   if (! compare)
      return 0;
   return ! strcmp (compare, should_be);
}

static void add_session (char * name, struct console * console, int process) {
  struct session * search, * new;
   new = my_malloc (sizeof (struct session));
   new->name = my_strdup (name);
   new->console = console;
   new->process = process;
   new->next = 0;
   if (sessions) {
      for (search = sessions; search->next; search = search->next);
      search->next = new;
      new->prev = search;
   } else {
      sessions = new;
      new->prev = 0;
   }
   sessions_flag = 1;
}

static void remove_session (struct session * session) {
   if (session->prev)
      session->prev->next = session->next;
   else
      sessions = session->next;
   if (session->next)
      session->next->prev = session->prev;
   free (session->name);
   free (session);
   sessions_flag = 1;
}

static char show_window (void)
{
   gtk_widget_show_all (window);
   gtk_window_present ((GtkWindow *) window);
   if (! block_x (GDK_WINDOW_XDISPLAY (window->window), GDK_WINDOW_XID
    (window->window))) {
      gtk_widget_hide (window);
      return 0;
   }
   popup_x (first_console);
   return 1;
}

static void hide_window (void)
{
   unblock_x (GDK_WINDOW_XDISPLAY (window->window));
   gtk_widget_hide (window);
}

static int launch_session (char * user) {
  char * args [3];
   args [0] = "/bin/sh";
   args [1] = "/usr/bin/j-session";
   args [2] = 0;
   return launch_set_user (user, args);
}

static void start_session (char * name)
{
  struct console * console;
   if (sessions)
      console = start_x ();
   else {
      hide_window ();
      popup_x (first_console);
      console = first_console;
   }
   set_display (console->display);
   add_session (name, console, launch_session (name));
}

static char try_activate_session (char * name) {
  struct session * session;
   for (session = sessions; session; session = session->next) {
      if (strcmp (session->name, name))
         continue;
      if (session->console == first_console)
         hide_window ();
      popup_x (session->console);
      return 1;
   }
   return 0;
}

static void end_session (struct session * session)
{
   if (session->console != first_console)
      close_x (session->console);
   remove_session (session);
   show_window ();
}

static int update (void * unused) {
  int length;
  struct session * session, * next;
  char work [256];
   for (session = sessions; session; ) {
      next = session->next;
      if (exited (session->process)) {
         end_session (session);
      }
      session = next;
   }
   snprintf (work, sizeof work, "Logged in:");
   for (session = sessions; session; session = session->next) {
      length = strlen (work);
      snprintf (work + length, sizeof work - length, " %s", session->name);
   }
   gtk_label_set_text ((GtkLabel *) status_bar, work);
   if (sessions) {
      gtk_widget_set_sensitive (shut_down_button, 0);
      gtk_widget_set_sensitive (reboot_button, 0);
   } else {
      gtk_widget_set_sensitive (shut_down_button, 1);
      gtk_widget_set_sensitive (reboot_button, 1);
   }
   return 0;
}

static int popup (void * unused)
{
   show_window ();
   return 0;
}

static void my_sleep (void) {
  char * args [3];
  int process;
   args [0] = "/bin/sh";
   args [1] = "/usr/bin/j-login-sleep";
   args [2] = 0;
   process = launch (args);
   wait_for_exit (process);
   reset ();
}

static int popup_sleep (void * unused)
{
   if (show_window ())
      my_sleep ();
   return 0;
}

static void * signal_thread (void * unused) {
  sigset_t signals;
  int signal;
   sigemptyset (& signals);
   sigaddset (& signals, SIGCHLD);
   sigaddset (& signals, SIGUSR1);
   sigaddset (& signals, SIGUSR2);
   while (! sigwait (& signals, & signal)) {
      if (signal == SIGCHLD) {
         g_timeout_add (0, update, 0);
      }
      else if (signal == SIGUSR1)
         g_timeout_add (0, popup, 0);
      else if (signal == SIGUSR2)
         g_timeout_add (0, popup_sleep, 0);
   }
   fail ("sigwait");
   return 0;
}

static void start_signal_thread (void) {
  sigset_t signals;
  pthread_t thread;
   sigemptyset (& signals);
   sigaddset (& signals, SIGCHLD);
   sigaddset (& signals, SIGUSR1);
   sigaddset (& signals, SIGUSR2);
   if (sigprocmask (SIG_SETMASK, & signals, 0) < 0)
      fail ("sigprocmask");
   if (pthread_create (& thread, 0, signal_thread, 0) < 0)
      fail ("pthread_create");
}

static void log_in (void) {
  char * name, * password;
   name = my_strdup ((char *) gtk_entry_get_text ((GtkEntry *) name_entry));
   password = my_strdup ((char *) gtk_entry_get_text ((GtkEntry *)
    password_entry));
   reset ();
   if (is_right_password (name, password)) {
      if (! try_activate_session (name)) {
         start_session (name);
         update (0);
      }
   } else {
      gtk_widget_hide (log_in_page);
      gtk_label_set_text ((GtkLabel *) fail_message, "Wrong name or password.\n");
      gtk_widget_show (fail_page);
      gtk_widget_grab_focus (ok_button);
      gtk_widget_grab_default (ok_button);
   }
   free (name);
   free (password);
}

static void reboot (void) {
   runlevel = 6;
   gtk_main_quit ();
}

/* Switching consoles while a key is held causes trouble with X, so avoid it. */
static int key_press (GtkWidget * widget, GdkEventKey * event, void * unused) {
   if (event->keyval != GDK_Return)
      return 0;
   return 1;
}

static int key_release (GtkWidget * widget, GdkEventKey * event, void * unused) {
   if (event->keyval != GDK_Return)
      return 0;
   if (GTK_WIDGET_HAS_FOCUS (name_entry))
      gtk_widget_grab_focus (password_entry);
   else if (GTK_WIDGET_VISIBLE (log_in_page))
      log_in ();
   else
      reset ();
   return 1;
}

static void set_up_window (void) {
   g_signal_connect (window, "key-press-event", (GCallback) key_press, 0);
   g_signal_connect (window, "key-release-event", (GCallback) key_release, 0);
   g_signal_connect (log_in_button, "clicked", (GCallback) log_in, 0);
   g_signal_connect (ok_button, "clicked", (GCallback) reset, 0);
   g_signal_connect (sleep_button, "clicked", (GCallback) my_sleep, 0);
   g_signal_connect ((GObject *) shut_down_button, "clicked",
    (GCallback) gtk_main_quit, 0);
   g_signal_connect ((GObject *) reboot_button, "clicked", (GCallback) reboot, 0);
   gtk_widget_grab_focus (name_entry);
   gtk_widget_grab_default (log_in_button);
}

static void change_runlevel (void) {
  char * args [3];
  char work [256];
   snprintf (work, sizeof work, "%d", runlevel);
   args [0] = "init";
   args [1] = work;
   args [2] = 0;
   launch (args);
}

int main (void) {
   set_user ("root");
   my_setenv ("LANG", "en_US.UTF-8");
   if (! setlocale (LC_ALL, ""))
      fail_two ("setlocale", "en_US.UTF-8");
   first_console = start_x ();
   set_display (first_console->display);
   run_setup ();
   g_thread_init (NULL);
   gtk_init (0, 0);
   make_window ();
   make_log_in_page ();
   make_fail_page ();
   make_tool_box ();
   set_up_window ();
   show_window ();
   update (0);
   start_signal_thread ();
   gtk_main ();
   gtk_widget_destroy (window);
   close_x (first_console);
   change_runlevel ();
   return 0;
}
