/*
----------------------------------------------------------------
Contents:
This file implements arb ui system.

----------------------------------------------------------------
Code info:
- arb prefix
- ARBOR_ARCHITECTURE_IMPL macro to build

----------------------------------------------------------------
Usage: See dedicated documentation
*/

#ifndef ARBOR_USER_INTERFACE_H
#define ARBOR_USER_INTERFACE_H

// ===========================
// Depedency

#include <stdint.h>
#include <stddef.h>

// ===========================
// Forwards

typedef struct arb_type   arb_type;
typedef struct arb_node   arb_node;
typedef struct arb_cache  arb_cache;

// ===========================
// Layout Length

// variable representing infinte length
// not set to int max, to avoid overflows in implementation
// needs to be increased if you are rendering on a (64+)K screen
const static int arb_inf_length = 64 * 1000;

// structure representing 1d length
// min  - minimal size element can be rendered with
// max  - maximal size element can be rendered with
// flex - relative grow speed, compared to other elements inside an container (row/column)
// each length object is expected to met:
// min  <= max
// flex >= 0
typedef struct arb_length {
    int   min;  // minimum dimension
    int   max;  // maximum dimension
    float flex; // flex ratio
} arb_length;

// ===========================
// Transformations

typedef struct { float m[3][3]; } arb_mat3;

static inline arb_mat3 arb_mat3_identity() {
    return (arb_mat3){1, 0, 0, 0, 1, 0, 0, 0, 1};
}

static inline arb_mat3 arb_mat3_scale(arb_mat3 m, float sx, float sy) {
    m.m[0][0] *= sx;  m.m[0][1] *= sy;
    m.m[1][0] *= sx;  m.m[1][1] *= sy;
    return m;
}

static inline arb_mat3 arb_mat3_offset(arb_mat3 m, float ox, float oy) {
    m.m[2][0] += ox; m.m[2][1] += oy;
    return m;
}

// ===========================
// Texture Region

typedef struct arb_uv_2d {
    float min_x, min_y;
    float max_x, max_y;
} arb_uv_2d;

// ===========================
// Colors

// basic 32 bit color
typedef struct arb_color {
    unsigned char r, g, b, a;
} arb_color;

// Convert single hex char to value at compile time (for arb hex functions)
#define ARB_HEX_VAL(c) (                            \
    ((c) >= '0' && (c) <= '9') ? ((c)-'0') :        \
    ((c) >= 'a' && (c) <= 'f') ? ((c)-'a'+10) :     \
    ((c) >= 'A' && (c) <= 'F') ? ((c)-'A'+10) : 0   \
)

// Runtime hex to arb_color conversion
// letters case does not matter, '#' prefix is required
// if hex[7] is not '\0', then alpha channel is read, else it is set to FF
// ARB_HEX <- compile time alternative
static inline arb_color arb_hex(const char *hex) {
    arb_color result = {
        (ARB_HEX_VAL(hex[1]) << 4) | ARB_HEX_VAL(hex[2]),
        (ARB_HEX_VAL(hex[3]) << 4) | ARB_HEX_VAL(hex[4]),
        (ARB_HEX_VAL(hex[5]) << 4) | ARB_HEX_VAL(hex[6]),
        0xFF
    };

    if (hex[7] != '\0' && hex[8] != '\0' && hex[9] == '\0')
        result.a = (ARB_HEX_VAL(hex[7]) << 4) | ARB_HEX_VAL(hex[8]);

    return result;
}

// Compile-time arb_color from a "#RRGGBB" or "#RRGGBBAA" string literal.
#define ARB_HEX(s) (arb_color){                                      \
    ((ARB_HEX_VAL((s)[1]) << 4) | ARB_HEX_VAL((s)[2])),              \
    ((ARB_HEX_VAL((s)[3]) << 4) | ARB_HEX_VAL((s)[4])),              \
    ((ARB_HEX_VAL((s)[5]) << 4) | ARB_HEX_VAL((s)[6])),              \
    (sizeof(s) > 8 ? ((ARB_HEX_VAL((s)[7]) << 4) | ARB_HEX_VAL((s)[8])) : 0xFF) \
}

// ===========================
// Cursor

typedef struct arb_cursor_state {
    int     left_down,  right_down;
    int     position_x, position_y;
    float   scroll_delta;
} arb_cursor_state;

typedef struct arb_node_cursor_input {
    arb_cursor_state*       mutable_state;      // State with fields possibly consumed by previous handles  
    const arb_cursor_state* raw_state;          // Untouched state
    const arb_cursor_state* prev_raw_state;     // Previous frame raw state
    int                     hovered;            // Whether cursor is inside node and no upper node was
    int                     raw_hovered;        // Whether cursor is inside node
    float                   delta_time;         // Time in seconds since last frame
} arb_node_cursor_input;

// ===========================
// Node Typedefs

typedef struct arb_node_layout_state {
    arb_length              measured_width;     // desired width  of this node
    arb_length              measured_height;    // desired height of this node
    int                     given_width;        // received width
    int                     given_height;       // received height
    int                     hori_offset;        // node center horizontal offset from parent center
    int                     vert_offset;        // node center vertical offset from parent center
} arb_node_layout_state;

typedef void(arb_node_layout_func_signature)(
    void*                   node_data,          // node data
    arb_node_layout_state*  node_state,         // node own state
    size_t                  children_count,     // node children count
    arb_node_layout_state** children_states     // node children states
);
typedef arb_node_layout_func_signature* arb_node_layout_func;

typedef void(arb_node_render_func_signature)(
    void*                   node_data,          // node data
    arb_mat3*               transform,          // given transform, can be changed
    int                     resolution_x,       // screen resolution x
    int                     resolution_y        // screen resolution y
);
typedef arb_node_render_func_signature* arb_node_render_func;

typedef void(arb_node_cursor_func_signature)(
    void*                   node_data,          // node data
    arb_node_cursor_input*  node_input          // cursor input
);
typedef arb_node_cursor_func_signature* arb_node_cursor_func;

typedef struct arb_type {
    // Structure

    // Whether child pointer in node means single node
    // Or and array terminated with ARB_ARRAY_END
    int array_child;

    // Layout Stages

    // First layout stage
    // Generates desired nodes widths, bottom-up
    // IN:  [children measured width]
    // OUT: [own measured width]
    arb_node_layout_func    width_measure;

    // Second layout stage
    // Generates actuall nodes widths, top-down
    // IN:  [width measurements, own given width]
    // OUT: [children given width]
    arb_node_layout_func    width_distribute;

    // Third layout stage
    // Generates desired nodes widths, bottom-up
    // IN:  [given widths, children measured heights]
    // OUT: [own measured height]
    arb_node_layout_func    height_measure;

    // Fourth layout stage
    // Generates actuall nodes heights, top-down
    // IN:  [given widths, measured heights, own given height]
    // OUT  [children given heights]
    arb_node_layout_func    height_distribute;

    // Fifth layout stage
    // Position nodes on screen, top-down
    // IN:  [all widths and heights]
    // OUT: [node offset from ]
    arb_node_layout_func    position;

    // Rendering Stages

    // First render stage
    // Allow altering children render transforms, top down
    // IN:  [complete layout states, parent render transform]
    // OUT: [own and children render transform]
    arb_node_render_func    transform;

    // Cursor Input

    // Funtion solving cursor input
    // Called every frame after render
    arb_node_cursor_func    cursor;
} arb_type;

