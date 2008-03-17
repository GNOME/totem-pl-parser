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
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Bastien Nocera <hadess@hadess.net>
 */

#include "config.h"

#ifndef TOTEM_PL_PARSER_MINI
#include <string.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <gtk/gtk.h>
#include <libgnomevfs/gnome-vfs.h>
#include "totem-pl-parser.h"
#include "totemplparser-marshal.h"
#endif /* !TOTEM_PL_PARSER_MINI */

#include "totem-pl-parser-mini.h"
#include "totem-pl-parser-xspf.h"
#include "totem-pl-parser-private.h"

#ifndef TOTEM_PL_PARSER_MINI

#define SAFE_FREE(x) { if (x != NULL) xmlFree (x); }

static xmlDocPtr
totem_pl_parser_parse_xml_file (const char *url)
{
	xmlDocPtr doc;
	char *contents;
	int size;

	if (gnome_vfs_read_entire_file (url, &size, &contents) != GNOME_VFS_OK)
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

	doc = xmlParseMemory (contents, size);
	if (doc == NULL)
		doc = xmlRecoverMemory (contents, size);
	g_free (contents);

	return doc;
}

gboolean
totem_pl_parser_write_xspf (TotemPlParser *parser, GtkTreeModel *model,
			   TotemPlParserIterFunc func, 
			   const char *output, const char *title,
			   gpointer user_data, GError **error)
{
	GnomeVFSHandle *handle;
	GnomeVFSResult res;
	int num_entries_total, num_entries, i;
	char *buf;
	gboolean success;

	num_entries = totem_pl_parser_num_entries (parser, model, func, user_data);
	num_entries_total = gtk_tree_model_iter_n_children (model, NULL);

	res = gnome_vfs_open (&handle, output, GNOME_VFS_OPEN_WRITE);
	if (res == GNOME_VFS_ERROR_NOT_FOUND) {
		res = gnome_vfs_create (&handle, output,
				GNOME_VFS_OPEN_WRITE, FALSE,
				GNOME_VFS_PERM_USER_WRITE
				| GNOME_VFS_PERM_USER_READ
				| GNOME_VFS_PERM_GROUP_READ);
	}

	if (res != GNOME_VFS_OK) {
		g_set_error(error,
			    TOTEM_PL_PARSER_ERROR,
			    TOTEM_PL_PARSER_ERROR_VFS_OPEN,
			    _("Couldn't open file '%s': %s"),
			    output, gnome_vfs_result_to_string (res));
		return FALSE;
	}

	buf = g_strdup_printf ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
				"<playlist version=\"1\" xmlns=\"http://xspf.org/ns/0/\">\n"
				" <trackList>\n");
	success = totem_pl_parser_write_string (handle, buf, error);
	g_free (buf);
	if (success == FALSE)
	{
		gnome_vfs_close (handle);
		return FALSE;
	}

	for (i = 1; i <= num_entries_total; i++) {
		GtkTreeIter iter;
		char *url, *url_escaped, *relative, *title;
		gboolean custom_title;

		if (gtk_tree_model_iter_nth_child (model, &iter, NULL, i - 1) == FALSE)
			continue;

		func (model, &iter, &url, &title, &custom_title, user_data);

		if (totem_pl_parser_scheme_is_ignored (parser, url) != FALSE)
		{
			g_free (url);
			g_free (title);
			continue;
		}

		relative = totem_pl_parser_relative (url, output);
		url_escaped = g_markup_escape_text (relative ? relative : url, -1);
		buf = g_strdup_printf ("  <track>\n"
					"   <location>%s</location>\n", url_escaped);
		success = totem_pl_parser_write_string (handle, buf, error);
		g_free (url);
		g_free (url_escaped);
		g_free (relative);
		g_free (buf);
		if (success == FALSE)
		{
			gnome_vfs_close (handle);
			g_free (title);
			return FALSE;
		}

		if (custom_title == TRUE)
			buf = g_strdup_printf ("   <title>%s</title>\n"
						"  </track>\n", title);
		else
			buf = g_strdup_printf ("  </track>\n");
		
		success = totem_pl_parser_write_string (handle, buf, error);
		g_free (buf);
		g_free (title);
		if (success == FALSE)
		{
			gnome_vfs_close (handle);
			return FALSE;
		}
	}

	buf = g_strdup_printf (" </trackList>\n"
				"</playlist>");
	success = totem_pl_parser_write_string (handle, buf, error);
	g_free (buf);
	gnome_vfs_close (handle);

	return success;
}

