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

#ifndef TOTEM_PL_PARSER_MINI
#include "totem-disc.h"
#endif /* !TOTEM_PL_PARSER_MINI */

#ifdef HAVE_QUVI
#include <quvi/quvi.h>
#endif /* HAVE_QUVI */

#include "totem-pl-parser-mini.h"
#include "totem-pl-parser-videosite.h"
#include "totem-pl-parser-private.h"

gboolean
totem_pl_parser_is_videosite (const char *uri, gboolean debug)
{
#ifdef HAVE_QUVI
	quvi_t handle;
	QUVIcode rc;

	if (quvi_init (&handle) != QUVI_OK)
		return FALSE;

	rc = quvi_supported(handle, (char *) uri);
	quvi_close (&handle);

	if (debug)
		g_print ("Checking videosite for URI '%s' returned %d (%s)\n",
			 uri, rc, (rc == QUVI_OK) ? "true" : "false");

	return (rc == QUVI_OK);
#else
	return FALSE;
#endif /* HAVE_QUVI */
}

#ifndef TOTEM_PL_PARSER_MINI

#define getprop(prop, p)					\
	if (quvi_getprop (v, prop, &p) != QUVI_OK)		\
		p = NULL;

TotemPlParserResult
totem_pl_parser_add_videosite (TotemPlParser *parser,
			       GFile *file,
			       GFile *base_file,
			       TotemPlParseData *parse_data,
			       gpointer data)
{
#ifdef HAVE_QUVI
	QUVIcode rc;
	quvi_t handle;
	quvi_media_t v;
	char *uri;
	/* properties */
	const char *video_uri;
	double length;
	char *length_str;
	const char *title;
	const char *id;
	const char *page_uri;
	const char *starttime;
	const char *content_type;
	const char *thumb_url;
	double duration;
	char *duration_str;

	if (quvi_init (&handle) != QUVI_OK)
		return TOTEM_PL_PARSER_RESULT_ERROR;

	quvi_setopt(handle, QUVIOPT_NOVERIFY, TRUE);
	quvi_setopt(handle, QUVIOPT_FORMAT, "best");

	uri = g_file_get_uri (file);
	rc = quvi_parse(handle, uri, &v);
	if (rc != QUVI_OK) {
		if (totem_pl_parser_is_debugging_enabled (parser))
			g_print ("quvi_parse for '%s' returned %d\n", uri, rc);
		g_free (uri);
		quvi_close (&handle);
		if (rc == QUVI_NOSUPPORT)
			return TOTEM_PL_PARSER_RESULT_UNHANDLED;
		return TOTEM_PL_PARSER_RESULT_ERROR;
	}

	getprop (QUVIPROP_MEDIAURL, video_uri);
	if (quvi_getprop (v, QUVIPROP_MEDIACONTENTLENGTH, &length) == QUVI_OK && length)
		length_str = g_strdup_printf ("%f", length);
	else
		length_str = NULL;
	getprop (QUVIPROP_PAGETITLE, title);
	getprop (QUVIPROP_MEDIAID, id);
	getprop (QUVIPROP_PAGEURL, page_uri);
	getprop (QUVIPROP_STARTTIME, starttime);
	getprop (QUVIPROP_MEDIACONTENTTYPE, content_type);
	getprop (QUVIPROP_MEDIATHUMBNAILURL, thumb_url);
	if (quvi_getprop (v, QUVIPROP_MEDIADURATION, &duration) == QUVI_OK && duration)
		duration_str = g_strdup_printf ("%f", duration);
	else
		duration_str = NULL;

	if (video_uri != NULL)
		totem_pl_parser_add_uri (parser,
					 TOTEM_PL_PARSER_FIELD_TITLE, title,
					 TOTEM_PL_PARSER_FIELD_ID, id,
					 TOTEM_PL_PARSER_FIELD_MOREINFO, page_uri,
					 TOTEM_PL_PARSER_FIELD_URI, video_uri,
					 TOTEM_PL_PARSER_FIELD_FILESIZE, length_str,
					 TOTEM_PL_PARSER_FIELD_STARTTIME, starttime,
					 TOTEM_PL_PARSER_FIELD_CONTENT_TYPE, content_type,
					 TOTEM_PL_PARSER_FIELD_IMAGE_URI, thumb_url,
					 TOTEM_PL_PARSER_FIELD_DURATION, duration_str,
					 NULL);
	g_free (uri);
	g_free (length_str);
	g_free (duration_str);

	quvi_parse_close (&v);
	quvi_close (&handle);

	return TOTEM_PL_PARSER_RESULT_SUCCESS;
#else
	return TOTEM_PL_PARSER_RESULT_UNHANDLED;
#endif /* !HAVE_QUVI */
}

#endif /* !TOTEM_PL_PARSER_MINI */