typedef enum arb_flag {
    arb_flag_instanced_data     = 1 << 0,   // This node data  = instance + data_offset
    arb_flag_instanced_child    = 1 << 1,   // This node child = instance + child_offset
    arb_flag_ignore_min_width   = 1 << 2,   // Min width  of this node is set to 0
    arb_flag_ignore_min_height  = 1 << 3,   // Min height of this node is set to 0
    arb_flag_ignore_max_width   = 1 << 4,   // Max width  of this node is set to inf
    arb_flag_ignore_max_height  = 1 << 5,   // Max height of this node is set to inf
    arb_flag_clipbox            = 1 << 6,   // Children of this node on render are clipped to this node boundary
    arb_flag_pink_box           = 1 << 7,   // Render pink box in node boundary - for debugging
} arb_flag;

typedef struct arb_node {
    const arb_type* type;
    const uint32_t  flags;
    
    union {
        const arb_node* child;
        size_t          child_offset;
    };

    union {
        void*   data;
        size_t  data_offset;
    };
} arb_node;

// Sentinel value to mark array end
#define ARB_ARRAY_END (arb_node){.type = NULL, .child = NULL, .data = NULL}

// ===========================
// Predefinied Functions
// Those implement basic box/overlay behavior, used by most nodes

// width = (min = max(children mins), max = max(children max), flex = 1.0f if min != max, else 0)
arb_node_layout_func_signature arb_overlay_width_measure_func;

// children width = parent width, with applied maxes
arb_node_layout_func_signature arb_overlay_width_distribute_func;

// height = (min = max(children mins), max = max(children max), flex = 1.0f if min != max, else 0)
arb_node_layout_func_signature arb_overlay_height_measure_func;

// children height = parent height, with applied maxes
arb_node_layout_func_signature arb_overlay_height_distribute_func;

// centers children inside parent
arb_node_layout_func_signature arb_overlay_position_func;

// Initializes type with arb_overlay functions
#define ARB_TYPE_OVERLAY_INIT \
    .width_measure      = arb_overlay_width_measure_func,       \
    .width_distribute   = arb_overlay_width_distribute_func,    \
    .height_measure     = arb_overlay_height_measure_func,      \
    .height_distribute  = arb_overlay_height_distribute_func

// ===========================
// Architectural Node Types

// Sets instance pointer to own data value
// Data shall be arbitrary pointer (or offset in current instance) to instance structure
extern const arb_type arb_instance_type;

// Layout-rebuild gate for the subtree - children layout will only
// be rebuilt if invalidation node was marked with a proper dirty flag
// No data, single child
extern const arb_type arb_invalidation_type;
typedef enum arb_invalidation_flag {
    arb_invalidation_flag_text              = 63,
    arb_invalidation_flag_width_measure     = 62,
    arb_invalidation_flag_width_distribute  = 60,
    arb_invalidation_flag_height_measure    = 56,
    arb_invalidation_flag_height_distribute = 48,
    arb_invalidation_flag_position          = 32,
    arb_invalidation_flag_none              = 0,
    arb_invalidation_flag_all               = 63,
} arb_invalidation_flag;
typedef struct arb_invalidation_data {
    arb_invalidation_flag flag_consumable;
    arb_invalidation_flag flag_always;
} arb_invalidation_data;

// ===========================
// Layout Node Types

// Layouts children one on another
// The first child is deepest, rendered first
// No data, array children
extern const arb_type arb_overlay_type;

// ===========================
// Rendering Node Types

// Adds node depth offset
// Decreasing depth means going 'into' the screen
// Data is arb_depth_data, ingle childed
extern const arb_type arb_depth_type;
typedef struct arb_depth_data {
    short depth_change;
} arb_depth_data;

// Box render primitive
// Data is arb_box_data, single child
extern const arb_type arb_box_type;
typedef struct arb_box_data {
    arb_color       tint;       // box color
    const char*     image;      // image name/path, may be NULL
    uint32_t        shader;     // shader effect index
} arb_box_data;

// Text render primitive
// Data is arb_text_data, single child
extern const arb_type arb_text_type;
typedef struct arb_text_data {
    unsigned int    size;       // font size
    const char*     font;       // font name/path
    const char*     text;       // text pointer
    arb_color       tint;       // text color modyficator
    uint32_t        shader;     // shader effect index
} arb_text_data;

// ===========================
// Cursor Node Types
// Those can be used to add input, without writing a new node type

// Cursor handle type
// Sets callback for children cursor input nodes to own data (shall be arb_node_cursor_func)
extern const arb_type arb_cursor_handle_type;

// Cursor input type
// Creates an input box, which will call arb_node_cursor_func provided by parent handle type
extern const arb_type arb_cursor_call_type;

// ===========================
// Transform Node Types
// Those can be used to add transforms, without writing a new node type

// Transform handle type
// Sets callback for children transform input nodes to own data (shall be arb_node_render_func)
extern const arb_type arb_transform_handle_type;

// Cursor input type
// Creates an transform box, which will call arb_node_render_func provided by parent handle type
extern const arb_type arb_transform_call_type;

// ===========================
// Miscellaneous Node Types

// Indirect type
// This type does not change any state nor render anything
// Simply jumps to it's single child, usefull with arrays
extern const arb_type arb_indirect_type;

// ===========================
// Requests

typedef struct arb_text_free_request {
    void*               text_pointer;
} arb_text_free_request;

typedef struct arb_glyph_request {
    arb_uv_2d           atlas_position;
    float               off_x,  off_y;
    float               size_x, size_y;
} arb_glyph_request;

typedef struct arb_text_alloc_request {
    void**              text_pointer_out;
    size_t              glyphs_count;
    arb_glyph_request*  glyphs;
} arb_text_alloc_request;

typedef struct arb_clipbox_request {
    arb_mat3            transform;
} arb_clipbox_request;

typedef struct arb_draw_request {
    arb_mat3            transform;
    int                 clip_index;
    short               depth_index;
    char                is_box_not_text;
    union {
        arb_box_data    box_data;
        void**          text_pointer;
    };
} arb_draw_request;

// ===========================
// Upload Access

typedef struct arb_upload_access {
    size_t                          text_free_count;
    const arb_text_free_request*    text_free_requests;

    size_t                          text_alloc_count;
    const arb_text_alloc_request*   text_alloc_requests;

    size_t                          clipboxes_count;
    const arb_clipbox_request*      clipboxes_requests;

    size_t                          draws_count;
    const arb_draw_request*         draws_requests;
} arb_upload_access;

// ===========================
// Cache

arb_cache* arb_create_cache();
void arb_free_cache(arb_cache*);

// Function updating UI
// Returns structure allowing access to cache-owned upload/render lists
// The pointers will be valid until arb_update_cache is called again
arb_upload_access arb_update_cache(
    arb_cache*          cache,
    const arb_node*     root,
    int                 resolution_x,
    int                 resolution_y,
    arb_cursor_state    cursor_state,
    float               delta_time
);

#endif // ARBOR_USER_INTERFACE_H

#ifdef ARBOR_ARCHITECTURE_IMPL

#include <stdlib.h>
#include <string.h>

/* 
    Implementation Notes:
    1 - last_frame_used_in_render values reference
        last_frame_used_in_render is used to clear hashmap from dead nodes
    0     - empty cell
    1     - imposible value, to force garbage collection on all
    2     - tombstone
    3-255 - rendered at frame of index
*/

#define LAST_FRAME_USED_IN_RENDER_EMPTY      0
#define LAST_FRAME_USED_IN_RENDER_IMPOSIBLE  1
#define LAST_FRAME_USED_IN_RENDER_TOMBSTONE  2
#define LAST_FRAME_USED_IN_RENDER_FIRST      3

// ===========================
// Math helpers

