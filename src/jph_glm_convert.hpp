#pragma once

#include <glm/gtc/quaternion.hpp>
#include <glm/glm.hpp>
#include <Jolt/Jolt.h>

static inline JPH::Vec3 glm_to_jph(const glm::vec3& v)
{
    return JPH::Vec3(v.x, v.y, v.z);
}

static inline glm::vec3 jph_to_glm(const JPH::Vec3& v)
{
    return glm::vec3(v.GetX(), v.GetY(), v.GetZ());
}

static inline JPH::Quat glm_to_jph(const glm::quat& q)
{
    return JPH::Quat(q.x, q.y, q.z, q.w);
}

static inline glm::quat jph_to_glm(const JPH::Quat& q)
{
    return glm::quat(q.GetW(), q.GetX(), q.GetY(), q.GetZ());
}
