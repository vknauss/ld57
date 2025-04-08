project('ld57', 'cpp',
  default_options: {
    'cpp_std': 'c++23',
    'prefix': join_paths(meson.project_source_root(), 'install'),
    'bindir': '',
    'datadir': '',
    'libdir': 'lib'
  },
  meson_version: '>=1.2',
)

fs = import('fs')

glm_dep = dependency('glm')
jolt_dep = dependency('jolt')
sdl_dep = dependency('sdl3')
stb_dep = dependency('stb')
vma_dep = dependency('vma')
vma_hpp_dep = dependency('vma-hpp')
vulkan_dep = dependency('vulkan')

subdir('src')

install_subdir('resources', install_dir: get_option('datadir'))
