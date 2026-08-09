#include "renderer.h"

#include "arbor/arbor.h"
#include "deps/stb_truetype.h"

#include "deps/glad.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// ===========================
// Table of contents
//
//   1. Rendering Objects       - the format of data we send to GPU shaders
//   2. Data Structures         - remaining internal structs (font + subpartitioner)
//   3. Internal State          - the renderer's state
//   4. Forward Declarations    - functions forwards
//   5. Rendering Itself        - arbor_renderer_draw_frame, the actual per-frame draw
//   6. Rendering API           - arbor_renderer_init / _shutdown and their helpers
//   7. Text Generation         - arb_injection_text_layout
//   8. Font System             - dynamic font atlas: baking & caching glyphs
//   9. Subpartitioning         - glyph SSBO suballocator
//  10. Helpers                 - small utilities used all over the file

// ===========================
// Notes:
//
// Subpartitioner:
// Glyphs buffer subpartitioner was vibe coded, as is not main point of this example
// It is inefficitent, rewriting entire glyphs buffer, when a tiny change happens 
// Intended model would be to only keep glyphs on GPU, and direcly writing changed partitions only
//
// Images:
// For sake of simplicity adding images was completly ommited in this renderer
// this would require additional 200 lines of code of bookeeping
//
// Fonts:
// Only one font allowed, same reason as with images

// ===========================
// 1. Rendering Objects
// The format of the data we actually send to the OpenGL shaders.

// Image/Font atlas region
typedef struct uv_2d {
    float       min_x, min_y;       // Rectangle left-down corner
    float       max_x, max_y;       // Rectangle right-up corner
} uv_2d;

// Quad instance: which draw item to draw, and which glyph if it's text
typedef struct gpu_instance {
    int         item;               // Which draw request?
    int         glyph;              // Which glyph from the glyphs buffer? (-1 if box)
} gpu_instance;

// Draw item parameter bundle
typedef struct gpu_draw_item {
    arb_mat3x2 transform;           // Draw item (bounding) box transform
    uv_2d      atlas_position;      // Texture region
    int        texture_index;       // Negative = font, positive = image (offset by one both sides so 0 means "no texture")
    int        clipbox_index;       // Clipbox index (-1 = no clipbox, else pulls clipboxes_buffer[clipbox_index])
    uint32_t   shader_index;        // Passed to shader box_data.shader / text_data.shader - up to the shader to interpret, for custom effects
    int        rounding_pixel;      // Box corner rounding radius
    float      r, g, b, a;          // Draw item tint
} gpu_draw_item;

// Clipboxes buffer element
typedef struct gpu_clipbox {
    arb_mat3x2 transform;           // Clipbox transform
} gpu_clipbox;

// Single glyph of text, offset in pixels, so a change of window resolution
// does not invalidate text layout
typedef struct gpu_glyph {
    uv_2d       atlas_position;     // Glyph font texture region
    float       off_x, off_y;       // Pixel offset
    float       size_x, size_y;     // Pixel dimensions
} gpu_glyph;


// ===========================
// 2. Data Structures
// Definied here, so can be used by renderer

// Font system

typedef struct {
    uint32_t            codepoint;
    float               uv_min_x, uv_min_y, uv_max_x, uv_max_y;
    float               off_x, off_y;
    float               size_x, size_y;
    float               advance;
} dynamic_glyph_info;

typedef struct {
    stbtt_fontinfo      font_info;
    unsigned char*      ttf_buffer;
    GLuint              font_texture;
    int                 atlas_width;
    int                 atlas_height;
    int                 current_x;
    int                 current_y;
    int                 max_row_h;
    float               font_size_pixels;

    dynamic_glyph_info* cached_glyphs;
    size_t              glyph_count;
    size_t              glyph_capacity;
} dynamic_font_atlas;

// Subpartitioned glyph allocator
// Used to fit all glyphs in one buffer for batched rendering
// The allocator provided is very simple (just for the sake of example) and not good for shipping

typedef struct text_allocation { size_t count; size_t offset; } text_allocation;
typedef struct glyph_block { size_t offset; size_t size; } glyph_block;
typedef struct glyph_allocator {
    gpu_glyph*      buffer;
    size_t          capacity;
    glyph_block*    free_blocks;
    size_t          free_block_count;
    size_t          free_block_capacity;
    int             needs_gpu_resize;
    int             needs_gpu_upload;
} glyph_allocator;

