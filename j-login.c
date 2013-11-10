/* J Login >> j-login.c */
/* John Lindgren */
/* September 17, 2012 */

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include "pam.h"
#include "screen.h"
#include "utils.h"

struct session {
   char * user;
   struct console * console;
   void * pam_handle;
   int process;
};

static struct console * first_console;
static GtkWidget * window, * fixed, * frame, * pages, * log_in_page,
 * fail_page, * name_entry, * password_entry, * log_in_button, * ok_button,
 * status_bar, * sleep_button, * shut_down_button, * reboot_button;
static const struct session * active_session;
static GList * sessions;
static char reboot;

static void set_up_window (void);
static int update_cb (void * unused);

static void run_setup (void) {
   static const char * const args[] = {"/usr/sbin/j-login-setup", 0};
   wait_for_exit (launch (args));
}

static void make_window (void) {
   window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
   gtk_window_set_decorated ((GtkWindow *) window, 0);
   gtk_window_set_keep_above ((GtkWindow *) window, 1);
   gtk_window_set_has_resize_grip ((GtkWindow *) window, 0);
   fixed = gtk_fixed_new ();
   frame = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
   GtkWidget * icon = gtk_image_new_from_file ("/usr/share/pixmaps/j-login.png");
   gtk_box_pack_start ((GtkBox *) frame, icon, 0, 0, 0);
   pages = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
   gtk_box_pack_start ((GtkBox *) frame, pages, 1, 0, 0);
   gtk_fixed_put ((GtkFixed *) fixed, frame, 0, 0);
   gtk_container_add ((GtkContainer *) window, fixed);
}

static void make_log_in_page (void) {
   log_in_page = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
   gtk_box_pack_start ((GtkBox *) pages, log_in_page, 1, 0, 0);
   GtkWidget * prompt_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
   GtkWidget * prompt = gtk_label_new ("Name and password:");
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
   GtkWidget * log_in_button_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
   log_in_button = gtk_button_new_with_label ("Log in");
   gtk_widget_set_can_focus (log_in_button, 0);
   gtk_widget_set_can_default (log_in_button, 1);
   gtk_box_pack_end ((GtkBox *) log_in_button_box, log_in_button, 0, 0, 0);
   gtk_box_pack_start ((GtkBox *) log_in_page, log_in_button_box, 0, 0, 0);
   gtk_widget_show_all (log_in_page);
}

static void make_fail_page (void) {
   fail_page = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
   gtk_widget_set_no_show_all (fail_page, 1);
   gtk_box_pack_start ((GtkBox *) pages, fail_page, 1, 0, 0);
   GtkWidget * fail_message_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
   GtkWidget * fail_message = gtk_label_new ("Authentication failed.");
   gtk_box_pack_start ((GtkBox *) fail_message_box, fail_message, 0, 0, 0);
   gtk_widget_show_all (fail_message_box);
   gtk_box_pack_start ((GtkBox *) fail_page, fail_message_box, 0, 0, 0);
   GtkWidget * ok_button_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
   ok_button = gtk_button_new_with_label ("O.K.");
   gtk_widget_set_can_default (ok_button, 1);
   gtk_box_pack_end ((GtkBox *) ok_button_box, ok_button, 0, 0, 0);
   gtk_widget_show_all (ok_button_box);
   gtk_box_pack_start ((GtkBox *) fail_page, ok_button_box, 0, 0, 0);
}

static void make_tool_box (void) {
   GtkWidget * tool_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
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

static struct session * add_session (const char * user,
 struct console * console, void * pam_handle, int process) {
   struct session * new = my_malloc (sizeof (struct session));
   new->user = my_strdup (user);
   new->console = console;
   new->pam_handle = pam_handle;
   new->process = process;
   sessions = g_list_append (sessions, new);
   return new;
}

static void remove_session (struct session * session) {
   sessions = g_list_remove (sessions, session);
   free (session->user);
   free (session);
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
   do_layout ();
   reset ();
   gtk_widget_show_all (window);
   gtk_window_present ((GtkWindow *) window);
   GdkWindow * gdkw = gtk_widget_get_window (window);
   if (! block_x (GDK_WINDOW_XDISPLAY (gdkw), GDK_WINDOW_XID (gdkw))) {
      gtk_widget_hide (window);
      return 0;
   }
   unlock_vt ();
   popup_x (first_console);
   lock_vt ();
   return 1;
}

static void hide_window (void) {
   GdkWindow * gdkw = gtk_widget_get_window (window);
   unblock_x (GDK_WINDOW_XDISPLAY (gdkw));
   gtk_widget_hide (window);
}

static int launch_session (const char * user) {
   static const char * const args[] = {"/usr/bin/j-session", 0};
   return launch_set_user (user, args);
}

static void start_session (const char * user, const char * pass) {
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
   void * pam_handle = open_pam (user, pass, console->vt, console->display);
   active_session = add_session (user, console, pam_handle, launch_session (user));
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
   active_session = session;
   return 1;
}

static void end_session (struct session * session) {
   if (session->console != first_console)
      close_x (session->console);
   close_pam (session->pam_handle);
   remove_session (session);
   if (session == active_session) {
      active_session = 0;
      show_window ();
   }
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
   static const char * const args[] = {"/usr/sbin/j-login-sleep", 0};
   while (gtk_events_pending ())
      gtk_main_iteration ();
   unlock_vt ();
   wait_for_exit (launch (args));
   lock_vt ();
}

static int sleep_cb (void * unused) {
   if (show_window ())
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
         g_timeout_add (0, sleep_cb, 0);
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
         start_session (name, password);
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

static void queue_reboot (void) {
   reboot = 1;
   gtk_main_quit ();
}

static void set_up_window (void) {
   g_signal_connect (log_in_button, "clicked", (GCallback) log_in, 0);
   g_signal_connect (ok_button, "clicked", (GCallback) reset, 0);
   g_signal_connect (sleep_button, "clicked", (GCallback) do_sleep, 0);
   g_signal_connect (shut_down_button, "clicked", (GCallback) gtk_main_quit, 0);
   g_signal_connect (reboot_button, "clicked", (GCallback) queue_reboot, 0);
   gtk_widget_grab_focus (name_entry);
   gtk_widget_grab_default (log_in_button);
}

static void set_up_interface (void) {
   make_window ();
   make_log_in_page ();
   make_fail_page ();
   make_tool_box ();
   set_up_window ();
   update_cb (0);
}

static void poweroff ()
{
   const char * const poweroff_args[] = {"poweroff", 0};
   const char * const reboot_args[] = {"reboot", 0};
   wait_for_exit (launch (reboot ? reboot_args : poweroff_args));
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
   set_up_interface ();
   show_window ();
   gtk_main ();
   gtk_widget_destroy (window);
   close_x (first_console);
   unlock_vt ();
   set_vt (old_vt);
   close_vt ();
   poweroff ();
   return 0;
}
