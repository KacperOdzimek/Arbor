// ===========================
// Warning! This is an example engine do not use in production!
// See "Notes"!

#include "thirdparty/glad.h"
#include <GLFW/glfw3.h>

#include "arbor/arbor.h"
#include "thirdparty/stb_truetype.h"
#include "thirdparty/stb_image.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// ===========================
// Example Specific

// The UI tree to render, provided by whichever 
// example is linked against this engine
extern arb_node main_structure[];

// Example callbacks
void init();
void frame();
void term();

// ===========================
// Table of contents
//
//   1. Configuration           - compile-time engine configuration (paths, window...)
//   2. Rendering Objects       - the format of data we send to GPU shaders
//   3. Data Structures         - remaining internal structs (font, image, subpartitioner)
//   4. Internal State          - the engine's/renderer's state
//   5. Forward Declarations    - functions forwards
//   6. Engine API              - init / frame / term / main, and window setup
//   7. Rendering Itself        - the actual per-frame draw
//   8. Text Generation         - arb_injection_text_layout
//   9. Texture Slots           - shared GL texture unit allocator (fonts & images)
//  10. Font System             - dynamic, multi-font SDF atlases: baking & caching glyphs
//  11. Image System            - dynamic image atlas: loading & caching images
//  12. Subpartitioning         - glyph SSBO suballocator
//  13. Helpers                 - small utilities used all over the file

// ===========================
// Notes:
//
// Subpartitioner:
// Glyphs buffer subpartitioner was vibe coded, as is not main point of this example
// It is inefficitent, rewriting entire glyphs buffer, when a tiny change happens
// Intended model would be to only keep glyphs on GPU, and direcly writing changed partitions only
//
// Images:
// Images are packed into a single fixed-size atlas texture, using the same
// shelf packer as the font glyphs below, and cached by their resolved string
// so referencing the same image twice never reloads or re-packs it. Like each
// font atlas it never evicts entries, which is fine for an example but not
// for a renderer expected to churn through a lot of dynamic imagery. It gets
// one slot from the shared texture-slot pool - see "Texture Slots" below.
//
// Fonts:
// Multiple fonts are supported the same way images are: arb_text_data.font is
// a font file path, resolved and baked into its own SDF atlas (and its own
// texture slot) the first time it's seen, then cached and reused for every
// later reference to that same path. A NULL/empty font falls back to
// ENGINE_DEFAULT_FONT_PATH, which is loaded eagerly at startup so a bad
// default is caught immediately; any other font failing to load just warns
// and that piece of text draws as nothing, rather than crashing the app.

// ===========================
// 1. Configuration

#define ENGINE_WINDOW_WIDTH          1920
#define ENGINE_WINDOW_HEIGHT         1080
#define ENGINE_WINDOW_TITLE          "Arbor UI - GL Host"
#define ENGINE_GL_VERSION_MAJOR      4
#define ENGINE_GL_VERSION_MINOR      3

#define ENGINE_DEFAULT_FONT_PATH      "assets/roboto.ttf"
#define ENGINE_FONT_BAKE_PIXEL_HEIGHT 64.0f
#define ENGINE_FONT_ATLAS_WIDTH       2048
#define ENGINE_FONT_ATLAS_HEIGHT      2048

#define ENGINE_VERTEX_SHADER_PATH    "shaders/shader.vert"
#define ENGINE_FRAGMENT_SHADER_PATH  "shaders/shader.frag"

#define ENGINE_IMAGE_ATLAS_WIDTH     2048
#define ENGINE_IMAGE_ATLAS_HEIGHT    2048

// Must match the fragment shader's tex_samplers[] array length - fonts and
// images are both handed out slots from this same shared pool (see "Texture
// Slots" below), so it caps the *combined* number of loaded fonts + images.
#define ENGINE_MAX_TEXTURE_SLOTS     16

#define ENGINE_DELTA_TIME            0.016f

// ===========================
// 2. Rendering Objects
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
// 3. Data Structures
// Definied here, so can be used by renderer

// Font system
// Fonts are resolved by string (their file path), each baked into its own
// SDF atlas and assigned its own slot from the shared texture-slot pool.

