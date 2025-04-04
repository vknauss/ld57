glslc = find_program('glslc')

shaders_input = [
  {
    'input': 'decal.fs.glsl',
    'type': 'fragment'
  },
  {
    'input': 'decal.vs.glsl',
    'type': 'vertex'
  },
  {
    'input': 'deferred.fs.glsl',
    'type': 'fragment'
  },
  {
    'input': 'fullscreen.vs.glsl',
    'type': 'vertex'
  },
  {
    'input': 'g_buffer.fs.glsl',
    'type': 'fragment'
  },
  {
    'input': 'geometry.vs.glsl',
    'type': 'vertex'
  },
  {
    'input': 'geometry_depth.vs.glsl',
    'type': 'vertex'
  },
  {
    'input': 'sprite.vs.glsl',
    'type': 'vertex'
  },
  {
    'input': 'ssao.fs.glsl',
    'type': 'fragment'
  },
  {
    'input': 'sprite.vs.glsl',
    'type': 'vertex',
    'output': 'sprite_overlay.vs.spv',
    'arguments': ['-DOVERLAY'],
  },
]

shader_targets = []
foreach record : shaders_input
  input_file = files(record['input'])
  output_name = record.get('output', fs.stem(input_file) + '.spv')

  command = [glslc]
  if 'type' in record
    command += '-fshader-stage=' + record['type']
  endif
  command += ['-c', '@INPUT@', '-o', '@OUTPUT@' ]
  command += record.get('arguments', [])

  depend_files = []
  if 'depends' in record
    depend_files = record['depends']
  endif

  shader_targets += custom_target(output_name,
    input: input_file,
    output: output_name,
    command: command,
    depend_files: depend_files,
    build_by_default: true,
    install: true,
    install_dir: join_paths(get_option('datadir'), 'shaders')
  )
endforeach
