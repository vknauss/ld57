#version 450 core

const vec2[3] corners = vec2[3](vec2(-1, -3), vec2(-1, 1), vec2(3, 1));
const vec2[3] cornerTexCoords = vec2[3](vec2(0, 2), vec2(0, 0), vec2(2, 0));

layout(location = 0) out vec2 texCoord;

void main()
{
    gl_Position = vec4(corners[gl_VertexIndex], 0, 1);
    texCoord = cornerTexCoords[gl_VertexIndex];
}
