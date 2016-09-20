/* 
   Copyright (C) 2007 Bastien Nocera <hadess@hadess.net>

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
#include <libsoup/soup.h>
#include "xmlparser.h"
#include "totem-pl-parser.h"
#include "totem-disc.h"
#endif /* !TOTEM_PL_PARSER_MINI */

#include "totem-pl-parser-mini.h"
#include "totem-pl-parser-podcast.h"
#include "totem-pl-parser-videosite.h"
#include "totem-pl-parser-private.h"

#define RSS_NEEDLE "<rss "
#define RSS_NEEDLE2 "<rss\n"
#define ATOM_NEEDLE "<feed "
#define OPML_NEEDLE "<opml "

const char *
totem_pl_parser_is_rss (const char *data, gsize len)
{
	if (len == 0)
		return FALSE;
	if (len > MIME_READ_CHUNK_SIZE)
		len = MIME_READ_CHUNK_SIZE;

	if (g_strstr_len (data, len, RSS_NEEDLE) != NULL)
		return RSS_MIME_TYPE;
	if (g_strstr_len (data, len, RSS_NEEDLE2) != NULL)
		return RSS_MIME_TYPE;

	return NULL;
}

const char *
totem_pl_parser_is_atom (const char *data, gsize len)
{
	if (len == 0)
		return FALSE;
	if (len > MIME_READ_CHUNK_SIZE)
		len = MIME_READ_CHUNK_SIZE;

	if (g_strstr_len (data, len, ATOM_NEEDLE) != NULL)
		return ATOM_MIME_TYPE;

	return NULL;
}

const char *
totem_pl_parser_is_opml (const char *data, gsize len)
{
	if (len == 0)
		return FALSE;
	if (len > MIME_READ_CHUNK_SIZE)
		len = MIME_READ_CHUNK_SIZE;

	if (g_strstr_len (data, len, OPML_NEEDLE) != NULL)
		return OPML_MIME_TYPE;

	return NULL;
}

const char *
totem_pl_parser_is_xml_feed (const char *data, gsize len)
{
	if (totem_pl_parser_is_rss (data, len) != NULL)
		return RSS_MIME_TYPE;
	if (totem_pl_parser_is_atom (data, len) != NULL)
		return ATOM_MIME_TYPE;
	if (totem_pl_parser_is_opml (data, len) != NULL)
		return OPML_MIME_TYPE;
	return NULL;
}

#ifndef TOTEM_PL_PARSER_MINI

static gboolean
is_image (const char *url)
{
	char *content_type;
	gboolean retval = FALSE;

	content_type = g_content_type_guess (url, NULL, 0, NULL);
	if (content_type == NULL)
		return FALSE;
	if (g_content_type_is_a (content_type, "image/*"))
		retval = TRUE;
	g_free (content_type);
	return retval;
}

