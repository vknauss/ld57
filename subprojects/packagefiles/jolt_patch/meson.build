# Maybe add default c++ flags. Jolt only seems to do this for MSVC so maybe not needed. If not I would rather let user/build system dictate
project('JoltPhysics', 'cpp',
  default_options: {
    'cpp_std': 'c++17',
  })

includes = include_directories('.')

subdir('Jolt')

Jolt_dep = declare_dependency(
  compile_args: cpp_args,
  include_directories: includes,
  link_with: jolt_lib)
