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

#define BASE 20

gboolean
totem_pl_parser_is_videosite (const char *uri, gboolean debug)
{
#ifdef HAVE_QUVI
	const char *args[] = {
		LIBEXECDIR "/totem-pl-parser-videosite",
		"--check",
		"--url",
		NULL,
		NULL
	};
	char *out;

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
	if (debug)
		g_print ("Checking videosite for URI '%s' returned '%s' (%s)\n",
			 uri, out, g_strcmp0 (out, "TRUE") == 0 ? "true" : "false");

	return (g_strcmp0 (out, "TRUE") == 0);
#else
	return FALSE;
#endif /* HAVE_QUVI */
}

#ifndef TOTEM_PL_PARSER_MINI

TotemPlParserResult
totem_pl_parser_add_videosite (TotemPlParser *parser,
			       GFile *file,
			       GFile *base_file,
			       TotemPlParseData *parse_data,
			       gpointer data)
{
#ifdef HAVE_QUVI
	const char *args[] = {
		LIBEXECDIR "/totem-pl-parser-videosite",
		"--url",
		NULL,
		NULL
	};
	char *uri;
	char *out = NULL;
	char **lines;
	guint i;
	GHashTable *ht;
	char *new_uri = NULL;

	uri = g_file_get_uri (file);

	args[2] = uri;
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
		g_print ("Parsing videosite for URI '%s' returned '%s'\n", uri, out);

	if (out != NULL) {
		if (g_str_equal (out, "TOTEM_PL_PARSER_RESULT_ERROR"))
			return TOTEM_PL_PARSER_RESULT_ERROR;
		if (g_str_equal (out, "TOTEM_PL_PARSER_RESULT_UNHANDLED"))
			return TOTEM_PL_PARSER_RESULT_UNHANDLED;
	} else {
		/* totem-pl-parser-videosite failed to launch */
		return TOTEM_PL_PARSER_RESULT_ERROR;
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
	g_free (new_uri);

	return TOTEM_PL_PARSER_RESULT_SUCCESS;
#else
	return TOTEM_PL_PARSER_RESULT_UNHANDLED;
#endif /* !HAVE_QUVI */
}

#endif /* !TOTEM_PL_PARSER_MINI */