static TotemPlParserResult
parse_rss_item (TotemPlParser *parser, xml_node_t *parent)
{
	const char *title, *uri, *description, *author, *img;
	const char *pub_date, *duration, *filesize, *content_type, *id;
	xml_node_t *node;

	title = uri = description = author = content_type = NULL;
	img = pub_date = duration = filesize = id = NULL;

	for (node = parent->child; node != NULL; node = node->next) {
		if (node->name == NULL)
			continue;

		if (g_ascii_strcasecmp (node->name, "title") == 0) {
			title = node->data;
		} else if (g_ascii_strcasecmp (node->name, "url") == 0) {
			uri = node->data;
		} else if (g_ascii_strcasecmp (node->name, "pubDate") == 0) {
			pub_date = node->data;
		} else if (g_ascii_strcasecmp (node->name, "guid") == 0) {
			id = node->data;
		} else if (g_ascii_strcasecmp (node->name, "description") == 0
			   || g_ascii_strcasecmp (node->name, "itunes:summary") == 0) {
			description = node->data;
		} else if (g_ascii_strcasecmp (node->name, "author") == 0
			   || g_ascii_strcasecmp (node->name, "itunes:author") == 0) {
			author = node->data;
		} else if (g_ascii_strcasecmp (node->name, "itunes:duration") == 0) {
			duration = node->data;
		} else if (g_ascii_strcasecmp (node->name, "length") == 0) {
			filesize = node->data;
		} else if (g_ascii_strcasecmp (node->name, "media:content") == 0) {
			const char *tmp;

			tmp = xml_parser_get_property (node, "type");
			if (tmp != NULL &&
			    g_str_has_prefix (tmp, "audio/") == FALSE) {
				if (g_str_has_prefix (tmp, "image/"))
					img = xml_parser_get_property (node, "url");
				continue;
			}
			content_type = tmp;

			tmp = xml_parser_get_property (node, "url");
			if (tmp != NULL)
				uri = tmp;
			else
				continue;

			tmp = xml_parser_get_property (node, "fileSize");
			if (tmp != NULL)
				filesize = tmp;

			tmp = xml_parser_get_property (node, "duration");
			if (tmp != NULL)
				duration = tmp;
		} else if (g_ascii_strcasecmp (node->name, "enclosure") == 0) {
			const char *tmp;

			tmp = xml_parser_get_property (node, "url");
			if (tmp != NULL && is_image (tmp) == FALSE)
				uri = tmp;
			else
				continue;
			tmp = xml_parser_get_property (node, "length");
			if (tmp != NULL)
				filesize = tmp;
		} else if (g_ascii_strcasecmp (node->name, "link") == 0 &&
			   totem_pl_parser_is_videosite (node->data, FALSE) != FALSE) {
			uri = node->data;
		}
	}

	if (id != NULL &&
	    uri == NULL &&
	    totem_pl_parser_is_videosite (id, FALSE) != FALSE)
		uri = id;

	if (uri != NULL) {
		totem_pl_parser_add_uri (parser,
					 TOTEM_PL_PARSER_FIELD_URI, uri,
					 TOTEM_PL_PARSER_FIELD_ID, id,
					 TOTEM_PL_PARSER_FIELD_TITLE, title,
					 TOTEM_PL_PARSER_FIELD_PUB_DATE, pub_date,
					 TOTEM_PL_PARSER_FIELD_DESCRIPTION, description,
					 TOTEM_PL_PARSER_FIELD_AUTHOR, author,
					 TOTEM_PL_PARSER_FIELD_DURATION, duration,
					 TOTEM_PL_PARSER_FIELD_FILESIZE, filesize,
					 TOTEM_PL_PARSER_FIELD_CONTENT_TYPE, content_type,
					 TOTEM_PL_PARSER_FIELD_IMAGE_URI, img,
					 NULL);
	}

	return TOTEM_PL_PARSER_RESULT_SUCCESS;
}

