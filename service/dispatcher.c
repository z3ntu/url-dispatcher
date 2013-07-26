/**
 * Copyright (C) 2013 Canonical, Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 3, as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranties of MERCHANTABILITY,
 * SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <gio/gio.h>
#include "service-iface.h"

/* Globals */
static GMainLoop * mainloop = NULL;
static GCancellable * cancellable = NULL;
static ServiceIfaceComCanonicalURLDispatcher * skel = NULL;
static GRegex * applicationre = NULL;

/* Errors */
enum {
	ERROR_BAD_URL
};

G_DEFINE_QUARK(url_dispatcher, url_dispatcher_error);

/* Register our errors */
static void
register_dbus_errors (void)
{
	g_dbus_error_register_error(url_dispatcher_error_quark(), ERROR_BAD_URL, "com.canonical.URLDispatcher.BadURL");
	return;
}

/* Say that we have a bad URL and report a recoverable error on the process that
   sent it to us. */
static gboolean
bad_url (GDBusMethodInvocation * invocation, const gchar * url)
{
	/* TODO: Recoverable Error */

	g_dbus_method_invocation_return_error(invocation,
		url_dispatcher_error_quark(),
		ERROR_BAD_URL,
		"URL '%s' is not handleable by the URL Dispatcher",
		url);

	return TRUE;
}

/* Handles taking an application and an URL and sending them to Upstart */
static void
pass_url_to_app (const gchar * app_id, const gchar * url)
{
	g_debug("Emitting 'application-start' for APP_ID='%s' and URLS='%s'", app_id, url);

	/* TODO: Port to libupstart */
	gchar * cmdline = NULL;

	if (url == NULL) {
		cmdline = g_strdup_printf("initctl emit application-start APP_ID=\"%s\"", app_id);
	} else {
		cmdline = g_strdup_printf("initctl emit application-start APP_ID=\"%s\" APP_URLS=\"%s\"", app_id, url);
	}

	GError * error = NULL;
	g_spawn_command_line_async(cmdline, &error);
	g_free(cmdline);

	if (error != NULL) {
		g_warning("Unable to spawn initctl: %s", error->message);
		g_error_free(error);
	}

	return;
}

/* URL handlers need to be identified */
typedef struct _url_type_t url_type_t;
struct _url_type_t {
	gchar * regex_patern;
	GRegex * regex_object;
	gchar * app_id;
};

/* TODO: Make these come from registrations, but this works for now */
url_type_t url_types[] = {
	{
		.regex_patern = "^tel://[\\d\\.+x,\\(\\)-]*$",
		.regex_object = NULL,
		.app_id = "telephony-app"
	}
};

/* Get a URL off of the bus */
static gboolean
dispatch_url (GObject * skel, GDBusMethodInvocation * invocation, const gchar * url, gpointer user_data)
{
	g_debug("Dispatching URL: %s", url);

	if (url == NULL || url[0] == '\0') {
		return bad_url(invocation, url);
	}

	/* Special case the application URL */
	GMatchInfo * appmatch = NULL;
	if (g_regex_match(applicationre, url, 0, &appmatch)) {
		gchar * appid = g_match_info_fetch(appmatch, 1);
		pass_url_to_app(appid, NULL);

		g_free(appid);
		g_match_info_free(appmatch);

		g_dbus_method_invocation_return_value(invocation, NULL);
		return TRUE;
	}
	g_match_info_free(appmatch);

	int i;
	for (i = 0; i < G_N_ELEMENTS(url_types); i++) {
		if (url_types[i].regex_object == NULL) {
			url_types[i].regex_object = g_regex_new(url_types[i].regex_patern, 0, 0, NULL);
		}

		if (g_regex_match(url_types[i].regex_object, url, 0, NULL)) {
			pass_url_to_app(url_types[i].app_id, url);

			g_dbus_method_invocation_return_value(invocation, NULL);
			return TRUE;
		}
	}

	return bad_url(invocation, url);
}

/* We're goin' down cap'n */
static void
name_lost (GDBusConnection * con, const gchar * name, gpointer user_data)
{
	g_error("Unable to get name '%s'", name);
	g_main_loop_quit(mainloop);
	return;
}

/* Callback when we're connected to dbus */
static void
bus_got (GObject * obj, GAsyncResult * res, gpointer user_data)
{
	GDBusConnection * bus = NULL;
	GError * error = NULL;

	bus = g_bus_get_finish(res, &error);

	if (error != NULL) {
		g_error("Unable to connect to D-Bus: %s", error->message);
		g_main_loop_quit(mainloop);
		return;
	}

	register_dbus_errors();

	g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(skel), bus, "/com/canonical/URLDispatcher", &error);
	if (error != NULL) {
		g_error("Unable to export interface skeleton: %s", error->message);
		g_main_loop_quit(mainloop);
		return;
	}

	g_bus_own_name_on_connection(bus,
		"com.canonical.URLDispatcher",
		G_BUS_NAME_OWNER_FLAGS_NONE, /* flags */
		NULL, /* name acquired */
		name_lost,
		NULL, NULL); /* user data */

	g_object_unref(bus);

	return;
}

/* Where it all begins */
int
main (int argc, char * argv[])
{
	mainloop = g_main_loop_new(NULL, FALSE);
	cancellable = g_cancellable_new();
	applicationre = g_regex_new("^application:///([a-zA-Z0-9_-]*)\\.desktop$", 0, 0, NULL);

	g_bus_get(G_BUS_TYPE_SESSION, cancellable, bus_got, NULL);

	skel = service_iface_com_canonical_urldispatcher_skeleton_new();
	g_signal_connect(skel, "handle-dispatch-url", G_CALLBACK(dispatch_url), NULL);

	/* Run Main */
	g_main_loop_run(mainloop);

	/* Clean up globals */
	g_main_loop_unref(mainloop);
	g_object_unref(cancellable);
	g_object_unref(skel);
	g_regex_unref(applicationre);

	int i;
	for (i = 0; i < G_N_ELEMENTS(url_types); i++) {
		g_clear_pointer(&url_types[i].regex_object, g_regex_unref);
	}

	return 0;
}