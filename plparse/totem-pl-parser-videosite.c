/*
   Copyright (C) 2010 Bastien Nocera <hadess@hadess.net>

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301  USA.

   Author: Bastien Nocera <hadess@hadess.net>
 */

#include "config.h"

#include <string.h>
#include <glib.h>

#include "totem-pl-parser-mini.h"
#include "totem-pl-parser-videosite.h"
#include "totem-pl-parser-private.h"

#define SCRIPT_ENVVAR "TOTEM_PL_PARSER_VIDEOSITE_SCRIPT"

/* See README-videosite-script.md */

static char *
find_helper_script (void)
{
	GDir *dir;
	const char *name, *script_dir;
	char *script_name = NULL;

	if (g_getenv (SCRIPT_ENVVAR) != NULL)
		return g_strdup (g_getenv (SCRIPT_ENVVAR));

	script_dir = g_getenv ("TOTEM_PL_PARSER_VIDEOSITE_SCRIPT_DIR");
	if (!script_dir)
		script_dir = LIBEXECDIR "/totem-pl-parser";

	dir = g_dir_open (script_dir, 0, NULL);
	if (!dir)
		goto bail;

	while ((name = g_dir_read_name (dir)) != NULL) {
		/* Skip hidden files */
		if (name[0] == '.')
			continue;
		if (script_name == NULL || g_strcmp0 (name, script_name) < 0) {
			g_free (script_name);
			script_name = g_strdup (name);
		}
	}
	g_clear_pointer (&dir, g_dir_close);

	if (script_name != NULL) {
		char *ret;
		ret = g_build_filename (script_dir, script_name, NULL);
		g_free (script_name);
		return ret;
	}

bail:
	return NULL;
}

gboolean
totem_pl_parser_is_videosite (const char *uri, gboolean debug)
{
	const char *args[] = {
		NULL,
		"--check",
		"--url",
		NULL,
		NULL
	};
	char *out;
	char *script;
	gboolean ret = FALSE;

	script = find_helper_script ();
	if (script == NULL) {
		if (debug)
			g_print ("Did not find a script to check whether '%s' is a videosite\n", uri);
		return ret;
	}

	args[0] = script;
	args[3] = uri;
	g_spawn_sync (NULL,
		      (char **) args,
		      NULL,
		      0,
		      NULL,
		      NULL,
		      &out,
		      NULL,
		      NULL,
		      NULL);

	ret = g_strcmp0 (out, "TRUE") == 0;
	if (debug)
		g_print ("Checking videosite with script '%s' for URI '%s' returned '%s' (%s)\n",
			 script, uri, out, ret ? "true" : "false");

	g_free (script);
	g_free (out);

	return ret;
}

#ifndef TOTEM_PL_PARSER_MINI

TotemPlParserResult
totem_pl_parser_add_videosite (TotemPlParser *parser,
			       GFile *file,
			       GFile *base_file,
			       TotemPlParseData *parse_data,
			       gpointer data)
{
	const char *args[] = {
		NULL,
		"--url",
		NULL,
		NULL
	};
	char *_uri;
	char *out = NULL;
	char **lines;
	guint i;
	GHashTable *ht;
	char *new_uri = NULL;
	char *script;
	TotemPlParserResult ret;

	script = find_helper_script ();
	if (script == NULL) {
		DEBUG (file, g_print ("Did not find a script to check whether '%s' is a videosite\n", uri));
		return FALSE;
	}

	_uri = g_file_get_uri (file);
	args[0] = script;
	args[2] = _uri;
	g_spawn_sync (NULL,
		      (char **) args,
		      NULL,
		      0,
		      NULL,
		      NULL,
		      &out,
		      NULL,
		      NULL,
		      NULL);
	if (totem_pl_parser_is_debugging_enabled (parser))
		g_print ("Parsing videosite for URI '%s' returned '%s'\n", _uri, out);

	if (out != NULL) {
		if (g_str_equal (out, "TOTEM_PL_PARSER_RESULT_ERROR")) {
			ret = TOTEM_PL_PARSER_RESULT_ERROR;
			goto out;
		}
		if (g_str_equal (out, "TOTEM_PL_PARSER_RESULT_UNHANDLED")) {
			ret = TOTEM_PL_PARSER_RESULT_UNHANDLED;
			goto out;
		}
	} else {
		/* totem-pl-parser-videosite failed to launch */
		ret = TOTEM_PL_PARSER_RESULT_ERROR;
		goto out;
	}

	ht = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	lines = g_strsplit (out, "\n", -1);
	g_free (out);
	for (i = 0; lines[i] != NULL && *lines[i] != '\0'; i++) {
		char **line;

		line = g_strsplit (lines[i], "=", 2);
		if (g_strcmp0 (line[0], TOTEM_PL_PARSER_FIELD_URI) == 0) {
			if (new_uri == NULL)
				new_uri = g_strdup (line[1]);
		} else {
			g_hash_table_insert (ht, g_strdup (line[0]), g_strdup (line[1]));
		}
		g_strfreev (line);
	}
	g_strfreev (lines);

	totem_pl_parser_add_hash_table (parser, ht, new_uri, FALSE);
	g_hash_table_unref (ht);
	g_free (new_uri);

	ret = TOTEM_PL_PARSER_RESULT_SUCCESS;

out:
	g_free (script);
	g_free (_uri);
	return ret;
}

#endif /* !TOTEM_PL_PARSER_MINI */

