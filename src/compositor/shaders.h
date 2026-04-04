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
in  vec2 vUV;
out vec4 fragColor;
void main() {
    vec4 c = texture(uTex, vUV);
    fragColor = vec4(c.rgb, c.a * uOpacity);
}
)GLSL";

static const char* FRAG_BLUR = R"GLSL(
#version 330 core
uniform sampler2D uTex;
uniform vec2      uDir;
in  vec2 vUV;
out vec4 fragColor;
void main() {
    const float w[5] = float[](0.227027, 0.194595, 0.121621, 0.054054, 0.016216);
    vec4 result = texture(uTex, vUV) * w[0];
    for (int i = 1; i < 5; i++) {
        result += texture(uTex, vUV + float(i) * uDir) * w[i];
        result += texture(uTex, vUV - float(i) * uDir) * w[i];
    }
    fragColor = result;
}
)GLSL";

static const char* FRAG_DISC = R"GLSL(
#version 330 core
uniform vec3      uColor;
uniform float     uAngle;
uniform sampler2D uArt;
uniform int       uHasArt;
in  vec2 vUV;
out vec4 fragColor;
void main() {
    vec2 uv  = vUV - 0.5;
    float s  = sin(uAngle), c = cos(uAngle);
    vec2  ruv = vec2(c*uv.x - s*uv.y, s*uv.x + c*uv.y);
    float r  = length(uv) * 2.0;
    if (r > 1.0) discard;
    const float hole  = 0.09;
    const float label = 0.42;
    if (r < hole) { fragColor = vec4(0.0, 0.0, 0.0, 1.0); return; }
    if (uHasArt != 0) {
        vec2 art_uv = ruv + 0.5;
        vec3 art = texture(uArt, art_uv).rgb;
        float groove = fract(r * 55.0);
        float gv = (r > label) ? mix(0.0, 0.22, groove * groove) : 0.0;
        fragColor = vec4(art * (1.0 - gv), 1.0);
        return;
    }
    if (r < label) {
        float edge = 1.0 - smoothstep(label - 0.04, label, r * 0.5);
        fragColor = vec4(uColor * edge + uColor * 0.4 * (1.0 - edge), 1.0);
        return;
    }
    float groove = fract(r * 55.0);
    float v = mix(0.07, 0.18, groove * groove);
    float shine = max(0.0, dot(normalize(ruv + vec2(0.5)), vec2(0.7, 0.7)));
    v += shine * 0.06;
    fragColor = vec4(v, v, v, 1.0);
}
)GLSL";

static const char* FRAG_PILL = R"GLSL(
#version 330 core
uniform sampler2D uTex;
uniform vec4      uColor;
uniform vec2      uSize;
in  vec2 vUV;
out vec4 fragColor;
void main() {
    vec2 px = vUV * uSize;
    float r  = uSize.y * 0.5;
    vec2  q  = clamp(px, vec2(r), uSize - vec2(r));
    float d  = length(px - q);
    float mask = 1.0 - smoothstep(r - 1.5, r, d);
    if (mask <= 0.0) discard;
    vec4 c = texture(uTex, vUV) * uColor;
    fragColor = vec4(c.rgb, c.a * mask);
}
)GLSL";

static const char* FRAG_SOLID = R"GLSL(
#version 330 core
uniform vec4 uColor;
out vec4 fragColor;
void main() { fragColor = uColor; }
)GLSL";

static const char* FRAG_TEXT = R"GLSL(
#version 330 core
uniform sampler2D uTex;
uniform vec4      uColor;
in  vec2 vUV;
out vec4 fragColor;
void main() {
    float a = texture(uTex, vUV).r;
    fragColor = vec4(uColor.rgb, a * uColor.a);
}
)GLSL";

static const char* FRAG_SHADOW = R"GLSL(
#version 330 core
uniform float uAlpha;
out vec4 fragColor;
void main() {
    fragColor = vec4(0.0, 0.0, 0.0, uAlpha);
}
)GLSL";