static inline int min_int(int a, int b) { return a < b ? a : b; }
static inline int max_int(int a, int b) { return a < b ? b : a; }

static inline int limit_length(int length, arb_length limits) {
    if (length > limits.max) length = limits.max;
    if (length < limits.min) length = limits.min;
    return length;
}

static inline int limit_length_gain(int current, arb_length limit, int proposed) {
    if (current + proposed < limit.min) return limit.min - current;
    if (current + proposed > limit.max) return limit.max - current;
    return proposed;
}

int is_point_in_transformed_box(arb_mat3 t, float px, float py) {
    // Translation
    float tx = t.m[2][0];
    float ty = t.m[2][1];

    // Linear part (column-major)
    float a = t.m[0][0];
    float b = t.m[0][1];
    float c = t.m[1][0];
    float d = t.m[1][1];

    // Point relative to box center
    float x = px - tx;
    float y = py - ty;

    // Inverse of 2×2 matrix
    float det = a * d - b * c;
    if (det == 0.0f) return 0; // degenerate transform

    float inv_det = 1.0f / det;

    // Local coordinates
    float lx = ( d * x - c * y) * inv_det;
    float ly = (-b * x + a * y) * inv_det;

    return lx >= -1.0f && lx <= 1.0f && ly >= -1.0f && ly <= 1.0f;
}

// ===========================
// Types helper

#define box_behavior_type (arb_type){                           \
    .array_child        = 0,                                    \
    .width_measure      = arb_overlay_width_measure_func,       \
    .width_distribute   = arb_overlay_width_distribute_func,    \
    .height_measure     = arb_overlay_height_measure_func,      \
    .height_distribute  = arb_overlay_height_distribute_func,   \
    .position           = arb_overlay_position_func,            \
    .transform          = NULL                                  \
} 

// ===========================
// Instance type
// This type is specially handled in pass implementation
const arb_type arb_instance_type = box_behavior_type;

// ===========================
// Invalidation type
// This type is specially handled in pass implementation
const arb_type arb_invalidation_type = box_behavior_type;

// ===========================
// Overlay Type

void arb_overlay_width_measure_func(
    void*                   node_data,
    arb_node_layout_state*  node_state,
    size_t                  children_count,
    arb_node_layout_state** children_states
) {
    (void)node_data; arb_length own = {0, 0, 0.0f};

    for (size_t i = 0; i < children_count; ++i) {
        arb_length child = children_states[i]->measured_width;
        own.min  = max_int(own.min, child.min);
        own.max  = max_int(own.max, child.max);
    }

    if (own.min != own.max) own.flex = 1.0f;
    node_state->measured_width = own;
}

void arb_overlay_width_distribute_func(
    void*                   node_data,
    arb_node_layout_state*  node_state,
    size_t                  children_count,
    arb_node_layout_state** children_states
) {
    (void)node_data;

    for (size_t i = 0; i < children_count; ++i) {
        children_states[i]->given_width = limit_length(node_state->given_width, children_states[i]->measured_width);
    }
}

void arb_overlay_height_measure_func(
    void*                   node_data,
    arb_node_layout_state*  node_state,
    size_t                  children_count,
    arb_node_layout_state** children_states
) {
    (void)node_data; arb_length own = {0, 0, 0.0f}; 
    for (size_t i = 0; i < children_count; ++i) {
        arb_length child = children_states[i]->measured_height;
        own.min  = max_int(own.min, child.min);
        own.max  = max_int(own.max, child.max);
    }

    if (own.min != own.max) own.flex = 1.0f;
    node_state->measured_height = own;
}

void arb_overlay_height_distribute_func(
    void*                   node_data,
    arb_node_layout_state*  node_state,
    size_t                  children_count,
    arb_node_layout_state** children_states
) {
    (void)node_data; for (size_t i = 0; i < children_count; ++i) {
        children_states[i]->given_height = limit_length(node_state->given_height, children_states[i]->measured_height);
    }
}

void arb_overlay_position_func(
    void*                   node_data,
    arb_node_layout_state*  node_state,
    size_t                  children_count,
    arb_node_layout_state** children_states
) {
    (void)node_data; for (size_t i = 0; i < children_count; ++i) {
        children_states[i]->hori_offset = 0;
        children_states[i]->vert_offset = 0;
    }
}

const arb_type arb_overlay_type = {
    .array_child        = 1,
    .width_measure      = arb_overlay_width_measure_func,
    .width_distribute   = arb_overlay_width_distribute_func,
    .height_measure     = arb_overlay_height_measure_func,
    .height_distribute  = arb_overlay_height_distribute_func,
    .position           = arb_overlay_position_func,
    .transform          = NULL
};

// ===========================
// Depth Type
// This type is specially handled in pass implementation
const arb_type arb_depth_type = box_behavior_type;

// ===========================
// Box Type
// This type is specially handled in pass implementation
const arb_type arb_box_type = box_behavior_type;

// ===========================
// Text type
// This type is specially handled in pass implementation
const arb_type arb_text_type = (arb_type){0};   // No functions

// ===========================
// Cursor handle type
// This type is specially handled in pass implementation
const arb_type arb_cursor_handle_type = box_behavior_type;

// ===========================
// Cursor input type
// This type is specially handled in pass implementation
const arb_type arb_cursor_call_type = box_behavior_type;

// ===========================
// Transform handle type
// This type is specially handled in pass implementation
const arb_type arb_transform_handle_type = box_behavior_type;

// ===========================
// Cursor input type
// This type is specially handled in pass implementation
const arb_type arb_transform_call_type = box_behavior_type;

// ===========================
// Indirect Type
const arb_type arb_indirect_type = box_behavior_type;

// ===========================
// Node fields reads

static inline void* get_node_data(const arb_node* node, const char* instance) {
    if (node->flags & arb_flag_instanced_data) return (void*)(instance + node->data_offset);
    return node->data;
}

static inline const arb_node* get_node_child(const arb_node* node, const char* instance) {
    if (node->flags & arb_flag_instanced_child) return *(const arb_node**)(instance + node->child_offset);
    return node->child;
}

// ===========================
// Stable sort helper

// currently implemeted as mergesort
void stable_sort(void* base, size_t nmemb, size_t size, int (*compar)(const void*, const void*)) {
    if (nmemb < 2) return;

    char *arr = (char*)base;
    char *tmp = malloc(nmemb * size);
    if (!tmp) return;

    for (size_t width = 1; width < nmemb; width *= 2) {
        for (size_t i = 0; i < nmemb; i += 2 * width) {
            size_t l = i;
            size_t m = i + width < nmemb ? i + width : nmemb;
            size_t r = i + 2 * width < nmemb ? i + 2 * width : nmemb;
            size_t p = l, q = m, k = i;

            while (p < m && q < r) {
                if (compar(arr + p * size, arr + q * size) <= 0) memcpy(tmp + k++ * size, arr + p++ * size, size);
                else memcpy(tmp + k++ * size, arr + q++ * size, size);
            }

            while (p < m) memcpy(tmp + k++ * size, arr + p++ * size, size);
            while (q < r) memcpy(tmp + k++ * size, arr + q++ * size, size);
        }

        memcpy(arr, tmp, nmemb * size);
    }

    free(tmp);
    return;
}

// ===========================
// Cache Object

typedef struct cache_slot cache_slot;
typedef struct text_cache_slot text_cache_slot;
typedef struct cursor_input_box cursor_input_box;

struct arb_cache {
    // Passes constants
    int                     resolution_x;
    int                     resolution_y;
    unsigned char           frame_index;

    // Nodes cache hashmap
    size_t                  cache_capacity;
    size_t                  cache_fill;
    cache_slot*             cache_slots;