// ===========================
// 3. Internal State

// Rendering itself / API
static GLuint ssbo_instances, ssbo_draw_items, ssbo_clipboxes, ssbo_glyphs;
static GLuint shader_program;
static GLuint empty_vao;
static int    inited = 0;

// Font system
static dynamic_font_atlas g_font_atlas;

// Subpartitioning
static glyph_allocator    g_glyph_alloc;

// ===========================
// 4. Forward Declarations

// Rendering API
static void     init_buffers(void);
static void     shutdown_buffers(void);
static GLuint   compile_shader(GLenum type, const char* source, const char* filepath);
static GLuint   load_shaders(const char* vertex_path, const char* fragment_path);

// Font system
static int      init_dynamic_font_atlas(const char* filepath, float pixel_height);
static void     shutdown_dynamic_font_atlas(void);
static dynamic_glyph_info* get_or_bake_glyph(uint32_t codepoint);

// Subpartitioning
static void     init_glyph_allocator(size_t cap);
static void     shutdown_glyph_allocator(void);
static int      cmp_blocks(const void* a, const void* b);
static void     free_glyphs(size_t offset, size_t count);
static size_t   alloc_glyphs(size_t count, const gpu_glyph* data);

// Helpers
static char*    read_file_content(const char* filepath);
static uint32_t decode_utf8(const char** p);

// ===========================
// 5. Rendering Itself

