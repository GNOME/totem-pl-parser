test_cargs = ['-DTEST_SRCDIR="@0@/"'.format(meson.current_source_dir())]

tests = ['parser', 'disc']

foreach test_name : tests
  exe = executable(test_name, '@0@.c'.format(test_name),
                   c_args: test_cargs,
                   include_directories: [config_inc, totemlib_inc],
                   dependencies: plparser_dep)

  env = environment()
  if have_quvi
    env.set('TOTEM_PL_PARSER_VIDEOSITE_SCRIPT', videosite_exe.full_path())
  endif

  test(test_name, exe, env: env, timeout: 3 * 60)
endforeach
