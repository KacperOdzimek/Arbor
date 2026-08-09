#version 430 core

layout(location = 0) out      vec2  out_pos;
layout(location = 1) out      vec2  out_uv;
layout(location = 2) out      vec2  out_local_pos;
layout(location = 3) flat out int   out_clipbox_index;
layout(location = 4) flat out int   out_texture_index;
layout(location = 5) flat out float out_rounding;
layout(location = 6) flat out vec4  out_color;
layout(location = 7) flat out vec2  out_half_size_px;

struct uv_2d {
    float min_x, min_y;
    float max_x, max_y;
};

struct gpu_instance {
    int item;
    int glyph;
};

struct gpu_draw_item {
    mat3x2 transform;
    uv_2d  atlas_position;
    int    texture_index;
    int    clipbox_index;
    uint   shader_index;
    int    rounding_pixel;
    float  r, g, b, a;
};

struct gpu_glyph {
    uv_2d atlas_position;
    float off_x, off_y;
    float size_x, size_y;
};

uniform uint resolution_width;
uniform uint resolution_height;

// Direct SSBO bindings
layout(std430, binding = 0) readonly buffer InstancesBuffer {
    gpu_instance instances[];
};

layout(std430, binding = 1) readonly buffer DrawItemsBuffer {
    gpu_draw_item draw_items[];
};

layout(std430, binding = 2) readonly buffer GlyphsBuffer {
    gpu_glyph glyphs[];
};

vec2 vert_pos[4] = {
    vec2(-1.0, -1.0),
    vec2( 1.0, -1.0),
    vec2(-1.0,  1.0),
    vec2( 1.0,  1.0)
};

vec2 vert_uv[4] = {
    vec2(0.0, 0.0),
    vec2(1.0, 0.0),
    vec2(0.0, 1.0),
    vec2(1.0, 1.0)
};

mat3 expand_affine(mat3x2 a) {
    return mat3(
        vec3(a[0], 0.0),
        vec3(a[1], 0.0),
        vec3(a[2], 1.0)
    );
}

void main() {
    // Simplified buffer access
    gpu_instance  inst = instances[gl_InstanceID];
    gpu_draw_item item = draw_items[inst.item];

    mat3 transform = expand_affine(item.transform);

    vec2 box_half_size_px = vec2(
        resolution_width  * 0.5 * (abs(transform[0][0]) + abs(transform[0][1])),
        resolution_height * 0.5 * (abs(transform[1][0]) + abs(transform[1][1]))
    );

    vec2 uv = mix(
        vec2(item.atlas_position.min_x, item.atlas_position.min_y),
        vec2(item.atlas_position.max_x, item.atlas_position.max_y),
        vert_uv[gl_VertexID]
    );

    if (inst.glyph != -1) {
        gpu_glyph glyph = glyphs[inst.glyph];

        vec2 text_box_size = vec2(
            resolution_width  * (abs(transform[0][0]) + abs(transform[0][1])),
            resolution_height * (abs(transform[1][0]) + abs(transform[1][1]))
        );

        uv = mix(
            vec2(glyph.atlas_position.min_x, glyph.atlas_position.min_y),
            vec2(glyph.atlas_position.max_x, glyph.atlas_position.max_y),
            uv
        );

        float pixel_to_norm_x = 2.0 / text_box_size.x;
        float pixel_to_norm_y = 2.0 / text_box_size.y;

        mat3 local_transform = mat3(1.0);

        local_transform *= mat3(
            1.0, 0.0, 0.0,
            0.0, 1.0, 0.0,
            -1.0 + (2.0 * glyph.off_x + glyph.size_x) / text_box_size.x,
            -1.0 + (2.0 * glyph.off_y - glyph.size_y) / text_box_size.y,
            1.0
        );

        local_transform *= mat3(
            glyph.size_x * pixel_to_norm_x / 2, 0.0, 0.0,
            0.0, glyph.size_y * pixel_to_norm_y / 2, 0.0,
            0.0, 0.0, 1.0
        );

        transform = transform * local_transform;
    }

    vec2 local_pos = vert_pos[gl_VertexID];
    vec2 pos = (transform * vec3(local_pos, 1.0)).xy;

    gl_Position = vec4(pos.x, pos.y, 0.0, 1.0);

    out_pos           = pos;
    out_uv            = uv;
    out_clipbox_index = item.clipbox_index;
    out_texture_index = item.texture_index;
    out_rounding      = float(item.rounding_pixel);
    out_color         = vec4(item.r, item.g, item.b, item.a);
    out_local_pos     = local_pos;
    out_half_size_px  = box_half_size_px;
}