void arbor_renderer_draw_frame(arb_upload_access access, int width, int height) {
    // Process text frees
    for (size_t i = 0; i < access.text_free_count; i++) {
        text_allocation* alloc = (text_allocation*)access.text_free_requests[i].text_pointer;
        if (alloc) {
            free_glyphs(alloc->offset, alloc->count);
            free(alloc);
        }
    }

    // Process new text allocations
    for (size_t i = 0; i < access.text_alloc_count; i++) {
        text_allocation* alloc = (text_allocation*)malloc(sizeof(text_allocation));
        alloc->count = access.text_alloc_requests[i].glyphs_count;

        // Let the suballocator find a block, expand the SSBO if necessary, and dump the data
        alloc->offset = alloc_glyphs(alloc->count, (const gpu_glyph*)access.text_alloc_requests[i].glyphs);
        *access.text_alloc_requests[i].text_pointer_out = alloc;
    }

    // Upload glyphs to glyphs buffer
    if (g_glyph_alloc.needs_gpu_resize) {
        // Full reallocation/upload if the buffer capacity grew
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_glyphs);
        glBufferData(GL_SHADER_STORAGE_BUFFER, g_glyph_alloc.capacity * sizeof(gpu_glyph), g_glyph_alloc.buffer, GL_DYNAMIC_DRAW);
        g_glyph_alloc.needs_gpu_resize = 0;
        g_glyph_alloc.needs_gpu_upload = 0;
    } else if (g_glyph_alloc.needs_gpu_upload) {
        // Fast sync if block chunks were modified
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_glyphs);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, g_glyph_alloc.capacity * sizeof(gpu_glyph), g_glyph_alloc.buffer);
        g_glyph_alloc.needs_gpu_upload = 0;
    }

    // --- 3. Build Layout Instances & Draw Items ---

    // Calculate dynamic upper bounds to prevent out-of-bounds segfaults
    size_t max_instances = access.draws_count;
    for (size_t i = 0; i < access.draws_count; i++) {
        if (!access.draws_requests[i].is_box_not_text) {
            text_allocation* alloc = (text_allocation*)(*access.draws_requests[i].text.pointer);
            if (alloc) max_instances += alloc->count;
        }
    }

    size_t total_instances = 0;
    gpu_instance* instances   = (gpu_instance*)malloc(sizeof(gpu_instance) * (max_instances > 0 ? max_instances : 1));
    gpu_draw_item* draw_items = (gpu_draw_item*)malloc(sizeof(gpu_draw_item) * (access.draws_count > 0 ? access.draws_count : 1));
    gpu_clipbox* clipboxes    = (gpu_clipbox*)malloc(sizeof(gpu_clipbox) * (access.clipboxes_count > 0 ? access.clipboxes_count : 1));

    for (size_t i = 0; i < access.clipboxes_count; i++) {
        clipboxes[i].transform = access.clipboxes_requests[i].transform;
    }

    for (size_t i = 0; i < access.draws_count; i++) {
        const arb_draw_request* req = &access.draws_requests[i];

        draw_items[i].transform     = req->transform;
        draw_items[i].clipbox_index = req->clip_index;

        if (req->is_box_not_text) {
            draw_items[i].texture_index  = req->box.data.image ? 1 : 0;
            draw_items[i].shader_index   = req->box.data.shader;
            draw_items[i].rounding_pixel = req->box.data.rounding;
            draw_items[i].r = req->box.data.tint.r / 255.0f;
            draw_items[i].g = req->box.data.tint.g / 255.0f;
            draw_items[i].b = req->box.data.tint.b / 255.0f;
            draw_items[i].a = req->box.data.tint.a / 255.0f;

            instances[total_instances++] = (gpu_instance){ .item = (int)i, .glyph = -1 };
        } else {
            draw_items[i].texture_index  = -1;
            draw_items[i].shader_index   = req->text.data.shader;
            draw_items[i].rounding_pixel = 0;
            draw_items[i].r = req->text.data.tint.r / 255.0f;
            draw_items[i].g = req->text.data.tint.g / 255.0f;
            draw_items[i].b = req->text.data.tint.b / 255.0f;
            draw_items[i].a = req->text.data.tint.a / 255.0f;
            draw_items[i].atlas_position = (uv_2d){0, 0, 1, 1};

            text_allocation* alloc = (text_allocation*)(*req->text.pointer);
            if (alloc) {
                // Loop over our partition instead of injecting into a master glyph array
                for (size_t g = 0; g < alloc->count; g++) {
                    instances[total_instances++] = (gpu_instance){ .item = (int)i, .glyph = (int)(alloc->offset + g) };
                }
            }
        }
    }

    // --- 4. Upload & Draw ---
    if (access.clipboxes_count > 0) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_clipboxes);
        glBufferData(GL_SHADER_STORAGE_BUFFER, access.clipboxes_count * sizeof(gpu_clipbox), clipboxes, GL_DYNAMIC_DRAW);
    }

    if (access.draws_count > 0) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_draw_items);
        glBufferData(GL_SHADER_STORAGE_BUFFER, access.draws_count * sizeof(gpu_draw_item), draw_items, GL_DYNAMIC_DRAW);
    }

    if (total_instances > 0) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_instances);
        glBufferData(GL_SHADER_STORAGE_BUFFER, total_instances * sizeof(gpu_instance), instances, GL_DYNAMIC_DRAW);
    }

    glViewport(0, 0, width, height);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(shader_program);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo_instances);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssbo_draw_items);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ssbo_glyphs);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, ssbo_clipboxes);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_font_atlas.font_texture);

    GLint loc_res_width  = glGetUniformLocation(shader_program, "resolution_width");
    GLint loc_res_height = glGetUniformLocation(shader_program, "resolution_height");
    glUniform1ui(loc_res_width, width);
    glUniform1ui(loc_res_height, height);

    if (total_instances > 0) {
        glBindVertexArray(empty_vao);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)total_instances);
        glBindVertexArray(0);
    }

    free(instances);
    free(draw_items);
    free(clipboxes);
}

// ===========================
// 6. Rendering API

int arbor_renderer_init(
    const char*     font_path,
    float           font_pixel_height,
    const char*     vertex_shader_path,
    const char*     fragment_shader_path
) {
    inited = 1;

    init_buffers();
    init_glyph_allocator(1024); // Start with 1024 contiguous glyph slots

    if (!init_dynamic_font_atlas(font_path, font_pixel_height)) {
        fprintf(stderr, "Failed to init font atlas. Exiting.\n");
        return 0;
    }

    shader_program = load_shaders(vertex_shader_path, fragment_shader_path);
    if (shader_program == 0) {
        fprintf(stderr, "Failed to load shaders. Exiting.\n");
        return 0;
    }

    return 1;
}

void arbor_renderer_shutdown(void) {
    if (!inited) return;

    if (shader_program) {
        glDeleteProgram(shader_program);
        shader_program = 0;
    }

    shutdown_buffers();
    shutdown_glyph_allocator();
    shutdown_dynamic_font_atlas();
}

