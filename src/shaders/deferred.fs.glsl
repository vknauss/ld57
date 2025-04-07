#version 450 core
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec2 texCoord;

layout(location = 0) out vec4 fragColor;

struct Light
{
    vec3 position;
    vec3 intensity;
    int shadowMapIndex;
};

layout(std140, set = 1, binding = 1) uniform SceneLighting
{
    mat4 inverseProjection;
    mat4 projection;
    vec3 ambientLight;
    uint lightsOffset;
    uint numLights;
    uvec2 screenResolution;
};

layout(std430, set = 1, binding = 2) readonly buffer Lights
{
    Light lights[];
};

// #define DITHER_LIGHTING
// #define DITHER_AO

layout(set = 0, binding = 0) uniform sampler2D gBufferColor;
layout(set = 0, binding = 1) uniform sampler2D gBufferNormal;
layout(set = 0, binding = 2) uniform sampler2D gBufferDepth;

layout(set = 2, binding = 0) uniform sampler2D ambientOcclusionTexture;
layout(set = 3, binding = 0) uniform samplerCube shadowCubeMaps[];

const mat4 bayer4x4 = mat4(0, 12, 3, 15,   8, 4, 11, 7,   2, 14, 1, 13,   10, 6, 9, 5) / 16;

float sampleBayer()
{
    return bayer4x4[(int(gl_FragCoord.x) / 8) % 4][(int(gl_FragCoord.y) / 8) % 4];
}

float linearDepth(float d, float near, float far)
{
    return near * far / (far - d * (far - near));
}

vec4 linearDepth4(vec4 d, float near, float far)
{
    return near * far / (far - d * (far - near));
}

const float ShadowNear = 0.1;
const float ShadowFar = 100.0;
const float SoftShadowRadius = 0.1;
const float ShadowNormalOffsetBias = 0.005;
const float ShadowDepthBias = 0.005;

// stolen https://github.com/proskur1n/vwa-code/blob/777f475ae68db6d05ce524f6d0040774cf2d6280/source/shaders/normalPass.frag#L21
const vec2 POISSON8[] = vec2[8](
	vec2(-0.2602728,0.3234085), vec2(-0.3268174,0.0442592), vec2(0.1996002,0.1386711),
	vec2(0.2615348,-0.1569698), vec2(-0.2869459,-0.3421305), vec2(0.1351001,-0.4352284),
	vec2(-0.0635913,-0.1520724), vec2(0.1454225,0.4629610)
);