typedef struct {
    uint32_t            codepoint;
    float               uv_min_x, uv_min_y, uv_max_x, uv_max_y;
    float               off_x, off_y;
    float               size_x, size_y;
    float               advance;
} dynamic_glyph_info;

typedef struct {
    char*               key;             // Resolved font path (owns this memory)
    int                 slot;            // This font's slot in the shared texture-slot pool
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

// Image system
// Images are resolved by string (their path) and packed into one shared
// atlas texture, exactly like glyphs are packed into each font atlas above.

typedef struct {
    char*   key;                    // Resolved image string (owns this memory)
    int     width, height;          // Pixel size of the source image
    float   uv_min_x, uv_min_y, uv_max_x, uv_max_y;
} cached_image_info;

typedef struct {
    int                 slot;            // This atlas's slot in the shared texture-slot pool
    GLuint              image_texture;
    int                 atlas_width;
    int                 atlas_height;
    int                 current_x;
    int                 current_y;
    int                 max_row_h;

    cached_image_info*  cached_images;
    size_t              image_count;
    size_t              image_capacity;
} dynamic_image_atlas;

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
// 4. Internal State

// Window / GL context
static GLFWwindow* window;
static arb_cache*  ui_cache;
static float       g_scroll_delta_x = 0.0f;
static float       g_scroll_delta_y = 0.0f;

// Rendering itself / API
static GLuint ssbo_instances, ssbo_draw_items, ssbo_clipboxes, ssbo_glyphs;
static GLuint shader_program;
static GLuint empty_vao;
static int    inited = 0;

// Texture slots
static int g_next_texture_slot = 0;

// Font system
static dynamic_font_atlas* g_fonts;
static size_t              g_font_count;
static size_t              g_font_capacity;

// Image system
static dynamic_image_atlas g_image_atlas;

// Subpartitioning
static glyph_allocator    g_glyph_alloc;

// ===========================
// 5. Forward Declarations

// Window / GL context
static void     create_window_load_opengl();
static void     scroll_callback(GLFWwindow* w, double xoffset, double yoffset);

// Rendering itself / API
static void     draw_frame(arb_upload_access access, int width, int height);
static void     init_buffers();
static void     shutdown_buffers();
static GLuint   compile_shader(GLenum type, const char* source, const char* filepath);
static GLuint   load_shaders(const char* vertex_path, const char* fragment_path);

// Texture slots
static int      allocate_texture_slot();

// Font system
static dynamic_font_atlas* get_or_load_font(const char* path);
static void     shutdown_fonts();
static dynamic_glyph_info* get_or_bake_glyph(dynamic_font_atlas* font, uint32_t codepoint);

// Image system
static void     init_dynamic_image_atlas();
static void     shutdown_dynamic_image_atlas();
static cached_image_info* get_or_load_image(const char* path);

// Subpartitioning
static void     init_glyph_allocator(size_t cap);
static void     shutdown_glyph_allocator();
static int      cmp_blocks(const void* a, const void* b);
static void     free_glyphs(size_t offset, size_t count);
static size_t   alloc_glyphs(size_t count, const gpu_glyph* data);

// Helpers
static char*    read_file_content(const char* filepath);
static char*    local_strdup(const char* s);
static uint32_t decode_utf8(const char** p);

// ===========================
// 6. Engine API
// init / frame / term replace the old parametrized arbor_renderer_init /
// _shutdown / _draw_frame trio. None of them take arguments any more, so
// per-example configuration now lives in section 1 instead of being passed
// in - that's what lets this exact file back multiple examples unmodified;
// only the linked-in `main_structure[]` changes between them.

static void scroll_callback(GLFWwindow* w, double xoffset, double yoffset) {
    (void)w;
    g_scroll_delta_x += (float)xoffset;
    g_scroll_delta_y += (float)yoffset;
}

static void create_window_load_opengl() {
    if (!glfwInit()) return;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, ENGINE_GL_VERSION_MAJOR);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, ENGINE_GL_VERSION_MINOR);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(ENGINE_WINDOW_WIDTH, ENGINE_WINDOW_HEIGHT, ENGINE_WINDOW_TITLE, NULL, NULL);
    if (!window) {
        glfwTerminate(); return;
    }

    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glfwSetScrollCallback(window, scroll_callback);
}

