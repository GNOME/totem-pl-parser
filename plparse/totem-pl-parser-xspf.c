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
#include <libxml/tree.h>
#include <libxml/parser.h>

#include "totem-pl-parser.h"
#endif /* !TOTEM_PL_PARSER_MINI */

#include "totem-pl-parser-mini.h"
#include "totem-pl-parser-xspf.h"
#include "totem-pl-parser-private.h"

#ifndef TOTEM_PL_PARSER_MINI

#define SAFE_FREE(x) { if (x != NULL) xmlFree (x); }

static void
debug_noop (void *ctx, const char *msg, ...)
{
	return;
}

static xmlDocPtr
totem_pl_parser_parse_xml_file (GFile *file)
{
	xmlDocPtr doc;
	char *contents;
	gsize size;

	if (g_file_load_contents (file, NULL, &contents, &size, NULL, NULL) == FALSE)
		return NULL;

	/* Try to remove HTML style comments */
	{
		char *needle;

		while ((needle = strstr (contents, "<!--")) != NULL) {
			while (strncmp (needle, "-->", 3) != 0) {
				*needle = ' ';
				needle++;
				if (*needle == '\0')
					break;
			}
		}
	}

	xmlSetGenericErrorFunc (NULL, (xmlGenericErrorFunc) debug_noop);
	doc = xmlParseMemory (contents, size);
	if (doc == NULL)
		doc = xmlRecoverMemory (contents, size);
	g_free (contents);

	return doc;
}

static struct {
	const char *field;
	const char *element;
} fields[] = {
	{ TOTEM_PL_PARSER_FIELD_TITLE, "title" },
	{ TOTEM_PL_PARSER_FIELD_AUTHOR, "creator" },
	{ TOTEM_PL_PARSER_FIELD_IMAGE_URI, "image" },
	{ TOTEM_PL_PARSER_FIELD_ALBUM, "album" },
	{ TOTEM_PL_PARSER_FIELD_DURATION_MS, "duration" },
	{ TOTEM_PL_PARSER_FIELD_GENRE, NULL },
	{ TOTEM_PL_PARSER_FIELD_STARTTIME, NULL },
	{ TOTEM_PL_PARSER_FIELD_SUBTITLE_URI, NULL },
	{ TOTEM_PL_PARSER_FIELD_PLAYING, NULL },
	{ TOTEM_PL_PARSER_FIELD_CONTENT_TYPE, NULL }
};

