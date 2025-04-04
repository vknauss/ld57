#version 450 core

layout(location = 0) in vec3 v_position;

layout(std430, push_constant) uniform PushConstants
{
    mat4 viewProjection;
};

struct Instance
{
    mat4 transform;
    vec2 texCoordOffset;
    uint textureIndex;
    float pad0;
    vec3 tintColor;
};

layout(std140, set = 0, binding = 0) readonly buffer InstanceData
{
    Instance instances[];
};

void main()
{
    vec4 v4 = vec4(v_position, 1);
    v4 = instances[gl_InstanceIndex].transform * v4;
    gl_Position = viewProjection * v4;
}