static void term_main();
static void init_main() {
    create_window_load_opengl();
    if (!window) {
        fprintf(stderr, "ERROR: Failed to create window. Exiting.\n");
        exit(EXIT_FAILURE);
    }
    inited = 1;

    init_buffers();
    init_glyph_allocator(1024); // Start with 1024 contiguous glyph slots
    init_dynamic_image_atlas();

    // The default font is loaded eagerly, so a bad default path fails fast at
    // startup instead of silently later. Every other font loads lazily, on
    // first reference, from get_or_load_font() - see "Font System" below.
    if (!get_or_load_font(ENGINE_DEFAULT_FONT_PATH)) {
        fprintf(stderr, "ERROR: Failed to load default font. Exiting.\n");
        term_main(); exit(EXIT_FAILURE);
    }

    shader_program = load_shaders(ENGINE_VERTEX_SHADER_PATH, ENGINE_FRAGMENT_SHADER_PATH);
    if (shader_program == 0) {
        fprintf(stderr, "ERROR: Failed to load shaders. Exiting.\n");
        term_main(); exit(EXIT_FAILURE);
    }

    ui_cache = arb_create_cache();
    if (!ui_cache) {
        fprintf(stderr, "ERROR: Failed to create UI cache. Exiting.\n");
        term_main(); exit(EXIT_FAILURE);
    }
}

static void frame_main() {
    glfwPollEvents();

    // Load current window resolution
    int width, height; glfwGetFramebufferSize(window, &width, &height);

    // Load current cursor position
    double cursor_x, cursor_y; glfwGetCursorPos(window, &cursor_x, &cursor_y);

    // Create cursor state, which will tell our arb cache about cursor actions
    // We are using scroll delta from GLFW scroll callback
    // The cursor input is handled inside cache_update
    arb_cursor_state cursor_state = {
        .left_down    = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT)  == GLFW_PRESS,
        .right_down   = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS,
        .position_x   = (int)cursor_x,
        .position_y   = (int)cursor_y,
        .scroll_delta = g_scroll_delta_y
    };

    // Reset scroll deltas
    g_scroll_delta_x = 0.0f;
    g_scroll_delta_y = 0.0f;

    // Here we update our UI geometry
    // Returned access contains pointers to render lists
    arb_upload_access access = arb_cache_update(
        ui_cache, main_structure, width, height, cursor_state, ENGINE_DELTA_TIME
    );

    // Issue rendering
    draw_frame(access, width, height);

    // Present new rendered frame to user
    glfwSwapBuffers(window);
}

static void term_main() {
    if (!inited) return;

    if (ui_cache) {
        arb_free_cache(ui_cache);
        ui_cache = NULL;
    }

    if (shader_program) {
        glDeleteProgram(shader_program);
        shader_program = 0;
    }

    shutdown_buffers();
    shutdown_glyph_allocator();
    shutdown_fonts();
    shutdown_dynamic_image_atlas();

    glfwTerminate();
    inited = 0;
}

int main() {
    init_main(); init();

    // Looping until user wishes to close the window
    while (!glfwWindowShouldClose(window)) {
        frame(); frame_main(); 
    }

    term(); term_main();
    return 0;
}

static void init_buffers() {
    glGenVertexArrays(1, &empty_vao);

    glGenBuffers(1, &ssbo_instances);
    glGenBuffers(1, &ssbo_draw_items);
    glGenBuffers(1, &ssbo_clipboxes);
    glGenBuffers(1, &ssbo_glyphs);
}

