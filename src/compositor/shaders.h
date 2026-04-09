#pragma once

static const char* VERT_SRC = R"GLSL(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
out vec2 vUV;
void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    vUV = aUV;
}
)GLSL";

static const char* FRAG_COMP = R"GLSL(
#version 330 core
uniform sampler2D uTex;
uniform float     uOpacity;
uniform int       uOpaqueAlpha;
in  vec2 vUV;
out vec4 fragColor;
void main() {
    vec4 c = texture(uTex, vUV);
    float a = (uOpaqueAlpha == 1 && c.a < 0.01) ? 1.0 : c.a;
    fragColor = vec4(c.rgb, a * uOpacity);
}
)GLSL";

static const char* FRAG_BLUR = R"GLSL(
#version 330 core
uniform sampler2D uTex;
uniform vec2 uDir;
in vec2 vUV;
out vec4 fragColor;
void main() {
    fragColor  = texture(uTex, vUV + uDir * -4.0) * 0.0162;
    fragColor += texture(uTex, vUV + uDir * -3.0) * 0.0540;
    fragColor += texture(uTex, vUV + uDir * -2.0) * 0.1216;
    fragColor += texture(uTex, vUV + uDir * -1.0) * 0.1945;
    fragColor += texture(uTex, vUV)               * 0.2270;
    fragColor += texture(uTex, vUV + uDir *  1.0) * 0.1945;
    fragColor += texture(uTex, vUV + uDir *  2.0) * 0.1216;
    fragColor += texture(uTex, vUV + uDir *  3.0) * 0.0540;
    fragColor += texture(uTex, vUV + uDir *  4.0) * 0.0162;
}
)GLSL";

static const char* FRAG_SHADOW = R"GLSL(
#version 330 core
uniform float uAlpha;
uniform vec3  uColor;
out vec4 fragColor;
void main() {
    fragColor = vec4(uColor, uAlpha);
}
)GLSL";