    // Text cache hashmap
    size_t                  text_cache_capacity;
    size_t                  text_cache_fill;
    text_cache_slot*        text_cache_slots;
    
    // Text free requests dynamic array
    size_t                  text_free_requests_capacity;
    size_t                  text_free_requests_count;
    arb_text_free_request*  text_free_requests;

    // Text allocs requests dynamic array
    size_t                  text_alloc_requests_capacity;
    size_t                  text_alloc_requests_count;
    arb_text_alloc_request* text_alloc_requests;

    // Draw requests dynamic array
    size_t                  draw_requests_capacity;
    size_t                  draw_requests_count;
    arb_draw_request*       draw_requests;

    // Clipbox requests dynamic array
    size_t                  clipbox_requests_capacity;
    size_t                  clipbox_requests_count;
    arb_clipbox_request*    clipbox_requests;

    // Cursor input boxes dynamic array
    size_t                  cursor_input_boxes_capacity;
    size_t                  cursor_input_boxes_count;
    cursor_input_box*       cursor_input_boxes;

    // Previous frame cursor state
    arb_cursor_state        previous_frame_cursor_state;
};

arb_cache* arb_create_cache() {
    arb_cache* cache = calloc(1, sizeof(arb_cache));
    return cache;
}

static void free_cached_text_alloc_requests(arb_cache* cache);
static void text_cache_hashmap_garbage_collect(arb_cache* cache);
void arb_free_cache(arb_cache* cache) {
    if (!cache) return;

    // Free all cached texts by using impossible value
    cache->frame_index = LAST_FRAME_USED_IN_RENDER_IMPOSIBLE;
    text_cache_hashmap_garbage_collect(cache);

    // Free all cached textes
    free_cached_text_alloc_requests(cache);

    free(cache->cache_slots);
    free(cache->text_cache_slots);
    free(cache->text_free_requests);
    free(cache->text_alloc_requests);
    free(cache->clipbox_requests);
    free(cache->draw_requests);
    free(cache->cursor_input_boxes);
    free(cache);
}

// ===========================
// Cache Hashmaps

typedef struct node_stable_index {
    const arb_node*         node;
    const void*             instance;
} node_stable_index;

typedef struct cache_slot {
    node_stable_index       key;
    unsigned char           last_frame_used_in_render;
    size_t                  value_child_count;
    arb_node_layout_state   value_state;
} cache_slot;

typedef struct text_cache_slot {
    node_stable_index       key;
    unsigned char           last_frame_used_in_render;
    int                     text_width;
    int                     text_height;
    void*                   allocation;
} text_cache_slot;

static uint64_t hash_ptr(const void* p) {
    uint64_t x = (uint64_t)(uintptr_t)p;
    x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27; x *= 0x94d049bb133111ebULL;
    return (x ^ (x >> 31));
}

static size_t hash_key(node_stable_index key) {
    uint64_t h1 = hash_ptr(key.node);
    uint64_t h2 = hash_ptr(key.instance);
    return (size_t)(h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2)));
}

// Definies three functions:
// void       PREFIX##_hashmap_grow             (arb_cache* cache);
// SLOT_TYPE* PREFIX##_hashmap_get              (arb_cache* cache, node_stable_index key, int insert_if_none)
// void       PREFIX##_hashmap_garbage_collect  (arb_cache* cache) 
// Define HASHMAP_SLOT_INITIALIZER to define default slot value
// Define HASHMAP_SLOT_DESTRUCTOR(slot ptr) to set garbage collector slot free method
#define DEFINE_HASHMAP_FUNCS(PREFIX, SLOT_TYPE, SLOTS_FIELD, CAP_FIELD, FILL_FIELD) \
\
static SLOT_TYPE* PREFIX##_hashmap_get                                          \
(arb_cache* cache, node_stable_index key, int insert_if_none);                  \
\
static void PREFIX##_hashmap_grow(arb_cache* cache) {                           \
    size_t old_cap = cache->CAP_FIELD;                                          \
    SLOT_TYPE* old_slots = cache->SLOTS_FIELD;                                  \
\
    size_t new_cap = old_cap ? old_cap * 2 : 64;                                \
\
    cache->SLOTS_FIELD = calloc(new_cap, sizeof(*cache->SLOTS_FIELD));          \
    cache->CAP_FIELD   = new_cap;                                               \
    cache->FILL_FIELD  = 0;                                                     \
\
    for (size_t i = 0; i < old_cap; ++i) {                                      \
        unsigned char time = old_slots[i].last_frame_used_in_render;            \
        if (time == LAST_FRAME_USED_IN_RENDER_EMPTY ||                          \
            time == LAST_FRAME_USED_IN_RENDER_TOMBSTONE) continue;              \
        SLOT_TYPE* dst = PREFIX##_hashmap_get(cache, old_slots[i].key, 1);      \
        *dst = old_slots[i];                                                    \
    }                                                                           \
\
    free(old_slots);                                                            \
}                                                                               \
\
static SLOT_TYPE* PREFIX##_hashmap_get(                                         \
    arb_cache* cache, node_stable_index key, int insert_if_none) {              \
    if ((cache->FILL_FIELD + 1) * 10 >= cache->CAP_FIELD * 7) {                 \
        PREFIX##_hashmap_grow(cache);                                           \
    }                                                                           \
\
    size_t mask = cache->CAP_FIELD - 1;                                         \
    size_t idx  = hash_key(key) & mask;                                         \
\
    for (SLOT_TYPE* tombstone = NULL;;) {                                       \
        SLOT_TYPE* slot = &cache->SLOTS_FIELD[idx];                             \
        unsigned char time = slot->last_frame_used_in_render;                   \
\
        if (time == LAST_FRAME_USED_IN_RENDER_EMPTY) {                          \
            if (!insert_if_none) return NULL;                                   \
            if (tombstone) slot = tombstone;                                    \
            else cache->FILL_FIELD++;                                           \
            *slot = (SLOT_TYPE)HASHMAP_SLOT_INITIALIZER;                        \
            return slot;                                                        \
        }                                                                       \
\
        if (time == LAST_FRAME_USED_IN_RENDER_TOMBSTONE) {                      \
            if (!tombstone) tombstone = slot;                                   \
         }                                                                      \
        else if (                                                               \
            slot->key.node == key.node &&                                       \
            slot->key.instance == key.instance                                  \
        ) return slot;                                                          \
\
        idx = (idx + 1) & mask;                                                 \
    }                                                                           \
}                                                                               \
\
static void PREFIX##_hashmap_garbage_collect(arb_cache* cache) {                \
    for (size_t i = 0; i < cache->CAP_FIELD; i++) {                             \
        SLOT_TYPE*     slot = &cache->SLOTS_FIELD[i];                           \
        unsigned char* time = &slot->last_frame_used_in_render;                 \
        if (*time                                           &&                  \
            *time != LAST_FRAME_USED_IN_RENDER_TOMBSTONE    &&                  \
            *time != cache->frame_index                                         \
        ) {                                                                     \
            HASHMAP_SLOT_DESTRUCTOR(slot);                                      \
            *time = LAST_FRAME_USED_IN_RENDER_TOMBSTONE;                        \
        }                                                                       \
    }                                                                           \
}

#define HASHMAP_SLOT_INITIALIZER {.key = key}
#define HASHMAP_SLOT_DESTRUCTOR(slot_ptr)
DEFINE_HASHMAP_FUNCS(
    cache, cache_slot, cache_slots, cache_capacity, cache_fill
);