static void shutdown_buffers() {
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
// 7. Rendering Itself

static void draw_frame(arb_upload_access access, int width, int height) {
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
            draw_items[i].shader_index   = req->box.data.shader;
            draw_items[i].rounding_pixel = req->box.data.rounding;
            draw_items[i].r = req->box.data.tint.r / 255.0f;
            draw_items[i].g = req->box.data.tint.g / 255.0f;
            draw_items[i].b = req->box.data.tint.b / 255.0f;
            draw_items[i].a = req->box.data.tint.a / 255.0f;

            // Resolve the box's image string (if any) to an atlas region, loading
            // and packing it into the image atlas the first time it's seen. See
            // "Texture Slots" for how the atlas's slot becomes this texture_index.
            cached_image_info* image = req->box.data.image ? get_or_load_image(req->box.data.image) : NULL;
            if (image) {
                draw_items[i].texture_index  = g_image_atlas.slot + 1;
                draw_items[i].atlas_position = (uv_2d){ image->uv_min_x, image->uv_min_y, image->uv_max_x, image->uv_max_y };
            } else {
                draw_items[i].texture_index  = 0;
                draw_items[i].atlas_position = (uv_2d){0, 0, 1, 1};
            }

            instances[total_instances++] = (gpu_instance){ .item = (int)i, .glyph = -1 };
        } else {
            // Resolve which loaded font this text uses, the same way a box's
            // image is resolved above (see "Texture Slots"). In practice this
            // is always a cache hit here: arb_injection_text_layout() already
            // resolved (and, if needed, loaded) the font before this draw
            // request could exist.
            dynamic_font_atlas* font = get_or_load_font(req->text.font);
            draw_items[i].texture_index  = font ? -(font->slot + 1) : 0;
            draw_items[i].shader_index   = req->text.shader;
            draw_items[i].rounding_pixel = 0;
            draw_items[i].r = req->text.tint.r / 255.0f;
            draw_items[i].g = req->text.tint.g / 255.0f;
            draw_items[i].b = req->text.tint.b / 255.0f;
            draw_items[i].a = req->text.tint.a / 255.0f;
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

    // tex_samplers[16] in the fragment shader is a bindless-style array: each
    // element is pinned to its own GL texture unit via layout(binding = 0),
    // and texture_index picks which element/unit a draw item samples from:
    //   font  (texture_index < 0): tex_index = -(texture_index + 1)
    //   image (texture_index > 0): tex_index =  texture_index - 1
    // Every loaded font and the image atlas each own one slot in that same
    // pool (see "Texture Slots"), so all of them need (re)binding here, since
    // any of them may be sampled by this draw call.
    for (size_t f = 0; f < g_font_count; f++) {
        glActiveTexture(GL_TEXTURE0 + g_fonts[f].slot);
        glBindTexture(GL_TEXTURE_2D, g_fonts[f].font_texture);
    }

    glActiveTexture(GL_TEXTURE0 + g_image_atlas.slot);
    glBindTexture(GL_TEXTURE_2D, g_image_atlas.image_texture);

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
// 8. Text Generation

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

    // Resolve text_data->font to a loaded (or freshly-loaded) font atlas -
    // see "Font System" below. A font that fails to load degrades to empty
    // text rather than crashing the app.
    dynamic_font_atlas* font = get_or_load_font(text_data->style->font);
    if (!font) {
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

    const float font_scale = stbtt_ScaleForPixelHeight(&font->font_info, (float)text_data->style->size);

    int unscaled_ascent, unscaled_descent, unscaled_line_gap;
    stbtt_GetFontVMetrics(&font->font_info, &unscaled_ascent, &unscaled_descent, &unscaled_line_gap);

    const float ascent      = unscaled_ascent * font_scale;
    const float descent     = unscaled_descent * font_scale;
    const float line_gap    = unscaled_line_gap * font_scale;
    const float line_height = ascent - descent + line_gap;

    float  pen_x      = 0.0f;
    float  pen_y      = 0.0f;
    float  text_width = 0.0f;
    size_t glyph_idx  = 0;
    uint32_t prev_cp  = 0;

    const float atlas_base_size = font->font_size_pixels;
    const float atlas_scale = (float)text_data->style->size / atlas_base_size;

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
            pen_x += stbtt_GetCodepointKernAdvance(&font->font_info, (int)prev_cp, (int)cp) * font_scale;
        }

        dynamic_glyph_info* info = get_or_bake_glyph(font, cp);

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
// 9. Texture Slots
// Every font and image draws from one shared pool of GL texture units,
// mirroring the fragment shader's tex_samplers[ENGINE_MAX_TEXTURE_SLOTS]
// array (each slot pinned to the matching unit via layout(binding = 0)).
// A font at slot S encodes as texture_index = -(S + 1); an image at slot S
// encodes as texture_index = S + 1 (see draw_frame()). Slots are handed out
// once, in load order, and never reclaimed - fine for an example with a
// handful of fonts/images, not for a renderer that needs to stream through
// more than fit in the array.

static int allocate_texture_slot() {
    if (g_next_texture_slot >= ENGINE_MAX_TEXTURE_SLOTS) return -1;
    return g_next_texture_slot++;
}

// ===========================
// 10. Font System
// Fonts are resolved by string (their file path): the first time a path is
// seen it's parsed, given its own signed-distance-field atlas texture and
// its own texture slot, then cached and reused for every later reference to
// that same path - the same pattern the image system below uses for images.

static dynamic_font_atlas* get_or_load_font(const char* path) {
    if (!path || path[0] == '\0') path = ENGINE_DEFAULT_FONT_PATH;

    for (size_t i = 0; i < g_font_count; i++) {
        if (strcmp(g_fonts[i].key, path) == 0) {
            return &g_fonts[i];
        }
    }

    unsigned char* ttf_buffer = (unsigned char*)read_file_content(path);
    if (!ttf_buffer) return NULL;

    stbtt_fontinfo font_info;
    if (!stbtt_InitFont(&font_info, ttf_buffer, 0)) {
        fprintf(stderr, "WARNING: Failed to parse font: %s\n", path);
        free(ttf_buffer);
        return NULL;
    }

    int slot = allocate_texture_slot();
    if (slot < 0) {
        fprintf(stderr, "WARNING: No free texture slots left, cannot load font: %s\n", path);
        free(ttf_buffer);
        return NULL;
    }

    if (g_font_count >= g_font_capacity) {
        g_font_capacity = g_font_capacity ? g_font_capacity * 2 : 4;
        g_fonts = (dynamic_font_atlas*)realloc(g_fonts, g_font_capacity * sizeof(dynamic_font_atlas));
    }

    dynamic_font_atlas* font = &g_fonts[g_font_count++];
    font->key               = local_strdup(path);
    font->slot               = slot;
    font->font_info          = font_info; // shallow copy: its internal pointers reference ttf_buffer, which font now owns
    font->ttf_buffer         = ttf_buffer;
    font->atlas_width        = ENGINE_FONT_ATLAS_WIDTH;
    font->atlas_height       = ENGINE_FONT_ATLAS_HEIGHT;
    font->current_x          = 0;
    font->current_y          = 0;
    font->max_row_h          = 0;
    font->font_size_pixels   = ENGINE_FONT_BAKE_PIXEL_HEIGHT;
    font->cached_glyphs      = NULL;
    font->glyph_count        = 0;
    font->glyph_capacity     = 0;

    glGenTextures(1, &font->font_texture);
    glBindTexture(GL_TEXTURE_2D, font->font_texture);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, font->atlas_width, font->atlas_height, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    return font;
}

static void shutdown_fonts() {
    for (size_t i = 0; i < g_font_count; i++) {
        if (g_fonts[i].font_texture) {
            glDeleteTextures(1, &g_fonts[i].font_texture);
        }
        free(g_fonts[i].ttf_buffer);
        free(g_fonts[i].cached_glyphs);
        free(g_fonts[i].key);
    }
    free(g_fonts);
    g_fonts         = NULL;
    g_font_count    = 0;
    g_font_capacity = 0;
}

static dynamic_glyph_info* get_or_bake_glyph(dynamic_font_atlas* font, uint32_t codepoint) {
    for (size_t i = 0; i < font->glyph_count; i++) {
        if (font->cached_glyphs[i].codepoint == codepoint) {
            return &font->cached_glyphs[i];
        }
    }

    float scale = stbtt_ScaleForPixelHeight(&font->font_info, font->font_size_pixels);
    int width, height, xoff, yoff;

    if (codepoint == ' ' || codepoint == '\t' || codepoint == '\r' || codepoint == 0x00A0) {
        int advance, lsb;
        stbtt_GetCodepointHMetrics(&font->font_info, (int)codepoint, &advance, &lsb);

        if (font->glyph_count >= font->glyph_capacity) {
            font->glyph_capacity = font->glyph_capacity ? font->glyph_capacity * 2 : 256;
            font->cached_glyphs = (dynamic_glyph_info*)realloc(font->cached_glyphs, font->glyph_capacity * sizeof(dynamic_glyph_info));
        }

        dynamic_glyph_info* info = &font->cached_glyphs[font->glyph_count++];
        info->codepoint = codepoint;
        info->uv_min_x  = 0.0f; info->uv_min_y = 0.0f;
        info->uv_max_x  = 0.0f; info->uv_max_y = 0.0f;
        info->off_x     = 0.0f; info->off_y    = 0.0f;
        info->size_x    = 0.0f; info->size_y   = 0.0f;
        info->advance   = advance * scale;
        return info;
    }

    unsigned char* sdf = stbtt_GetCodepointSDF(
        &font->font_info, scale, (int)codepoint, 5, 128, 64,
        &width, &height, &xoff, &yoff
    );

    if (!sdf) {
        if (codepoint != '?') return get_or_bake_glyph(font, '?');
        return NULL;
    }

    if (font->current_x + width + 2 >= font->atlas_width) {
        font->current_x = 0;
        font->current_y += font->max_row_h;
        font->max_row_h = 0;
    }

    if (font->current_y + height + 2 >= font->atlas_height) {
        fprintf(stderr, "WARNING: Font atlas full for %s!\n", font->key);
        stbtt_FreeSDF(sdf, font->font_info.userdata);
        return get_or_bake_glyph(font, '?');
    }

    int px = font->current_x + 1;
    int py = font->current_y + 1;

    glBindTexture(GL_TEXTURE_2D, font->font_texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, px, py, width, height, GL_RED, GL_UNSIGNED_BYTE, sdf);
    stbtt_FreeSDF(sdf, font->font_info.userdata);

    int advance, lsb;
    stbtt_GetCodepointHMetrics(&font->font_info, (int)codepoint, &advance, &lsb);

    if (font->glyph_count >= font->glyph_capacity) {
        font->glyph_capacity = font->glyph_capacity ? font->glyph_capacity * 2 : 256;
        font->cached_glyphs = (dynamic_glyph_info*)realloc(font->cached_glyphs, font->glyph_capacity * sizeof(dynamic_glyph_info));
    }

    dynamic_glyph_info* info = &font->cached_glyphs[font->glyph_count++];
    info->codepoint = codepoint;
    info->uv_min_x  = (float)px / font->atlas_width;
    info->uv_max_x  = (float)(px + width) / font->atlas_width;
    info->uv_min_y  = (float)(py + height) / font->atlas_height;
    info->uv_max_y  = (float)py / font->atlas_height;
    info->off_x     = (float)xoff;
    info->off_y     = (float)yoff;
    info->size_x    = (float)width;
    info->size_y    = (float)height;
    info->advance   = advance * scale;

    font->current_x += width + 3;
    if (height + 3 > font->max_row_h) {
        font->max_row_h = height + 3;
    }

    return info;
}

// ===========================
// 11. Image System
// A dynamic image atlas: like each font atlas above, images are decoded and
// packed into a single GPU texture the first time their string is seen, then
// cached and reused for every subsequent reference to that same string.

static void init_dynamic_image_atlas() {
    g_image_atlas.slot           = allocate_texture_slot();
    g_image_atlas.atlas_width    = ENGINE_IMAGE_ATLAS_WIDTH;
    g_image_atlas.atlas_height   = ENGINE_IMAGE_ATLAS_HEIGHT;
    g_image_atlas.current_x      = 0;
    g_image_atlas.current_y      = 0;
    g_image_atlas.max_row_h      = 0;
    g_image_atlas.image_count    = 0;
    g_image_atlas.image_capacity = 0;
    g_image_atlas.cached_images  = NULL;

    if (g_image_atlas.slot < 0) {
        fprintf(stderr, "WARNING: No free texture slots left, image atlas disabled\n");
        g_image_atlas.image_texture = 0;
        return;
    }

    glGenTextures(1, &g_image_atlas.image_texture);
    glBindTexture(GL_TEXTURE_2D, g_image_atlas.image_texture);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, g_image_atlas.atlas_width, g_image_atlas.atlas_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

static void shutdown_dynamic_image_atlas() {
    if (g_image_atlas.image_texture) {
        glDeleteTextures(1, &g_image_atlas.image_texture);
        g_image_atlas.image_texture = 0;
    }
    for (size_t i = 0; i < g_image_atlas.image_count; i++) {
        free(g_image_atlas.cached_images[i].key);
    }
    free(g_image_atlas.cached_images);
    g_image_atlas.cached_images  = NULL;
    g_image_atlas.image_count    = 0;
    g_image_atlas.image_capacity = 0;
}

static cached_image_info* get_or_load_image(const char* path) {
    if (g_image_atlas.slot < 0) return NULL; // atlas failed to get a texture slot at init

    for (size_t i = 0; i < g_image_atlas.image_count; i++) {
        if (strcmp(g_image_atlas.cached_images[i].key, path) == 0) {
            return &g_image_atlas.cached_images[i];
        }
    }

    int width, height, source_channels;
    unsigned char* pixels = stbi_load(path, &width, &height, &source_channels, 4);
    if (!pixels) {
        fprintf(stderr, "WARNING: Failed to load image: %s\n", path);
        return NULL;
    }

    if (g_image_atlas.current_x + width + 2 >= g_image_atlas.atlas_width) {
        g_image_atlas.current_x = 0;
        g_image_atlas.current_y += g_image_atlas.max_row_h;
        g_image_atlas.max_row_h = 0;
    }

    if (g_image_atlas.current_y + height + 2 >= g_image_atlas.atlas_height) {
        fprintf(stderr, "WARNING: Image atlas full, cannot fit: %s\n", path);
        stbi_image_free(pixels);
        return NULL;
    }

    int px = g_image_atlas.current_x + 1;
    int py = g_image_atlas.current_y + 1;

    glBindTexture(GL_TEXTURE_2D, g_image_atlas.image_texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, px, py, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    stbi_image_free(pixels);

    if (g_image_atlas.image_count >= g_image_atlas.image_capacity) {
        g_image_atlas.image_capacity = g_image_atlas.image_capacity ? g_image_atlas.image_capacity * 2 : 16;
        g_image_atlas.cached_images = (cached_image_info*)realloc(
            g_image_atlas.cached_images, g_image_atlas.image_capacity * sizeof(cached_image_info));
    }

    cached_image_info* info = &g_image_atlas.cached_images[g_image_atlas.image_count++];
    info->key      = local_strdup(path);
    info->width    = width;
    info->height   = height;
    info->uv_min_x = (float)px / g_image_atlas.atlas_width;
    info->uv_max_x = (float)(px + width) / g_image_atlas.atlas_width;
    info->uv_min_y = (float)(py + height) / g_image_atlas.atlas_height;
    info->uv_max_y = (float)py / g_image_atlas.atlas_height;

    g_image_atlas.current_x += width + 3;
    if (height + 3 > g_image_atlas.max_row_h) {
        g_image_atlas.max_row_h = height + 3;
    }

    return info;
}

// ===========================
// 12. Subpartitioning
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

static void shutdown_glyph_allocator() {
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
// 13. Helpers
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

// A tiny, portable stand-in for POSIX strdup, since plain C99 doesn't have one
static char* local_strdup(const char* s) {
    size_t len = strlen(s) + 1;
    char* copy = (char*)malloc(len);
    if (copy) memcpy(copy, s, len);
    return copy;
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

// ============================
// Implementation Building
// Here we build our single header libraries

// Build arbor
#define ARBOR_IMPL
#include "arbor/arbor.h"

// Build stb truetype
#define STB_TRUETYPE_IMPLEMENTATION
#include "thirdparty/stb_truetype.h"

// Build stb image
#define STB_IMAGE_IMPLEMENTATION
#include "thirdparty/stb_image.h"