const vec2 POISSON16[] = vec2[16](
	vec2(0.3040781,-0.1861200), vec2(0.1485699,-0.0405212), vec2(0.4016555,0.1252352),
	vec2(-0.1526961,-0.1404687), vec2(0.3480717,0.3260515), vec2(0.0584860,-0.3266001),
	vec2(0.0891062,0.2332856), vec2(-0.3487481,-0.0159209), vec2(-0.1847383,0.1410431),
	vec2(0.4678784,-0.0888323), vec2(0.1134236,0.4119219), vec2(0.2856628,-0.3658066),
	vec2(-0.1765543,0.3937907), vec2(-0.0238326,0.0518298), vec2(-0.2949835,-0.3029899),
	vec2(-0.4593541,0.1720255)
);

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

    vec3 lightColor = vec3(0);
    for (uint i = 0; i < numLights; ++i)
    {
        Light light = lights[lightsOffset + i];
        vec3 toLight = light.position - position;

        float cos = 1;
        float l = dot(toLight, toLight);
        if (l > 0.0001)
        {
            cos = clamp(dot(toLight, n) / sqrt(l), 0, 1);
        }

        // percentage closer soft shadows
        float shadowOcclusion = 1.0;
        if (light.shadowMapIndex >= 0)
        {
            vec3 lightu = dot(toLight.xz, toLight.xz) > 0.01 ? vec3(0, 1, 0) : vec3(0, 0, 1);
            lightu = normalize(cross(toLight, lightu));
            vec3 lightv = normalize(cross(toLight, lightu));

            float distanceToCubeFace = max(abs(toLight.x), max(abs(toLight.y), abs(toLight.z)));

            // use vectors for efficient operation, but just 8 poisson sample of cubemap offset (in view space) by fixed light radius size
            vec4 blockers0, blockers1;
            blockers0.x = texture(shadowCubeMaps[light.shadowMapIndex], -toLight - SoftShadowRadius * (POISSON8[0].x * lightu + POISSON8[0].y * lightv)).r;
            blockers0.y = texture(shadowCubeMaps[light.shadowMapIndex], -toLight - SoftShadowRadius * (POISSON8[1].x * lightu + POISSON8[1].y * lightv)).r;
            blockers0.z = texture(shadowCubeMaps[light.shadowMapIndex], -toLight - SoftShadowRadius * (POISSON8[2].x * lightu + POISSON8[2].y * lightv)).r;
            blockers0.w = texture(shadowCubeMaps[light.shadowMapIndex], -toLight - SoftShadowRadius * (POISSON8[3].x * lightu + POISSON8[3].y * lightv)).r;
            blockers1.x = texture(shadowCubeMaps[light.shadowMapIndex], -toLight - SoftShadowRadius * (POISSON8[4].x * lightu + POISSON8[4].y * lightv)).r;
            blockers1.y = texture(shadowCubeMaps[light.shadowMapIndex], -toLight - SoftShadowRadius * (POISSON8[5].x * lightu + POISSON8[5].y * lightv)).r;
            blockers1.z = texture(shadowCubeMaps[light.shadowMapIndex], -toLight - SoftShadowRadius * (POISSON8[6].x * lightu + POISSON8[6].y * lightv)).r;
            blockers1.w = texture(shadowCubeMaps[light.shadowMapIndex], -toLight - SoftShadowRadius * (POISSON8[7].x * lightu + POISSON8[7].y * lightv)).r;
            blockers0 = linearDepth4(blockers0, ShadowNear, ShadowFar);
            blockers1 = linearDepth4(blockers1, ShadowNear, ShadowFar);
            bvec4 blockcmp0 = greaterThan(vec4(distanceToCubeFace), blockers0 + ShadowDepthBias);
            bvec4 blockcmp1 = greaterThan(vec4(distanceToCubeFace), blockers1 + ShadowDepthBias);
            // assume full illumination if no blockers found between surface and light
            if (any(blockcmp0) || any(blockcmp1))
            {
                // average depth of samples that found blockers
                vec4 blockcnt = vec4(blockcmp0) + vec4(blockcmp1);
                vec4 blockers = blockers0 * vec4(blockcmp0) + blockers1 * vec4(blockcmp1);
                float avgDepth = (blockers.x + blockers.y + blockers.z + blockers.w) / (blockcnt.x + blockcnt.y + blockcnt.z + blockcnt.w);
                float penumbraSize = (distanceToCubeFace - avgDepth) * SoftShadowRadius / avgDepth;
                float normalOffsetScale = ShadowNormalOffsetBias * (1 - cos) * distanceToCubeFace;
                vec3 shadowOffsetPos = position + normalOffsetScale * n;
                vec3 shadowToLight = light.position - shadowOffsetPos;
                float shadowRefDistance = max(abs(shadowToLight.x), max(abs(shadowToLight.y), abs(shadowToLight.z)));

                for (int j = 0; j < 16; ++j)
                {
                    vec3 offset = POISSON16[j].x * lightu + POISSON16[j].y * lightv;
                    offset *= penumbraSize;
                    float shadowDepthSample = texture(shadowCubeMaps[light.shadowMapIndex], -shadowToLight - offset).r;
                    shadowDepthSample = linearDepth(shadowDepthSample, ShadowNear, ShadowFar);
                    shadowOcclusion += float(shadowRefDistance <= shadowDepthSample + ShadowDepthBias);
                }

                shadowOcclusion /= 16.0;
            }
        }

        vec3 intensity = shadowOcclusion * light.intensity * cos / (1 + l);
        #ifdef DITHER_LIGHTING
        lightColor += light.intensity * vec3(greaterThan(intensity + sampleBayer(), vec3(1.0)));
        #else
        lightColor += intensity;
        #endif
    }

    float occlusion = texture(ambientOcclusionTexture, texCoord).r;
    occlusion = pow(occlusion, 4.0);
    #ifdef DITHER_AO
    occlusion = float(occlusion + sampleBayer() >= 1.0);
    #endif
    fragColor = vec4((lightColor + ambientLight * occlusion) * texColor.rgb, texColor.a);
}
