/*
   Copyright (C) 2013 Bastien Nocera <hadess@hadess.net>

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

#include <locale.h>

#include <glib.h>
#include <quvi.h>
#include "totem-pl-parser.h"

#define BASE 20

static char *url = NULL;
static gboolean check = FALSE;
static gboolean debug = FALSE;

const GOptionEntry options[] = {
	{ "url", 'u', 0, G_OPTION_ARG_FILENAME, &url, "URL of the video site page", NULL },
	{ "check", 'c', 0, G_OPTION_ARG_NONE, &check, "Check whether this URL is supported", NULL },
	{ "debug", 'd', 0, G_OPTION_ARG_NONE, &debug, "Turn on debug mode", NULL },
	{ NULL }
};

static gboolean
supports_uri (const char *uri)
{
	quvi_t q;
	QuviBoolean r;

	q = quvi_new ();
	r = quvi_supports (q, uri, QUVI_SUPPORTS_MODE_OFFLINE, QUVI_SUPPORTS_TYPE_ANY);
	quvi_free (q);

	return r;
}

static struct {
	const char *container;
	const char *content_type;
} containers [] = {
	{ "webm", "video/webm" },
};

static const char *
container_to_content_type (const char *container)
{
	guint i;

	if (container == NULL)
		return NULL;
	for (i = 0; i < G_N_ELEMENTS (containers); i++) {
		if (g_str_equal (container, containers[i].container))
			return containers[i].content_type;
	}
	return NULL;
}

static void
print (const char *name,
       const char *value)
{
	g_return_if_fail (name != NULL);

	if (value == NULL)
		return;

	g_print ("%s=%s\n", name, value);
}

static void
parse_videosite (const char *uri)
{
	quvi_t q;
	quvi_media_t qm;
	/* properties */
	const char *video_uri;
	const char *title;
	const char *id;
	const char *content_type;
	const char *thumb_url;
	const char *container;
	double duration;
	double starttime;
	char *duration_str = NULL;
	char *starttime_str = NULL;

	if (!supports_uri (uri)) {
		g_print ("TOTEM_PL_PARSER_RESULT_UNHANDLED");
		return;
	}

	q = quvi_new ();
	qm = quvi_media_new (q, uri);

	/* Empty results list? */
	if (quvi_media_stream_next(qm) != QUVI_TRUE) {
		if (debug)
			g_print ("Parsing '%s' failed with error: %s\n",
				 uri, quvi_errmsg (q));
		g_print ("TOTEM_PL_PARSER_RESULT_ERROR");
		goto out;
	}

	/* Choose the best stream */
	quvi_media_stream_choose_best (qm);

	quvi_media_get (qm, QUVI_MEDIA_PROPERTY_TITLE, &title);
	quvi_media_get (qm, QUVI_MEDIA_PROPERTY_ID, &id);
	quvi_media_get (qm, QUVI_MEDIA_PROPERTY_THUMBNAIL_URL, &thumb_url);
	quvi_media_get (qm, QUVI_MEDIA_PROPERTY_DURATION_MS, &duration);
	if (duration)
		duration_str = g_strdup_printf ("%f", duration);
	quvi_media_get (qm, QUVI_MEDIA_STREAM_PROPERTY_URL, &video_uri);
	quvi_media_get (qm, QUVI_MEDIA_PROPERTY_START_TIME_MS, &starttime);
	if (starttime)
		starttime_str = g_strdup_printf ("%f", starttime);

	quvi_media_get (qm, QUVI_MEDIA_STREAM_PROPERTY_CONTAINER, &container);
	content_type = container_to_content_type (container);

	if (video_uri != NULL) {
		print (TOTEM_PL_PARSER_FIELD_TITLE, title);
		print (TOTEM_PL_PARSER_FIELD_ID, id);
		print (TOTEM_PL_PARSER_FIELD_MOREINFO, uri);
		print (TOTEM_PL_PARSER_FIELD_URI, video_uri);
		print (TOTEM_PL_PARSER_FIELD_STARTTIME, starttime_str);
		print (TOTEM_PL_PARSER_FIELD_CONTENT_TYPE, content_type);
		print (TOTEM_PL_PARSER_FIELD_IMAGE_URI, thumb_url);
		print (TOTEM_PL_PARSER_FIELD_DURATION, duration_str);
	}

	g_free (starttime_str);
	g_free (duration_str);

out:
	quvi_media_free (qm);
	quvi_free (q);
}

int main (int argc, char **argv)
{
	GOptionContext *context;

	setlocale (LC_ALL, "");

	context = g_option_context_new (NULL);
	g_option_context_set_summary (context, "totem-pl-parser libquvi Helper");
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);

	if (url == NULL) {
		char *txt;

		txt = g_option_context_get_help (context, FALSE, NULL);
		g_print ("%s", txt);
		g_free (txt);

		g_option_context_free (context);

		return 1;
	}
	g_option_context_free (context);

	if (check) {
		g_print ("%s", supports_uri (url) ? "TRUE" : "FALSE");
		return 0;
	}

	parse_videosite (url);

	return 0;
}