gboolean
totem_pl_parser_save_xspf (TotemPlParser    *parser,
                           TotemPlPlaylist  *playlist,
                           GFile            *output,
                           const char       *title,
                           GCancellable     *cancellable,
                           GError          **error)
{
        TotemPlPlaylistIter iter;
	GFileOutputStream *stream;
	char *buf;
	GString *str;
	gboolean valid, success;

	stream = g_file_replace (output, NULL, FALSE, G_FILE_CREATE_NONE, cancellable, error);
	if (stream == NULL)
		return FALSE;

	str = g_string_new ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
			    "<playlist version=\"1\" xmlns=\"http://xspf.org/ns/0/\">\n");

	if (title != NULL && title[0] != '\0') {
		g_string_append_printf (str, "<title>%s</title>\n", title);
	}

	str = g_string_append (str, " <trackList>\n");
	buf = g_string_free (str, FALSE);

	success = totem_pl_parser_write_string (G_OUTPUT_STREAM (stream), buf, cancellable, error);
	g_free (buf);
	if (success == FALSE)
		return FALSE;

        valid = totem_pl_playlist_iter_first (playlist, &iter);

        while (valid) {
		char *uri, *uri_escaped, *relative;
		guint i;
		gboolean wrote_ext;

                totem_pl_playlist_get (playlist, &iter,
                                       TOTEM_PL_PARSER_FIELD_URI, &uri,
                                       NULL);


                if (!uri) {
			valid = totem_pl_playlist_iter_next (playlist, &iter);
                        continue;
		}

		/* Whether we already wrote the GNOME extensions section header
		 * for that particular track */
		wrote_ext = FALSE;

		relative = totem_pl_parser_relative (output, uri);
		uri_escaped = g_markup_escape_text (relative ? relative : uri, -1);
		buf = g_strdup_printf ("  <track>\n"
                                       "   <location>%s</location>\n", uri_escaped);
		success = totem_pl_parser_write_string (G_OUTPUT_STREAM (stream), buf, cancellable, error);
		g_free (uri);
		g_free (uri_escaped);
		g_free (relative);
		g_free (buf);

                if (success == FALSE)
			return FALSE;

		for (i = 0; i < G_N_ELEMENTS (fields); i++) {
			char *str, *escaped;

			totem_pl_playlist_get (playlist, &iter,
					       fields[i].field, &str,
					       NULL);
			if (!str || *str == '\0') {
				g_free (str);
				continue;
			}
			escaped = g_markup_escape_text (str, -1);
			g_free (str);
			if (!escaped)
				continue;
			if (g_str_equal (fields[i].field, TOTEM_PL_PARSER_FIELD_GENRE)) {
				buf = g_strdup_printf ("   <extension application=\"http://www.rhythmbox.org\">\n"
						       "     <genre>%s</genre>\n"
						       "   </extension>\n",
						       escaped);
			} else if (g_str_equal (fields[i].field, TOTEM_PL_PARSER_FIELD_SUBTITLE_URI) ||
				   g_str_equal (fields[i].field, TOTEM_PL_PARSER_FIELD_PLAYING) ||
				   g_str_equal (fields[i].field, TOTEM_PL_PARSER_FIELD_CONTENT_TYPE) ||
				   g_str_equal (fields[i].field, TOTEM_PL_PARSER_FIELD_STARTTIME)) {
				if (!wrote_ext) {
					buf = g_strdup_printf ("   <extension application=\"http://www.gnome.org\">\n"
							       "     <%s>%s</%s>\n",
							       fields[i].field, escaped, fields[i].field);
					wrote_ext = TRUE;
				} else {
					buf = g_strdup_printf ("     <%s>%s</%s>\n",
							       fields[i].field, escaped, fields[i].field);
				}
			} else if (g_str_equal (fields[i].field, TOTEM_PL_PARSER_FIELD_GENRE) == FALSE) {
				buf = g_strdup_printf ("   <%s>%s</%s>\n",
						       fields[i].element,
						       escaped,
						       fields[i].element);
			}

			success = totem_pl_parser_write_string (G_OUTPUT_STREAM (stream), buf, cancellable, error);
			g_free (buf);
			g_free (escaped);

			if (success == FALSE)
				break;
		}

                if (success == FALSE)
			return FALSE;

		if (wrote_ext)
			success = totem_pl_parser_write_string (G_OUTPUT_STREAM (stream), "   </extension>\n", cancellable, error);

		if (success == FALSE)
			return FALSE;

		success = totem_pl_parser_write_string (G_OUTPUT_STREAM (stream), "  </track>\n", cancellable, error);
		if (success == FALSE)
			return FALSE;

                valid = totem_pl_playlist_iter_next (playlist, &iter);
	}

	buf = g_strdup_printf (" </trackList>\n"
                               "</playlist>");
	success = totem_pl_parser_write_string (G_OUTPUT_STREAM (stream), buf, cancellable, error);
	g_free (buf);

	g_object_unref (stream);

	return success;
}

static gboolean
parse_bool_str (const char *str)
{
	if (str == NULL)
		return FALSE;
	if (g_ascii_strcasecmp (str, "true") == 0)
		return TRUE;
	if (g_ascii_strcasecmp (str, "false") == 0)
		return FALSE;
	return atoi (str);
}