static TotemPlParserResult
parse_rss_items (TotemPlParser *parser, const char *uri, xml_node_t *parent)
{
	const char *title, *language, *description, *author;
	const char *contact, *img, *pub_date, *copyright;
	xml_node_t *node;

	title = language = description = author = NULL;
	contact = img = pub_date = copyright = NULL;

	/* We need to parse for the feed metadata first, then for the items */
	for (node = parent->child; node != NULL; node = node->next) {
		if (node->name == NULL)
			continue;

		if (g_ascii_strcasecmp (node->name, "title") == 0) {
			title = node->data;
		} else if (g_ascii_strcasecmp (node->name, "language") == 0) {
			language = node->data;
		} else if (g_ascii_strcasecmp (node->name, "description") == 0
			 || g_ascii_strcasecmp (node->name, "itunes:subtitle") == 0) {
		    	description = node->data;
		} else if (g_ascii_strcasecmp (node->name, "author") == 0
			 || g_ascii_strcasecmp (node->name, "itunes:author") == 0
			 || (g_ascii_strcasecmp (node->name, "generator") == 0 && author == NULL)) {
		    	author = node->data;
		} else if (g_ascii_strcasecmp (node->name, "webMaster") == 0) {
			contact = node->data;
		} else if (g_ascii_strcasecmp (node->name, "image") == 0) {
			img = node->data;
		} else if (g_ascii_strcasecmp (node->name, "itunes:image") == 0) {
			const char *href;

			href = xml_parser_get_property (node, "href");
			if (href != NULL)
				img = href;
		} else if (g_ascii_strcasecmp (node->name, "lastBuildDate") == 0
			 || g_ascii_strcasecmp (node->name, "pubDate") == 0) {
		    	pub_date = node->data;
		} else if (g_ascii_strcasecmp (node->name, "copyright") == 0) {
			copyright = node->data;
		}
	}

	/* Send the info we already have about the feed */
	totem_pl_parser_add_uri (parser,
				 TOTEM_PL_PARSER_FIELD_IS_PLAYLIST, TRUE,
				 TOTEM_PL_PARSER_FIELD_URI, uri,
				 TOTEM_PL_PARSER_FIELD_TITLE, title,
				 TOTEM_PL_PARSER_FIELD_LANGUAGE, language,
				 TOTEM_PL_PARSER_FIELD_DESCRIPTION, description,
				 TOTEM_PL_PARSER_FIELD_AUTHOR, author,
				 TOTEM_PL_PARSER_FIELD_PUB_DATE, pub_date,
				 TOTEM_PL_PARSER_FIELD_COPYRIGHT, copyright,
				 TOTEM_PL_PARSER_FIELD_IMAGE_URI, img,
				 TOTEM_PL_PARSER_FIELD_CONTACT, contact,
				 NULL);

	for (node = parent->child; node != NULL; node = node->next) {
		if (node->name == NULL)
			continue;

		if (g_ascii_strcasecmp (node->name, "item") == 0)
			parse_rss_item (parser, node);
	}

	totem_pl_parser_playlist_end (parser, uri);

	return TOTEM_PL_PARSER_RESULT_SUCCESS;
}

TotemPlParserResult
totem_pl_parser_add_rss (TotemPlParser *parser,
			 GFile *file,
			 GFile *base_file,
			 TotemPlParseData *parse_data,
			 gpointer data)
{
#ifndef HAVE_GMIME
	WARN_NO_GMIME;
#else
	xml_node_t* doc, *channel;
	char *contents;
	gsize size;

	if (g_file_load_contents (file, NULL, &contents, &size, NULL, NULL) == FALSE)
		return TOTEM_PL_PARSER_RESULT_ERROR;

	doc = totem_pl_parser_parse_xml_relaxed (contents, size);
	if (doc == NULL) {
		g_free (contents);
		return TOTEM_PL_PARSER_RESULT_ERROR;
	}

	/* If the document has no name */
	if (doc->name == NULL
	    || (g_ascii_strcasecmp (doc->name , "rss") != 0
		&& g_ascii_strcasecmp (doc->name , "rss\n") != 0)) {
		g_free (contents);
		xml_parser_free_tree (doc);
		return TOTEM_PL_PARSER_RESULT_ERROR;
	}

	for (channel = doc->child; channel != NULL; channel = channel->next) {
		if (g_ascii_strcasecmp (channel->name, "channel") == 0) {
			char *uri;

			uri = g_file_get_uri (file);
			parse_rss_items (parser, uri, channel);
			g_free (uri);

			/* One channel per file */
			break;
		}
	}

	g_free (contents);
	xml_parser_free_tree (doc);

	return TOTEM_PL_PARSER_RESULT_SUCCESS;
#endif /* !HAVE_GMIME */
}

