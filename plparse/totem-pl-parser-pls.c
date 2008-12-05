/* 
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007 Bastien Nocera
   Copyright (C) 2003, 2004 Colin Walters <walters@rhythmbox.org>

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

#ifndef TOTEM_PL_PARSER_MINI
#include <string.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include "totem-pl-parser.h"
#include "totemplparser-marshal.h"
#endif /* !TOTEM_PL_PARSER_MINI */

#include "totem-pl-parser-mini.h"
#include "totem-pl-parser-pls.h"
#include "totem-pl-parser-private.h"

#ifndef TOTEM_PL_PARSER_MINI
gboolean
totem_pl_parser_write_pls (TotemPlParser *parser, GtkTreeModel *model,
			   TotemPlParserIterFunc func, 
			   GFile *output, const char *title,
			   gpointer user_data, GError **error)
{
	GFileOutputStream *stream;
	int num_entries_total, num_entries, i;
	char *buf;
	gboolean success;

	num_entries = totem_pl_parser_num_entries (parser, model, func, user_data);
	num_entries_total = gtk_tree_model_iter_n_children (model, NULL);

	stream = g_file_replace (output, NULL, FALSE, G_FILE_CREATE_NONE, NULL, error);
	if (stream == NULL)
		return FALSE;

	buf = g_strdup ("[playlist]\n");
	success = totem_pl_parser_write_string (G_OUTPUT_STREAM (stream), buf, error);
	g_free (buf);
	if (success == FALSE)
		return FALSE;

	if (title != NULL) {
		buf = g_strdup_printf ("X-GNOME-Title=%s\n", title);
		success = totem_pl_parser_write_string (G_OUTPUT_STREAM (stream), buf, error);
		g_free (buf);
		if (success == FALSE)
			return FALSE;
	}

	buf = g_strdup_printf ("NumberOfEntries=%d\n", num_entries);
	success = totem_pl_parser_write_string (G_OUTPUT_STREAM (stream), buf, error);
	g_free (buf);
	if (success == FALSE)
		return FALSE;

	for (i = 1; i <= num_entries_total; i++) {
		GtkTreeIter iter;
		char *uri, *title, *relative;
		GFile *file;
		gboolean custom_title;

		if (gtk_tree_model_iter_nth_child (model, &iter, NULL, i - 1) == FALSE)
			continue;

		func (model, &iter, &uri, &title, &custom_title, user_data);

		file = g_file_new_for_uri (uri);
		if (totem_pl_parser_scheme_is_ignored (parser, file) != FALSE) {
			g_free (uri);
			g_free (title);
			g_object_unref (file);
			continue;
		}
		g_object_unref (file);

		relative = totem_pl_parser_relative (output, uri);
		buf = g_strdup_printf ("File%d=%s\n", i, relative ? relative : uri);
		g_free (relative);
		g_free (uri);
		success = totem_pl_parser_write_string (G_OUTPUT_STREAM (stream), buf, error);
		g_free (buf);
		if (success == FALSE) {
			g_free (title);
			return FALSE;
		}

		if (custom_title == FALSE) {
			g_free (title);
			continue;
		}

		buf = g_strdup_printf ("Title%d=%s\n", i, title);
		success = totem_pl_parser_write_string (G_OUTPUT_STREAM (stream), buf, error);
		g_free (buf);
		g_free (title);
		if (success == FALSE)
			return FALSE;
	}

	g_object_unref (stream);
	return TRUE;
}

