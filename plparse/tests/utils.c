/*
 * Copyright 2007-2022 Bastien Nocera <hadess@hadess.net>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"
#include "utils.h"

#include <glib.h>
#include <gio/gio.h>

extern gboolean option_debug;
extern gboolean option_force;
extern gboolean option_disable_unsafe;
extern gboolean option_no_recurse;
extern char *option_base_uri;

typedef struct {
	const char *field;
	char *ret;
} ParserResult;

char *
get_relative_uri (const char *rel)
{
	GFile *file;
	char *uri;

	file = g_file_new_for_commandline_arg (rel);
	uri = g_file_get_uri (file);
	g_object_unref (file);
	g_assert_nonnull (uri);

	return uri;
}

static void
entry_parsed_cb (TotemPlParser *parser,
		 const char *uri,
		 GHashTable *metadata,
		 ParserResult *res)
{
	if (res->ret == NULL) {
		if (g_strcmp0 (res->field, TOTEM_PL_PARSER_FIELD_URI) == 0)
			res->ret = g_strdup (uri);
		else
			res->ret = g_strdup (g_hash_table_lookup (metadata, res->field));
	}
}

static void
entry_parsed_num_cb (TotemPlParser *parser,
		     const char *uri,
		     GHashTable *metadata,
		     guint *ret)
{
	*ret = *ret + 1;
}

guint
parser_test_get_num_entries (const char *uri)
{
	TotemPlParserResult retval;
	guint ret = 0;
	TotemPlParser *pl = totem_pl_parser_new ();

	g_object_set (pl, "recurse", FALSE,
			  "debug", option_debug,
			  "force", option_force,
			  "disable-unsafe", option_disable_unsafe,
			  NULL);
	g_signal_connect (G_OBJECT (pl), "entry-parsed",
			  G_CALLBACK (entry_parsed_num_cb), &ret);

	retval = totem_pl_parser_parse_with_base (pl, uri, option_base_uri, FALSE);
	g_test_message ("Got retval %d for uri '%s'", retval, uri);
	g_object_unref (pl);

	return ret;
}

TotemPlParserResult
simple_parser_test (const char *uri)
{
	TotemPlParserResult retval;
	TotemPlParser *pl = totem_pl_parser_new ();

	g_object_set (pl, "recurse", !option_no_recurse,
			  "debug", option_debug,
			  "force", option_force,
			  "disable-unsafe", option_disable_unsafe,
			  NULL);

	retval = totem_pl_parser_parse_with_base (pl, uri, option_base_uri, FALSE);
	g_test_message ("Got retval %d for uri '%s'", retval, uri);
	g_object_unref (pl);

	return retval;
}

char *
parser_test_get_entry_field (const char *uri, const char *field)
{
	TotemPlParserResult retval;
	TotemPlParser *pl = totem_pl_parser_new ();
	ParserResult res;

	g_object_set (pl, "recurse", !option_no_recurse,
			  "debug", option_debug,
			  "force", option_force,
			  "disable-unsafe", option_disable_unsafe,
			  NULL);
	res.field = field;
	res.ret = NULL;
	g_signal_connect (G_OBJECT (pl), "entry-parsed",
			  G_CALLBACK (entry_parsed_cb), &res);

	retval = totem_pl_parser_parse_with_base (pl, uri, option_base_uri, FALSE);
	g_test_message ("Got retval %d for uri '%s'", retval, uri);
	g_object_unref (pl);

	return res.ret;
}

static void
playlist_started_title_cb (TotemPlParser *parser,
			   const char *uri,
			   GHashTable *metadata,
			   char **ret)
{
	*ret = g_strdup (uri);
}

char *
parser_test_get_playlist_uri (const char *uri)
{
	TotemPlParserResult retval;
	char *ret = NULL;
	TotemPlParser *pl = totem_pl_parser_new ();

	g_object_set (pl, "recurse", !option_no_recurse,
			  "debug", option_debug,
			  "force", option_force,
			  "disable-unsafe", option_disable_unsafe,
			  NULL);
	g_signal_connect (G_OBJECT (pl), "playlist-started",
			  G_CALLBACK (playlist_started_title_cb), &ret);

	retval = totem_pl_parser_parse_with_base (pl, uri, option_base_uri, FALSE);
	g_test_message ("Got retval %d for uri '%s'", retval, uri);
	g_object_unref (pl);

	return ret;
}

static void
playlist_started_field_cb (TotemPlParser *parser,
			   const char *uri,
			   GHashTable *metadata,
			   ParserResult *res)
{
	res->ret = g_strdup (g_hash_table_lookup (metadata, res->field));
}

char *
parser_test_get_playlist_field (const char *uri,
				const char *field)
{
	TotemPlParserResult retval;
	ParserResult res;
	TotemPlParser *pl = totem_pl_parser_new ();

	g_object_set (pl, "recurse", FALSE,
			  "debug", option_debug,
			  "force", option_force,
			  "disable-unsafe", option_disable_unsafe,
			  NULL);
	res.field = field;
	res.ret = NULL;
	g_signal_connect (G_OBJECT (pl), "playlist-started",
			  G_CALLBACK (playlist_started_field_cb), &res);

	retval = totem_pl_parser_parse_with_base (pl, uri, option_base_uri, FALSE);
	g_test_message ("Got retval %d for uri '%s'", retval, uri);
	g_object_unref (pl);

	return res.ret;
}

typedef struct {
	gboolean pl_started;
	gboolean parsed_item;
	gboolean pl_ended;
} PlOrderingData;

static void
playlist_started_order (TotemPlParser *parser,
			const char *uri,
			GHashTable *metadata,
			PlOrderingData *data)
{
	data->pl_started = TRUE;
}

static void
playlist_ended_order (TotemPlParser *parser,
		      const char *uri,
		      PlOrderingData *data)
{
	g_assert_true (data->pl_started);
	g_assert_true (data->parsed_item);
	data->pl_ended = TRUE;
}

static void
entry_parsed_cb_order (TotemPlParser *parser,
		       const char *uri,
		       GHashTable *metadata,
		       PlOrderingData *data)
{
	/* Check that the playlist started happened before the entry appeared */
	g_assert_true (data->pl_started);
	data->parsed_item = TRUE;
}