/* http://www.apple.com/itunes/store/podcaststechspecs.html */
TotemPlParserResult
totem_pl_parser_add_itpc (TotemPlParser *parser,
			  GFile *file,
			  GFile *base_file,
			  TotemPlParseData *parse_data,
			  gpointer data)
{
#ifndef HAVE_GMIME
	WARN_NO_GMIME;
#else
	TotemPlParserResult ret;
	char *uri, *new_uri, *uri_scheme;
	GFile *new_file;

	uri = g_file_get_uri (file);
	uri_scheme = g_file_get_uri_scheme (file);
	new_uri = g_strdup_printf ("http%s", uri + strlen (uri_scheme));
	g_free (uri);
	g_free (uri_scheme);

	new_file = g_file_new_for_uri (new_uri);
	g_free (new_uri);

	ret = totem_pl_parser_add_rss (parser, new_file, base_file, parse_data, data);

	g_object_unref (new_file);

	return ret;
#endif /* !HAVE_GMIME */
}

TotemPlParserResult
totem_pl_parser_add_zune (TotemPlParser *parser,
			  GFile *file,
			  GFile *base_file,
			  TotemPlParseData *parse_data,
			  gpointer data)
{
#ifndef HAVE_GMIME
	WARN_NO_GMIME;
#else
	TotemPlParserResult ret;
	char *uri, *new_uri;
	GFile *new_file;

	uri = g_file_get_uri (file);
	if (g_str_has_prefix (uri, "zune://subscribe/?") == FALSE) {
		g_free (uri);
		return TOTEM_PL_PARSER_RESULT_UNHANDLED;
	}

	new_uri = strchr (uri + strlen ("zune://subscribe/?"), '=');
	if (new_uri == NULL) {
		g_free (uri);
		return TOTEM_PL_PARSER_RESULT_UNHANDLED;
	}
	/* Skip over the '=' */
	new_uri++;

	new_file = g_file_new_for_uri (new_uri);
	g_free (uri);

	ret = totem_pl_parser_add_rss (parser, new_file, base_file, parse_data, data);

	g_object_unref (new_file);

	return ret;
#endif /* !HAVE_GMIME */
}

/* Atom docs:
 * http://www.atomenabled.org/developers/syndication/atom-format-spec.php#rfc.section.4.1
 * http://tools.ietf.org/html/rfc4287
 * http://tools.ietf.org/html/rfc4946 */
static TotemPlParserResult
parse_atom_entry (TotemPlParser *parser, xml_node_t *parent)
{
	const char *title, *author, *uri, *filesize;
	const char *copyright, *pub_date, *description;
	xml_node_t *node;

	title = author = uri = filesize = NULL;
	copyright = pub_date = description = NULL;

	for (node = parent->child; node != NULL; node = node->next) {
		if (node->name == NULL)
			continue;

		if (g_ascii_strcasecmp (node->name, "title") == 0) {
			title = node->data;
		} else if (g_ascii_strcasecmp (node->name, "author") == 0) {
			//FIXME
		} else if (g_ascii_strcasecmp (node->name, "link") == 0) {
			const char *rel;

			//FIXME how do we choose the default enclosure type?
			rel = xml_parser_get_property (node, "rel");
			if (g_ascii_strcasecmp (rel, "enclosure") == 0) {
				const char *href;

				//FIXME what's the difference between url and href there?
				href = xml_parser_get_property (node, "href");
				if (href == NULL)
					continue;
				uri = href;
				filesize = xml_parser_get_property (node, "length");
			} else if (g_ascii_strcasecmp (node->name, "license") == 0) {
				const char *href;

				href = xml_parser_get_property (node, "href");
				if (href == NULL)
					continue;
				/* This isn't really a copyright, but what the hey */
				copyright = href;
			}
		} else if (g_ascii_strcasecmp (node->name, "updated") == 0
			   || (g_ascii_strcasecmp (node->name, "modified") == 0 && pub_date == NULL)) {
			pub_date = node->data;
		} else if (g_ascii_strcasecmp (node->name, "summary") == 0
			   || (g_ascii_strcasecmp (node->name, "content") == 0 && description == NULL)) {
			const char *type;

			type = xml_parser_get_property (node, "content");
			if (type != NULL && g_ascii_strcasecmp (type, "text/plain") == 0)
				description = node->data;
		}
		//FIXME handle category
	}

	if (uri != NULL) {
		totem_pl_parser_add_uri (parser,
					 TOTEM_PL_PARSER_FIELD_TITLE, title,
					 TOTEM_PL_PARSER_FIELD_AUTHOR, author,
					 TOTEM_PL_PARSER_FIELD_URI, uri,
					 TOTEM_PL_PARSER_FIELD_FILESIZE, filesize,
					 TOTEM_PL_PARSER_FIELD_COPYRIGHT, copyright,
					 TOTEM_PL_PARSER_FIELD_PUB_DATE, pub_date,
					 TOTEM_PL_PARSER_FIELD_DESCRIPTION, description,
					 NULL);
	}

	return TOTEM_PL_PARSER_RESULT_SUCCESS;
}

