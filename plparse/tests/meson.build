test_cargs = ['-DTEST_SRCDIR="@0@/"'.format(meson.current_source_dir())]

tests = ['parser', 'podcast']

foreach test_name : tests
  exe = executable(test_name, ['@0@.c'.format(test_name), 'utils.c'],
                   c_args: test_cargs,
                   include_directories: [config_inc, totemlib_inc],
                   dependencies: plparser_dep)

  env = environment()
  test(test_name, exe, env: env)
endforeach

exe = executable('disc', ['disc.c'],
                 c_args: test_cargs,
                 include_directories: [config_inc, totemlib_inc],
                 dependencies: plparser_dep)
test('disc', exe)
