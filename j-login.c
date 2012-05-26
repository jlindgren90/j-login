/* J Login >> j-login.c */
/* John Lindgren */
/* September 11, 2011 */

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ck-connector.h>
#include <dbus/dbus.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include "screen.h"
#include "utils.h"

struct session {
   char * user;
   struct console * console;
   CkConnector * ck;
   int process;
};

static char gui = 0;
static struct console * first_console;
static GtkWidget * window, * fixed, * frame, * icon, * pages, * log_in_page,
 * fail_page, * prompt_box, * prompt, * name_entry, * password_entry,
 * log_in_button_box, * log_in_button, * fail_message_box, * fail_message,
 * ok_button_box, * ok_button, * tool_box, * status_bar, * sleep_button,
 * shut_down_button, * reboot_button;
static GList * sessions = 0;
static char sessions_flag = 1;
static int runlevel = 0;

static void set_up_window (void);
static int update_cb (void * unused);

static void run_setup (void) {
   static const char * const args[2] = {"/usr/sbin/j-login-setup", 0};
   wait_for_exit (launch (args));
}

static void make_window (void) {
   window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
   gtk_window_set_decorated ((GtkWindow *) window, 0);
   gtk_window_set_keep_above ((GtkWindow *) window, 1);
   fixed = gtk_fixed_new ();
   frame = gtk_vbox_new (0, 6);
   icon = gtk_image_new_from_file ("/usr/share/pixmaps/j-login.png");
   gtk_box_pack_start ((GtkBox *) frame, icon, 0, 0, 0);
   pages = gtk_hbox_new (0, 6);
   gtk_box_pack_start ((GtkBox *) frame, pages, 1, 0, 0);
   gtk_fixed_put ((GtkFixed *) fixed, frame, 0, 0);
   gtk_container_add ((GtkContainer *) window, fixed);
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
   g_signal_connect_swapped (name_entry, "activate", (GCallback)
    gtk_widget_grab_focus, password_entry);
   log_in_button_box = gtk_hbox_new (0, 6);
   log_in_button = gtk_button_new_with_label ("Log in");
   gtk_widget_set_can_focus (log_in_button, 0);
   gtk_widget_set_can_default (log_in_button, 1);
   gtk_box_pack_end ((GtkBox *) log_in_button_box, log_in_button, 0, 0, 0);
   gtk_box_pack_start ((GtkBox *) log_in_page, log_in_button_box, 0, 0, 0);
   gtk_widget_show_all (log_in_page);
}

static void make_fail_page (void) {
   fail_page = gtk_vbox_new (0, 6);
   gtk_widget_set_no_show_all (fail_page, 1);
   gtk_box_pack_start ((GtkBox *) pages, fail_page, 1, 0, 0);
   fail_message_box = gtk_hbox_new (0, 6);
   fail_message = gtk_label_new ("Authentication failed.");
   gtk_box_pack_start ((GtkBox *) fail_message_box, fail_message, 0, 0, 0);
   gtk_widget_show_all (fail_message_box);
   gtk_box_pack_start ((GtkBox *) fail_page, fail_message_box, 0, 0, 0);
   ok_button_box = gtk_hbox_new (0, 6);
   ok_button = gtk_button_new_with_label ("O.K.");
   gtk_widget_set_can_default (ok_button, 1);
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
   gtk_box_pack_end ((GtkBox *) tool_box, reboot_button, 0, 0, 0);
   shut_down_button = gtk_button_new_with_mnemonic ("Sh_ut down");
   gtk_box_pack_end ((GtkBox *) tool_box, shut_down_button, 0, 0, 0);
   sleep_button = gtk_button_new_with_mnemonic ("_Sleep");
   gtk_box_pack_end ((GtkBox *) tool_box, sleep_button, 0, 0, 0);
   gtk_widget_set_can_focus (reboot_button, 0);
   gtk_widget_set_can_focus (shut_down_button, 0);
   gtk_widget_set_can_focus (sleep_button, 0);
}

static void reset (void) {
   gtk_widget_hide (fail_page);
   gtk_entry_set_text ((GtkEntry *) name_entry, "");
   gtk_entry_set_text ((GtkEntry *) password_entry, "");
   gtk_widget_show (log_in_page);
   gtk_widget_grab_focus (name_entry);
   gtk_widget_grab_default (log_in_button);
}