#undef HASHMAP_SLOT_INITIALIZER
#undef HASHMAP_SLOT_DESTRUCTOR

#define HASHMAP_SLOT_INITIALIZER {.key = key}
#define HASHMAP_SLOT_DESTRUCTOR(slot_ptr) \
    {} // todo request user glyph buffer free
DEFINE_HASHMAP_FUNCS(
    text_cache, text_cache_slot, text_cache_slots, text_cache_capacity, text_cache_fill
);

// Gets slot, always inserts, as cache must always exist for node
static inline cache_slot* cache_get_utill(arb_cache* cache, node_stable_index index) {
    return cache_hashmap_get(cache, index, 1);
}

// Gets slot, always inserts, as cache must always exist for node
static inline text_cache_slot* text_cache_get_utill(arb_cache* cache, node_stable_index index) {
    return text_cache_hashmap_get(cache, index, 1);
}

// ===========================
// Cursor Input Box

typedef struct cursor_input_box {
    node_stable_index       owner;
    arb_node_cursor_func    handle;
    int                     clip_index;
    short                   depth_index;
    arb_mat3                box_transform;
} cursor_input_box;

// ===========================
// Cache dynamic arrays

// Definies one function:
// void PREFIX##_cache_push (arb_cache* cache, ELEMENT_TYPE element);
#define DEFINE_DYNAMIC_ARRAY_FUNCS(PREFIX, ELEMENT_TYPE, ARRAY_FIELD, CAP_FIELD, CNT_FIELD)     \
static int PREFIX##_cache_push(arb_cache* cache, ELEMENT_TYPE element) {                        \
    if (cache->CNT_FIELD + 1 > cache->CAP_FIELD) {                                              \
        size_t          new_cap = cache->CAP_FIELD ? cache->CAP_FIELD * 2 : 64;                 \
        ELEMENT_TYPE*   new_arr = realloc(cache->ARRAY_FIELD, new_cap * sizeof(ELEMENT_TYPE));  \
        if (!new_arr)   return - 1;                                                             \
\
        cache->ARRAY_FIELD  = new_arr;                                                          \
        cache->CAP_FIELD    = new_cap;                                                          \
    }                                                                                           \
\
    cache->ARRAY_FIELD[cache->CNT_FIELD] = element;                                             \
    return (int)cache->CNT_FIELD++;                                                             \
}

DEFINE_DYNAMIC_ARRAY_FUNCS(
    text_free_request, arb_text_free_request, text_free_requests, text_free_requests_capacity, text_free_requests_count
);

DEFINE_DYNAMIC_ARRAY_FUNCS(
    text_alloc_request, arb_text_alloc_request, text_alloc_requests, text_alloc_requests_capacity, text_alloc_requests_count
);

DEFINE_DYNAMIC_ARRAY_FUNCS(
    draw_request, arb_draw_request, draw_requests, draw_requests_capacity, draw_requests_count
);

DEFINE_DYNAMIC_ARRAY_FUNCS(
    clipbox_request, arb_clipbox_request, clipbox_requests, clipbox_requests_capacity, clipbox_requests_count
);

DEFINE_DYNAMIC_ARRAY_FUNCS(
    cursor_input_box, cursor_input_box, cursor_input_boxes, cursor_input_boxes_capacity, cursor_input_boxes_count
);

static inline void free_cached_text_alloc_requests(arb_cache* cache) {
    for (size_t i = 0; i < cache->text_alloc_requests_count; i++) {
        free(cache->text_alloc_requests[i].glyphs);
    }
    cache->text_alloc_requests_count = 0;
}

// ===========================
// Cache Update

// Invalidation Node Gate

typedef enum invalidation_flag_only {
    invalidation_flag_only_text              = 1,
    invalidation_flag_only_width_measure     = 2,
    invalidation_flag_only_width_distribute  = 4,
    invalidation_flag_only_height_measure    = 8,
    invalidation_flag_only_height_distribute = 16,
    invalidation_flag_only_position          = 32,
} invalidation_flag_only;

static inline int find_shall_recurse(cache_slot* node_slot, const void* data, invalidation_flag_only pass) {
    if (node_slot->key.node->type != &arb_invalidation_type) return 1;
    arb_invalidation_data* inv_data = (arb_invalidation_data*)data; // special case where const may be discarded

    // recurse one time in this pass
    if (inv_data->flag_consumable & pass) {
        inv_data->flag_consumable &= ~(pass);   // turn off this pass bit
        return 1;
    }

    // recurse only if marked always to do it
    return inv_data->flag_always & pass;
}

// Cache Walk Pass
// Called on remeasure
// Computes: 
// - walk order (the order layout caches are visited, to avoid hashmaping multiple times)
// - nodes children count (simplify implementations)
// - nodes subtree size (including self, to easily skip on invalidation nodes)

// shall initialized with cache and 0 in other fields
typedef struct caches_walk_order {
    arb_cache*              cache;      // cache owning cache slots
    size_t                  capacity;   // in cache_slot pointers
    size_t                  position;   // in cache_slot pointers
    cache_slot**            slots;      // sized capacity, node cache slots in enter order
    arb_node_layout_state** states;     // sized capacity, node layout states in children oreder
    size_t*                 subtree;    // sized capacity, node subtree size, including self
} caches_walk_order;

// Guaranteed valid 0-intialized object after free
// except cache field being untouched
void free_caches_walk_order(caches_walk_order* order) {
    free(order->slots);     order->slots    = NULL;
    free(order->states);    order->states   = NULL;
    free(order->subtree);   order->subtree  = NULL;
    order->capacity = 0;    order->position = 0;
}

// Returns non-zero at success
static inline int caches_walk_order_push(caches_walk_order* walk_order, cache_slot* slot) {
    if (walk_order->position + 1 > walk_order->capacity) {
        size_t new_cap = walk_order->capacity ? walk_order->capacity * 2 : 64;
    
        cache_slot**            new_slt = realloc(walk_order->slots,    new_cap * sizeof(cache_slot*));
        arb_node_layout_state** new_sts = realloc(walk_order->states,   new_cap * sizeof(arb_node_layout_state*));
        size_t*                 new_sub = realloc(walk_order->subtree,  new_cap * sizeof(size_t));

        if (!new_slt || !new_sts || !new_sub) {
            free(new_slt); free(new_sts); free(new_sub);
            free_caches_walk_order(walk_order);
            return 0; // failed to realloc -> failed to push -> entire layout fails
        }

        walk_order->capacity = new_cap;
        walk_order->slots    = new_slt;
        walk_order->states   = new_sts;
        walk_order->subtree  = new_sub;
    }

    walk_order->slots   [walk_order->position]  = slot;
    walk_order->states  [walk_order->position]  = &slot->value_state;
    walk_order->subtree [walk_order->position]  = 1; // included node itself
    walk_order->position++;

    return 1; // success
}

// Pushes all child nodes caches of node to caches_walk_order
// Recurse into children left to right
// Returns non-zero at success
int caches_walk_dfs(
    caches_walk_order*  walk_order, 
    cache_slot*         current, 
    size_t*             subtree_size_target, 
    const void*         instance
) {
    const arb_node* node  = current->key.node;
    const arb_node* child = get_node_child(current->key.node, current->key.instance);
    size_t          count = 0;
    int             scc   = 1;

    // change instance for subtree
    if (node->type == &arb_instance_type) {
        instance = get_node_data(current->key.node, instance);
    }

    if (!node->type->array_child && child) {
        cache_slot*     child_slot = cache_get_utill(walk_order->cache, (node_stable_index){child, instance});
        scc &= caches_walk_order_push(walk_order, child_slot); count++;
    }
    else if (child) for (const arb_node* cc = child; cc->type != NULL; cc++) {
        cache_slot*     child_slot = cache_get_utill(walk_order->cache, (node_stable_index){cc, instance});
        scc &= caches_walk_order_push(walk_order, child_slot); count++;
    }

    // recurse
    size_t begin_pos = walk_order->position - count;
    for (size_t i = 0; i < count; i++) {
        scc &= caches_walk_dfs(walk_order, walk_order->slots[begin_pos + i], &walk_order->subtree[begin_pos + i], instance);
        *subtree_size_target += walk_order->subtree[begin_pos + i];
    }

    current->value_child_count = count;
    return scc;
}

