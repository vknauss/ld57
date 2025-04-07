#version 450 core
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec2 texCoord;
layout(location = 1) in flat uint textureIndex;
layout(location = 2) in vec4 tintColor;
layout(location = 3) in vec3 position;
layout(location = 4) in vec3 normal;

layout(location = 0) out vec4 gBufferColor;
layout(location = 1) out vec4 gBufferNormal;

layout(set = 0, binding = 0) uniform sampler2D texSamplers[];

void main()
{
    vec4 texColor = texture(texSamplers[nonuniformEXT(textureIndex)], texCoord);
    if(texColor.a < 0.1) discard;

    gBufferColor = vec4(tintColor * texColor);
    gBufferNormal = vec4(normalize(normal), 1);
}
