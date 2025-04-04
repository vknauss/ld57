#version 450 core

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec2 v_texCoord;
layout(location = 2) in vec3 v_normal;
layout(location = 0) out vec2 texCoord;
layout(location = 1) out flat uint textureIndex;
layout(location = 2) out vec3 tintColor;
layout(location = 3) out vec3 position;
layout(location = 4) out vec3 normal;

layout(set = 1, binding = 0) uniform Matrices
{
    mat4 projection;
};

struct Instance
{
    mat4 transform;
    vec2 texCoordOffset;
    uint textureIndex;
    float pad0;
    vec3 tintColor;
};

layout(std140, set = 2, binding = 0) readonly buffer InstanceData
{
    Instance instances[];
};

void main()
{
    texCoord = instances[gl_InstanceIndex].texCoordOffset + vec2(v_texCoord.x, 1.0 - v_texCoord.y);
    textureIndex = instances[gl_InstanceIndex].textureIndex;
    tintColor = instances[gl_InstanceIndex].tintColor;
    normal = mat3(instances[gl_InstanceIndex].transform) * v_normal;

    vec4 v4 = vec4(v_position, 1);
    v4 = instances[gl_InstanceIndex].transform * v4;
    position = vec3(v4);
    v4 = projection * v4;
    gl_Position = v4;

}