// Generic layout dfs generation macros

// Definies function:
// void PREFIX##_dfs(caches_walk_order* walk_order, cache_slot* current, size_t first_child)
// Exec order: recurse -> own function -> additional code
#define BOTTOM_UP_DFS(PREFIX, TYPE_FUNC_NAME, INV_PASS_ONLY_FLAG, ...)                              \
void PREFIX##_dfs(                                                                                  \
    caches_walk_order*  walk_order,                                                                 \
    cache_slot*         current,                                                                    \
    size_t              first_child                                                                 \
) {                                                                                                 \
    cache_slot**    children    = &walk_order->slots[first_child];                                  \
    size_t*         subtrees    = &walk_order->subtree[first_child];                                \
    void*           data        = get_node_data(current->key.node, current->key.instance);          \
\
    if (find_shall_recurse(current, data, INV_PASS_ONLY_FLAG)) {                                    \
        size_t child_first_child = first_child + current->value_child_count;                        \
        for (size_t i = 0; i < current->value_child_count; i++) {                                   \
            PREFIX##_dfs(walk_order, children[i], child_first_child);                               \
            child_first_child += subtrees[i] - 1;                                                   \
        }                                                                                           \
    }                                                                                               \
\
    arb_node_layout_func func = current->key.node->type->TYPE_FUNC_NAME;                            \
    if (func != NULL) func(                                                                         \
        data, &current->value_state, current->value_child_count, &walk_order->states[first_child]   \
    );                                                                                              \
\
    __VA_ARGS__                                                                                     \
}

// Definies function:
// void PREFIX##_dfs(caches_walk_order* walk_order, cache_slot* current, size_t first_child)
// Exec order: additional code -> own function -> recurse
#define TOP_DOWN_DFS(PREFIX, TYPE_FUNC_NAME, INV_PASS_ONLY_FLAG, ...)                               \
void PREFIX##_dfs(                                                                                  \
    caches_walk_order*  walk_order,                                                                 \
    cache_slot*         current,                                                                    \
    size_t              first_child                                                                 \
) {                                                                                                 \
    cache_slot**    children    = &walk_order->slots[first_child];                                  \
    size_t*         subtrees    = &walk_order->subtree[first_child];                                \
    void*           data        = get_node_data(current->key.node, current->key.instance);          \
\
    __VA_ARGS__                                                                                     \
\
    arb_node_layout_func func = current->key.node->type->TYPE_FUNC_NAME;                            \
    if (func != NULL) func(                                                                         \
        data, &current->value_state, current->value_child_count, &walk_order->states[first_child]   \
    );                                                                                              \
\
    if (find_shall_recurse(current, data, INV_PASS_ONLY_FLAG)) {                                    \
        size_t child_first_child = first_child + current->value_child_count;                        \
        for (size_t i = 0; i < current->value_child_count; i++) {                                   \
            PREFIX##_dfs(walk_order, children[i], child_first_child);                               \
            child_first_child += subtrees[i] - 1;                                                   \
        }                                                                                           \
    }                                                                                               \
}

// Layout passes
// Travels tree, call functions as specified in type comments
// to calcualate what specfied in type comments

void create_text_request(arb_cache* cache, text_cache_slot* slot);

// Text generate pass
void text_gen_dfs(
    caches_walk_order*  walk_order,
    cache_slot*         current,
    size_t              first_child
) {
    cache_slot**    children    = &walk_order->slots[first_child];
    size_t*         subtrees    = &walk_order->subtree[first_child];
    void*           data        = get_node_data(current->key.node, current->key.instance);

    if (find_shall_recurse(current, data, invalidation_flag_only_text)) {
        size_t child_first_child = first_child + current->value_child_count;
        for (size_t i = 0; i < current->value_child_count; i++) {
            text_gen_dfs(walk_order, children[i], child_first_child);
            child_first_child += subtrees[i] - 1;
        }
    }

    if (current->key.node->type == &arb_text_type) {
        text_cache_slot* text_cache = text_cache_get_utill(walk_order->cache, current->key);
        create_text_request(walk_order->cache, text_cache);
        current->value_state.measured_width  = (arb_length){text_cache->text_width,  text_cache->text_width, 1};
        current->value_state.measured_height = (arb_length){text_cache->text_height, text_cache->text_height, 1};
    }
}

// Width measure pass, additionaly handle ignore flags
BOTTOM_UP_DFS(
    width_measure, width_measure, invalidation_flag_only_width_measure,
    if (current->key.node->flags & arb_flag_ignore_min_width) {
        current->value_state.measured_width.min = 0;
        current->value_state.measured_width.flex = 1.0f;
    }
    if (current->key.node->flags & arb_flag_ignore_max_width) {
        current->value_state.measured_width.max  = arb_inf_length;
        current->value_state.measured_width.flex = 1.0f;
    }
);

// Width distribute pass, additionaly ensure received width
// is within node measured limits
TOP_DOWN_DFS(
    width_distribute, width_distribute, invalidation_flag_only_width_distribute,
    current->value_state.given_width = limit_length(
        current->value_state.given_width,
        current->value_state.measured_width
    );
);

// Height measure pass, additionaly handle ignore flags
BOTTOM_UP_DFS(
    height_measure, height_measure, invalidation_flag_only_height_measure,
    if (current->key.node->flags & arb_flag_ignore_min_height) {
        current->value_state.measured_height.min = 0;
        current->value_state.measured_height.flex = 1.0f;
    }
    if (current->key.node->flags & arb_flag_ignore_max_height) {
        current->value_state.measured_height.max  = arb_inf_length;
        current->value_state.measured_height.flex = 1.0f;
    }
);

// Height distribute pass, additionaly ensure received height 
// is within node measured limits
TOP_DOWN_DFS(
    height_distribute, height_distribute, invalidation_flag_only_height_distribute,
    current->value_state.given_height = limit_length(
        current->value_state.given_height,
        current->value_state.measured_height
    );
);

// Position pass, no additional code
TOP_DOWN_DFS(
    position, position, invalidation_flag_only_position
);

// Renders widget
// Issues rendering of ui primitives
// Also renders input boxes into input dynamic array
// Render pass is safe in terms of hashmap pointers invalidation
// Since it refers on the pointer only on enter - after visiting any child it is not used

typedef struct render_dfs_subtree_state {
    const void*             instance;
    short                   depth_index;
    int                     clipbox_index;
    arb_node_cursor_func    cursor_handle;
    arb_node_render_func    transform_handle;
} render_dfs_subtree_state;

static void render_dfs(
    arb_cache*                      cache, 
    int                             previous_width,
    int                             previous_height, 
    const arb_node*                 node,
    arb_mat3                        transform, 
    const render_dfs_subtree_state* state
);

