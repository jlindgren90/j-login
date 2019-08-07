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

#include <gtk/gtk.h>

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
static ui_t * ui;
static char status[256];
static bool reboot;

static void run_setup (void) {
   static const char * const args[] = {"/usr/sbin/j-login-setup", NULL};
   wait_for_exit (launch (args));
}

static void update_ui (void) {
   if (ui)
      ui_update (ui, status, ! sessions);
}

static bool show_ui (void) {
   if (! ui) {
      ui = ui_create (status, ! sessions);
      if (! ui)
         return false;
   }
   set_vt (first_console->vt);
   return true;
}

static void hide_ui (void) {
   if (! ui)
      return;
   ui_destroy (ui);
   ui = NULL;
}

static void start_session (const char * user, const char * pass) {
   console_t * console;
   if (sessions)
      console = start_x ();
   else {
      hide_ui ();
      set_vt (first_console->vt);
      console = first_console;
   }
   static const char * const args[] = {"/usr/bin/j-session", NULL};
   pid_t process = launch_set_user (user, pass, console->vt, console->display, args);
   NEW (session_t, session, my_strdup (user), console, process);
   sessions = g_list_append (sessions, session);
   active_session = session;
}

static bool try_activate_session (const char * user) {
   for(GList * node = sessions; node; node = node->next) {
      const session_t * session = node->data;
      if (strcmp (session->user, user))
         continue;
      if (session->console == first_console)
         hide_ui ();
      set_vt (session->console->vt);
      active_session = session;
      return true;
   }
   return false;
}

static void end_session (session_t * session) {
   if (session == active_session)
      active_session = NULL;
   if (session->console != first_console)
      close_x (session->console);
   sessions = g_list_remove (sessions, session);
   free (session->user);
   free (session);
}

static int update_cb (void * unused) {
   (void) unused;
   snprintf (status, sizeof status, "Logged in:");
   for (GList * node = sessions; node;) {
      GList * next = node->next;
      session_t * session = node->data;
      if (exited (session->process))
         end_session (session);
      else {
         int length = strlen (status);
         snprintf (status + length, sizeof status - length, " %s", session->user);
      }
      node = next;
   }
   if (! active_session)
      show_ui ();
   update_ui ();
   return G_SOURCE_REMOVE;
}

static int popup_cb (void * unused) {
   (void) unused;
   show_ui ();
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
   if (show_ui ())
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
   update_cb (NULL);
   gtk_main ();
   hide_ui ();
   close_x (first_console);
   set_vt (old_vt);
   close_vt ();
   poweroff ();
   return 0;
}