static void init_buffers(void) {
    glGenVertexArrays(1, &empty_vao);

    glGenBuffers(1, &ssbo_instances);
    glGenBuffers(1, &ssbo_draw_items);
    glGenBuffers(1, &ssbo_clipboxes);
    glGenBuffers(1, &ssbo_glyphs);
}

static void shutdown_buffers(void) {
    glDeleteVertexArrays(1, &empty_vao);
    glDeleteBuffers(1, &ssbo_instances);
    glDeleteBuffers(1, &ssbo_draw_items);
    glDeleteBuffers(1, &ssbo_clipboxes);
    glDeleteBuffers(1, &ssbo_glyphs);
}

static GLuint compile_shader(GLenum type, const char* source, const char* filepath) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar info_log[1024];
        glGetShaderInfoLog(shader, sizeof(info_log), NULL, info_log);
        fprintf(stderr, "ERROR: Shader compilation failed in %s\n%s\n", filepath, info_log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint load_shaders(const char* vertex_path, const char* fragment_path) {
    char* vertex_code = read_file_content(vertex_path);
    char* fragment_code = read_file_content(fragment_path);

    if (!vertex_code || !fragment_code) {
        free(vertex_code);
        free(fragment_code);
        return 0;
    }

    GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_code, vertex_path);
    GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_code, fragment_path);

    free(vertex_code);
    free(fragment_code);

    if (!vertex_shader || !fragment_shader) return 0;

    GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        GLchar info_log[1024];
        glGetProgramInfoLog(program, sizeof(info_log), NULL, info_log);
        fprintf(stderr, "ERROR: Shader linking failed\n%s\n", info_log);
        glDeleteProgram(program);
        program = 0;
    }

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    return program;
}

// ===========================
// 7. Text Generation

void arb_injection_text_layout(
    const arb_text_data* text_data,
    int width_constrain,
    size_t* out_count,
    void** out_glyphs,
    int* out_width,
    int* out_height
) {
    const char* text = (const char*)text_data->text;

    if (!text || text[0] == '\0') {
        *out_count  = 0; *out_glyphs = NULL;
        *out_width  = 0; *out_height = 0;
        return;
    }

    size_t glyph_count = 0;
    size_t extra_lines_count = 0;
    {
        const char* ptr = text;
        while (*ptr != '\0') {
            uint32_t cp = decode_utf8(&ptr);
            if (cp == '\n') extra_lines_count++;
            else glyph_count++;
        }
    }

    gpu_glyph* glyphs = glyph_count ? (gpu_glyph*)malloc(sizeof(gpu_glyph) * glyph_count) : NULL;

    const float font_scale = stbtt_ScaleForPixelHeight(&g_font_atlas.font_info, (float)text_data->size);

    int unscaled_ascent, unscaled_descent, unscaled_line_gap;
    stbtt_GetFontVMetrics(&g_font_atlas.font_info, &unscaled_ascent, &unscaled_descent, &unscaled_line_gap);

    const float ascent      = unscaled_ascent * font_scale;
    const float descent     = unscaled_descent * font_scale;
    const float line_gap    = unscaled_line_gap * font_scale;
    const float line_height = ascent - descent + line_gap;

    float  pen_x      = 0.0f;
    float  pen_y      = 0.0f;
    float  text_width = 0.0f;
    size_t glyph_idx  = 0;
    uint32_t prev_cp  = 0;

    const float atlas_base_size = g_font_atlas.font_size_pixels;
    const float atlas_scale = (float)text_data->size / atlas_base_size;

    const char* ptr = text;
    while (*ptr != '\0') {
        uint32_t cp = decode_utf8(&ptr);

        if (cp == '\n') {
            if (pen_x > text_width) text_width = pen_x;
            pen_x   = 0.0f;
            pen_y  -= line_height;
            prev_cp = 0;
            continue;
        }

        if (prev_cp) {
            pen_x += stbtt_GetCodepointKernAdvance(&g_font_atlas.font_info, (int)prev_cp, (int)cp) * font_scale;
        }

        dynamic_glyph_info* info = get_or_bake_glyph(cp);

        if (glyph_idx < glyph_count && info) {
            glyphs[glyph_idx++] = (gpu_glyph){
                .atlas_position = { info->uv_min_x, info->uv_min_y, info->uv_max_x, info->uv_max_y },
                .off_x          = pen_x + (info->off_x * atlas_scale),
                .off_y          = (extra_lines_count * line_height + pen_y) - (info->off_y * atlas_scale),
                .size_x         = info->size_x * atlas_scale,
                .size_y         = info->size_y * atlas_scale,
            };
        }

        pen_x  += info ? (info->advance * atlas_scale) : (10.0f * atlas_scale);
        prev_cp = cp;
    }

    if (pen_x > text_width) text_width = pen_x;
    float text_height = -pen_y + ascent;

    *out_width  = (int)text_width;
    *out_height = (int)text_height;
    *out_count  = glyph_count;
    *out_glyphs = glyphs;
}