static inline void render_dfs_recurse(
    arb_cache*                      cache, 
    cache_slot*                     own,
    arb_mat3                        transform, 
    const render_dfs_subtree_state* state
) {
    const arb_node* child = get_node_child(own->key.node, own->key.instance);

    // back node dimensions to avoid reading own slot after visiting child
    int own_width  = own->value_state.given_width;
    int own_height = own->value_state.given_height;

    // single child
    if (!own->key.node->type->array_child && child) {
        render_dfs(cache, own_width, own_height, child, transform, state);
    }
    // multiple children
    else if (child) for (const arb_node* current_child = child; current_child->type != NULL; current_child++) {
        render_dfs(cache, own_width, own_height, current_child, transform, state);
    }
}

static void render_dfs(
    arb_cache*                      cache, 
    int                             previous_width,
    int                             previous_height, 
    const arb_node*                 node,
    arb_mat3                      transform, 
    const render_dfs_subtree_state* state
) {
    node_stable_index index = {node, state->instance};

    // get node data
    void*       data = get_node_data (node, state->instance);
    cache_slot* own  = cache_get_utill(cache, index);

    // mark used, to avoid garbage collect
    own->last_frame_used_in_render = cache->frame_index;

    // change transform based on node's position and scale
    float off_x   = ((float)own->value_state.hori_offset * 2)   / cache->resolution_x;
    float off_y   = ((float)own->value_state.vert_offset * 2)   / cache->resolution_y;
    float scale_x = ((float)own->value_state.given_width)       / previous_width;
    float scale_y = ((float)own->value_state.given_height)      / previous_height;
    transform = arb_mat3_offset(transform, off_x, off_y);
    transform = arb_mat3_scale (transform, scale_x, scale_y);

    // Do transform if method provided
    if (node->type->transform) node->type->transform(
        data, &transform, cache->resolution_x, cache->resolution_y
    );

    // Transform
    if (node->type == &arb_transform_call_type && state->transform_handle) {
        state->transform_handle(data, &transform, cache->resolution_x, cache->resolution_y);
    }

    // Push pinkbox request
    if (node->flags & arb_flag_pink_box) {
        draw_request_cache_push(cache, (arb_draw_request){
            .transform          = transform,
            .clip_index         = state->clipbox_index,
            .depth_index        = state->depth_index,
            .is_box_not_text    = 1,
            .box_data           = (arb_box_data){
                .image  = NULL,
                .shader = 0,
                .tint   = ARB_HEX("#df04ba")
            }
        });
    }

    // Render Nodes
    // Request box draw
    if (node->type == &arb_box_type){
        const arb_box_data* bdata = data;
        draw_request_cache_push(cache, (arb_draw_request){
            .transform          = transform,
            .clip_index         = state->clipbox_index,
            .depth_index        = state->depth_index,
            .is_box_not_text    = 1,
            .box_data           = *bdata
        });
    }
    // Request text draw
    else if (node->type == &arb_text_type) {
        const arb_text_data* tdata = data;

        // Prevent text garbage collection
        text_cache_slot* text_cache = text_cache_get_utill(cache, index);
        if (text_cache) text_cache->last_frame_used_in_render = cache->frame_index;

        draw_request_cache_push(cache, (arb_draw_request){
            .transform          = transform,
            .clip_index         = state->clipbox_index,
            .depth_index        = state->depth_index,
            .is_box_not_text    = 0,
            .text_pointer       = &text_cache->allocation
        });
    }

    // Cursor
    // Request cursor input box
    if (node->type->cursor) {
        cursor_input_box_cache_push(cache, (cursor_input_box){
            .owner          = index,
            .handle         = node->type->cursor,
            .depth_index    = state->depth_index,
            .clip_index     = state->clipbox_index,
            .box_transform  = transform
        });
    }
    if (node->type == &arb_cursor_call_type && state->cursor_handle) {
        cursor_input_box_cache_push(cache, (cursor_input_box){
            .owner          = index,
            .handle         = state->cursor_handle,
            .depth_index    = state->depth_index,
            .clip_index     = state->clipbox_index,
            .box_transform  = transform
        });
    }

    // New state for subtree
    render_dfs_subtree_state new_state = *state;

    // Special nodes
    // Update instance for subtree
    if (node->type == &arb_instance_type) {
        new_state.instance = data;
    }
    // Update depth for subtree
    else if (node->type == &arb_depth_type) {
        const arb_depth_data* ddata = data;
        new_state.depth_index += ddata->depth_change;
    }
    // Update cursor handle for subtree
    else if (node->type == &arb_cursor_handle_type) {
        new_state.cursor_handle = (arb_node_cursor_func)data;
    }
    // Update trnasform handle for subtree
    else if (node->type == &arb_transform_handle_type) {
        new_state.transform_handle = (arb_node_render_func)data;
    }

    // Clipbox flag
    if (node->flags & arb_flag_clipbox) {
        new_state.clipbox_index = clipbox_request_cache_push(cache, (arb_clipbox_request){.transform = transform});
    }

    // Default recursion without state changes
    render_dfs_recurse(cache, own, transform, &new_state);
}

// Helper for draw requests depth sorting : deepest first
static inline int helper_draw_requests_greater_depth(const void* av, const void* bv) {
    const arb_draw_request* a = (const arb_draw_request*)av; 
    const arb_draw_request* b = (const arb_draw_request*)bv;
    if (a->depth_index > b->depth_index) return 1;
    return 0;
}

// Helper for cursor input boxes depth sorting : deepest first
static inline int helper_cursor_input_boxes_greater_depth(const void* av, const void* bv) {
    const cursor_input_box* a = (const cursor_input_box*)av; 
    const cursor_input_box* b = (const cursor_input_box*)bv;
    if (a->depth_index > b->depth_index) return 1;
    return 0;
}