static gboolean
parse_xspf_track (TotemPlParser *parser, GFile *base_file, xmlDocPtr doc,
		xmlNodePtr parent)
{
	xmlNodePtr node;
	xmlChar *title, *uri, *image_uri, *artist, *album, *duration, *moreinfo;
	xmlChar *download_uri, *id, *genre, *filesize, *subtitle, *mime_type;
	xmlChar *playing, *starttime;
	GFile *resolved;
	char *resolved_uri;
	TotemPlParserResult retval = TOTEM_PL_PARSER_RESULT_ERROR;

	title = NULL;
	uri = NULL;
	image_uri = NULL;
	artist = NULL;
	album = NULL;
	duration = NULL;
	moreinfo = NULL;
	download_uri = NULL;
	id = NULL;
	genre = NULL;
	filesize = NULL;
	subtitle = NULL;
	mime_type = NULL;
	playing = NULL;
	starttime = NULL;

	for (node = parent->children; node != NULL; node = node->next)
	{
		if (node->name == NULL)
			continue;

		if (g_ascii_strcasecmp ((char *)node->name, "location") == 0)
			uri = xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
		else if (g_ascii_strcasecmp ((char *)node->name, "title") == 0)
			title = xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
		else if (g_ascii_strcasecmp ((char *)node->name, "image") == 0)
			image_uri = xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
		/* Last.fm uses creator for the artist */
		else if (g_ascii_strcasecmp ((char *)node->name, "creator") == 0)
			artist = xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
		else if (g_ascii_strcasecmp ((char *)node->name, "duration") == 0)
			duration = xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
		else if (g_ascii_strcasecmp ((char *)node->name, "link") == 0) {
			xmlChar *rel;

			rel = xmlGetProp (node, (const xmlChar *) "rel");
			if (rel != NULL) {
				if (g_ascii_strcasecmp ((char *) rel, "http://www.last.fm/trackpage") == 0)
					moreinfo = xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
				else if (g_ascii_strcasecmp ((char *) rel, "http://www.last.fm/freeTrackURL") == 0)
					download_uri = xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
				xmlFree (rel);
			} else {
				/* If we don't have a rel="", then it's not a last.fm playlist */
				moreinfo = xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
			}
		/* Parse the genre extension for Rhythmbox */
		} else if (g_ascii_strcasecmp ((char *)node->name, "extension") == 0) {
			xmlChar *app;
			app = xmlGetProp (node, (const xmlChar *) "application");
			if (app != NULL && g_ascii_strcasecmp ((char *) app, "http://www.rhythmbox.org") == 0) {
				xmlNodePtr child;
				for (child = node->xmlChildrenNode ; child; child = child->next) {
					if (child->name != NULL &&
					    g_ascii_strcasecmp ((char *)child->name, "genre") == 0) {
						genre = xmlNodeListGetString (doc, child->xmlChildrenNode, 0);
						break;
					}
				}
			} else if (app != NULL && g_ascii_strcasecmp ((char *) app, "http://www.gnome.org") == 0) {
				xmlNodePtr child;
				for (child = node->xmlChildrenNode ; child; child = child->next) {
					if (child->name == NULL)
						continue;
					if (g_ascii_strcasecmp ((char *)child->name, "playing") == 0) {
						xmlChar *str;
						str = xmlNodeListGetString (doc, child->xmlChildrenNode, 0);
						playing = parse_bool_str ((char *) str) ? (xmlChar *) "true" : NULL;
					} else if (g_ascii_strcasecmp ((char *)child->name, "subtitle") == 0) {
						subtitle = xmlNodeListGetString (doc, child->xmlChildrenNode, 0);
					} else if (g_ascii_strcasecmp ((char *)child->name, "mime-type") == 0) {
						mime_type = xmlNodeListGetString (doc, child->xmlChildrenNode, 0);
					} else if (g_ascii_strcasecmp ((char *)child->name, "starttime") == 0) {
						starttime = xmlNodeListGetString (doc, child->xmlChildrenNode, 0);
					}
				}
			} else if (app != NULL && g_ascii_strcasecmp ((char *) app, "http://www.last.fm") == 0) {
				xmlNodePtr child;
				for (child = node->xmlChildrenNode ; child; child = child->next) {
					if (child->name != NULL) {
						if (g_ascii_strcasecmp ((char *)child->name, "trackauth") == 0) {
							id = xmlNodeListGetString (doc, child->xmlChildrenNode, 0);
							continue;
						}
						if (g_ascii_strcasecmp ((char *)child->name, "freeTrackURL") == 0) {
							download_uri = xmlNodeListGetString (doc, child->xmlChildrenNode, 0);
							continue;
						}
					}
				}
			}
			g_clear_pointer (&app, xmlFree);
		/* Parse Amazon AMZ extensions */
		} else if (g_ascii_strcasecmp ((char *)node->name, "meta") == 0) {
			xmlChar *rel;

			rel = xmlGetProp (node, (const xmlChar *) "rel");
			if (rel != NULL) {
				if (g_ascii_strcasecmp ((char *) rel, "http://www.amazon.com/dmusic/primaryGenre") == 0)
					genre = xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
				else if (g_ascii_strcasecmp ((char *) rel, "http://www.amazon.com/dmusic/ASIN") == 0)
					id = xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
				else if (g_ascii_strcasecmp ((char *) rel, "http://www.amazon.com/dmusic/fileSize") == 0)
					filesize = xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
				xmlFree (rel);
			}
		} else if (g_ascii_strcasecmp ((char *)node->name, "album") == 0)
			album = xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
		else if (g_ascii_strcasecmp ((char *)node->name, "trackauth") == 0)
			id = xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
	}

	if (uri == NULL) {
		retval = TOTEM_PL_PARSER_RESULT_ERROR;
		goto bail;
	}

	resolved_uri = totem_pl_parser_resolve_uri (base_file, (char *) uri);

	if (g_strcmp0 (resolved_uri, (char *) uri) == 0) {
		g_free (resolved_uri);
		totem_pl_parser_add_uri (parser,
					 TOTEM_PL_PARSER_FIELD_URI, uri,
					 TOTEM_PL_PARSER_FIELD_TITLE, title,
					 TOTEM_PL_PARSER_FIELD_DURATION_MS, duration,
					 TOTEM_PL_PARSER_FIELD_IMAGE_URI, image_uri,
					 TOTEM_PL_PARSER_FIELD_AUTHOR, artist,
					 TOTEM_PL_PARSER_FIELD_ALBUM, album,
					 TOTEM_PL_PARSER_FIELD_MOREINFO, moreinfo,
					 TOTEM_PL_PARSER_FIELD_DOWNLOAD_URI, download_uri,
					 TOTEM_PL_PARSER_FIELD_ID, id,
					 TOTEM_PL_PARSER_FIELD_GENRE, genre,
					 TOTEM_PL_PARSER_FIELD_FILESIZE, filesize,
					 TOTEM_PL_PARSER_FIELD_SUBTITLE_URI, subtitle,
					 TOTEM_PL_PARSER_FIELD_PLAYING, playing,
					 TOTEM_PL_PARSER_FIELD_CONTENT_TYPE, mime_type,
					 TOTEM_PL_PARSER_FIELD_STARTTIME, starttime,
					 NULL);
	} else {
		resolved = g_file_new_for_uri (resolved_uri);
		g_free (resolved_uri);

		totem_pl_parser_add_uri (parser,
					 TOTEM_PL_PARSER_FIELD_FILE, resolved,
					 TOTEM_PL_PARSER_FIELD_TITLE, title,
					 TOTEM_PL_PARSER_FIELD_DURATION_MS, duration,
					 TOTEM_PL_PARSER_FIELD_IMAGE_URI, image_uri,
					 TOTEM_PL_PARSER_FIELD_AUTHOR, artist,
					 TOTEM_PL_PARSER_FIELD_ALBUM, album,
					 TOTEM_PL_PARSER_FIELD_MOREINFO, moreinfo,
					 TOTEM_PL_PARSER_FIELD_DOWNLOAD_URI, download_uri,
					 TOTEM_PL_PARSER_FIELD_ID, id,
					 TOTEM_PL_PARSER_FIELD_GENRE, genre,
					 TOTEM_PL_PARSER_FIELD_FILESIZE, filesize,
					 TOTEM_PL_PARSER_FIELD_SUBTITLE_URI, subtitle,
					 TOTEM_PL_PARSER_FIELD_PLAYING, playing,
					 TOTEM_PL_PARSER_FIELD_CONTENT_TYPE, mime_type,
					 TOTEM_PL_PARSER_FIELD_STARTTIME, starttime,
					 NULL);
		g_object_unref (resolved);
	}

	retval = TOTEM_PL_PARSER_RESULT_SUCCESS;

bail:
	SAFE_FREE (title);
	SAFE_FREE (uri);
	SAFE_FREE (image_uri);
	SAFE_FREE (artist);
	SAFE_FREE (album);
	SAFE_FREE (duration);
	SAFE_FREE (moreinfo);
	SAFE_FREE (download_uri);
	SAFE_FREE (id);
	SAFE_FREE (genre);

	return retval;
}

