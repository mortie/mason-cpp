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
  'mason-to-json',
  'bin/mason-to-json.cc',
  dependencies: [libmason_dep],
)

executable(
  'mason-roundtrip',
  'bin/mason-roundtrip.cc',
  dependencies: [libmason_dep],
)