static void add_session (const char * user, struct console * console,
 CkConnector * ck, int process) {
   struct session * new = my_malloc (sizeof (struct session));
   new->user = my_strdup (user);
   new->console = console;
   new->ck = ck;
   new->process = process;
   sessions = g_list_append (sessions, new);
   sessions_flag = 1;
}

static void remove_session (struct session * session) {
   sessions = g_list_remove (sessions, session);
   free (session->user);
   free (session);
   sessions_flag = 1;
}

static void do_layout (void) {
   GdkScreen * screen = gtk_window_get_screen ((GtkWindow *) window);
   gtk_window_resize ((GtkWindow *) window, gdk_screen_get_width (screen),
    gdk_screen_get_height (screen));
   GdkRectangle rect;
   gdk_screen_get_monitor_geometry (screen, gdk_screen_get_primary_monitor
    (screen), & rect);
   gtk_fixed_move ((GtkFixed *) fixed, frame, rect.x + 6, rect.y + 6);
   gtk_widget_set_size_request (frame, rect.width - 12, rect.height - 12);
}

static char show_window (void) {
   if (! gui) {
      make_window ();
      make_log_in_page ();
      make_fail_page ();
      make_tool_box ();
      set_up_window ();
      update_cb (0);
      gui = 1;
   }
   do_layout ();
   gtk_widget_show_all (window);
   gtk_window_present ((GtkWindow *) window);
   if (! block_x (GDK_WINDOW_XDISPLAY (window->window), GDK_WINDOW_XID
    (window->window))) {
      gtk_widget_hide (window);
      return 0;
   }
   unlock_vt ();
   popup_x (first_console);
   lock_vt ();
   return 1;
}

static void hide_window (void) {
   if (! gui)
      return;
   unblock_x (GDK_WINDOW_XDISPLAY (window->window));
   gtk_widget_hide (window);
}

static CkConnector * start_consolekit (const char * user, struct console *
 console) {
   int32_t id = get_user_id (user);
   if (id < 0)
      error ("unknown user");
   int32_t local = 1;
   char disp[32];
   snprintf (disp, sizeof disp, ":%d", console->display);
   char term[32];
   snprintf (term, sizeof term, "/dev/tty%d", console->vt);
   const char * disp2 = disp, * term2 = term;
   CkConnector * conn = ck_connector_new ();
   if (! conn)
      error ("ck_connector_new failed");
   DBusError err;
   dbus_error_init (& err);
   if (! ck_connector_open_session_with_parameters (conn, & err, "unix-user",
    & id, "is-local", & local, "x11-display", & disp2, "x11-display-device",
    & term2, NULL))
      error ("ck_connector_open_session_with_parameters failed");
   my_setenv ("XDG_SESSION_COOKIE", ck_connector_get_cookie (conn));
   return conn;
}

static void stop_consolekit (CkConnector * connector) {
   DBusError err;
   dbus_error_init (& err);
   ck_connector_close_session (connector, & err);
   ck_connector_unref (connector);
}

static int launch_session (const char * user) {
   static const char * const args [2] = {"/usr/bin/j-session", 0};
   return launch_set_user (user, args);
}

static void start_session (const char * user) {
   struct console * console;
   if (sessions) {
      unlock_vt ();
      console = start_x ();
      lock_vt ();
   } else {
      hide_window ();
      unlock_vt ();
      popup_x (first_console);
      lock_vt ();
      console = first_console;
   }
   set_display (console->display);
   CkConnector * ck = start_consolekit (user, console);
   add_session (user, console, ck, launch_session (user));
}

static int find_session_cb (const struct session * session, const char * user) {
   return strcmp (session->user, user);
}

static char try_activate_session (const char * user) {
   GList * node = g_list_find_custom (sessions, user, (GCompareFunc)
    find_session_cb);
   if (! node)
      return 0;
   const struct session * session = node->data;
   if (session->console == first_console)
      hide_window ();
   unlock_vt ();
   popup_x (session->console);
   lock_vt ();
   return 1;
}

static void end_session (struct session * session) {
   stop_consolekit (session->ck);
   if (session->console != first_console)
      close_x (session->console);
   remove_session (session);
   show_window ();
}