static gboolean
parse_xspf_trackList (TotemPlParser *parser, GFile *base_file, xmlDocPtr doc,
		xmlNodePtr parent)
{
	xmlNodePtr node;
	TotemPlParserResult retval = TOTEM_PL_PARSER_RESULT_ERROR;

	for (node = parent->children; node != NULL; node = node->next)
	{
		if (node->name == NULL)
			continue;

		if (g_ascii_strcasecmp ((char *)node->name, "track") == 0)
			if (parse_xspf_track (parser, base_file, doc, node) != FALSE)
				retval = TOTEM_PL_PARSER_RESULT_SUCCESS;
	}

	return retval;
}

static gboolean
parse_xspf_entries (TotemPlParser *parser,
		    GFile         *file,
		    GFile         *base_file,
		    xmlDocPtr      doc,
		    xmlNodePtr     parent)
{
	xmlNodePtr node;
	TotemPlParserResult retval = TOTEM_PL_PARSER_RESULT_ERROR;
	const xmlChar *title;
	char *uri;

	uri = g_file_get_uri (file);
	title = NULL;

	/* We go through the list twice to avoid the playlist-started
	 * signal being out of order */

	for (node = parent->children; node != NULL; node = node->next) {
		if (node->name == NULL)
			continue;

		if (g_ascii_strcasecmp ((char *)node->name, "title") == 0) {
			title = xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
			break;
		}
	}

	totem_pl_parser_add_uri (parser,
				 TOTEM_PL_PARSER_FIELD_IS_PLAYLIST, TRUE,
				 TOTEM_PL_PARSER_FIELD_URI, uri,
				 TOTEM_PL_PARSER_FIELD_TITLE, title,
				 TOTEM_PL_PARSER_FIELD_CONTENT_TYPE, "application/xspf+xml",
				 NULL);

	for (node = parent->children; node != NULL; node = node->next) {
		if (node->name == NULL)
			continue;

		if (g_ascii_strcasecmp ((char *)node->name, "trackList") == 0) {
			if (parse_xspf_trackList (parser, base_file, doc, node) != FALSE)
				retval = TOTEM_PL_PARSER_RESULT_SUCCESS;
		}
	}

	if (uri != NULL) {
		totem_pl_parser_playlist_end (parser, uri);
		g_free (uri);
	}

	SAFE_FREE (title);

	return retval;
}

