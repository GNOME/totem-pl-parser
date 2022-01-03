/*
 * Copyright 2007-2022 Bastien Nocera <hadess@hadess.net>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <glib.h>
#include "totem-pl-parser.h"

char *get_relative_uri (const char *rel);
guint parser_test_get_num_entries (const char *uri);
TotemPlParserResult simple_parser_test (const char *uri);
char *parser_test_get_entry_field (const char *uri,
				   const char *field);
char *parser_test_get_playlist_uri (const char *uri);
char *parser_test_get_playlist_field (const char *uri,
				      const char *field);
gboolean parser_test_get_order_result (const char *uri);
gboolean check_http (void);