static void check_session_cb (struct session * session) {
   if (exited (session->process))
      end_session (session);
}

static void print_session_cb (const struct session * session, char status[256]) {
   int length = strlen (status);
   snprintf (status + length, sizeof status - length, " %s", session->user);
}

static int update_cb (void * unused) {
   g_list_foreach (sessions, (GFunc) check_session_cb, 0);
   char status[256];
   snprintf (status, sizeof status, "Logged in:");
   g_list_foreach (sessions, (GFunc) print_session_cb, status);
   gtk_label_set_text ((GtkLabel *) status_bar, status);
   if (sessions) {
      gtk_widget_set_sensitive (shut_down_button, 0);
      gtk_widget_set_sensitive (reboot_button, 0);
   } else {
      gtk_widget_set_sensitive (shut_down_button, 1);
      gtk_widget_set_sensitive (reboot_button, 1);
   }
   return 0;
}

static int popup_cb (void * unused) {
   show_window ();
   return 0;
}

static void do_sleep (void) {
   static const char * const args[2] = {"/usr/sbin/j-login-sleep", 0};
   unlock_vt ();
   wait_for_exit (launch (args));
   lock_vt ();
   if (gui)
      reset ();
}

static int popup_and_sleep_cb (void * unused) {
   if (! gui || show_window ())
      do_sleep ();
   return 0;
}

static void * signal_thread (void * unused) {
   sigset_t signals;
   sigemptyset (& signals);
   sigaddset (& signals, SIGCHLD);
   sigaddset (& signals, SIGUSR1);
   sigaddset (& signals, SIGUSR2);
   int signal;
   while (! sigwait (& signals, & signal)) {
      if (signal == SIGCHLD)
         g_timeout_add (0, update_cb, 0);
      else if (signal == SIGUSR1)
         g_timeout_add (0, popup_cb, 0);
      else if (signal == SIGUSR2)
         g_timeout_add (0, popup_and_sleep_cb, 0);
   }
   fail ("sigwait");
   return 0;
}

static void start_signal_thread (void) {
   sigset_t signals;
   sigemptyset (& signals);
   sigaddset (& signals, SIGCHLD);
   sigaddset (& signals, SIGUSR1);
   sigaddset (& signals, SIGUSR2);
   if (sigprocmask (SIG_SETMASK, & signals, 0) < 0)
      fail ("sigprocmask");
   pthread_t thread;
   if (pthread_create (& thread, 0, signal_thread, 0) < 0)
      fail ("pthread_create");
}

static void log_in (void) {
   char * name = my_strdup (gtk_entry_get_text ((GtkEntry *) name_entry));
   char * password = my_strdup (gtk_entry_get_text ((GtkEntry *) password_entry));
   reset ();
   if (strcmp (name, "root") && check_password (name, password)) {
      if (! try_activate_session (name)) {
         start_session (name);
         update_cb (0);
      }
   } else {
      gtk_widget_hide (log_in_page);
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

static void set_up_window (void) {
   g_signal_connect (log_in_button, "clicked", (GCallback) log_in, 0);
   g_signal_connect (ok_button, "clicked", (GCallback) reset, 0);
   g_signal_connect (sleep_button, "clicked", (GCallback) do_sleep, 0);
   g_signal_connect (shut_down_button, "clicked", (GCallback) gtk_main_quit, 0);
   g_signal_connect (reboot_button, "clicked", (GCallback) reboot, 0);
   gtk_widget_grab_focus (name_entry);
   gtk_widget_grab_default (log_in_button);
}

static void change_runlevel (void) {
   char name[16];
   snprintf (name, sizeof name, "%d", runlevel);
   const char * const args[3] = {"init", name, 0};
   wait_for_exit (launch (args));
}

int main (int argc, char * * argv) {
   set_user ("root");
   init_vt ();
   int old_vt = get_vt ();
   first_console = start_x ();
   lock_vt ();
   set_display (first_console->display);
   run_setup ();
   gtk_init (0, 0);
   start_signal_thread ();
   if (argc == 2)
      start_session (argv[1]);
   else
      show_window ();
   gtk_main ();
   if (gui)
      gtk_widget_destroy (window);
   close_x (first_console);
   unlock_vt ();
   set_vt (old_vt);
   close_vt ();
   change_runlevel ();
   return 0;
}