static TotemPlParserResult
parse_atom_entries (TotemPlParser *parser, const char *uri, xml_node_t *parent)
{
	const char *title, *pub_date, *description;
	const char *author, *img;
	xml_node_t *node;
	gboolean started = FALSE;

	title = pub_date = description = NULL;
	author = img = NULL;

	for (node = parent->child; node != NULL; node = node->next) {
		if (node->name == NULL)
			continue;

		if (g_ascii_strcasecmp (node->name, "title") == 0) {
			title = node->data;
		} else if (g_ascii_strcasecmp (node->name, "tagline") == 0) {
		    	description = node->data;
		} else if (g_ascii_strcasecmp (node->name, "modified") == 0
			   || g_ascii_strcasecmp (node->name, "updated") == 0) {
			pub_date = node->data;
		} else if (g_ascii_strcasecmp (node->name, "author") == 0
			 || (g_ascii_strcasecmp (node->name, "generator") == 0 && author == NULL)) {
		    	author = node->data;
		} else if ((g_ascii_strcasecmp (node->name, "icon") == 0 && img == NULL)
			   || g_ascii_strcasecmp (node->name, "logo") == 0) {
			img = node->data;
		}

		if (g_ascii_strcasecmp (node->name, "entry") == 0) {
			if (started == FALSE) {
				/* Send the info we already have about the feed */
				totem_pl_parser_add_uri (parser,
							 TOTEM_PL_PARSER_FIELD_IS_PLAYLIST, TRUE,
							 TOTEM_PL_PARSER_FIELD_URI, uri,
							 TOTEM_PL_PARSER_FIELD_TITLE, title,
							 TOTEM_PL_PARSER_FIELD_DESCRIPTION, description,
							 TOTEM_PL_PARSER_FIELD_AUTHOR, author,
							 TOTEM_PL_PARSER_FIELD_PUB_DATE, pub_date,
							 TOTEM_PL_PARSER_FIELD_IMAGE_URI, img,
							 NULL);
				started = TRUE;
			}

			parse_atom_entry (parser, node);
		}
	}

	totem_pl_parser_playlist_end (parser, uri);

	return TOTEM_PL_PARSER_RESULT_SUCCESS;
}

TotemPlParserResult
totem_pl_parser_add_atom (TotemPlParser *parser,
			  GFile *file,
			  GFile *base_file,
			  TotemPlParseData *parse_data,
			  gpointer data)
{
#ifndef HAVE_GMIME
	WARN_NO_GMIME;
#else
	xml_node_t* doc;
	char *contents, *uri;
	gsize size;

	if (g_file_load_contents (file, NULL, &contents, &size, NULL, NULL) == FALSE)
		return TOTEM_PL_PARSER_RESULT_ERROR;

	doc = totem_pl_parser_parse_xml_relaxed (contents, size);
	if (doc == NULL) {
		g_free (contents);
		return TOTEM_PL_PARSER_RESULT_ERROR;
	}

	/* If the document has no name */
	if (doc->name == NULL
	    || g_ascii_strcasecmp (doc->name , "feed") != 0) {
		g_free (contents);
		xml_parser_free_tree (doc);
		return TOTEM_PL_PARSER_RESULT_ERROR;
	}

	uri = g_file_get_uri (file);
	parse_atom_entries (parser, uri, doc);
	g_free (uri);

	g_free (contents);
	xml_parser_free_tree (doc);

	return TOTEM_PL_PARSER_RESULT_SUCCESS;
#endif /* !HAVE_GMIME */
}

