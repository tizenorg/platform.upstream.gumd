/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of gumd
 *
 * Copyright (C) 2012 - 2013 Intel Corporation.
 *
 * Contact: Amarnath Valluri <amarnath.valluri@linux.intel.com>
 *          Imran Zaman <imran.zaman@intel.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include "config.h"

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <glib-unix.h>
#include <glib.h>
#include <grp.h>
#include <gio/gio.h>

#include "common/gum-log.h"
#include "common/gum-dbus.h"
#include "dbus/gumd-dbus-server-interface.h"
#include "dbus/gumd-dbus-server-msg-bus.h"
#include "dbus/gumd-dbus-server-p2p.h"

static GumdDbusServer *_server = NULL;
static guint _sig_source_id[3];

static void
_on_server_closed (
		gpointer data,
		GObject *server)
{
    _server = NULL;
    if (data) g_main_loop_quit ((GMainLoop *)data);
}

static gboolean
_handle_quit_signal (
		gpointer user_data)
{
    GMainLoop *ml = (GMainLoop *) user_data;

    g_return_val_if_fail (ml != NULL, FALSE);
    DBG ("Received quit signal");
    if (ml) g_main_loop_quit (ml);

    return FALSE;
}

static gboolean
_start_dbus_server (
		GMainLoop *main_loop)
{

#ifdef GUM_BUS_TYPE_P2P
    _server = GUMD_DBUS_SERVER (gumd_dbus_server_p2p_new ());
#else
    _server = GUMD_DBUS_SERVER (gumd_dbus_server_msg_bus_new ());
#endif

    if (!_server) {
    	return FALSE;
    }

    if (!gumd_dbus_server_start (_server)) {
    	g_object_unref (_server);
    	_server = NULL;
    	return FALSE;
    }

    g_object_weak_ref (G_OBJECT (_server), _on_server_closed,
    		main_loop);
	return TRUE;
}

static gboolean
_handle_reload_signal (
		gpointer user_data)
{
    GMainLoop *ml = (GMainLoop *) user_data;

    DBG ("Received reload signal");
    g_return_val_if_fail (ml != NULL, FALSE);
    if (_server) {
        g_object_weak_unref (G_OBJECT(_server), _on_server_closed, ml);
        g_object_unref (_server);
        DBG ("Restarting daemon ....");
        _start_dbus_server (ml);
    }

    return TRUE;
}

static void 
_install_sighandlers (
		GMainLoop *main_loop)
{
    GSource *source = NULL;
    GMainContext *ctx = g_main_loop_get_context (main_loop);

    source = g_unix_signal_source_new (SIGHUP);
    g_source_set_callback (source,
                           _handle_reload_signal,
                           main_loop,
                           NULL);
    _sig_source_id[0] = g_source_attach (source, ctx);
    source = g_unix_signal_source_new (SIGTERM);
    g_source_set_callback (source,
                           _handle_quit_signal,
                           main_loop,
                           NULL);
    _sig_source_id[1] = g_source_attach (source, ctx);
    source = g_unix_signal_source_new (SIGINT);
    g_source_set_callback (source,
                           _handle_quit_signal,
                           main_loop,
                           NULL);
    _sig_source_id[2] = g_source_attach (source, ctx);
}

int
main (int argc, char **argv)
{
    GError *error = NULL;
    GMainLoop *main_loop = NULL;
    GOptionContext *opt_context = NULL;
    gboolean retval = FALSE;
    GOptionEntry opt_entries[] = {
        {NULL }
    };
    gid_t daemon_gid;
    struct group *daemon_group;

    DBG ("Before set: r-gid %d e-gid %d", getgid (), getegid ());
    daemon_gid = getgid ();
    daemon_group = getgrnam ("gumd");
    if (daemon_group)
        daemon_gid = daemon_group->gr_gid;
    if (setegid (daemon_gid))
        WARN ("setegid() failed");
    DBG ("After set: r-gid %d e-gid %d", getgid (), getegid ());

#if !GLIB_CHECK_VERSION (2, 36, 0)
    g_type_init ();
#endif

#ifdef ENABLE_DEBUG
    g_log_set_always_fatal (G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);
#endif

    opt_context = g_option_context_new ("User Management daemon");
    g_option_context_add_main_entries (opt_context, opt_entries, NULL);
    retval = g_option_context_parse (opt_context, &argc, &argv, &error);
    g_option_context_free (opt_context);
    if (!retval) {
        WARN ("Error parsing options: %s", error->message);
        g_error_free (error);
        return -1;
    }

    main_loop = g_main_loop_new (NULL, FALSE);
    _start_dbus_server (main_loop);
    if (!_server) {
        return -1;
    }

    _install_sighandlers (main_loop);

    INFO ("Entering main event loop");
    g_main_loop_run (main_loop);

    DBG ("");

    if(_server) g_object_unref (_server);
    DBG ("");
 
    if (main_loop) g_main_loop_unref (main_loop);

    DBG("Clean shutdown");
    return 0;
}
