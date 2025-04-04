#version 450 core
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec2 texCoord;

layout(location = 0) out vec4 occlusion;

layout(set = 0, binding = 0) uniform sampler2D gBufferColor;
layout(set = 0, binding = 1) uniform sampler2D gBufferNormal;
layout(set = 0, binding = 2) uniform sampler2D gBufferDepth;

layout(std140, set = 1, binding = 1) uniform SceneLighting
{
    mat4 inverseProjection;
    mat4 projection;
    vec3 ambientLight;
    uint lightsOffset;
    uint numLights;
};

const float pi = 3.1415926535897932384626433832795;

float random(vec2 v)
{
    // https://stackoverflow.com/questions/4200224/random-noise-functions-for-glsl
    return fract(sin(dot(v, vec2(12.9898, 78.233))) * 43758.5453);
}

float stateRandom(inout vec2 v)
{
    float r = random(v);
    v = vec2(v.y, r);
    return r;
}

const float sampleRadius = 0.5;
const float depthThreshold = 0.5;
const float bias = 0.025;
const uint sampleCount = 64;

void main()
{
    vec4 texColor = texture(gBufferColor, texCoord);
    vec4 normalSample = texture(gBufferNormal, texCoord);
    if (dot(normalSample.xyz, normalSample.xyz) < 0.001) discard;
    vec3 n = normalize(normalSample.xyz);
    vec4 depthSample = texture(gBufferDepth, texCoord);

    vec4 ndcPos = vec4(vec2(texCoord.x, 1 - texCoord.y) * 2.0 - 1.0, depthSample.r, 1.0);
    vec4 v4Pos = inverseProjection * ndcPos;
    vec3 position = v4Pos.xyz / v4Pos.w;

    vec3 t = cross(vec3(0, 1, 0), n);
    if (dot(t, t) < 0.001)
    {
        t = cross(n, vec3(0, 0, 1));
    }
    t = normalize(t);
    mat3 tbn = mat3(t, cross(n, t), n);

    float occ = 0;
    float contrib = 0;
    vec2 rc = gl_FragCoord.xy;
    for (uint i = 0; i < sampleCount; ++i)
    {
        float va = 0.5 * pi * stateRandom(rc);
        float ha = 2 * pi * stateRandom(rc);
        vec2 hd = vec2(cos(ha), sin(ha));
        vec3 d = vec3(cos(va) * hd, sin(va));
        d = d * sampleRadius * float(i + 1) / sampleCount;
        d = tbn * d;
        vec3 p = position + d;
        vec4 pp = vec4(p, 1.0);
        pp = projection * pp;
        pp /= pp.w;
        float pd = texture(gBufferDepth, 0.5 + vec2(0.5, -0.5) * pp.xy).r;
        float bounds = float(all(lessThanEqual(abs(pp.xy), vec2(1))));

        // linearize depth
        pp.z = pd;
        pp = inverseProjection * pp;
        pp /= pp.w;
        float rangeCheck = smoothstep(0.0, 1.0, depthThreshold / abs(pp.z - position.z));

        occ += float(pp.z >= p.z + bias) * rangeCheck * bounds;
        contrib += rangeCheck * bounds;
    }

    occlusion = vec4(1.0 - occ / contrib, 0, 0, 1);
}