TotemPlParserResult
totem_pl_parser_add_pls_with_contents (TotemPlParser *parser,
				       GFile *file,
				       GFile *_base_file,
				       const char *contents)
{
	TotemPlParserResult retval = TOTEM_PL_PARSER_RESULT_UNHANDLED;
	GFile *base_file;
	char **lines;
	int i, num_entries;
	char *split_char, *playlist_title;
	gboolean dos_mode = FALSE;
	gboolean fallback;

	/* figure out whether we're a unix pls or dos pls */
	if (strstr(contents,"\x0d") == NULL) {
		split_char = "\n";
	} else {
		split_char = "\x0d\n";
		dos_mode = TRUE;
	}
	lines = g_strsplit (contents, split_char, 0);

	/* [playlist] */
	i = 0;
	playlist_title = NULL;

	/* Ignore empty lines */
	while (totem_pl_parser_line_is_empty (lines[i]) != FALSE)
		i++;

	if (lines[i] == NULL
			|| g_ascii_strncasecmp (lines[i], "[playlist]",
				(gsize)strlen ("[playlist]")) != 0) {
		goto bail;
	}

	playlist_title = totem_pl_parser_read_ini_line_string (lines,
			"X-GNOME-Title", dos_mode);

	if (playlist_title != NULL) {
		totem_pl_parser_add_uri (parser,
					 TOTEM_PL_PARSER_FIELD_IS_PLAYLIST, TRUE,
					 TOTEM_PL_PARSER_FIELD_FILE, file,
					 TOTEM_PL_PARSER_FIELD_TITLE, playlist_title,
					 NULL);
	}

	/* numberofentries=? */
	num_entries = totem_pl_parser_read_ini_line_int (lines, "numberofentries");

	if (num_entries == -1) {
		num_entries = 0;

		for (i = 0; lines[i] != NULL; i++) {
			if (totem_pl_parser_line_is_empty (lines[i]))
				continue;

			if (g_ascii_strncasecmp (g_strchug (lines[i]), "file", (gsize)strlen ("file")) == 0)
				num_entries++;
		}

		if (num_entries == 0)
			goto bail;
	}

	/* Base? */
	if (_base_file == NULL)
		base_file = g_file_get_parent (file);
	else
		base_file = g_object_ref (_base_file);

	retval = TOTEM_PL_PARSER_RESULT_SUCCESS;

	for (i = 1; i <= num_entries; i++) {
		char *file_str, *title, *genre, *length;
		char *file_key, *title_key, *genre_key, *length_key;
		gint64 length_num;

		file_key = g_strdup_printf ("file%d", i);
		title_key = g_strdup_printf ("title%d", i);
		length_key = g_strdup_printf ("length%d", i);
		length_num = 0;
		/* Genre is our own little extension */
		genre_key = g_strdup_printf ("genre%d", i);

		file_str = totem_pl_parser_read_ini_line_string (lines, (const char*)file_key, dos_mode);
		title = totem_pl_parser_read_ini_line_string (lines, (const char*)title_key, dos_mode);
		genre = totem_pl_parser_read_ini_line_string (lines, (const char*)genre_key, dos_mode);
		length = totem_pl_parser_read_ini_line_string (lines, (const char*)length_key, dos_mode);

		g_free (file_key);
		g_free (title_key);
		g_free (genre_key);
		g_free (length_key);

		if (file_str == NULL) {
			g_free (title);
			g_free (genre);
			g_free (length);
			continue;
		}

		fallback = parser->priv->fallback;
		if (parser->priv->recurse)
			parser->priv->fallback = FALSE;

		/* Get the length, if it's negative, that means that we have a stream
		 * and should push the entry straight away */
		if (length != NULL)
			length_num = totem_pl_parser_parse_duration (length, parser->priv->debug);

		if (strstr (file_str, "://") != NULL || file_str[0] == G_DIR_SEPARATOR) {
			GFile *target;

			target = g_file_new_for_commandline_arg (file_str);
			if (length_num < 0 || totem_pl_parser_parse_internal (parser, target, NULL) != TOTEM_PL_PARSER_RESULT_SUCCESS) {
				totem_pl_parser_add_uri (parser,
							 TOTEM_PL_PARSER_FIELD_URI, file_str,
							 TOTEM_PL_PARSER_FIELD_TITLE, title,
							 TOTEM_PL_PARSER_FIELD_GENRE, genre,
							 TOTEM_PL_PARSER_FIELD_DURATION, length,
							 TOTEM_PL_PARSER_FIELD_BASE_FILE, base_file, NULL);
			}
			g_object_unref (target);
		} else {
			GFile *target;

			target = g_file_get_child_for_display_name (base_file, file_str, NULL);

			if (length_num < 0 || totem_pl_parser_parse_internal (parser, target, base_file) != TOTEM_PL_PARSER_RESULT_SUCCESS) {

				totem_pl_parser_add_uri (parser,
							 TOTEM_PL_PARSER_FIELD_FILE, target,
							 TOTEM_PL_PARSER_FIELD_TITLE, title,
							 TOTEM_PL_PARSER_FIELD_GENRE, genre,
							 TOTEM_PL_PARSER_FIELD_DURATION, length,
							 TOTEM_PL_PARSER_FIELD_BASE_FILE, base_file, NULL);
			}

			g_object_unref (target);
		}

		parser->priv->fallback = fallback;
		g_free (file_str);
		g_free (title);
		g_free (genre);
		g_free (length);
	}

	if (playlist_title != NULL)
		totem_pl_parser_playlist_end (parser, playlist_title);

	g_object_unref (base_file);

bail:
	g_free (playlist_title);
	g_strfreev (lines);

	return retval;
}

TotemPlParserResult
totem_pl_parser_add_pls (TotemPlParser *parser,
			 GFile *file,
			 GFile *base_file,
			 gpointer data)
{
	TotemPlParserResult retval = TOTEM_PL_PARSER_RESULT_UNHANDLED;
	char *contents;
	gsize size;

	if (g_file_load_contents (file, NULL, &contents, &size, NULL, NULL) == FALSE)
		return TOTEM_PL_PARSER_RESULT_ERROR;

	if (size == 0) {
		g_free (contents);
		return TOTEM_PL_PARSER_RESULT_SUCCESS;
	}

	retval = totem_pl_parser_add_pls_with_contents (parser, file, base_file, contents);
	g_free (contents);

	return retval;
}

#endif /* !TOTEM_PL_PARSER_MINI */