// ===========================
// 8. Font System
// A dynamic font atlas: glyphs are baked into a signed-distance-field texture the first
// time they're requested, then cached and reused for every subsequent request.

static int init_dynamic_font_atlas(const char* filepath, float pixel_height) {
    g_font_atlas.ttf_buffer = (unsigned char*)read_file_content(filepath);
    if (!g_font_atlas.ttf_buffer) return 0;

    if (!stbtt_InitFont(&g_font_atlas.font_info, g_font_atlas.ttf_buffer, 0)) {
        fprintf(stderr, "ERROR: Failed to initialize font for dynamic loading\n");
        return 0;
    }

    g_font_atlas.atlas_width = 2048;
    g_font_atlas.atlas_height = 2048;
    g_font_atlas.current_x = 0;
    g_font_atlas.current_y = 0;
    g_font_atlas.max_row_h = 0;
    g_font_atlas.font_size_pixels = pixel_height;
    g_font_atlas.glyph_count = 0;
    g_font_atlas.glyph_capacity = 0;
    g_font_atlas.cached_glyphs = NULL;

    glGenTextures(1, &g_font_atlas.font_texture);
    glBindTexture(GL_TEXTURE_2D, g_font_atlas.font_texture);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, g_font_atlas.atlas_width, g_font_atlas.atlas_height, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    return 1;
}

static void shutdown_dynamic_font_atlas(void) {
    if (g_font_atlas.font_texture) {
        glDeleteTextures(1, &g_font_atlas.font_texture);
        g_font_atlas.font_texture = 0;
    }
    free(g_font_atlas.ttf_buffer);
    g_font_atlas.ttf_buffer = NULL;
    free(g_font_atlas.cached_glyphs);
    g_font_atlas.cached_glyphs = NULL;
    g_font_atlas.glyph_count = 0;
    g_font_atlas.glyph_capacity = 0;
}

