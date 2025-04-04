#version 450 core

layout(location = 0) in vec3 v_position;

layout(location = 0) out vec4 ndcPos;
layout(location = 1) out flat uint textureIndex;
layout(location = 2) out mat4 viewToObject;

layout(set = 1, binding = 0) uniform Matrices
{
    mat4 projection;
};

struct Instance
{
    mat4 modelView;
    mat4 inverseModelView;
    uint textureIndex;
};

layout(std140, set = 2, binding = 0) readonly buffer InstanceData
{
    Instance instances[];
};

void main()
{
    textureIndex = instances[gl_InstanceIndex].textureIndex;
    viewToObject = instances[gl_InstanceIndex].inverseModelView;

    vec4 v4 = vec4(v_position, 1);
    v4 = instances[gl_InstanceIndex].modelView * v4;
    v4 = projection * v4;
    ndcPos = v4;
    gl_Position = v4;
}