TotemPlParserResult
totem_pl_parser_add_xml_feed (TotemPlParser *parser,
			      GFile *file,
			      GFile *base_file,
			      TotemPlParseData *parse_data,
			      gpointer data)
{
#ifndef HAVE_GMIME
	WARN_NO_GMIME;
#else
	guint len;

	if (data == NULL)
		return TOTEM_PL_PARSER_RESULT_UNHANDLED;

	len = strlen (data);

	if (totem_pl_parser_is_rss (data, len) != FALSE)
		return totem_pl_parser_add_rss (parser, file, base_file, parse_data, data);
	if (totem_pl_parser_is_atom (data, len) != FALSE)
		return totem_pl_parser_add_atom (parser, file, base_file, parse_data, data);
	if (totem_pl_parser_is_opml (data, len) != FALSE)
		return totem_pl_parser_add_opml (parser, file, base_file, parse_data, data);

	return TOTEM_PL_PARSER_RESULT_UNHANDLED;
#endif /* !HAVE_GMIME */
}

static char *
totem_pl_parser_parse_json (char *data, gsize len, gboolean debug)
{
	char *s, *end;

	s = g_strstr_len (data, len, "feedUrl\":\"");
	if (s == NULL)
		return NULL;
	s += strlen ("feedUrl\":\"");
	if (*s == '\0')
		return NULL;
	end = g_strstr_len (s, len - (s - data), "\"");
	if (end == NULL)
		return NULL;
	return g_strndup (s, end - s);
}

static char *
get_itms_id (GFile *file)
{
	char *uri, *start, *end, *id;

	uri = g_file_get_uri (file);
	if (!uri)
		return NULL;

	start = strstr (uri, "/id");
	if (!start) {
		g_free (uri);
		return NULL;
	}

	end = strchr (start, '?');
	if (!end)
		end = strchr (start, '#');
	if (!end || end - start <= 3) {
		g_free (uri);
		return NULL;
	}

	id = g_strndup (start + 3, end - start - 3);
	g_free (uri);

	return id;
}

TotemPlParserResult
totem_pl_parser_add_itms (TotemPlParser *parser,
			  GFile *file,
			  GFile *base_file,
			  TotemPlParseData *parse_data,
			  gpointer data)
{
#ifndef HAVE_GMIME
	WARN_NO_GMIME;
#else
	GFile *json_file, *feed_file;
	TotemPlParserResult ret;
	char *contents, *id, *json_uri, *feed_url;
	gsize len;

	id = get_itms_id (file);
	if (id == NULL) {
		DEBUG(file, g_print ("Could not get ITMS ID for URL '%s'\n", uri));
		return TOTEM_PL_PARSER_RESULT_ERROR;
	}

	DEBUG(file, g_print ("Got ID '%s' for URL '%s'", id, uri));

	json_uri = g_strdup_printf ("https://itunes.apple.com/lookup?id=%s&entity=podcast", id);
	g_free (id);
	json_file = g_file_new_for_uri (json_uri);
	g_free (json_uri);

	if (g_file_load_contents (json_file, NULL, &contents, &len, NULL, NULL) == FALSE) {
		DEBUG(json_file, g_print ("Failed to load URL '%s'\n", uri));
		g_object_unref (json_file);
		return TOTEM_PL_PARSER_RESULT_ERROR;
	}

	feed_url = totem_pl_parser_parse_json (contents, len, totem_pl_parser_is_debugging_enabled (parser));
	g_free (contents);
	if (feed_url == NULL) {
		DEBUG(json_file, g_print ("Failed to parse JSON file at '%s'\n", uri));
		g_object_unref (json_file);
		return TOTEM_PL_PARSER_RESULT_ERROR;
	}

	g_object_unref (json_file);
	feed_file = g_file_new_for_uri (feed_url);
	g_free (feed_url);

	DEBUG(feed_file, g_print ("Found feed URI: %s\n", uri));

	ret = totem_pl_parser_add_rss (parser, feed_file, NULL, parse_data, NULL);
	g_object_unref (feed_file);

	return ret;
#endif /* !HAVE_GMIME */
}