// Main update function, calls passes
arb_upload_access arb_update_cache(
    arb_cache*          cache,
    const arb_node*     root,
    int                 resolution_x,
    int                 resolution_y,
    arb_cursor_state    cursor_state,
    float               delta_time
) {
    // Init state
    cache->resolution_x                 = resolution_x;
    cache->resolution_y                 = resolution_y;
    cache->draw_requests_count          = 0;
    cache->text_free_requests_count     = 0;
    cache->text_alloc_requests_count    = 0;
    cache->clipbox_requests_count       = 0;
    cache->cursor_input_boxes_count     = 0;

    // Pick next frame index
    cache->frame_index++; if (cache->frame_index < LAST_FRAME_USED_IN_RENDER_FIRST) cache->frame_index = LAST_FRAME_USED_IN_RENDER_FIRST;

    // Render pass
    render_dfs_subtree_state default_subtree_state = {
        .instance       = NULL,
        .depth_index    = 0,
        .clipbox_index  = -1
    };
    render_dfs(cache, cache->resolution_x, cache->resolution_y, root, arb_mat3_identity(), &default_subtree_state);

    // Sort render requests and input boxes by depth
    stable_sort(cache->draw_requests,       cache->draw_requests_count,      sizeof(arb_draw_request),      helper_draw_requests_greater_depth);
    stable_sort(cache->cursor_input_boxes,  cache->cursor_input_boxes_count, sizeof(cursor_input_box),  helper_cursor_input_boxes_greater_depth);

    // Find out normalized cursor position
    float norm_cursor_x = -1.0f + 2.0f * ((float)cursor_state.position_x / resolution_x);
    float norm_cursor_y =  1.0f - 2.0f * ((float)cursor_state.position_y / resolution_y);

    // Do cursor input callbacks, walking from topmost to deepest
    arb_cursor_state changable_state = cursor_state;
    arb_node_cursor_input input_data = {
        .mutable_state  = &changable_state,
        .raw_state      = &cursor_state,
        .prev_raw_state = &cache->previous_frame_cursor_state,
        .delta_time     = delta_time
    };
    
    int ever_was_inside = 0;
    if (cache->cursor_input_boxes_count) for (size_t i = cache->cursor_input_boxes_count - 1; ; i--) {
        cursor_input_box* ibox = &cache->cursor_input_boxes[i];
        if (!ibox->handle) continue; // nothing to call

        int cursor_inside  = is_point_in_transformed_box(ibox->box_transform, norm_cursor_x, norm_cursor_y);
        if (ibox->clip_index != -1) {
            cursor_inside &= is_point_in_transformed_box(cache->clipbox_requests[ibox->clip_index].transform, norm_cursor_x, norm_cursor_y);
        }

        input_data.hovered     = cursor_inside && !ever_was_inside;
        input_data.raw_hovered = cursor_inside;
        ibox->handle(get_node_data(ibox->owner.node, ibox->owner.instance), &input_data);

        ever_was_inside |= cursor_inside;
        if (i == 0) break; // break loop at last element
    }

    // Current state in now previous cursor state
    cache->previous_frame_cursor_state = cursor_state;

    // Prepare upload access
    arb_upload_access upload_access = {
        .text_free_count     = cache->text_free_requests_count,
        .text_free_requests  = cache->text_free_requests,

        .text_alloc_count    = cache->text_alloc_requests_count,
        .text_alloc_requests = cache->text_alloc_requests,

        .clipboxes_count     = cache->clipbox_requests_count,
        .clipboxes_requests  = cache->clipbox_requests,

        .draws_count         = cache->draw_requests_count,
        .draws_requests      = cache->draw_requests,
    };

    // Always relayout
    // Do it after render - then we can trust all nodes have their inserted cache and auxilary slots
    // This is important so hashmap pointers does not get invalidated during passes
    // This means we are one frame behind with layout, but it is not a big deal actually.
    if (1) {
        cache_slot* root_cache = cache_get_utill(cache, (node_stable_index){root, NULL});

        // Give root entire screen
        // Will auto bound to desired at distribute
        root_cache->value_state.given_width  = resolution_x;
        root_cache->value_state.given_height = resolution_y;

        // Find walk order
        caches_walk_order walk_order    = {.cache = cache};
        size_t            root_subtree  = 1; // root itself included
        if (!caches_walk_dfs(&walk_order, root_cache, &root_subtree, NULL)) {
            free_caches_walk_order(&walk_order); return upload_access;
        }
        
        // Perform all passes
        text_gen_dfs(&walk_order, root_cache, 0);
        width_measure_dfs(&walk_order, root_cache, 0);
        width_distribute_dfs(&walk_order, root_cache, 0);
        height_measure_dfs(&walk_order, root_cache, 0);
        height_distribute_dfs(&walk_order, root_cache, 0);
        position_dfs(&walk_order, root_cache, 0);

        free_caches_walk_order(&walk_order);
    }

    // Garbage collect dead cache entries
    // If entry was not used in render, mark it free
    // Do every 16 frames not to spend to much time on it
    if (cache->frame_index % 16 == 0) {
        cache_hashmap_garbage_collect(cache);
        text_cache_hashmap_garbage_collect(cache);
    }

    return upload_access;
}

// ===========================
// Text layout generation

static inline int utf8_decode(const char* str, size_t itr, uint32_t* codepoint) {
    str += itr; unsigned char c = (unsigned char)str[0];

    if (c < 0x80) {
        *codepoint = c;
        return 1;
    }
    else if ((c >> 5) == 0x6) {
        *codepoint = ((c & 0x1F) << 6) | (str[1] & 0x3F);
        return 2;
    }
    else if ((c >> 4) == 0xE) {
        *codepoint = ((c & 0x0F) << 12) | ((str[1] & 0x3F) << 6) | (str[2] & 0x3F);
        return 3;
    }
    else if ((c >> 3) == 0x1E) {
        *codepoint = ((c & 0x07) << 18) | ((str[1] & 0x3F) << 12) | ((str[2] & 0x3F) << 6) | (str[3] & 0x3F);
        return 4;
    }

    // invalid fallback
    *codepoint = '?';
    return 1;
}

void create_text_request(arb_cache* cache, text_cache_slot* slot) {
    /*const arb_text_data* tdata = get_node_data(slot->key.node, slot->key.instance);
    dfont_font* font; if (!arb_injection_query_font(tdata->font, &font)) return;
    const char* text = tdata->text;

    // If text empty or font invalid
    // Sent empty text request
    if (!text || !font) {
        text_request req = {
            .text_data    = *tdata,
            .glyphs_count = 0,
            .glyphs       = NULL,
        };
        text_request_cache_push(cache, req);
        return; // overwrite current text buffer with empty text
    }

    // Count glyphs to allocate
    size_t glyph_count = 0; size_t extra_lines_count = 0;
    for (size_t i = 0; text[i] != '\0';) {
        uint32_t cp; i += utf8_decode(text, i, &cp);
        if (cp != '\n') glyph_count++; 
        else extra_lines_count++;
    }

    // Allocate glyphs buffer
    glyph_request* glyphs = glyph_count ? malloc(sizeof(glyph_request) * glyph_count) : NULL;
    if (glyph_count && !glyphs) {
        text_request req = { .text_data = *tdata, .glyphs_count = 0, .glyphs = NULL };
        text_request_cache_push(cache, req); return;
    }

    // Find font scale
    const float font_scale = tdata->size / dfont_get_base_size(font);

    // Populate glyphs buffer
    const float ascent      = dfont_get_base_ascent(font)   * font_scale;
    const float descent     = dfont_get_base_descent(font)  * font_scale;
    const float line_gap    = dfont_get_base_line_gap(font) * font_scale;
    const float line_height = ascent - descent + line_gap;

    float    pen_x      = 0.0f;
    float    pen_y      = 0.0f;
    float    text_width = 0.0f; // max line width across all lines
    size_t   glyph_idx  = 0;
    uint32_t prev_cp    = 0;    // for kerning; 0 = no previous glyph

    for (size_t itr = 0; text[itr] != '\0';) {
        uint32_t cp;
        itr += dfont_utf8_decode(text, itr, &cp);

        // Handle newline
        if (cp == '\n') {
            if (pen_x > text_width) text_width = pen_x;
            pen_x   = 0.0f;
            pen_y  -= line_height;
            prev_cp = 0; // reset kerning across lines
            continue;
        }

        // Kerning between consecutive glyphs on the same line
        if (prev_cp) pen_x += dfont_get_kerning(font, prev_cp, cp);

        // Write glyph
        const dfont_glyph g = dfont_get_glyph(font, cp);
        glyphs[glyph_idx++] = (glyph_request){
            .atlas_position = g.atlas_position,
            .off_x          = pen_x + g.bearing_x * font_scale,
            .off_y          = (extra_lines_count * line_height + pen_y) - g.bearing_y * font_scale,
            .size_x         = g.size_x * font_scale,
            .size_y         = g.size_y * font_scale,
        };

        // Advance
        pen_x  += g.advance_x * font_scale;
        prev_cp = cp;
    }

    // Account for final line (no trailing newline)
    if (pen_x > text_width) text_width = pen_x;

    // Total pixel height: baseline of last line + full single-line cap height
    float text_height = -pen_y + ascent;

    // Store text dimensions
    slot->text_width  = text_width;
    slot->text_height = text_height;

    // Request text upload
    text_request req = {
        .text_data    = *tdata,
        .glyphs_count = glyph_count,
        .glyphs       = glyphs,
    };
    text_request_cache_push(cache, req);*/
}

#endif // ARBOR_ARCHITECTURE_IMPL
