#version 450 core
#extension GL_EXT_nonuniform_qualifier : require

layout(set = 0, binding = 2) uniform sampler2D gBufferDepth;

layout(set = 3, binding = 0) uniform sampler2D texSamplers[];

layout(location = 0) out vec4 gBufferColor;

layout(location = 0) in vec4 ndcPos;
layout(location = 1) in flat uint textureIndex;
layout(location = 2) in mat4 viewToObject;

layout(std140, set = 1, binding = 1) uniform SceneLighting
{
    mat4 inverseProjection;
};

void main()
{
    vec3 ppos = ndcPos.xyz / ndcPos.w;
    vec2 screenCoords = ppos.xy * 0.5 + 0.5;
    vec4 depthSample = texture(gBufferDepth, vec2(screenCoords.x, 1 - screenCoords.y));
    vec4 np = vec4(ppos.xy, depthSample.r, 1.0);
    np = inverseProjection * np;
    np /= np.w;
    np = viewToObject * np;

    if (any(greaterThan(abs(np.xyz), vec3(0.5)))) discard;
    vec2 texCoord = np.xy + 0.5;

    vec4 texColor = texture(texSamplers[nonuniformEXT(textureIndex)], texCoord);
    gBufferColor = texColor;
}