gboolean
parser_test_get_order_result (const char *uri)
{
	TotemPlParserResult retval;
	PlOrderingData data;
	TotemPlParser *pl;

	pl = totem_pl_parser_new ();

	g_object_set (pl, "recurse", !option_no_recurse,
			  "debug", option_debug,
			  "force", option_force,
			  "disable-unsafe", option_disable_unsafe,
			  NULL);
	g_signal_connect (G_OBJECT (pl), "playlist-started",
			  G_CALLBACK (playlist_started_order), &data);
	g_signal_connect (G_OBJECT (pl), "playlist-ended",
			  G_CALLBACK (playlist_ended_order), &data);
	g_signal_connect (G_OBJECT (pl), "entry-parsed",
			  G_CALLBACK (entry_parsed_cb_order), &data);

	data.pl_started = FALSE;
	data.pl_ended = FALSE;
	data.parsed_item = FALSE;
	retval = totem_pl_parser_parse_with_base (pl, uri, option_base_uri, FALSE);
	g_assert_true (data.pl_ended);
	g_test_message ("Got retval %d for uri '%s'", retval, uri);
	g_object_unref (pl);

	return data.pl_started && data.pl_ended && data.parsed_item;
}



gboolean
check_http (void)
{
	GVfs *vfs;
	gboolean http_supported;
	gboolean error_out_on_http = FALSE;

	if (g_strcmp0 (g_get_user_name (), "hadess") == 0)
		error_out_on_http = TRUE;

	vfs = g_vfs_get_default ();
	if (vfs == NULL) {
		if (error_out_on_http)
			g_error ("gvfs with http support is required (no gvfs)");
		else
			g_message ("gvfs with http support is required (no gvfs)");
		return FALSE;
	} else {
		const char * const *schemes;

		schemes = g_vfs_get_supported_uri_schemes (vfs);
		if (schemes == NULL) {
			if (error_out_on_http)
				g_error ("gvfs with http support is required (no http)");
			else
				g_message ("gvfs with http support is required (no http)");
			return FALSE;
		} else {
			guint i;
			for (i = 0; schemes[i] != NULL; i++) {
				if (g_str_equal (schemes[i], "http")) {
					http_supported = TRUE;
					break;
				}
			}
		}
	}

	if (http_supported == FALSE) {
		if (error_out_on_http)
			g_error ("gvfs with http support is required (no http)");
		else
			g_message ("gvfs with http support is required (no http)");
	}

	return http_supported;
}