gboolean
totem_pl_parser_is_itms_feed (GFile *file)
{
	char *uri;

	g_return_val_if_fail (file != NULL, FALSE);

	uri = g_file_get_uri (file);

	if (g_file_has_uri_scheme (file, "itms") != FALSE ||
	    g_file_has_uri_scheme (file, "itmss") != FALSE ||
	    (g_file_has_uri_scheme (file, "http") != FALSE &&
	     strstr (uri, ".apple.com/") != FALSE)) {
		if (strstr (uri, "/podcast/") != NULL ||
		    strstr (uri, "viewPodcast") != NULL) {
			g_free (uri);
			return TRUE;
		}
	}

	g_free (uri);

	return FALSE;
}

static TotemPlParserResult
parse_opml_outline (TotemPlParser *parser, xml_node_t *parent)
{
	xml_node_t* node;

	for (node = parent->child; node != NULL; node = node->next) {
		const char *title, *uri;

		if (node->name == NULL || g_ascii_strcasecmp (node->name, "outline") != 0)
			continue;

		uri = xml_parser_get_property (node, "xmlUrl");
		title = xml_parser_get_property (node, "text");

		if (uri == NULL)
			continue;

		totem_pl_parser_add_uri (parser,
					 TOTEM_PL_PARSER_FIELD_TITLE, title,
					 TOTEM_PL_PARSER_FIELD_URI, uri,
					 NULL);
	}

	return TOTEM_PL_PARSER_RESULT_SUCCESS;
}

static TotemPlParserResult
parse_opml_head_body (TotemPlParser *parser, const char *uri, xml_node_t *parent)
{
	xml_node_t* node;
	gboolean started;

	started = FALSE;

	for (node = parent->child; node != NULL; node = node->next) {
		if (node->name == NULL)
			continue;

		if (g_ascii_strcasecmp (node->name, "body") == 0) {
			if (started == FALSE) {
				/* Send the info we already have about the feed */
				totem_pl_parser_add_uri (parser,
							 TOTEM_PL_PARSER_FIELD_IS_PLAYLIST, TRUE,
							 TOTEM_PL_PARSER_FIELD_URI, uri,
							 NULL);
				started = TRUE;
			}

			parse_opml_outline (parser, node);
		}
	}

	return TOTEM_PL_PARSER_RESULT_SUCCESS;
}

TotemPlParserResult
totem_pl_parser_add_opml (TotemPlParser *parser,
			  GFile *file,
			  GFile *base_file,
			  TotemPlParseData *parse_data,
			  gpointer data)
{
#ifndef HAVE_GMIME
	WARN_NO_GMIME;
#else
	xml_node_t* doc;
	char *contents, *uri;
	gsize size;

	if (g_file_load_contents (file, NULL, &contents, &size, NULL, NULL) == FALSE)
		return TOTEM_PL_PARSER_RESULT_ERROR;

	doc = totem_pl_parser_parse_xml_relaxed (contents, size);
	if (doc == NULL) {
		g_free (contents);
		return TOTEM_PL_PARSER_RESULT_ERROR;
	}

	/* If the document has no name */
	if (doc->name == NULL
	    || g_ascii_strcasecmp (doc->name , "opml") != 0) {
		g_free (contents);
		xml_parser_free_tree (doc);
		return TOTEM_PL_PARSER_RESULT_ERROR;
	}

	uri = g_file_get_uri (file);
	parse_opml_head_body (parser, uri, doc);
	g_free (uri);

	g_free (contents);
	xml_parser_free_tree (doc);

	return TOTEM_PL_PARSER_RESULT_SUCCESS;
#endif /* !HAVE_GMIME */
}

#endif /* !TOTEM_PL_PARSER_MINI */