static dynamic_glyph_info* get_or_bake_glyph(uint32_t codepoint) {
    for (size_t i = 0; i < g_font_atlas.glyph_count; i++) {
        if (g_font_atlas.cached_glyphs[i].codepoint == codepoint) {
            return &g_font_atlas.cached_glyphs[i];
        }
    }

    float scale = stbtt_ScaleForPixelHeight(&g_font_atlas.font_info, g_font_atlas.font_size_pixels);
    int width, height, xoff, yoff;

    if (codepoint == ' ' || codepoint == '\t' || codepoint == '\r' || codepoint == 0x00A0) {
        int advance, lsb;
        stbtt_GetCodepointHMetrics(&g_font_atlas.font_info, (int)codepoint, &advance, &lsb);

        if (g_font_atlas.glyph_count >= g_font_atlas.glyph_capacity) {
            g_font_atlas.glyph_capacity = g_font_atlas.glyph_capacity ? g_font_atlas.glyph_capacity * 2 : 256;
            g_font_atlas.cached_glyphs = (dynamic_glyph_info*)realloc(g_font_atlas.cached_glyphs, g_font_atlas.glyph_capacity * sizeof(dynamic_glyph_info));
        }

        dynamic_glyph_info* info = &g_font_atlas.cached_glyphs[g_font_atlas.glyph_count++];
        info->codepoint = codepoint;
        info->uv_min_x  = 0.0f; info->uv_min_y = 0.0f;
        info->uv_max_x  = 0.0f; info->uv_max_y = 0.0f;
        info->off_x     = 0.0f; info->off_y    = 0.0f;
        info->size_x    = 0.0f; info->size_y   = 0.0f;
        info->advance   = advance * scale;
        return info;
    }

    unsigned char* sdf = stbtt_GetCodepointSDF(
        &g_font_atlas.font_info, scale, (int)codepoint, 5, 128, 64,
        &width, &height, &xoff, &yoff
    );

    if (!sdf) {
        if (codepoint != '?') return get_or_bake_glyph('?');
        return NULL;
    }

    if (g_font_atlas.current_x + width + 2 >= g_font_atlas.atlas_width) {
        g_font_atlas.current_x = 0;
        g_font_atlas.current_y += g_font_atlas.max_row_h;
        g_font_atlas.max_row_h = 0;
    }

    if (g_font_atlas.current_y + height + 2 >= g_font_atlas.atlas_height) {
        fprintf(stderr, "WARNING: Font atlas full!\n");
        stbtt_FreeSDF(sdf, g_font_atlas.font_info.userdata);
        return get_or_bake_glyph('?');
    }

    int px = g_font_atlas.current_x + 1;
    int py = g_font_atlas.current_y + 1;

    glBindTexture(GL_TEXTURE_2D, g_font_atlas.font_texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, px, py, width, height, GL_RED, GL_UNSIGNED_BYTE, sdf);
    stbtt_FreeSDF(sdf, g_font_atlas.font_info.userdata);

    int advance, lsb;
    stbtt_GetCodepointHMetrics(&g_font_atlas.font_info, (int)codepoint, &advance, &lsb);

    if (g_font_atlas.glyph_count >= g_font_atlas.glyph_capacity) {
        g_font_atlas.glyph_capacity = g_font_atlas.glyph_capacity ? g_font_atlas.glyph_capacity * 2 : 256;
        g_font_atlas.cached_glyphs = (dynamic_glyph_info*)realloc(g_font_atlas.cached_glyphs, g_font_atlas.glyph_capacity * sizeof(dynamic_glyph_info));
    }

    dynamic_glyph_info* info = &g_font_atlas.cached_glyphs[g_font_atlas.glyph_count++];
    info->codepoint = codepoint;
    info->uv_min_x  = (float)px / g_font_atlas.atlas_width;
    info->uv_max_x  = (float)(px + width) / g_font_atlas.atlas_width;
    info->uv_min_y  = (float)(py + height) / g_font_atlas.atlas_height;
    info->uv_max_y  = (float)py / g_font_atlas.atlas_height;
    info->off_x     = (float)xoff;
    info->off_y     = (float)yoff;
    info->size_x    = (float)width;
    info->size_y    = (float)height;
    info->advance   = advance * scale;

    g_font_atlas.current_x += width + 3;
    if (height + 3 > g_font_atlas.max_row_h) {
        g_font_atlas.max_row_h = height + 3;
    }

    return info;
}

// ===========================
// 9. Subpartitioning
// Used to fit all glyphs in one buffer for batched rendering
// The allocator provided is very simple (just for the sake of example) and not good for shipping

static void init_glyph_allocator(size_t cap) {
    g_glyph_alloc.capacity = cap ? cap : 1024;
    g_glyph_alloc.buffer = (gpu_glyph*)malloc(g_glyph_alloc.capacity * sizeof(gpu_glyph));
    g_glyph_alloc.free_block_capacity = 256;
    g_glyph_alloc.free_blocks = (glyph_block*)malloc(g_glyph_alloc.free_block_capacity * sizeof(glyph_block));
    g_glyph_alloc.free_blocks[0] = (glyph_block){0, g_glyph_alloc.capacity};
    g_glyph_alloc.free_block_count = 1;
    g_glyph_alloc.needs_gpu_resize = 1;
}

static void shutdown_glyph_allocator(void) {
    free(g_glyph_alloc.buffer);
    free(g_glyph_alloc.free_blocks);
}

static int cmp_blocks(const void* a, const void* b) {
    size_t oa = ((const glyph_block*)a)->offset, ob = ((const glyph_block*)b)->offset;
    return (oa > ob) - (oa < ob);
}

