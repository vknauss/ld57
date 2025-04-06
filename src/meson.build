configure_file(output: 'config.h',
  configuration: {
    'USE_VALIDATION_LAYERS': get_option('use_validation_layers'),
    'USE_PORTABILITY_EXTENSION': get_option('use_portability_extension'),
  })

executable('game',
  dependencies: [
    glm_dep,
    jolt_dep,
    sdl_dep,
    stb_dep,
    tinyobjloader_dep,
    vma_dep,
    vma_hpp_dep,
    vulkan_dep,
  ],
  sources: [
    'dungeon.cpp',
    'engine.cpp',
    'geometry_loader.cpp',
    'input_manager.cpp',
    'loader_utility.cpp',
    'main.cpp',
    'obj_load.cpp',
    'physics.cpp',
    'renderer.cpp',
    'stb_image_implementation.cpp',
    'swapchain.cpp',
    'texture_loader.cpp',
    'vma_implementation.cpp',
  ],
  install: true,
  install_rpath: get_option('libdir'),
)

subdir('shaders')
