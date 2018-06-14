private_headers = [
    'xmllexer.h',
    'totem-pl-parser-media.h',
    'totem-pl-parser-xspf.h',
    'totem-pl-parser-podcast.h',
    'totem-pl-parser-private.h',
    'totem-pl-parser-smil.h',
    'totem-pl-parser-mini.h',
    'totem-pl-parser-lines.h',
    'totem-pl-parser-amz.h',
    'totem-disc.h',
    'totem-pl-parser-pls.h',
    'totem-pl-parser-misc.h',
    'xmlparser.h',
    'totem-pl-parser-qt.h',
    'totem-pl-parser-pla.h',
    'totem-pl-parser-wm.h',
    'bswap.h',
    'totem-pl-parser-videosite.h',
    'totem_internal.h',
    'totemplparser-marshal.h'
]

configure_file(input: 'version.xml.in',
               output: 'version.xml',
               configuration: version_cdata)

glib_prefix = glib_dep.get_pkgconfig_variable('prefix')
glib_docpath = join_paths(glib_prefix, 'share', 'plparse-doc', 'html')

plparse_prefix = get_option('prefix')
plparse_datadir = join_paths(plparse_prefix, get_option('datadir'))


docpath = join_paths(plparse_datadir, 'gtk-doc', 'html')

gnome.gtkdoc('totem-pl-parser',
             main_xml: 'totem-pl-parser-docs.xml',
             src_dir: [
	       join_paths(meson.source_root(), 'plparse'),
	       join_paths(meson.build_root(), 'plparse'),
	     ],
             dependencies: plparser_dep,
             scan_args: [
               '--ignore-headers=' + ' '.join(private_headers),
             ],
             fixxref_args: [
               '--html-dir=@0@'.format(docpath),
               '--extra-dir=@0@'.format(join_paths(glib_docpath, 'glib')),
               '--extra-dir=@0@'.format(join_paths(glib_docpath, 'gobject')),
               '--extra-dir=@0@'.format(join_paths(glib_docpath, 'gio')),
             ],
             install: true)