static void free_glyphs(size_t offset, size_t count) {
    if (!count) return;
    if (g_glyph_alloc.free_block_count >= g_glyph_alloc.free_block_capacity) {
        g_glyph_alloc.free_block_capacity *= 2;
        g_glyph_alloc.free_blocks = (glyph_block*)realloc(g_glyph_alloc.free_blocks,
            g_glyph_alloc.free_block_capacity * sizeof(glyph_block));
    }
    g_glyph_alloc.free_blocks[g_glyph_alloc.free_block_count++] = (glyph_block){offset, count};
    qsort(g_glyph_alloc.free_blocks, g_glyph_alloc.free_block_count, sizeof(glyph_block), cmp_blocks);

    // Merge adjacent free blocks
    size_t w = 0;
    for (size_t i = 1; i < g_glyph_alloc.free_block_count; i++) {
        if (g_glyph_alloc.free_blocks[w].offset + g_glyph_alloc.free_blocks[w].size == g_glyph_alloc.free_blocks[i].offset) {
            g_glyph_alloc.free_blocks[w].size += g_glyph_alloc.free_blocks[i].size;
        } else {
            g_glyph_alloc.free_blocks[++w] = g_glyph_alloc.free_blocks[i];
        }
    }
    g_glyph_alloc.free_block_count = w + 1;
}

static size_t alloc_glyphs(size_t count, const gpu_glyph* data) {
    if (!count) return 0;
    for (size_t i = 0; i < g_glyph_alloc.free_block_count; i++) {
        if (g_glyph_alloc.free_blocks[i].size >= count) {
            size_t offset = g_glyph_alloc.free_blocks[i].offset;
            g_glyph_alloc.free_blocks[i].offset += count;

            // If the block is fully consumed, shift the array left
            if ((g_glyph_alloc.free_blocks[i].size -= count) == 0) {
                memmove(&g_glyph_alloc.free_blocks[i], &g_glyph_alloc.free_blocks[i + 1],
                        (--g_glyph_alloc.free_block_count - i) * sizeof(glyph_block));
            }

            memcpy(&g_glyph_alloc.buffer[offset], data, count * sizeof(gpu_glyph));
            g_glyph_alloc.needs_gpu_upload = 1;
            return offset;
        }
    }

    // No block big enough - grow the buffer and retry
    size_t old_cap = g_glyph_alloc.capacity;
    while (g_glyph_alloc.capacity - old_cap < count) g_glyph_alloc.capacity *= 2;
    g_glyph_alloc.buffer = (gpu_glyph*)realloc(g_glyph_alloc.buffer, g_glyph_alloc.capacity * sizeof(gpu_glyph));
    g_glyph_alloc.needs_gpu_resize = 1;
    free_glyphs(old_cap, g_glyph_alloc.capacity - old_cap);
    return alloc_glyphs(count, data);
}

// ===========================
// 10. Helpers
// Nothing important here!

static char* read_file_content(const char* filepath) {
    FILE* file = fopen(filepath, "rb");
    if (!file) {
        fprintf(stderr, "ERROR: Could not open file: %s\n", filepath);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* buffer = (char*)malloc(length + 1);
    if (!buffer) {
        fprintf(stderr, "ERROR: Memory allocation failed for file: %s\n", filepath);
        fclose(file);
        return NULL;
    }

    size_t read_length = fread(buffer, 1, length, file);
    buffer[read_length] = '\0';
    fclose(file);

    return buffer;
}

static uint32_t decode_utf8(const char** p) {
    const unsigned char* s = (const unsigned char*)*p;
    uint32_t codepoint = 0;
    int bytes = 0;

    if (*s < 0x80) {
        codepoint = *s; bytes = 1;
    } else if ((*s & 0xE0) == 0xC0) {
        codepoint = *s & 0x1F; bytes = 2;
    } else if ((*s & 0xF0) == 0xE0) {
        codepoint = *s & 0x0F; bytes = 3;
    } else if ((*s & 0xF8) == 0xF0) {
        codepoint = *s & 0x07; bytes = 4;
    } else {
        *p += 1; return 0xFFFD;
    }

    *p += bytes;
    for (int i = 1; i < bytes; i++) {
        if ((s[i] & 0xC0) != 0x80) return 0xFFFD;
        codepoint = (codepoint << 6) | (s[i] & 0x3F);
    }
    return codepoint;
}