static gboolean
is_xspf_doc (xmlDocPtr doc)
{
	/* If the document has no root, or no name */
	if(!doc ||
	   !doc->children ||
	   !doc->children->name ||
	   g_ascii_strcasecmp ((char *)doc->children->name, "playlist") != 0) {
		return FALSE;
	}
	return TRUE;
}

TotemPlParserResult
totem_pl_parser_add_xspf_with_contents (TotemPlParser *parser,
					GFile *file,
					GFile *base_file,
					const char *contents,
					TotemPlParseData *parse_data)
{
	xmlDocPtr doc;
	xmlNodePtr node;
	TotemPlParserResult retval = TOTEM_PL_PARSER_RESULT_UNHANDLED;

	xmlSetGenericErrorFunc (NULL, (xmlGenericErrorFunc) debug_noop);
	doc = xmlParseMemory (contents, strlen (contents));
	if (doc == NULL)
		doc = xmlRecoverMemory (contents, strlen (contents));

	if (is_xspf_doc (doc) == FALSE) {
		if (doc != NULL)
			xmlFreeDoc(doc);
		return TOTEM_PL_PARSER_RESULT_ERROR;
	}

	for (node = doc->children; node != NULL; node = node->next) {
		if (parse_xspf_entries (parser, file, base_file, doc, node) != FALSE)
			retval = TOTEM_PL_PARSER_RESULT_SUCCESS;
	}

	xmlFreeDoc(doc);
	return retval;
}

TotemPlParserResult
totem_pl_parser_add_xspf (TotemPlParser *parser,
			  GFile *file,
			  GFile *base_file,
			  TotemPlParseData *parse_data,
			  gpointer data)
{
	xmlDocPtr doc;
	xmlNodePtr node;
	TotemPlParserResult retval = TOTEM_PL_PARSER_RESULT_UNHANDLED;

	doc = totem_pl_parser_parse_xml_file (file);
	if (is_xspf_doc (doc) == FALSE) {
		if (doc != NULL)
			xmlFreeDoc(doc);
		return TOTEM_PL_PARSER_RESULT_ERROR;
	}

	for (node = doc->children; node != NULL; node = node->next) {
		if (parse_xspf_entries (parser, file, base_file, doc, node) != FALSE)
			retval = TOTEM_PL_PARSER_RESULT_SUCCESS;
	}

	xmlFreeDoc(doc);
	return retval;
}
#endif /* !TOTEM_PL_PARSER_MINI */

