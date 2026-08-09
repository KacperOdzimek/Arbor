#version 430 core

layout(location = 0) in      vec2  in_pos;
layout(location = 1) in      vec2  in_uv;
layout(location = 2) in      vec2  in_local_pos;
layout(location = 3) flat in int   in_clipbox_index;
layout(location = 4) flat in int   in_texture_index;
layout(location = 5) flat in float in_rounding;
layout(location = 6) flat in vec4  in_color;
layout(location = 7) flat in vec2  in_half_size_px;

layout(location = 0) out vec4 outColor;

// Direct SSBO binding for clipboxes
layout(std430, binding = 3) readonly buffer ClipboxesBuffer {
    mat3x2 clipboxes[];
};

layout(binding = 0) uniform sampler2D tex_samplers[16]; // Scale array size to your needs

mat3 expand_affine(mat3x2 a) {
    return mat3(
        vec3(a[0], 0.0),
        vec3(a[1], 0.0),
        vec3(a[2], 1.0)
    );
}

bool point_in_clip(mat3 t, vec2 p) {
    vec2 c0 = t[0].xy; // (m00, m10)
    vec2 c1 = t[1].xy; // (m01, m11)
    vec2 c2 = t[2].xy; // (tx, ty)

    float det = c0.x * c1.y - c1.x * c0.y;
    if (abs(det) < 0.00001) return true;

    float inv00 =  c1.y / det; float inv01 = -c1.x / det;
    float inv10 = -c0.y / det; float inv11 =  c0.x / det;

    vec2 d = p - c2;
    vec2 local = vec2(
        inv00 * d.x + inv01 * d.y,
        inv10 * d.x + inv11 * d.y
    );

    return (
        local.x >= -1.0 && local.x <= 1.0 && 
        local.y >= -1.0 && local.y <= 1.0
    );
}

float rounded_box_sdf(vec2 p, vec2 half_size, float radius) {
    vec2 q = abs(p) - half_size + vec2(radius);
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
}

vec3 srgb_to_linear(vec3 c) {
    return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), step(0.04045, c));
}

void main() {
    if (in_clipbox_index >= 0) {
        mat3 clip = expand_affine(clipboxes[in_clipbox_index]);
        if (!point_in_clip(clip, in_pos)) discard;
    }

    float rounding_alpha = 1.0;
    if (in_rounding > 0.0) {
        vec2 p = in_local_pos * in_half_size_px;
        float dist = rounded_box_sdf(p, in_half_size_px, in_rounding);
        float aa = max(fwidth(dist), 0.0001);
        rounding_alpha = 1.0 - smoothstep(-aa, aa, dist);
        if (rounding_alpha <= 0.0) discard;
    }

    vec4 tint = vec4(srgb_to_linear(in_color.rgb), in_color.a);
    bool has_texture = in_texture_index != 0;
    bool is_font     = in_texture_index < 0;
    
    int tex_index = has_texture ? (is_font ? -(in_texture_index + 1) : in_texture_index - 1) : 0;
    
    // Standard OpenGL texture lookup
    vec4 texture_color = has_texture ? texture(tex_samplers[tex_index], in_uv) : vec4(1.0);

    if (is_font) {
        float dist  = texture_color.r;
        float edge  = 0.5;
        float width = fwidth(dist);
        float alpha = smoothstep(
            edge - width,
            edge + width,
            dist
        );
        texture_color = vec4(1.0, 1.0, 1.0, alpha);
    }

    outColor = vec4(
        texture_color.rgb * tint.rgb,
        texture_color.a * tint.a * rounding_alpha
    );
}