static gboolean
parse_xspf_track (TotemPlParser *parser, char *base, xmlDocPtr doc,
		xmlNodePtr parent)
{
	xmlNodePtr node;
	xmlChar *title, *url, *image_url, *artist, *album, *duration, *moreinfo;
	xmlChar *download_url, *id;
	char *fullpath;
	TotemPlParserResult retval = TOTEM_PL_PARSER_RESULT_ERROR;
	
	fullpath = NULL;
	title = NULL;
	url = NULL;
	image_url = NULL;
	artist = NULL;
	album = NULL;
	duration = NULL;
	moreinfo = NULL;
	download_url = NULL;
	id = NULL;

	for (node = parent->children; node != NULL; node = node->next)
	{
		if (node->name == NULL)
			continue;

		if (g_ascii_strcasecmp ((char *)node->name, "location") == 0)
			url = xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
		else if (g_ascii_strcasecmp ((char *)node->name, "title") == 0)
			title = xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
		else if (g_ascii_strcasecmp ((char *)node->name, "image") == 0)
			image_url = xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
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
					download_url = xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
				xmlFree (rel);
			} else {
				/* If we don't have a rel="", then it's not a last.fm playlist */
				moreinfo = xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
			}
		} else if (g_ascii_strcasecmp ((char *)node->name, "album") == 0)
			album = xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
		else if (g_ascii_strcasecmp ((char *)node->name, "trackauth") == 0)
			id = xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
	}

	if (url == NULL) {
		retval = TOTEM_PL_PARSER_RESULT_ERROR;
		goto bail;
	}

	fullpath = totem_pl_parser_resolve_url (base, (char *)url);
	totem_pl_parser_add_url (parser,
				 TOTEM_PL_PARSER_FIELD_URL, fullpath,
				 TOTEM_PL_PARSER_FIELD_TITLE, title,
				 TOTEM_PL_PARSER_FIELD_DURATION_MS, duration,
				 TOTEM_PL_PARSER_FIELD_IMAGE_URL, image_url,
				 TOTEM_PL_PARSER_FIELD_AUTHOR, artist,
				 TOTEM_PL_PARSER_FIELD_ALBUM, album,
				 TOTEM_PL_PARSER_FIELD_MOREINFO, moreinfo,
				 TOTEM_PL_PARSER_FIELD_DOWNLOAD_URL, download_url,
				 TOTEM_PL_PARSER_FIELD_ID, id,
				 NULL);

	retval = TOTEM_PL_PARSER_RESULT_SUCCESS;

bail:
	SAFE_FREE (title);
	SAFE_FREE (url);
	SAFE_FREE (image_url);
	SAFE_FREE (artist);
	SAFE_FREE (album);
	SAFE_FREE (duration);
	SAFE_FREE (moreinfo);
	g_free (fullpath);

	return retval;
}

static gboolean
parse_xspf_trackList (TotemPlParser *parser, char *base, xmlDocPtr doc,
		xmlNodePtr parent)
{
	xmlNodePtr node;
	TotemPlParserResult retval = TOTEM_PL_PARSER_RESULT_ERROR;

	for (node = parent->children; node != NULL; node = node->next)
	{
		if (node->name == NULL)
			continue;

		if (g_ascii_strcasecmp ((char *)node->name, "track") == 0)
			if (parse_xspf_track (parser, base, doc, node) != FALSE)
				retval = TOTEM_PL_PARSER_RESULT_SUCCESS;
	}

	return retval;
}

static gboolean
parse_xspf_entries (TotemPlParser *parser, char *base, xmlDocPtr doc,
		xmlNodePtr parent)
{
	xmlNodePtr node;
	TotemPlParserResult retval = TOTEM_PL_PARSER_RESULT_ERROR;

	for (node = parent->children; node != NULL; node = node->next) {
		if (node->name == NULL)
			continue;

		if (g_ascii_strcasecmp ((char *)node->name, "trackList") == 0)
			if (parse_xspf_trackList (parser, base, doc, node) != FALSE)
				retval = TOTEM_PL_PARSER_RESULT_SUCCESS;
	}

	return retval;
}

TotemPlParserResult
totem_pl_parser_add_xspf (TotemPlParser *parser,
			  GFile *file,
			  GFile *_base_file,
			  gpointer data)
{
#if 0
	xmlDocPtr doc;
	xmlNodePtr node;
	char *base;
	TotemPlParserResult retval = TOTEM_PL_PARSER_RESULT_UNHANDLED;

	doc = totem_pl_parser_parse_xml_file (url);

	/* If the document has no root, or no name */
	if(!doc || !doc->children
			|| !doc->children->name
			|| g_ascii_strcasecmp ((char *)doc->children->name,
				"playlist") != 0) {
		if (doc != NULL)
			xmlFreeDoc(doc);
		return TOTEM_PL_PARSER_RESULT_ERROR;
	}

	base = totem_pl_parser_base_url (url);

	for (node = doc->children; node != NULL; node = node->next)
		if (parse_xspf_entries (parser, base, doc, node) != FALSE)
			retval = TOTEM_PL_PARSER_RESULT_SUCCESS;

	g_free (base);
	xmlFreeDoc(doc);
	return retval;
#endif
}
#endif /* !TOTEM_PL_PARSER_MINI */

