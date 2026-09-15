#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float phase;
    vec2 resolution;
    vec4 first;
    vec4 second;
    vec4 third;
    vec4 fourth;
};

float blob(vec2 p, vec2 center, float size, float angle) {
    vec2 delta = p - center;
    float c = cos(angle);
    float s = sin(angle);
    delta = mat2(c, -s, s, c) * delta;
    delta *= vec2(1.0, 1.35 + 0.15 * sin(phase + angle));
    return exp(-dot(delta, delta) / size);
}

void main() {
    float aspect = resolution.x / max(resolution.y, 1.0);
    vec2 p = qt_TexCoord0 * vec2(aspect, 1.0);
    float a = blob(p, vec2(aspect * (0.4 + 0.12 * sin(phase)), 0.28 + 0.16 * cos(phase)), 0.18, phase);
    float b = blob(p, vec2(aspect * (0.8 + 0.1 * cos(phase)), 0.55 + 0.2 * sin(phase)), 0.22, -phase);
    float c = blob(p, vec2(aspect * (0.62 + 0.16 * sin(phase + 2.0)), 0.85), 0.14, phase + 1.0);
    float d = blob(p, vec2(aspect * 0.9, 0.1 + 0.13 * sin(phase + 4.0)), 0.2, -phase + 2.0);
    vec3 base = vec3(0.063, 0.071, 0.11);
    vec3 color = (base * 0.55 + first.rgb * a + second.rgb * b + third.rgb * c + fourth.rgb * d)
               / (0.55 + a + b + c + d);
    fragColor = vec4(color, 1.0) * qt_Opacity;
}
