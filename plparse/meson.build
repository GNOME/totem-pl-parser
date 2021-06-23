gnome = import('gnome')

includedir = join_paths(get_option('includedir'), 'totem-pl-parser/1/plparser')

plparser_public_headers = [
  'totem-pl-parser.h',
  'totem-pl-playlist.h',
  'totem-pl-parser-mini.h',
]
install_headers(plparser_public_headers, subdir : 'totem-pl-parser/1/plparser')

# generate totem-pl-parser-features.h
version_cdata = configuration_data()
version_cdata.set('TOTEM_PL_PARSER_VERSION_MAJOR', plparse_major_version)
version_cdata.set('TOTEM_PL_PARSER_VERSION_MINOR', plparse_minor_version)
version_cdata.set('TOTEM_PL_PARSER_VERSION_MICRO', plparse_micro_version)
features_h = configure_file(input:  'totem-pl-parser-features.h.in',
                            output: 'totem-pl-parser-features.h',
                            install_dir: includedir,
                            configuration: version_cdata)

totem_pl_parser_builtins = gnome.mkenums('totem-pl-parser-builtins',
                                         sources: 'totem-pl-parser.h',
                                         c_template : 'totem-pl-parser-builtins.c.template',
                                         h_template : 'totem-pl-parser-builtins.h.template',
                                         install_dir : includedir,
                                         install_header : true)

totem_pl_parser_builtins_h = totem_pl_parser_builtins[1]

totem_pl_parser_marshalers = gnome.genmarshal('totemplparser-marshal',
                                              sources : 'totemplparser-marshal.list',
                                              prefix : '_totemplparser_marshal')

plparser_sources = [
  'totem-disc.c',
  'totem-pl-parser.c',
  'totem-pl-parser-amz.c',
  'totem-pl-parser-decode-date.c',
  'totem-pl-parser-lines.c',
  'totem-pl-parser-media.c',
  'totem-pl-parser-misc.c',
  'totem-pl-parser-pla.c',
  'totem-pl-parser-pls.c',
  'totem-pl-parser-podcast.c',
  'totem-pl-parser-qt.c',
  'totem-pl-parser-smil.c',
  'totem-pl-parser-videosite.c',
  'totem-pl-parser-wm.c',
  'totem-pl-parser-xspf.c',
  'totem-pl-playlist.c',
  'xmlparser.c',
  'xmllexer.c',
]

totemlib_inc = include_directories('../lib')

libexecdir = join_paths(get_option('prefix'), get_option('libexecdir'))

plparser_cflags = extra_warning_cflags + ['-DLIBEXECDIR="@0@"'.format(libexecdir)]

symbol_map = 'plparser.map'
symbol_link_args = '-Wl,--version-script,@0@/@1@'.format(meson.current_source_dir(), symbol_map)

plparser_lib = library('totem-plparser',
                       plparser_sources, features_h,
                       totem_pl_parser_builtins,
                       totem_pl_parser_marshalers,
                       include_directories: [config_inc, totemlib_inc],
                       c_args: plparser_cflags,
                       dependencies: totem_pl_parser_deps,
                       link_args : symbol_link_args,
                       link_with: totem_glibc_lib,
                       link_depends : symbol_map,
                       version: plparse_libversion,
                       install: true)

plparser_dep = declare_dependency(sources: [totem_pl_parser_builtins_h, features_h],
                                  include_directories: [config_inc, plparser_inc],
                                  dependencies: gio_dep,
                                  link_with: plparser_lib)

plparser_mini_sources = [
  'totem-pl-parser.c',
  'totem-pl-parser-lines.c',
  'totem-pl-parser-misc.c',
  'totem-pl-parser-pls.c',
  'totem-pl-parser-podcast.c',
  'totem-pl-parser-qt.c',
  'totem-pl-parser-smil.c',
  'totem-pl-parser-videosite.c',
  'totem-pl-parser-wm.c',
  'totem-pl-parser-xspf.c',
  'totem-pl-parser-amz.c',
  'totem-pl-playlist.c',
]

mini_symbol_map = 'plparser-mini.map'
mini_symbol_link_args = '-Wl,--version-script,@0@/@1@'.format(meson.current_source_dir(), mini_symbol_map)

plparser_mini_lib = library('totem-plparser-mini',
                            plparser_mini_sources,
                            totem_pl_parser_builtins, features_h,
                            include_directories: [config_inc, totemlib_inc],
                            c_args: plparser_cflags + ['-DTOTEM_PL_PARSER_MINI'],
                            dependencies: gio_dep,
                            link_args : mini_symbol_link_args,
                            link_with: totem_glibc_lib,
                            link_depends : mini_symbol_map,
                            version: plparse_libversion,
                            install: true)

install_data('README-videosite-script.md',
             install_dir: join_paths(libexecdir, 'totem-pl-parser'))

# Introspection
if get_option('introspection')
  gnome.generate_gir(plparser_lib,
                     sources: plparser_public_headers + plparser_sources + [
                       totem_pl_parser_marshalers[1],
                       totem_pl_parser_builtins_h,
                       features_h,
                     ],
                     namespace: 'TotemPlParser',
                     nsversion: '1.0',
                     identifier_prefix: 'TotemPl',
                     symbol_prefix: 'totem_pl',
                     includes: ['GObject-2.0', 'Gio-2.0'],
                     extra_args: ['--identifier-prefix=TotemDisc',
                                  '--symbol-prefix=totem_disc',
                                  '--identifier-prefix=TotemCd',
                                  '--symbol-prefix=totem_cd',
                                  '--quiet'],
                     install: true)
endif

subdir('tests')
