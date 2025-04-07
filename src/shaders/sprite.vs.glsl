#version 450 core

const vec2[4] corners = vec2[4](vec2(-1, -1), vec2(1, -1), vec2(-1, 1), vec2(1, 1));
const vec2[4] cornerTexCoords = vec2[4](vec2(0, 1), vec2(1, 1), vec2(0, 0), vec2(1, 0));

layout(location = 0) out vec2 texCoord;
layout(location = 1) out flat uint textureIndex;
layout(location = 2) out vec4 tintColor;
layout(location = 3) out vec3 outPosition;
layout(location = 4) out vec3 normal;

layout(set = 1, binding = 0) uniform Matrices
{
    mat4 projection;
    mat4 view;
    mat4 viewportProjection;
};

struct Instance
{
    vec3 position;
    vec3 scale;
    vec2 minTexCoord;
    vec2 texCoordScale;
    float cosAngle;
    float sinAngle;
    uint textureIndex;
    float pad0;
    vec4 tintColor;
};

layout(std140, set = 2, binding = 0) readonly buffer InstanceData
{
    Instance instances[];
};

void main()
{
    texCoord = instances[gl_InstanceIndex].minTexCoord + instances[gl_InstanceIndex].texCoordScale * cornerTexCoords[gl_VertexIndex];
    textureIndex = instances[gl_InstanceIndex].textureIndex;
    tintColor = instances[gl_InstanceIndex].tintColor;
    normal = vec3(0, 0, 1);

    vec3 position = instances[gl_InstanceIndex].scale * vec3(corners[gl_VertexIndex], 0);
    mat2 rotation = mat2(instances[gl_InstanceIndex].cosAngle, instances[gl_InstanceIndex].sinAngle,
            -instances[gl_InstanceIndex].sinAngle, instances[gl_InstanceIndex].cosAngle);
    position.xy = rotation * position.xy;
    vec4 v4 = vec4(instances[gl_InstanceIndex].position, 1);
    v4 = view * v4;
    #ifdef OVERLAY
    outPosition = v4.xyz;
    v4 = projection * v4;
    v4.xy += v4.w * (viewportProjection * vec4(position, 1.0)).xy;
    #else
    v4.xyz = v4.xyz + position;
    outPosition = v4.xyz;
    v4 = projection * v4;
    #endif
    gl_Position = v4;
}
