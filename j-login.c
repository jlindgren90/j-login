/*
 * J-Login - j-login.c
 * Copyright 2010-2015 John Lindgren
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    provided with the distribution.
 *
 * This software is provided "as is" and without any warranty, express or
 * implied. In no event shall the authors be liable for any damages arising from
 * the use of this software.
 */

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "actions.h"
#include "screen.h"
#include "ui.h"
#include "utils.h"

typedef struct {
   char * user;
   console_t * console;
   pid_t process;
} session_t;

static console_t * first_console;
static const session_t * active_session;
static GList * sessions;
static bool reboot;
static ui_t ui;

static int update_cb (void * unused);

static void run_setup (void) {
   static const char * const args[] = {"/usr/sbin/j-login-setup", NULL};
   wait_for_exit (launch (args));
}

static session_t * add_session (const char * user, console_t * console, pid_t process) {
   NEW (session_t, session, my_strdup (user), console, process);
   sessions = g_list_append (sessions, session);
   return session;
}

static void remove_session (session_t * session) {
   sessions = g_list_remove (sessions, session);
   free (session->user);
   free (session);
}

static bool show_window (void) {
   if (! ui_show (& ui))
      return false;
   popup_x (first_console);
   return true;
}

static pid_t launch_session (const char * user, const char * pass, console_t * console) {
   static const char * const args[] = {"/usr/bin/j-session", NULL};
   return launch_set_user (user, pass, console->vt, console->display, args);
}

static void start_session (const char * user, const char * pass) {
   console_t * console;
   if (sessions)
      console = start_x ();
   else {
      ui_hide (& ui);
      popup_x (first_console);
      console = first_console;
   }
   pid_t process = launch_session (user, pass, console);
   active_session = add_session (user, console, process);
}

static int find_session_cb (const session_t * session, const char * user) {
   return strcmp (session->user, user);
}

static bool try_activate_session (const char * user) {
   GList * node = g_list_find_custom (sessions, user, (GCompareFunc) find_session_cb);
   if (! node)
      return false;
   const session_t * session = node->data;
   if (session->console == first_console)
      ui_hide (& ui);
   popup_x (session->console);
   active_session = session;
   return true;
}

static void end_session (session_t * session) {
   if (session->console != first_console)
      close_x (session->console);
   remove_session (session);
   if (session == active_session) {
      active_session = NULL;
      show_window ();
   }
}

static void check_session_cb (session_t * session, void * unused) {
   (void) unused;
   if (exited (session->process))
      end_session (session);
}

static void print_session_cb (const session_t * session, char * status) {
   int length = strlen (status);
   snprintf (status + length, 256 - length, " %s", session->user);
}

static int update_cb (void * unused) {
   (void) unused;
   g_list_foreach (sessions, (GFunc) check_session_cb, NULL);
   char status[256];
   snprintf (status, sizeof status, "Logged in:");
   g_list_foreach (sessions, (GFunc) print_session_cb, status);
   ui_set_status (& ui, status);
   ui_set_can_quit (& ui, ! sessions);
   return G_SOURCE_REMOVE;
}

static int popup_cb (void * unused) {
   (void) unused;
   show_window ();
   return G_SOURCE_REMOVE;
}

void do_sleep (void) {
   static const char * const args[] = {"/usr/sbin/j-login-sleep", NULL};
   while (gtk_events_pending ())
      gtk_main_iteration ();
   wait_for_exit (launch (args));
}

static int sleep_cb (void * unused) {
   (void) unused;
   if (show_window ())
      do_sleep ();
   return G_SOURCE_REMOVE;
}

static void * signal_thread (void * unused) {
   (void) unused;
   sigset_t signals;
   sigemptyset (& signals);
   sigaddset (& signals, SIGCHLD);
   sigaddset (& signals, SIGUSR1);
   sigaddset (& signals, SIGUSR2);
   int signal;
   while (! sigwait (& signals, & signal)) {
      if (signal == SIGCHLD)
         g_timeout_add (0, update_cb, NULL);
      else if (signal == SIGUSR1)
         g_timeout_add (0, popup_cb, NULL);
      else if (signal == SIGUSR2)
         g_timeout_add (0, sleep_cb, NULL);
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
   if (sigprocmask (SIG_SETMASK, & signals, NULL) < 0)
      fail ("sigprocmask");
   pthread_t thread;
   if (pthread_create (& thread, NULL, signal_thread, NULL) < 0)
      fail ("pthread_create");
}

bool log_in (const char * name, const char * password) {
   if (! strcmp (name, "root") || ! check_password (name, password))
      return false;
   if (try_activate_session (name))
      return true;
   start_session (name, password);
   update_cb (NULL);
   return true;
}

void queue_reboot (void) {
   reboot = true;
   gtk_main_quit ();
}

void queue_shutdown (void) {
   gtk_main_quit ();
}

static void poweroff ()
{
   const char * const poweroff_args[] = {"poweroff", NULL};
   const char * const reboot_args[] = {"reboot", NULL};
   wait_for_exit (launch (reboot ? reboot_args : poweroff_args));
}

int main (void) {
   set_user ("root");
   init_vt ();
   int old_vt = get_vt ();
   first_console = start_x ();
   set_display (first_console->display);
   gtk_init (NULL, NULL);
   run_setup ();
   start_signal_thread ();
   ui_create (& ui);
   update_cb (NULL);
   show_window ();
   gtk_main ();
   ui_destroy (& ui);
   close_x (first_console);
   set_vt (old_vt);
   close_vt ();
   poweroff ();
   return 0;
}
