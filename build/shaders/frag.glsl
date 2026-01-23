#version 450

layout(location = 0) in vec2 TexCoord;
layout(location = 0) out vec4 outColor;

// 左眼和右眼纹理采样器
layout(binding = 0) uniform sampler2D texLeft;
layout(binding = 1) uniform sampler2D texRight;

void main() {
    vec2 uv = TexCoord;
    vec4 color;

    if (uv.x < 0.5) {
        // 屏幕左半部分：采样左眼纹理
        // 将UV坐标从[0, 0.5]映射到[0, 1]
        vec2 leftUV = vec2(uv.x * 2.0, uv.y);
        color = texture(texLeft, leftUV);
    } else {
        // 屏幕右半部分：采样右眼纹理
        // 将UV坐标从[0.5, 1.0]映射到[0, 1]
        vec2 rightUV = vec2((uv.x - 0.5) * 2.0, uv.y);
        color = texture(texRight, rightUV);
    }

    outColor = color;
}