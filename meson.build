project(
  'swan', 'cpp',
  default_options: [
    'cpp_std=c++17',
    'warning_level=3',
  ],
)

libmason_lib = library(
  'mason',
  'src/mason.cc',
  include_directories: 'include/mason',
)

libmason_dep = declare_dependency(
  link_with: libmason_lib,
  include_directories: 'include',
)

executable(
  'mason-cli',
  'bin/mason-cli.cc',
  dependencies: [libmason_dep],
)
