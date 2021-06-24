```
                         _|_ __|_ _ ._ _
                          |_(_)|_(/_| | |

                         playlist parser
```

totem-pl-parser is a simple GObject-based library to parse a host of
playlist formats, as well as save those

News
====

See [NEWS](NEWS) file

Dependencies
============

- glib >= 2.36
- libxml2
- libarchive >= 3.0 (optional)
- libgcrypt (optional)

BUGS
====

Bugs should be filed in [GNOME GitLab](https://gitlab.gnome.org/GNOME/totem-pl-parser/-/issues).

To get a parsing debug output, run:

```sh
$ $BUILDDIR/plparse/tests/parser --debug <URI to playlist file>
```

License
=======

totem-pl-parser is licensed under the GNU Lesser General Public License
version 2.1 or later.
