Video site parsing scripts
--------------------------

totem-pl-parser can "parse" pages from certain websites into a single
video playback URL. This is particularly useful for websites which
show a unique video on a web page, and use one-time URLs to prevent direct
linking.

This feature is implemented in a helper binary, which needs to be installed
next to this README file, in totem-pl-parser's `libexec` directory, or
in the directory pointed to by the `TOTEM_PL_PARSER_VIDEOSITE_SCRIPT`
environment variable.

totem-pl-parser used to ship such a script that used libquvi, but doesn't
anymore. The first script (when sorted by lexicographic ordering) in the
aforementioned directory will be used.

The API to implement is straight-forward. For each URL that needs to
be checked, the script will be called with the command-line arguments
`--check --url` followed by the URL. The script should return the
string `TRUE` if the script knows how to handle video pages from
this site. This call should not making any network calls, and should
be fast.

If the video site is handled by the script, then the script can be
called with `--url` followed by the URL. The script can return the
strings `TOTEM_PL_PARSER_RESULT_ERROR` or
`TOTEM_PL_PARSER_RESULT_UNHANDLED` to indicate an error (see the
meaning of those values in the [totem-pl-parser API documentation](https://developer.gnome.org/totem-pl-parser/stable/TotemPlParser.html#TotemPlParserResult)),
or a list of `<key>=<value>` pairs separated by newlines characters (`\n`)
The keys are listed as [metadata fields](https://developer.gnome.org/totem-pl-parser/stable/TotemPlParser.html#TOTEM-PL-PARSER-FIELD-URI:CAPS)
in the API documentation, such as:

```
url=https://www.videosite.com/unique-link-to.mp4
title=Unique Link to MP4
author=Well-known creator
```

Integrators should make sure that totem-pl-parser is shipped with at
least one video site parser, in a separate package, such as a third-party parser
that implements a compatible API as explained above. Do **NOT** ship
third-party parsers in the same package as totem or totem-pl-parser itself.