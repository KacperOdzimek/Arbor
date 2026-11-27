/*
----------------------------------------------------------------
Contents:
This file implements arbor ui system.

----------------------------------------------------------------
Code info:
- arb prefix
- ARBOR_IMPL macro to build

----------------------------------------------------------------
Usage: See dedicated documentation
*/

#ifndef ARBOR_H
#define ARBOR_H

// ===========================
// Depedency

#include <stdint.h>
#include <stddef.h>

// ===========================
// Implementation Injections - User define those functions

typedef struct arb_text_data arb_text_data;
void arb_injection_text_layout(
    const arb_text_data*    text_data,          // Laid text pointer and style
    int                     width_constrain,    // Given text block width, 0 == unlimited width
    size_t*                 out_count,          // Out count of glyphs
    void**                  out_glyphs,         // Glyphs malloc'ated array, shall be NULL if count == 0
    int*                    out_width,          // Out pixel width of text box
    int*                    out_height          // Out pixel height of text box
);

// ===========================
// Forwards

typedef struct arb_type   arb_type;
typedef struct arb_node   arb_node;
typedef struct arb_cache  arb_cache;

// ===========================
// Layout Length

// Value representing infinte length
// not set to int max, to avoid overflows in implementation
// needs to be increased if you are rendering on a (64+)K screen
#define ARB_INF_LENGTH (64 * 1000)

// Infinitely small flex
// Usefull for spacing that should collapse first
#define ARB_EPS_FLEX (1.0f / 1e9)

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

typedef struct {float m[3][2];} arb_mat3x2;

static inline arb_mat3x2 arb_mat3x2_identity() {
    return (arb_mat3x2){{
        {1, 0}, {0, 1}, {0, 0}
    }};
}

static inline arb_mat3x2 arb_mat3x2_scale(arb_mat3x2 m, float sx, float sy) {
    m.m[0][0] *= sx; m.m[0][1] *= sy;
    m.m[1][0] *= sx; m.m[1][1] *= sy;
    return m;
}

static inline arb_mat3x2 arb_mat3x2_offset(arb_mat3x2 m, float ox, float oy) {
    m.m[2][0] += ox; m.m[2][1] += oy;
    return m;
}

// ===========================
// Colors

// Basic 32 bit color
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
// Input

typedef struct arb_cursor_state {
    int                     left_down,  right_down;
    int                     position_x, position_y;
    float                   scroll_delta;
} arb_cursor_state;

typedef struct arb_typewriter_state {
    const char*             utf8_input;         // NULL-terminated utf8 sequence - flattened text input
} arb_typewriter_state;

typedef struct arb_input_state {
    arb_cursor_state        cursor;             // Cursor input
    arb_typewriter_state    typewriter;         // Typewriter input
} arb_input_state;

// ===========================
// Node Layout

typedef struct arb_node_layout_state {
    arb_length  measured_width;     // Desired width  of this node
    arb_length  measured_height;    // Desired height of this node
    int         given_width;        // Received width
    int         given_height;       // Received height
    int         hori_offset;        // Node center horizontal offset from parent center
    int         vert_offset;        // Node center vertical offset from parent center
} arb_node_layout_state;

// ===========================
// Functions Typedefs

typedef struct arb_node_update_func_arguments {
    const void*                     node_data;          // Node data
    void*                           storage_data;       // Storage data
    int                             resolution_x;       // Resolution width pixels
    int                             resolution_y;       // Resolution height pixels
    float                           delta_time;         // Time in seconds since last frame
    const arb_node_layout_state*    node_state;         // Node layout state for info
    int                             raw_hovered;        // Whether cursor is inside node
    int                             hovered;            // Whether cursor is inside node and no upper node was
    arb_input_state*                mutable;            // State with fields possibly consumed by previous handles
    const arb_input_state*          original;           // Original, untoched state of input
    const arb_input_state*          previous;           // Previous frame original state
} arb_node_update_func_arguments;
typedef void(arb_node_update_func_signature)(const arb_node_update_func_arguments* a);
typedef arb_node_update_func_signature* arb_node_update_func;

typedef struct arb_node_layout_func_arguments {
    const void*                     node_data;          // Node data
    void*                           storage_data;       // Storage data
    int                             resolution_x;       // Resolution width pixels
    int                             resolution_y;       // Resolution height pixels
    float                           delta_time;         // Time in seconds since last frame
    arb_node_layout_state*          node_state;         // Node own state
    size_t                          children_count;     // Node children count
    arb_node_layout_state**         children_states;    // Node children states
} arb_node_layout_func_arguments;
typedef void(arb_node_layout_func_signature)(const arb_node_layout_func_arguments* a);
typedef arb_node_layout_func_signature* arb_node_layout_func;

typedef struct arb_node_transform_func_arguments {
    const void*                     node_data;          // Node data
    void*                           storage_data;       // Storage data
    int                             resolution_x;       // Resolution width pixels
    int                             resolution_y;       // Resolution height pixels
    float                           delta_time;         // Time in seconds since last frame
    arb_mat3x2*                     transform;          // Given transform, can be changed
    const arb_node_layout_state*    layout;             // Node layout state for info
} arb_node_transform_func_arguments;
typedef void(arb_node_transform_func_signature)(const arb_node_transform_func_arguments* a);
typedef arb_node_transform_func_signature* arb_node_transform_func;

// ===========================
// Node Typedefs

typedef struct arb_type {
    // Structure

    // If false, next node in memory will be treated as a single nested child
    // If true, after the node, a sequence of indirect nodes is expected
    // each of them pointing to a container child. See ARB_ELEM helper.
    int array_child;

    // Update Pass

    // First update pass
    // Handles input and logic of nodes
    arb_node_update_func    update;

    // Layout Passes

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
    // OUT: [node offset from center]
    arb_node_layout_func    position;

    // Rendering Passes

    // First render pass
    // Allow altering children render transforms, top down
    // IN:  [complete layout states, parent render transform]
    // OUT: [own and children render transform]
    arb_node_transform_func transform;
} arb_type;

typedef enum arb_flag {
    arb_flag_none                   = 0,
    arb_flag_variable_data          = 1 << 0,   // This node data = cache variable, see arb_variable_type
    arb_flag_instanced_data         = 1 << 1,   // This node data = instance + data_offset, prioritized over arb_flag_storaged_data
    arb_flag_storaged_data          = 1 << 2,   // This node data = storage  + data_offset 
    arb_flag_indirected_data        = 1 << 3,   // (After other data flags) This node data = *(node data pointer as double pointer)
    arb_flag_ignore_min_width       = 1 << 4,   // Min width  of this node is set to 0
    arb_flag_ignore_min_height      = 1 << 5,   // Min height of this node is set to 0
    arb_flag_ignore_max_width       = 1 << 6,   // Max width  of this node is set to inf
    arb_flag_ignore_max_height      = 1 << 7,   // Max height of this node is set to inf
    arb_flag_set_max_to_min_width   = 1 << 8,   // Sets max width to min width, after ignore flags
    arb_flag_set_max_to_min_height  = 1 << 9,   // Sets max height to min height, after ignore flags
    arb_flag_clipbox                = 1 << 10,  // Children of this node on render are clipped to this node boundary
    arb_flag_pink_box               = 1 << 11,  // Render pink box in node boundary - for debugging
} arb_flag;

typedef struct arb_node {
    const arb_type* type;
    uint32_t        flags;
    union {
        const void*  data;
        size_t       data_offset;
    };
} arb_node;

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

// Storage type
// Creates an owned storage memory of size uint64(data)
// This storage pointer will be passed to all children functions
// May be used to add mutable state to UI elements (eg. button "pressed" state)
extern const arb_type arb_storage_type;

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
    const arb_invalidation_flag*    flag_always_ptr;        // null-able
    arb_invalidation_flag*          flag_consumable_ptr;    // null-able
} arb_invalidation_data;

// Indirect type
// This type data points to it's single child (may be NULL)
// Can be used with instancing
extern const arb_type arb_indirect_type;

// Variable type
// Usefull for making complex instance trickery; Proceed with caution (or avoid in most cases)
// This node stores it's data to cache-local variable
// Other nodes may pull this pointer as own data pointer with arb_flag_variable_data
// The variable update propagates down-tree only
extern const arb_type arb_variable_type;

// ===========================
// Rendering Node Types

// Adds node depth offset
// Decreasing depth means going 'into' the screen
// Data is arb_depth_data, single childed
extern const arb_type arb_depth_type;
typedef struct arb_depth_data {
    short depth_change;
} arb_depth_data;

// Box render primitive
// Data is arb_box_data, single child
extern const arb_type arb_box_type;
typedef struct arb_box_data {
    arb_color   tint;       // Box color
    const char* image;      // Image name/path, may be NULL
    float       rounding;   // Pixel corner rounding radius
    uint32_t    shader;     // Shader effect index
} arb_box_data;

// Text render primitive
// Data is arb_text_data, single child
extern const arb_type arb_text_type;
typedef struct arb_text_style {
    unsigned int    size;       // Font size
    const char*     font;       // Font name/path
    arb_color       tint;       // Text color modyficator
    uint32_t        shader;     // Shader effect index
} arb_text_style;
typedef struct arb_text_caret {
    size_t          count;      // Carets count
    const size_t*   carets;     // Carets positions (0 = before 0th characters); Sorted
} arb_text_caret;
typedef struct arb_text_data {
    const arb_text_caret*   carets; // Text carets; Null-able
    const arb_text_style*   style;  // Text style; Must not be NULL
    const char*             text;   // Text pointer
} arb_text_data;

// ===========================
// Layout Node Types

// Layouts children one on another
// The first child is deepest, rendered first
// No data, array children
extern const arb_type arb_overlay_type;

// Padds child inside self
// Data is arb_padding_data, single child
extern const arb_type arb_padding_type;
typedef struct arb_padding_data {
    arb_length left, right, top, bottom;
} arb_padding_data;

// Layouts children in a row, left to right
// Data is arb_row_data, array children
extern const arb_type arb_row_type;
typedef struct arb_row_data {
    float           vertical_align;     // 0 - align top,  0.5 - align center, 1.0 - align bottom, other values also work
    arb_length      spacing;            // spacing between children
} arb_row_data;

// Layouts children in a column, top to down
// Data is arb_column_data, array children
extern const arb_type arb_column_type;
typedef struct arb_column_data {
    float           horizontal_align;   // 0 - align left,  0.5 - align center, 1.0 - align right, other values also work
    arb_length      spacing;            // spacing between children
} arb_column_data;

// During layout, overwrites selected fields with provided values
// Data is arb_sizebox_data, single child
extern const arb_type arb_sizebox_type;
typedef enum arb_sizebox_overwrite_flag {
    arb_sizebox_overwrite_none        = 0,
    arb_sizebox_overwrite_all         = 255,
    arb_sizebox_overwrite_all_width   = 7,
    arb_sizebox_overwrite_all_height  = 56,

    arb_sizebox_overwrite_width_min   = 1 << 0,
    arb_sizebox_overwrite_width_max   = 1 << 1,
    arb_sizebox_overwrite_width_flex  = 1 << 2,

    arb_sizebox_overwrite_height_min  = 1 << 3,
    arb_sizebox_overwrite_height_max  = 1 << 4,
    arb_sizebox_overwrite_height_flex = 1 << 5
} arb_sizebox_overwrite_flag;
typedef struct arb_sizebox_data {
    arb_sizebox_overwrite_flag  flag;
    arb_length                  width;
    arb_length                  height;    
} arb_sizebox_data;

// Align type
// Aligns content in position pass
// Note align is applied inside align node given space
// Therefore align node shall be used with 
// arb_flag_ignore_max_width/height, to gain space to align in
extern const arb_type arb_align_type;
typedef struct arb_align_data {
    float vertical_align;   // 0 - align top,  0.5 - align center, 1.0 - align bottom, other values also work 
    float horizontal_align; // 0 - align left,  0.5 - align center, 1.0 - align right, other values also work
} arb_align_data;

// ===========================
// Input Node Types
// Those can be used to add input, without writing a new node type

// Input handle type
// Sets callback for children cursor input nodes to own data (shall be arb_node_input_func)
extern const arb_type arb_input_handle_type;

// Input input type
// Creates an input box, which will call arb_node_input_func provided by parent handle type
extern const arb_type arb_input_call_type;

// ===========================
// Transform Node Types
// Those can be used to add transforms, without writing a new node type

// Transform handle type
// Sets callback for children transform input nodes to own data (shall be arb_node_transform_func)
extern const arb_type arb_transform_handle_type;

// Cursor input type
// Creates an transform box, which will call arb_node_transform_func provided by parent handle type
extern const arb_type arb_transform_call_type;

// ===========================
// Predefinied Structures

// Instance this with arb_button_data for button structure
// This button tries to fill entire given space (flex = 1, max = inf)
// On events calls callbacks from data if provided
extern const arb_node arb_button_structure[];
typedef void(arb_button_func_signature)(void* payload); 
typedef arb_button_func_signature* arb_button_func;
typedef struct arb_button_style {
    const arb_box_data*         default_style;          // Button style when not touched
    const arb_box_data*         hovered_style;          // Button style when hovered
    const arb_box_data*         pressed_style;          // Button style when pressed
} arb_button_style;
typedef struct arb_button_target {
    arb_button_func             on_clicked;             // On button first pressed button
    arb_button_func             on_released;            // On button release frame
    arb_button_func             on_held;                // Every frame callback, when button pressed
    void*                       payload;                // Pointer passed to arb_button_funcs
} arb_button_target;
typedef struct arb_button_data {
    const arb_button_style*     style;                  // Button style pointer;  Must not be NULL
    const arb_button_target*    target;                 // Button target pointer; Must not be NULL
    const arb_node*             child;                  // Button child, overlay on button
} arb_button_data;

// Instance this with arb_scrollbox_data for vertical scrollbox structure
// This scrollbox will attempt filling entire given space (flex = 1, max = inf)
extern const arb_node arb_vertical_scrollbox_structure[];
// Instance this with arb_scrollbox_data for horizontal scrollbox structure
// This scrollbox will attempt filling entire given space (flex = 1, max = inf)
extern const arb_node arb_horizontal_scrollbox_structure[];
typedef struct arb_scrollbox_style {
    const arb_box_data*             default_style;      // Scrollbox handle style when not touched
    const arb_box_data*             hovered_style;      // Scrollbox handle style when hovered
    const arb_box_data*             pressed_style;      // Scrollbox handle style when pressed
} arb_scrollbox_style;
typedef struct arb_scrollbox_data {
    const arb_scrollbox_style*      style;              // Scrollbox style pointer; Must not be NULL
    const arb_node*                 child;              // Scrollbox scrolled child
} arb_scrollbox_data;

// Instance this with arb_float_slider_data for customizable, scrollable float value fill-slider
// This scrollbox will attempt filling entire given space (flex = 1, max = inf)
extern const arb_node arb_float_slider_structure[];
typedef struct arb_float_slider_style {
    int                             is_vertical;        // If vertical slide vertical, else hortizontal
    int                             is_scroll_disabled; // If true scrolling disabled
    const arb_box_data*             default_style;      // Slider style default
    const arb_box_data*             hovered_style;      // Slider style when hovered
    const arb_box_data*             pressed_style;      // Slider style when pressed
} arb_float_slider_style;
typedef struct arb_float_slider_target {
    float                           min, max;           // Slider range
    float*                          target;             // Slider storage variable
} arb_float_slider_target;
typedef struct arb_float_slider_data {
    const arb_float_slider_style*   style;              // Slider style;  Must not be NULL
    const arb_float_slider_target*  target;             // Slider Target; Must not be NULL
    const arb_node*                 child;              // Slider Child
} arb_float_slider_data;

// Instance this with arb_fixed_text_box_data for customizable, text box with fixed length limit
// This scrollbox will attempt filling entire given space (flex = 1, max = inf)
extern const arb_node arb_fixed_text_box_structure[];
typedef struct arb_fixed_text_box_style {
    const arb_text_style*           style;  // Text box style
} arb_fixed_text_box_style;
typedef struct arb_fixed_text_box_target {
    uint32_t                        bytes;  // Text block bytes constrain
    char*                           text;   // Text input target, displayed and edited. Must be 0-terminated at creation
} arb_fixed_text_box_target;
typedef struct arb_fixed_text_box_data {
    const arb_fixed_text_box_style* style;  // Style pointer; Must not be NULL
    const arb_fixed_text_box_target*target; // Textbox target
    arb_invalidation_flag*          invalid;// Client consumable invalidation flag; Can be used to request text rebuild, eg. after style change
} arb_fixed_text_box_data;

// ===========================
// Node Shortcuts

// Shortcut node creation: type, flags, data
#define ARB_NODE(argtype, argflags, ...) (arb_node){ \
    .type  = (&argtype),    \
    .flags = (argflags),    \
    .data_offset  = (size_t)(__VA_ARGS__)  \
}

// Uniform padding (value, value, flex 0) node
#define ARB_PADD(value)  (arb_node){   \
    .type  = &arb_padding_type,                     \
    .data  = &(arb_padding_data){                   \
        .top    = (arb_length){value, value, 0},    \
        .bottom = (arb_length){value, value, 0},    \
        .left   = (arb_length){value, value, 0},    \
        .right  = (arb_length){value, value, 0},    \
    },                                              \
}

// Indirect node shortcut
#define ARB_IDIR(argchild) (arb_node){  \
    .type   = &arb_indirect_type,       \
    .data   = (void*)(argchild)         \
}

// Instance node shortcut, data
#define ARB_INST(...) (arb_node){       \
    .type   = &arb_instance_type,       \
    .data   = (__VA_ARGS__)             \
}

// Local indirect to inlined array
#define ARB_ELEM(...) (arb_node){   \
    .type   = &arb_indirect_type,   \
    .data   = (arb_node[]){         \
        __VA_ARGS__                 \
    }                               \
}

// Sentinel value to mark array end
#define ARB_LAST (arb_node){.type = NULL, .data = NULL}

// ===========================
// Requests

typedef struct arb_text_free_request {
    void*       text_pointer;       // Renderer emitted pointer at text_alloc_request
} arb_text_free_request;

typedef struct arb_text_alloc_request {
    void**      text_pointer_out;   // Storage variable for text handle
    size_t      glyphs_count;       // Text layout injection emitted glyphs count
    void*       glyphs;             // Text layout injection emitted glyphs
} arb_text_alloc_request;

typedef struct arb_clipbox_request {
    arb_mat3x2  transform;          // Clipbox transform
    int         parent;             // Parent clipbox; -1 if none
} arb_clipbox_request;

typedef struct arb_draw_request {
    arb_mat3x2                      transform;      // Drawn box transform
    int                             clip_index;     // Clip index from clipboxes requests, -1 means no clip
    short                           depth_index;    // Depth index
    const arb_node_layout_state*    layout;         // Layout for extra info
    char is_box_not_text;                           // Whether to read union.box or union.text
    union {
        struct {
            arb_box_data    data;                   // Rendered box data
        } box;                                      // Per box data
        struct {
            void**          pointer;                // Text renderer-handle storage variable pointer
            unsigned int    size;                   // Rendered text font size
            const char*     font;                   // Rendered text font name/path
            arb_color       tint;                   // Rendered text color modyficator
            uint32_t        shader;                 // Shader effect index
        } text;                                     // Per text box data
    };
} arb_draw_request;

// ===========================
// Upload Access

typedef struct arb_requests {
    // Resolution UI was layed for
    uint32_t    resolution_x;
    uint32_t    resolution_y;

    // Minimum desired UI
    uint32_t    minimum_x;
    uint32_t    minimum_y;

    // Maximum UI size
    uint32_t    maximum_x;
    uint32_t    maximum_y;

    // Text free requests
    size_t                          text_free_count;
    const arb_text_free_request*    text_free_requests;

    // Text allocation requests
    size_t                          text_alloc_count;
    const arb_text_alloc_request*   text_alloc_requests;

    // Clipboxes requests
    size_t                          clipboxes_count;
    const arb_clipbox_request*      clipboxes_requests;

    // Draws requests
    size_t                          draws_count;
    const arb_draw_request*         draws_requests;
} arb_requests;

// ===========================
// Operation Status

typedef enum arb_status {
    arb_status_okay,
    arb_status_allocation_failed,   // Allocation failed
} arb_status;

// ===========================
// Cache

arb_cache* arb_create_cache();
void arb_free_cache(arb_cache*);

// Function updating UI
// Returns structure allowing access to cache-owned upload/render lists
// The pointers will be valid until arb_cache_update is called again
arb_status arb_cache_update(
    arb_cache*          cache,
    const arb_node*     root,
    int                 resolution_x,
    int                 resolution_y,
    arb_input_state     input_state,
    float               delta_time,
    arb_requests*       out_requests
);

// Can be called before free cache
// To get free text requests, if user
// is willing to clean the glyphs buffer
arb_status arb_cache_free_all_text(
    arb_cache*          cache,
    arb_requests*       out_requests
);

#endif // ARBOR_H

#ifdef ARBOR_IMPL

#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

/* 
    Implementation Notes:
    1 - last_frame_used values reference
        last_frame_used is used to clear hashmap from dead nodes
    0     - empty cell
    1     - imposible value, to force garbage collection on all
    2     - tombstone
    3-255 - rendered at frame of index
*/

#define LAST_FRAME_USED_EMPTY      0
#define LAST_FRAME_USED_IMPOSIBLE  1
#define LAST_FRAME_USED_TOMBSTONE  2
#define LAST_FRAME_USED_FIRST      3

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

static inline int is_point_in_transformed_box(arb_mat3x2 t, float px, float py) {
    float a = t.m[0][0]; float c = t.m[0][1];
    float b = t.m[1][0]; float d = t.m[1][1];

    float x = px - t.m[2][0];
    float y = py - t.m[2][1];

    float det = a * d - b * c;
    if (det == 0.0f) return 0;

    float inv_det = 1.0f / det;
    float local_x = (d * x - b * y) * inv_det;
    float local_y = (-c * x + a * y) * inv_det;

    return (local_x >= -1.0f && local_x <= 1.0f && local_y >= -1.0f && local_y <= 1.0f);
}

// ===========================
// Stable sort helper

// Currently implemeted as mergesort
static void stable_sort(void* base, size_t nmemb, size_t size, int (*compar)(const void*, const void*)) {
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
// Invalidation Bits

typedef enum invalidation_flag_bit {
    invalidation_flag_bit_text              = 1,
    invalidation_flag_bit_width_measure     = 2,
    invalidation_flag_bit_width_distribute  = 4,
    invalidation_flag_bit_height_measure    = 8,
    invalidation_flag_bit_height_distribute = 16,
    invalidation_flag_bit_position          = 32,
} invalidation_flag_bit;
typedef unsigned char invalidation_mask;

// ===========================
// Cache Object

typedef enum emergency_jump_flag {
    emergency_jump_flag_grow_occured = 1,
    emergency_jump_flag_allocation_failure 
} emergency_jump_flag;

typedef struct node_stable_index {
    const arb_node*         node;
    const void*             instance;
} node_stable_index;

typedef struct cache_slot {
    node_stable_index       key;
    unsigned char           last_frame_used;
    size_t                  value_child_count;
    arb_node_layout_state   value_state;
} cache_slot;

typedef struct text_cache_slot {
    node_stable_index       key;
    unsigned char           last_frame_used;
    unsigned char           last_frame_rebuild;
    int                     text_width;
    int                     text_height;
    void*                   allocation;
} text_cache_slot;

typedef struct storage_cache_slot {
    node_stable_index       key;
    unsigned char           last_frame_used;
    uint64_t                bytes;
    void*                   allocation;
} storage_cache_slot;

typedef struct input_box {
    node_stable_index       owner;
    arb_node_update_func    handle;
    cache_slot*             slot;
    const void*             variable;
    storage_cache_slot*     storage;
    int                     clip_index;
    short                   depth_index;
    arb_mat3x2              box_transform;
} input_box;

struct arb_cache {
    // Passes constants
    int                     resolution_x;
    int                     resolution_y;
    unsigned char           frame_index;

    // Emergency jump to update function
    // Return with emergency_jump_flag flag
    // respective to error class
    jmp_buf                 emergency;

    // Nodes cache hashmap
    size_t                  cache_capacity;
    size_t                  cache_fill;
    cache_slot*             cache_slots;

    // Text cache hashmap
    size_t                  text_cache_capacity;
    size_t                  text_cache_fill;
    text_cache_slot*        text_cache_slots;

    // Storage cache hashmap
    size_t                  storage_cache_capacity;
    size_t                  storage_cache_fill;
    storage_cache_slot*     storage_cache_slots;
    
    // Input boxes dynamic array
    size_t                  input_boxes_capacity;
    size_t                  input_boxes_count;
    input_box*              input_boxes;

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

    // Flatten tree
    // SoA design; Each array elements capacity >= flatten_tree_capacity; 
    // flatten_tree_capacity is min of all
    size_t                  flatten_tree_capacity;
    size_t                  flatten_tree_count;
    cache_slot**            flatten_tree_slots;
    storage_cache_slot**    flatten_tree_storages;
    const void**            flatten_tree_variables;
    arb_node_layout_state** flatten_tree_states;
    size_t*                 flatten_tree_subtrees;
    invalidation_mask*      flatten_tree_masks;

    // Previous frame cursor state
    arb_input_state         previous_frame_input_state;
};

arb_cache* arb_create_cache() {
    arb_cache* cache = calloc(1, sizeof(arb_cache));
    if (cache) cache->frame_index = LAST_FRAME_USED_FIRST;
    return cache;
}

static void free_cached_text_alloc_requests(arb_cache* cache);
static void text_cache_hashmap_garbage_collect(arb_cache* cache);
static void storage_cache_hashmap_garbage_collect(arb_cache* cache);
void arb_free_cache(arb_cache* cache) {
    if (!cache) return;

    // Free all cached texts and storage by using impossible value
    cache->frame_index = LAST_FRAME_USED_IMPOSIBLE;
    text_cache_hashmap_garbage_collect(cache);
    storage_cache_hashmap_garbage_collect(cache);

    // Free all cached textes
    free_cached_text_alloc_requests(cache);

    free(cache->cache_slots);
    free(cache->text_cache_slots);
    free(cache->text_free_requests);
    free(cache->text_alloc_requests);
    free(cache->clipbox_requests);
    free(cache->draw_requests);
    free(cache->input_boxes);
    free(cache);
}

// ===========================
// Cache Hashmaps

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
// SLOT_TYPE* PREFIX##_hashmap_get              (arb_cache* cache, node_stable_index key)
// void       PREFIX##_hashmap_garbage_collect  (arb_cache* cache) 
// Define HASHMAP_SLOT_DESTRUCTOR(slot ptr) to set garbage collector slot free method
#define DEFINE_HASHMAP_FUNCS(PREFIX, SLOT_TYPE, SLOTS_FIELD, CAP_FIELD, FILL_FIELD) \
\
static SLOT_TYPE* PREFIX##_hashmap_get                                          \
(arb_cache* cache, node_stable_index key);                                      \
\
static void PREFIX##_hashmap_grow(arb_cache* cache) {                           \
    size_t old_cap = cache->CAP_FIELD;                                          \
    SLOT_TYPE* old_slots = cache->SLOTS_FIELD;                                  \
\
    size_t new_cap = old_cap ? old_cap * 2 : 64;                                \
\
    void* new_alloc = calloc(new_cap, sizeof(*cache->SLOTS_FIELD));             \
    if (!new_alloc) longjmp(                                                    \
        cache->emergency, emergency_jump_flag_allocation_failure                \
    );                                                                          \
    cache->SLOTS_FIELD = new_alloc;                                             \
    cache->CAP_FIELD   = new_cap;                                               \
    cache->FILL_FIELD  = 0;                                                     \
\
    for (size_t i = 0; i < old_cap; ++i) {                                      \
        unsigned char time = old_slots[i].last_frame_used;                      \
        if (time == LAST_FRAME_USED_EMPTY ||                                    \
            time == LAST_FRAME_USED_TOMBSTONE) continue;                        \
        SLOT_TYPE* dst = PREFIX##_hashmap_get(cache, old_slots[i].key);         \
        *dst = old_slots[i];                                                    \
    }                                                                           \
\
    free(old_slots);                                                            \
    longjmp(cache->emergency, emergency_jump_flag_grow_occured);                \
}                                                                               \
\
static SLOT_TYPE* PREFIX##_hashmap_get(                                         \
    arb_cache* cache, node_stable_index key                                     \
) {                                                                             \
    if ((cache->FILL_FIELD + 1) * 10 >= cache->CAP_FIELD * 7) {                 \
        PREFIX##_hashmap_grow(cache);                                           \
    }                                                                           \
\
    size_t mask = cache->CAP_FIELD - 1;                                         \
    size_t idx  = hash_key(key) & mask;                                         \
\
    for (SLOT_TYPE* tombstone = NULL;;) {                                       \
        SLOT_TYPE* slot = &cache->SLOTS_FIELD[idx];                             \
        unsigned char time = slot->last_frame_used;                             \
\
        if (time == LAST_FRAME_USED_EMPTY) {                                    \
            if (tombstone) slot = tombstone;                                    \
            else cache->FILL_FIELD++;                                           \
            *slot = (SLOT_TYPE){.key = key};                                    \
            slot->last_frame_used = cache->frame_index;                         \
            return slot;                                                        \
        }                                                                       \
\
        if (time == LAST_FRAME_USED_TOMBSTONE) {                                \
            if (!tombstone) tombstone = slot;                                   \
        }                                                                       \
        else if (                                                               \
            slot->key.node == key.node && slot->key.instance == key.instance    \
        ) { slot->last_frame_used = cache->frame_index; return slot; }          \
\
        idx = (idx + 1) & mask;                                                 \
    }                                                                           \
}                                                                               \
\
static void PREFIX##_hashmap_garbage_collect(arb_cache* cache) {                \
    for (size_t i = 0; i < cache->CAP_FIELD; i++) {                             \
        SLOT_TYPE*     slot = &cache->SLOTS_FIELD[i];                           \
        unsigned char* time = &slot->last_frame_used;                           \
        if (*time &&                                                            \
            *time != LAST_FRAME_USED_TOMBSTONE    &&                            \
            *time != cache->frame_index                                         \
        ) {                                                                     \
            HASHMAP_SLOT_DESTRUCTOR(slot);                                      \
            *time = LAST_FRAME_USED_TOMBSTONE;                                  \
        }                                                                       \
    }                                                                           \
}

// Cache slots hashmap
#define HASHMAP_SLOT_DESTRUCTOR(slot_ptr)
DEFINE_HASHMAP_FUNCS(cache, cache_slot, cache_slots, cache_capacity, cache_fill); 
#undef HASHMAP_SLOT_DESTRUCTOR

// Text cache slots hashmaps
static int text_free_request_cache_push(arb_cache*, arb_text_free_request);
#define HASHMAP_SLOT_DESTRUCTOR(slot_ptr) \
    { text_free_request_cache_push(cache, (arb_text_free_request){.text_pointer = slot_ptr->allocation}); } // Request client to free allocation

DEFINE_HASHMAP_FUNCS(text_cache, text_cache_slot, text_cache_slots, text_cache_capacity, text_cache_fill);
#undef HASHMAP_SLOT_DESTRUCTOR

// Storage cache slots hashmap
#define HASHMAP_SLOT_DESTRUCTOR(slot_ptr) \
    { free(slot_ptr->allocation); }

DEFINE_HASHMAP_FUNCS(storage_cache, storage_cache_slot, storage_cache_slots, storage_cache_capacity, storage_cache_fill);
#undef HASHMAP_SLOT_DESTRUCTOR

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
    input_box, input_box, input_boxes, input_boxes_capacity, input_boxes_count
);

static inline void free_cached_text_alloc_requests(arb_cache* cache) {
    for (size_t i = 0; i < cache->text_alloc_requests_count; i++) {
        free(cache->text_alloc_requests[i].glyphs);
    }
    cache->text_alloc_requests_count = 0;
}
// ===========================
// Cache flatten tree

static void flatten_tree_cache_free(arb_cache* cache) {
    free(cache->flatten_tree_variables);    cache->flatten_tree_variables = NULL;
    free(cache->flatten_tree_slots);        cache->flatten_tree_slots = NULL;
    free(cache->flatten_tree_storages);     cache->flatten_tree_storages = NULL;
    free(cache->flatten_tree_states);       cache->flatten_tree_states = NULL;
    free(cache->flatten_tree_subtrees);     cache->flatten_tree_subtrees = NULL;
    cache->flatten_tree_capacity = 0;       cache->flatten_tree_count = 0;
}

static void flatten_tree_cache_clear(arb_cache* cache) {
    cache->flatten_tree_count = 0;
}

static void flatten_tree_cache_push(
    arb_cache*              cache,
    cache_slot*             slot,
    storage_cache_slot*     storage,
    const void*             variable,
    arb_node_layout_state*  state,
    size_t                  subtree,
    invalidation_mask       mask
) {
    if (cache->flatten_tree_count >= cache->flatten_tree_capacity) {
        size_t new_cap = cache->flatten_tree_capacity ? cache->flatten_tree_capacity * 2 : 64;

        #define ARRAY_REALLOC(ARRAY) do {                       \
            void* new_ptr = realloc(                            \
                cache->flatten_tree_##ARRAY,                    \
                new_cap * sizeof(*cache->flatten_tree_##ARRAY)  \
            );                                                  \
            if (!new_ptr) longjmp(cache->emergency, emergency_jump_flag_allocation_failure); \
            cache->flatten_tree_##ARRAY = new_ptr;  \
        } while (0)
        ARRAY_REALLOC(variables);
        ARRAY_REALLOC(slots);
        ARRAY_REALLOC(storages);
        ARRAY_REALLOC(states);
        ARRAY_REALLOC(subtrees);
        ARRAY_REALLOC(masks);
        #undef ARRAY_REALLOC

        cache->flatten_tree_capacity = new_cap;
    }

    size_t index = cache->flatten_tree_count;
    cache->flatten_tree_variables[index]= variable;
    cache->flatten_tree_slots[index]    = slot;
    cache->flatten_tree_storages[index] = storage;
    cache->flatten_tree_states[index]   = state;
    cache->flatten_tree_subtrees[index] = subtree;
    cache->flatten_tree_masks[index]    = mask;
    cache->flatten_tree_count++;
}

// ===========================
// Node fields reads

static void* safe_storage_slot_get_allocation(storage_cache_slot* slot);

static inline const void* get_node_data(
    const arb_node* node, const char* instance, const void* variable, storage_cache_slot* storage
) {
    const void* data = NULL;

    // 1) Dispatch data by source
    if      (node->flags & arb_flag_variable_data)  data = (const char*)variable + node->data_offset;
    else if (node->flags & arb_flag_instanced_data) data = (const char*)instance + node->data_offset;
    else if (node->flags & arb_flag_storaged_data)  data = (const char*)safe_storage_slot_get_allocation(storage) + node->data_offset;
    else                                            data = node->data;

    // 2) If indirected, walk one more pointer
    if (node->flags & arb_flag_indirected_data && data) data = *((void**)data); 

    return data;
}

static inline const arb_node* get_node_child(
    const arb_node* node, const char* instance, const void* variable, storage_cache_slot* storage
) {
    if (node->type == &arb_indirect_type) { // If indirect child is pointed by data
        return get_node_data(node, instance, variable, storage);
    }

    // By default next child is next in memory
    const arb_node* next = (node + 1);
    if (next->type == NULL) return NULL;
    return next;
}

// ===========================
// Storage Queries

// Gets slot, always inserts, as storage must always exist for node
// May cause emergency jump
static inline storage_cache_slot* storage_cache_hashmap_get_with_alloc(
    arb_cache* cache, node_stable_index index, const void* variable, storage_cache_slot* storage
) {
    storage_cache_slot* slot = storage_cache_hashmap_get(cache, index);
    uint64_t bytes = (uint64_t)(get_node_data(index.node, index.instance, variable, storage));
    if (slot->bytes != bytes) {
        // 0 bytes realloc case
        if (bytes == 0) {
            free(slot->allocation); 
            slot->allocation = 0;
            slot->bytes = 0;
            return slot;
        }

        // shrink or grow case
        void* new_alloc = realloc(slot->allocation, bytes);
        if (!new_alloc) longjmp(cache->emergency, emergency_jump_flag_allocation_failure);

        // 0-initialize new space if grown
        if (bytes > slot->bytes) {
            memset((char*)new_alloc + slot->bytes, 0, bytes - slot->bytes);
        }

        slot->allocation = new_alloc;
        slot->bytes = bytes;
    }
    return slot;
}

static void* safe_storage_slot_get_allocation(storage_cache_slot* slot) {
    if (slot == NULL) return NULL; return slot->allocation;
}

// ===========================
// Text layout generation

static void create_text_request(arb_cache* cache, text_cache_slot* slot, const arb_text_data* tdata) {
    // The text was alredy rebuilt in this frame; Discard to avoid dumb work and memory leak
    // (may happen when same [node, instance] pair is used multiple times in single frame)
    if (slot->last_frame_rebuild == cache->frame_index) return;
    slot->last_frame_rebuild = cache->frame_index;

    if (slot->allocation) { // Request client to free outdated allocation
        text_free_request_cache_push(cache, (arb_text_free_request){.text_pointer = slot->allocation});
    }

    arb_text_alloc_request alloc_req = {0};
    alloc_req.text_pointer_out = &slot->allocation;
    
    arb_injection_text_layout(
        tdata, 0, &alloc_req.glyphs_count, &alloc_req.glyphs, &slot->text_width, &slot->text_height
    );

    if (alloc_req.glyphs_count && alloc_req.glyphs) {
        if (text_alloc_request_cache_push(cache, alloc_req) != -1) return;
        free(alloc_req.glyphs);
    }
    else {
        slot->text_width = 0; slot->text_height = 0;
        slot->allocation = NULL; // Free is pushed, there will be no overwrite by allocation
    }
}

// ===========================
// Update Pass

static void update_pass(
    arb_cache*      cache, 
    arb_input_state input_state, 
    float           delta_time
) {
    // Find out normalized cursor position
    float norm_cursor_x = -1.0f + 2.0f * ((float)input_state.cursor.position_x / cache->resolution_x);
    float norm_cursor_y =  1.0f - 2.0f * ((float)input_state.cursor.position_y / cache->resolution_y);

    // Construct input states
    arb_input_state changable_state = input_state;
    arb_node_update_func_arguments args = {
        .resolution_x   = cache->resolution_x,
        .resolution_y   = cache->resolution_y,
        .mutable        = &changable_state,
        .original       = &input_state,
        .previous       = &cache->previous_frame_input_state,
        .delta_time     = delta_time
    };

    // Do input callbacks, walking from topmost to deepest
    int ever_was_inside = 0; if (cache->input_boxes_count) for (size_t i = cache->input_boxes_count - 1; ; i--) {
        input_box* ibox = &cache->input_boxes[i];

        int cursor_inside  = is_point_in_transformed_box(ibox->box_transform, norm_cursor_x, norm_cursor_y);
        int clipbox_index = ibox->clip_index; while (clipbox_index != -1) {
            arb_clipbox_request req = cache->clipbox_requests[clipbox_index];
            cursor_inside &= is_point_in_transformed_box(req.transform, norm_cursor_x, norm_cursor_y);
            if (!cursor_inside) break;
            clipbox_index = req.parent;
        }

        args.node_data = get_node_data(
            ibox->owner.node, 
            ibox->owner.instance, 
            ibox->variable, 
            ibox->storage
        );

        args.storage_data = safe_storage_slot_get_allocation(ibox->storage);
        args.node_state   = &ibox->slot->value_state;
        args.hovered     = cursor_inside && !ever_was_inside;
        args.raw_hovered = cursor_inside;

        if (ibox->handle) ibox->handle(&args);
        ever_was_inside |= cursor_inside;
        if (i == 0) break; // break loop at last element
    }

    // Current state in now previous input state
    cache->previous_frame_input_state = input_state;
}

// ===========================
// Flatten Pass

static void flatten_pass(
    arb_cache*      cache,
    const arb_node* root,
    float           delta_time
) {
    
}

// ===========================
// Cache Update

// Main update function, calls passes
arb_status arb_cache_update(
    arb_cache*          cache,
    const arb_node*     root,
    int                 resolution_x,
    int                 resolution_y,
    arb_input_state     input_state,
    float               delta_time,
    arb_requests*       out_requests
) {
    // Clear output in case of early return
    *out_requests = (arb_requests){0};

    // Emergency fallback
    // May happen when growth in hashmap occurs, invalidating all pointers, then we redo everything
    // May happen when allocation failed, then we opt-out
    int setjmp_val = setjmp(cache->emergency);
    if (setjmp_val == emergency_jump_flag_allocation_failure) {
        return arb_status_allocation_failed;
    }
    else if (setjmp_val == emergency_jump_flag_grow_occured) {
        // Redo everyting as pointer became dangling
    }

    // Init state
    free_cached_text_alloc_requests(cache);
    cache->resolution_x                 = resolution_x;
    cache->resolution_y                 = resolution_y;
    cache->draw_requests_count          = 0;
    cache->text_free_requests_count     = 0;
    cache->text_alloc_requests_count    = 0;
    cache->clipbox_requests_count       = 0;
    cache->input_boxes_count            = 0;    // HERE!

    
    update_pass(cache, input_state, delta_time);    // Update UI; Handle input
    flatten_pass(cache, root, delta_time);          // Generate flat traversal order
    // Relayout UI
    // Render and generate input boxes

    return arb_status_okay;
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

void arb_overlay_width_measure_func(const arb_node_layout_func_arguments* args) {
    arb_length own = {0, 0, 0.0f};
    for (size_t i = 0; i < args->children_count; ++i) {
        arb_length child = args->children_states[i]->measured_width;
        own.min  = max_int(own.min, child.min);
        own.max  = max_int(own.max, child.max);
    }

    if (own.min != own.max) own.flex = 1.0f;
    args->node_state->measured_width = own;
}

void arb_overlay_width_distribute_func(const arb_node_layout_func_arguments* args) {
    for (size_t i = 0; i < args->children_count; ++i) {
        args->children_states[i]->given_width = limit_length(args->node_state->given_width, args->children_states[i]->measured_width);
    }
}

void arb_overlay_height_measure_func(const arb_node_layout_func_arguments* args) {
    arb_length own = {0, 0, 0.0f};
    for (size_t i = 0; i < args->children_count; ++i) {
        arb_length child = args->children_states[i]->measured_height;
        own.min  = max_int(own.min, child.min);
        own.max  = max_int(own.max, child.max);
    }

    if (own.min != own.max) own.flex = 1.0f;
    args->node_state->measured_height = own;
}

void arb_overlay_height_distribute_func(const arb_node_layout_func_arguments* args) {
    for (size_t i = 0; i < args->children_count; ++i) {
        args->children_states[i]->given_height = limit_length(args->node_state->given_height, args->children_states[i]->measured_height);
    }
}

void arb_overlay_position_func(const arb_node_layout_func_arguments* args) {
    for (size_t i = 0; i < args->children_count; ++i) {
        args->children_states[i]->hori_offset = 0;
        args->children_states[i]->vert_offset = 0;
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
const arb_type arb_input_handle_type = box_behavior_type;

// ===========================
// Cursor input type
// This type is specially handled in pass implementation
const arb_type arb_input_call_type = box_behavior_type;

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
// This type is specially handled in pass implementation
const arb_type arb_indirect_type = box_behavior_type;

// ===========================
// Variable Type
// This type is specially handled in pass implementation
const arb_type arb_variable_type = box_behavior_type;

// ===========================
// Storage Type
// This type is specially handled in pass implementation
const arb_type arb_storage_type = box_behavior_type;

// ===========================
// Align type

static void align_position(const arb_node_layout_func_arguments* args) {
    const arb_align_data* data = args->node_data;

    // Position children in horizontal axis
    for (size_t i = 0; i < args->children_count; ++i) {
        arb_node_layout_state* child = args->children_states[i];
        child->hori_offset = (args->node_state->given_width  - child->given_width)  * (data->horizontal_align - 0.5f);
    }

    // Position children in vertial axis
    for (size_t i = 0; i < args->children_count; ++i) {
        arb_node_layout_state* child = args->children_states[i];
        child->vert_offset = (args->node_state->given_height - child->given_height) * (0.5f - data->vertical_align);
    }
}

const arb_type arb_align_type = {
    ARB_TYPE_OVERLAY_INIT,
    .position = align_position
};

// ===========================
// Padding Type

static inline int padding_distribute_length(
    int* a, arb_length al,
    int* b, arb_length bl,
    int* c, arb_length cl,
    int remaining
) {
    for (int pass = 0; pass < 3 && remaining > 0; pass++) {
        float tf = 0.0f;
        if (*a < al.max) tf += al.flex;
        if (*b < bl.max) tf += bl.flex;
        if (*c < cl.max) tf += cl.flex;
        if (tf <= 0.0f) break;

        int ga = (*a < al.max && al.flex > 0.0f) ? limit_length_gain(*a, al, (int)((float)remaining * al.flex / tf)) : 0;
        int gb = (*b < bl.max && bl.flex > 0.0f) ? limit_length_gain(*b, bl, (int)((float)remaining * bl.flex / tf)) : 0;
        int gc = (*c < cl.max && cl.flex > 0.0f) ? limit_length_gain(*c, cl, (int)((float)remaining * cl.flex / tf)) : 0;

        if (ga + gb + gc == 0) break; // all remaining too small after int cast
        *a += ga; *b += gb; *c += gc;
        remaining -= ga + gb + gc;
    }
    return remaining;
}

static void padding_width_measure(const arb_node_layout_func_arguments* args) {
    const arb_padding_data* data = args->node_data;
    
    int child_min = 0, child_max = 0;
    if (args->children_count > 0) {
        child_min = args->children_states[0]->measured_width.min;
        child_max = args->children_states[0]->measured_width.max;
    }

    int w_min = data->left.min + child_min + data->right.min;
    int w_max = data->left.max + child_max + data->right.max;

    args->node_state->measured_width = (arb_length){
        .min  = w_min,
        .max  = w_max,
        .flex = (w_min != w_max) ? 1.0f : 0.0f,
    };
}

static void padding_width_distribute(const arb_node_layout_func_arguments* args) {
    if (args->children_count == 0) return;
    const arb_padding_data* data = args->node_data;
    arb_node_layout_state* child = args->children_states[0];

    // Give every element its minimum
    int left_w  = data->left.min;
    int right_w = data->right.min;
    int child_w = child->measured_width.min;

    // Divide remaining space, give leftover to child
    int remaining = args->node_state->given_width - left_w - right_w - child_w;
    if (remaining > 0) {
        remaining = padding_distribute_length(
            &left_w, data->left, &right_w, data->right, &child_w, child->measured_width, remaining
        ); child_w += limit_length_gain(child_w, child->measured_width, remaining);
    }

    // Assign child width and position
    child->given_width = child_w;
    child->hori_offset = (left_w - right_w) / 2;
}

static void padding_height_measure(const arb_node_layout_func_arguments* args) {
    const arb_padding_data* data = args->node_data;
    int child_min = 0, child_max = 0;
    if (args->children_count > 0) {
        child_min = args->children_states[0]->measured_height.min;
        child_max = args->children_states[0]->measured_height.max;
    }

    int h_min = data->top.min + child_min + data->bottom.min;
    int h_max = data->top.max + child_max + data->bottom.max;

    args->node_state->measured_height = (arb_length){
        .min  = h_min,
        .max  = h_max,
        .flex = (h_min != h_max) ? 1.0f : 0.0f,
    };
}

static void padding_height_distribute(const arb_node_layout_func_arguments* args) {
    if (args->children_count == 0) return;
    const arb_padding_data* data = args->node_data;
    arb_node_layout_state* child = args->children_states[0];

    // Give every element its minimum
    int top_h    = data->top.min;
    int bottom_h = data->bottom.min;
    int child_h  = child->measured_height.min;

    // Divide remaining space, give leftover to child
    int remaining = args->node_state->given_height - top_h - bottom_h - child_h;
    if (remaining > 0) {
        remaining = padding_distribute_length(
            &top_h, data->top, &bottom_h, data->bottom, &child_h, child->measured_height, remaining
        ); child_h += limit_length_gain(child_h, child->measured_height, remaining);
    }

    // Assign child width and position
    child->given_height = child_h;
    child->vert_offset  = (bottom_h - top_h) / 2;
}

const arb_type arb_padding_type = {
    .array_child        = 0,
    .width_measure      = padding_width_measure,
    .width_distribute   = padding_width_distribute,
    .height_measure     = padding_height_measure,
    .height_distribute  = padding_height_distribute,
    .position           = NULL,
    .transform          = NULL
};

// ===========================
// Sizebox Type

static void sizebox_width_measure(const arb_node_layout_func_arguments* args) {
    const arb_sizebox_data* data = args->node_data;
    arb_overlay_width_measure_func(args);
    if (data->flag & arb_sizebox_overwrite_width_min)   args->node_state->measured_width.min   = data->width.min;
    if (data->flag & arb_sizebox_overwrite_width_max)   args->node_state->measured_width.max   = data->width.max;
    if (data->flag & arb_sizebox_overwrite_width_flex)  args->node_state->measured_width.flex  = data->width.flex;
}

static void sizebox_height_measure(const arb_node_layout_func_arguments* args) {
    const arb_sizebox_data* data = args->node_data;
    arb_overlay_height_measure_func(args);
    if (data->flag & arb_sizebox_overwrite_height_min)  args->node_state->measured_height.min  = data->height.min;
    if (data->flag & arb_sizebox_overwrite_height_max)  args->node_state->measured_height.max  = data->height.max;
    if (data->flag & arb_sizebox_overwrite_height_flex) args->node_state->measured_height.flex = data->height.flex;
}

const arb_type arb_sizebox_type = {
    .array_child        = 0,
    .width_measure      = sizebox_width_measure,
    .width_distribute   = arb_overlay_width_distribute_func,
    .height_measure     = sizebox_height_measure,
    .height_distribute  = arb_overlay_height_distribute_func,
    .position           = arb_overlay_position_func,
    .transform          = NULL
};

// ===========================
// Row Type

static void row_width_measure(const arb_node_layout_func_arguments* args) {
    const arb_row_data* data = (const arb_row_data*)args->node_data;
    arb_length          own  = {0, 0, 0.0f};

    for (size_t i = 0; i < args->children_count; ++i) {
        arb_length child = args->children_states[i]->measured_width;
        own.min += child.min; own.max += child.max;
    }

    size_t spaces = args->children_count ? args->children_count - 1 : 0;
    own.min += spaces * data->spacing.min;

    if (own.max != ARB_INF_LENGTH && data->spacing.max != ARB_INF_LENGTH) own.max += spaces * data->spacing.max;
    else own.max = ARB_INF_LENGTH;

    if (own.min != own.max) own.flex = 1.0f;
    args->node_state->measured_width = own;
}

static void row_width_distribute(const arb_node_layout_func_arguments* args) {
    const arb_row_data* data = (const arb_row_data*)args->node_data;

    // Find spaces count
    size_t spaces_count = args->children_count ? args->children_count - 1 : 0;

    // Minimal pass
    int   used_width = 0;
    float flexsum    = 0.0f;
    int   spacing    = data->spacing.min;
    for (size_t i = 0; i < args->children_count; ++i) {
        flexsum += args->children_states[i]->measured_width.flex;
        args->children_states[i]->given_width = args->children_states[i]->measured_width.min;
        used_width += args->children_states[i]->given_width;
    }
    flexsum += data->spacing.flex;
    used_width += spaces_count * data->spacing.min;

    // Divide extra space
    int left_width = args->node_state->given_width - used_width;
    if (left_width < 0) left_width = 0;
    while (left_width) {
        float next_flexsum = 0.0f;
        int   partitioned  = 0;
        
        // add to spacing
        if (spacing < data->spacing.max) {
            int gain = (int)(left_width * (data->spacing.flex / flexsum));
            if (spaces_count) gain /= (int)spaces_count;
            gain = limit_length_gain(spacing, data->spacing, gain);

            spacing     += gain; 
            partitioned += gain * spaces_count;

            if (spacing != data->spacing.max) next_flexsum += data->spacing.flex;
        }

        // add to children
        for (size_t i = 0; i < args->children_count; ++i) {
            arb_length  m = args->children_states[i]->measured_width;
            int* assigned = &args->children_states[i]->given_width;
            if (*assigned == m.max) continue;   // maxed

            int gain = (int)(left_width * (m.flex / flexsum));
            gain = limit_length_gain(*assigned, m, gain);

            *assigned   += gain;
            partitioned += gain;

            if (*assigned != m.max) next_flexsum += m.flex;
        }

        // if failed to divide the space, break
        if (partitioned == 0) break;

        left_width -= partitioned;
        flexsum     = next_flexsum;
    }

    // Position children in horizontal axis
    int cursor_x = -args->node_state->given_width / 2;
    for (size_t i = 0; i < args->children_count; ++i) {
        arb_node_layout_state* child = args->children_states[i];
        cursor_x += child->given_width / 2;
        child->hori_offset = cursor_x;
        cursor_x += child->given_width / 2 + spacing;
    }
}

static void row_position(const arb_node_layout_func_arguments* args) {
    const arb_row_data* data = (const arb_row_data*)args->node_data;

    // Position children in vertial axis
    for (size_t i = 0; i < args->children_count; ++i) {
        arb_node_layout_state* child = args->children_states[i];
        child->vert_offset = (args->node_state->given_height - child->given_height) * (0.5f - data->vertical_align);
    }
}

const arb_type arb_row_type = {
    .array_child        = 1,
    .width_measure      = row_width_measure,
    .width_distribute   = row_width_distribute,
    .height_measure     = arb_overlay_height_measure_func,
    .height_distribute  = arb_overlay_height_distribute_func,
    .position           = row_position,
    .transform          = NULL
};

// ===========================
// Column Type

static void column_height_measure(const arb_node_layout_func_arguments* args) {
    const arb_column_data* data = (const arb_column_data*)args->node_data;
    arb_length             own  = {0, 0, 0.0f};

    for (size_t i = 0; i < args->children_count; ++i) {
        arb_length child = args->children_states[i]->measured_height;
        own.min += child.min; own.max += child.max;
    }

    size_t spaces = args->children_count ? args->children_count - 1 : 0;
    own.min += spaces * data->spacing.min;

    if (own.max != ARB_INF_LENGTH && data->spacing.max != ARB_INF_LENGTH) own.max += spaces * data->spacing.max;
    else own.max = ARB_INF_LENGTH;

    if (own.min != own.max) own.flex = 1.0f;
    args->node_state->measured_height = own;
}

static void column_height_distribute(const arb_node_layout_func_arguments* args) {
    const arb_column_data* data = (const arb_column_data*)args->node_data;

    // Find spaces count
    size_t spaces_count = args->children_count ? args->children_count - 1 : 0;

    // Minimal pass
    int   used_height = 0;
    float flexsum     = 0.0f;
    int   spacing     = data->spacing.min;
    for (size_t i = 0; i < args->children_count; ++i) {
        flexsum += args->children_states[i]->measured_height.flex;
        args->children_states[i]->given_height = args->children_states[i]->measured_height.min;
        used_height += args->children_states[i]->given_height;
    }
    flexsum += data->spacing.flex;
    used_height += spaces_count * data->spacing.min;

    // Divide extra space
    int left_height = args->node_state->given_height - used_height;
    if (left_height < 0) left_height = 0;
    while (left_height) {
        float next_flexsum = 0.0f;
        int   partitioned  = 0;

        // add to spacing
        if (spacing < data->spacing.max) {
            int gain = (int)(left_height * (data->spacing.flex / flexsum));
            if (spaces_count) gain /= (int)spaces_count;
            gain = limit_length_gain(spacing, data->spacing, gain);

            spacing     += gain;
            partitioned += gain * spaces_count;

            if (spacing != data->spacing.max) next_flexsum += data->spacing.flex;
        }

        // add to children
        for (size_t i = 0; i < args->children_count; ++i) {
            arb_length  m = args->children_states[i]->measured_height;
            int* assigned = &args->children_states[i]->given_height;
            if (*assigned == m.max) continue;   // maxed

            int gain = (int)(left_height * (m.flex / flexsum));
            gain = limit_length_gain(*assigned, m, gain);

            *assigned   += gain;
            partitioned += gain;

            if (*assigned != m.max) next_flexsum += m.flex;
        }

        // if failed to divide the space, break
        if (partitioned == 0) break;

        left_height -= partitioned;
        flexsum      = next_flexsum;
    }

    // Position children in vertical axis
    int cursor_y = args->node_state->given_height / 2;
    for (size_t i = 0; i < args->children_count; i++) {
        arb_node_layout_state* child = args->children_states[i];
        cursor_y -= child->given_height / 2;
        child->vert_offset = cursor_y;
        cursor_y -= child->given_height / 2 + spacing;
    }
}

static void column_position(const arb_node_layout_func_arguments* args) {
    const arb_column_data* data = (const arb_column_data*)args->node_data;

    // Position children in horizontal axis
    for (size_t i = 0; i < args->children_count; ++i) {
        arb_node_layout_state* child = args->children_states[i];
        child->hori_offset = (args->node_state->given_width  - child->given_width)  * (data->horizontal_align - 0.5f);
    }
}

const arb_type arb_column_type = {
    .array_child        = 1,
    .width_measure      = arb_overlay_width_measure_func,
    .width_distribute   = arb_overlay_width_distribute_func,
    .height_measure     = column_height_measure,
    .height_distribute  = column_height_distribute,
    .position           = column_position,
    .transform          = NULL
};

// ===========================
// Button Structure

typedef struct button_storage {
    int          pressed;
    arb_box_data current;
} button_storage;

static void button_cursor_func(const arb_node_update_func_arguments* args) {
    const arb_button_data*   data   = args->node_data;
    const arb_button_style*  style  = data->style;
    const arb_button_target* target = data->target;
    button_storage*          stor   = args->storage_data;

    arb_cursor_state crr = args->mutable->cursor;
    arb_cursor_state prv = args->previous->cursor;

    char just_pressed  = crr.left_down  && !prv.left_down;
    char just_released = !crr.left_down && prv.left_down;

    if (just_pressed && args->hovered) {  // press started
        stor->pressed = 1;
        stor->current = *style->pressed_style;
        if (target->on_clicked) target->on_clicked(target->payload);
        crr.left_down = 0;
    }
    else if (crr.left_down && stor->pressed) {    // held
        stor->current = *style->pressed_style;
        if (target->on_held) target->on_held(target->payload);
        crr.left_down = 0;
    }
    else if (just_released && stor->pressed) {   // released
        stor->pressed = 0;
        if (args->hovered) stor->current = *style->hovered_style;
        else stor->current = *style->default_style;
        if (target->on_released)  target->on_released(target->payload);
    }
    else if (args->hovered) stor->current = *style->hovered_style;    // hover
    else stor->current = *style->default_style;  // idle
}

const arb_node arb_button_structure[] = {
    ARB_NODE(arb_storage_type,       arb_flag_none, sizeof(button_storage)),
    ARB_NODE(arb_input_handle_type, arb_flag_none, button_cursor_func),
    ARB_NODE(arb_input_call_type,   arb_flag_instanced_data, 0),
    ARB_NODE(arb_box_type, arb_flag_storaged_data | arb_flag_ignore_max_width | arb_flag_ignore_max_height, offsetof(button_storage, current)),
    ARB_NODE(arb_indirect_type, arb_flag_instanced_data | arb_flag_indirected_data, offsetof(arb_button_data, child))
};

// ===========================
// Scrollbox Common

typedef struct scrollbox_storage {
    int position;
    int handle_drag;
    int display_pixels;
    int content_pixels;
    int last_content_offset;
    arb_box_data current_handle_style;
} scrollbox_storage;

// ===========================
// Vertical Scrollbox

static const float scroll_speed_vertical = 2500;

static void vertical_scrollbox_scroll_cursor_func(const arb_node_update_func_arguments* args) {
    const arb_scrollbox_data* data = args->node_data; scrollbox_storage* stor = args->storage_data;
    if (args->raw_hovered) {
        float pixel_change = args->mutable->cursor.scroll_delta * args->delta_time * scroll_speed_vertical;
        stor->position -= pixel_change;
    }
}

static void vertical_scrollbox_transform_func(const arb_node_transform_func_arguments* args) {
    const arb_scrollbox_data* data = args->node_data; scrollbox_storage* stor = args->storage_data;

    // Calculate offset
    int offset_to_align = -stor->content_pixels / 2;  // start offseting from align - hardcoded top
    int total_offset    = offset_to_align + stor->position;

    // No scrolling needed
    if (stor->content_pixels <= stor->display_pixels) {
        total_offset  = (stor->content_pixels - stor->display_pixels) / 2;
        stor->position = 0;
    } 
    // Clamp
    else {
        int max_offset = (stor->content_pixels - stor->display_pixels) / 2;
        if (total_offset >  max_offset) {
            total_offset = max_offset;
            stor->position = max_offset - offset_to_align;
        }
        if (total_offset < -max_offset) {
            total_offset = -max_offset;
            stor->position = -max_offset - offset_to_align;
        }
    }

    // Offset transform
    *args->transform = arb_mat3x2_offset(*args->transform, 0, 2 * (float)total_offset / args->resolution_y);
    stor->last_content_offset = total_offset;

    // Calculate handle size
    float diplayed_portion = stor->content_pixels ? (float)stor->display_pixels / stor->content_pixels : 0.0f;
    float handle_height    = stor->display_pixels * diplayed_portion;
    if (handle_height > stor->display_pixels) handle_height = stor->display_pixels;
}

static void vertical_scrollbox_position(const arb_node_layout_func_arguments* args) {
    // Do not position child, as it's transform is dynamic not static
    // Ensure static offset is 0
    arb_overlay_position_func(args);

    // Probe height
    scrollbox_storage* stor = args->storage_data;
    stor->display_pixels = args->node_state->given_height;
    stor->content_pixels = args->children_count ? args->children_states[0]->given_height : 0;
}

// Special type to offset content and probe height given and measured
static const arb_type vertical_scrollbox_scroller_type = {
    ARB_TYPE_OVERLAY_INIT,
    .position   = vertical_scrollbox_position,
    .transform  = vertical_scrollbox_transform_func
};

static void vertical_scrollbox_handle_transform_func(const arb_node_transform_func_arguments* args) {
    scrollbox_storage* stor = args->storage_data;

    if (!stor->content_pixels) {
        *args->transform = (arb_mat3x2){0}; return;
    }

    // Find handle height as a fraction of displayed height
    float visible_fraction = (float)stor->display_pixels / stor->content_pixels;
    if (visible_fraction > 1.0f) visible_fraction = 1.0f; // clamp

    // Find handle height
    int height = stor->display_pixels * visible_fraction;
    if (height > stor->content_pixels) height = stor->content_pixels;

    // Position handle
    int handle_offset = 0;
    if (visible_fraction >= 1.0f) {
        handle_offset = 0;
    }
    else {
        // Find current lerp alpha of content between ends
        float begin = (stor->content_pixels - stor->display_pixels) / 2;
        float end   = -begin;
        float alpha = (stor->last_content_offset - begin) / (end -  begin);

        // Apply alpha to handle movement
        begin = -(stor->display_pixels / 2) + (height / 2);
        end   = -begin;
        handle_offset = begin + (end - begin) * alpha;
    }

    // Find vertical scale
    float sy = (float)height / stor->display_pixels;

    // Apply to transform
    *args->transform = arb_mat3x2_offset(*args->transform, 0, 2 * (float)handle_offset / args->resolution_y);
    *args->transform = arb_mat3x2_scale(*args->transform, 1, sy);
}

static void vertical_scrollbox_handle_cursor_func(const arb_node_update_func_arguments* args) {
    const arb_scrollbox_data*  data  = args->node_data; 
    const arb_scrollbox_style* style = data->style;
    scrollbox_storage*         stor  = args->storage_data;

    // Reset style
    stor->current_handle_style = *style->default_style;

    // Set style to hovered if hovered
    if (args->hovered) stor->current_handle_style = *style->hovered_style;

    // Scroll by draging handle
    int left_pressed = args->mutable->cursor.left_down;
    if (left_pressed) {
        int cursor_y = args->mutable->cursor.position_y;
        if (stor->handle_drag != -1) {                         // Was dragged
            int pixels_change = stor->handle_drag - cursor_y;  // Calculate pixel movement within handle
            float ratio = stor->display_pixels ? (float)stor->content_pixels / stor->display_pixels : 0.0f;
            pixels_change = (int)(pixels_change * ratio);

            stor->position -= pixels_change;
            stor->current_handle_style = *style->pressed_style;

            stor->handle_drag = cursor_y;
            args->mutable->cursor.left_down = 0;   // Consume left click
        }
        else if (args->hovered) {
            int c_left_pressed = args->mutable->cursor.left_down;
            int p_left_pressed = args->previous->cursor.left_down;
            if (!(c_left_pressed && !p_left_pressed)) return; // Avoid accidental drag, require new click inside handle    
            stor->handle_drag = cursor_y;
            args->mutable->cursor.left_down = 0;   // Consume left click
        }
    }
    else stor->handle_drag = -1;
}

// Special type to apply handle transform and receive cursor events
static const arb_type vertical_scrollbox_handle_type = {
    ARB_TYPE_OVERLAY_INIT,
    .transform  = vertical_scrollbox_handle_transform_func,
    .update     = vertical_scrollbox_handle_cursor_func
};

const arb_node vertical_scrollbox_main_body[] = {
    // Align node
    ARB_NODE(arb_align_type, arb_flag_ignore_max_height, &(arb_align_data){
        .horizontal_align = 0,
        .vertical_align   = 0
    }),
    // Scroller node, data = scrollbox data itself
    ARB_NODE(vertical_scrollbox_scroller_type, arb_flag_instanced_data | arb_flag_ignore_min_height, 0),
    ARB_NODE(arb_indirect_type, arb_flag_instanced_data | arb_flag_indirected_data, offsetof(arb_scrollbox_data, child))
};

const arb_node vertical_scrollbox_handle[] = {
    // Require handle width
    ARB_NODE(arb_sizebox_type, arb_flag_none, &(arb_sizebox_data){
        .flag  = arb_sizebox_overwrite_all_width,
        .width = (arb_length){16, 16, 1}
    }),
    // Handle node, data = scrollbox data itself
    ARB_NODE(vertical_scrollbox_handle_type, arb_flag_instanced_data, 0),
    // Handle visual
    ARB_NODE(
        arb_box_type,
        arb_flag_storaged_data | arb_flag_ignore_max_width | arb_flag_ignore_max_height,
        offsetof(scrollbox_storage, current_handle_style)
    ),
    ARB_LAST
};

const arb_node arb_vertical_scrollbox_structure[] = {
    // Storage for state
    ARB_NODE(arb_storage_type, arb_flag_none, sizeof(scrollbox_storage)),
    // Clipbox
    ARB_NODE(arb_box_type, arb_flag_clipbox | arb_flag_ignore_min_height, &(arb_box_data){.tint = ARB_HEX("#00000000")}),
    // Scroll input
    ARB_NODE(arb_input_handle_type, arb_flag_none, vertical_scrollbox_scroll_cursor_func),
    ARB_NODE(arb_input_call_type, arb_flag_instanced_data, 0), // Call with scrollbox data
    // Row - content/handle
    ARB_NODE(arb_row_type, arb_flag_none, &(arb_row_data){
        .spacing        = (arb_length){16, ARB_INF_LENGTH, ARB_EPS_FLEX},
        .vertical_align = 0.5
    }),
    ARB_IDIR(vertical_scrollbox_main_body),
    ARB_IDIR(vertical_scrollbox_handle),
    ARB_LAST
};

// ===========================
// Horizontal Scrollbox

static const float scroll_speed_horizontal = 3500;

static void horizontal_scrollbox_scroll_cursor_func(const arb_node_update_func_arguments* args) {
    const arb_scrollbox_data* data = args->node_data; scrollbox_storage* stor = args->storage_data;
    if (args->raw_hovered) {
        float pixel_change = args->mutable->cursor.scroll_delta * args->delta_time * scroll_speed_horizontal;
        stor->position += pixel_change;
    }
}

static void horizontal_scrollbox_transform_func(const arb_node_transform_func_arguments* args) {
    scrollbox_storage* stor = args->storage_data;

    // Calculate offset
    int offset_to_align = stor->content_pixels / 2;  // start offseting from align - hardcoded left
    int total_offset    = offset_to_align + stor->position;

    // No scrolling needed
    if (stor->content_pixels <= stor->display_pixels) {
        total_offset  = 0;
        stor->position = 0;
    } 
    // Clamp
    else {
        int max_offset = (stor->content_pixels - stor->display_pixels) / 2;
        if (total_offset >  max_offset) {
            total_offset = max_offset;
            stor->position = max_offset - offset_to_align;
        }
        if (total_offset < -max_offset) {
            total_offset = -max_offset;
            stor->position = -max_offset - offset_to_align;
        }
    }

    // Offset transform
    *args->transform = arb_mat3x2_offset(*args->transform, 2 * (float)total_offset / args->resolution_x, 0);
    stor->last_content_offset = total_offset;

    // Calculate handle size
    float diplayed_portion = stor->content_pixels ? (float)stor->display_pixels / stor->content_pixels : 0.0f;
    float handle_width     = stor->display_pixels * diplayed_portion;
    if (handle_width > stor->display_pixels) handle_width = stor->display_pixels;
}

static void horizontal_scrollbox_position(const arb_node_layout_func_arguments* args) {
    // Do not position child, as it's transform is dynamic not static
    // Ensure static offset is 0
    arb_overlay_position_func(args);

    // Probe width
    scrollbox_storage* stor = args->storage_data;
    stor->display_pixels = args->node_state->given_width;
    stor->content_pixels = args->children_count ? args->children_states[0]->given_width : 0;
}

// Special type to offset content and probe width given and measured
static const arb_type horizontal_scrollbox_scroller_type = {
    ARB_TYPE_OVERLAY_INIT,
    .position   = horizontal_scrollbox_position,
    .transform  = horizontal_scrollbox_transform_func
};

static void horizontal_scrollbox_handle_transform_func(const arb_node_transform_func_arguments* args) {
    scrollbox_storage* stor = args->storage_data;

    if (!stor->content_pixels) {
        *args->transform = (arb_mat3x2){0}; return;
    }

    // Find handle width as a fraction of displayed width
    float visible_fraction = (float)stor->display_pixels / stor->content_pixels;
    if (visible_fraction > 1.0f) visible_fraction = 1.0f; // clamp

    // Find handle width
    int width = stor->display_pixels * visible_fraction;
    if (width > stor->content_pixels) width = stor->content_pixels;

    // Position handle
    int handle_offset = 0;
    if (visible_fraction >= 1.0f) {
        handle_offset = 0;
    }
    else {
        // Find current lerp alpha of content between ends
        float begin = (stor->content_pixels - stor->display_pixels) / 2;
        float end   = -begin;
        float alpha = (stor->last_content_offset - begin) / (end -  begin);

        // Apply alpha to handle movement
        begin = -(stor->display_pixels / 2) + (width / 2);
        end   = -begin;
        handle_offset = begin + (end - begin) * alpha;
    }

    // Find horizontal scale
    float sx = (float)width / stor->display_pixels;

    // Apply to transform
    *args->transform = arb_mat3x2_offset(*args->transform, 2 * (float)handle_offset / args->resolution_x, 0);
    *args->transform = arb_mat3x2_scale(*args->transform, sx, 1);
}

static void horizontal_scrollbox_handle_cursor_func(const arb_node_update_func_arguments* args) {
    const arb_scrollbox_data*   data  = args->node_data; 
    const arb_scrollbox_style*  style = data->style;
    scrollbox_storage*          stor  = args->storage_data;

    // Reset style
    stor->current_handle_style = *style->default_style;

    // Set style to hovered if hovered
    if (args->hovered) stor->current_handle_style = *style->hovered_style;

    // Scroll by draging handle
    int left_pressed = args->mutable->cursor.left_down;
    if (left_pressed) {
        int cursor_x = args->mutable->cursor.position_x;
        if (stor->handle_drag != -1) {                         // Was dragged
            int pixels_change = stor->handle_drag - cursor_x;  // Calculate pixel movement within handle
            float ratio = stor->display_pixels ? (float)stor->content_pixels / stor->display_pixels : 0.0f;
            pixels_change = (int)(pixels_change * ratio);

            stor->position += pixels_change;
            stor->current_handle_style = *style->pressed_style;

            stor->handle_drag = cursor_x;
            args->mutable->cursor.left_down = 0;   // Consume left click
        }
        else if (args->hovered) {
            int c_left_pressed = args->mutable->cursor.left_down;
            int p_left_pressed = args->previous->cursor.left_down;
            if (!(c_left_pressed && !p_left_pressed)) return; // Avoid accidental drag, require new click inside handle    
            stor->handle_drag = cursor_x;
            args->mutable->cursor.left_down = 0;   // Consume left click
        }
    }
    else stor->handle_drag = -1;
}

// Special type to apply handle transform and receive cursor events
static const arb_type horizontal_scrollbox_handle_type = {
    ARB_TYPE_OVERLAY_INIT,
    .transform  = horizontal_scrollbox_handle_transform_func,
    .update     = horizontal_scrollbox_handle_cursor_func
};

const arb_node horizontal_scrollbox_main_body[] = {
    // Align node
    ARB_NODE(arb_align_type, arb_flag_ignore_max_width, &(arb_align_data){
        .horizontal_align = 0,
        .vertical_align   = 0
    }),
    // Scroller node, data = scrollbox data itself
    ARB_NODE(horizontal_scrollbox_scroller_type, arb_flag_instanced_data | arb_flag_ignore_min_width, 0),
    ARB_NODE(arb_indirect_type, arb_flag_instanced_data | arb_flag_indirected_data, offsetof(arb_scrollbox_data, child))
};

const arb_node horizontal_scrollbox_handle[] = {
    // Require handle height
    ARB_NODE(arb_sizebox_type, arb_flag_none, &(arb_sizebox_data){
        .flag   = arb_sizebox_overwrite_all_height,
        .height = (arb_length){16, 16, 1}
    }),
    // Handle node, data = scrollbox data itself
    ARB_NODE(horizontal_scrollbox_handle_type, arb_flag_instanced_data, 0),
    // Handle visual
    ARB_NODE(
        arb_box_type,
        arb_flag_storaged_data | arb_flag_ignore_max_width | arb_flag_ignore_max_height,
        offsetof(scrollbox_storage, current_handle_style)
    ),
    ARB_LAST
};

const arb_node arb_horizontal_scrollbox_structure[] = {
    // Storage for state
    ARB_NODE(arb_storage_type, arb_flag_none, sizeof(scrollbox_storage)),
    // Clipbox
    ARB_NODE(arb_box_type, arb_flag_clipbox | arb_flag_ignore_min_width, &(arb_box_data){.tint = ARB_HEX("#00000000")}),
    // Scroll input
    ARB_NODE(arb_input_handle_type, arb_flag_none, horizontal_scrollbox_scroll_cursor_func),
    ARB_NODE(arb_input_call_type, arb_flag_instanced_data, 0), // Call with scrollbox data
    // Column - content/handle
    ARB_NODE(arb_column_type, arb_flag_none, &(arb_column_data){
        .spacing          = (arb_length){16, ARB_INF_LENGTH, ARB_EPS_FLEX},
        .horizontal_align = 0.5
    }),
    ARB_IDIR(horizontal_scrollbox_main_body),
    ARB_IDIR(horizontal_scrollbox_handle),
    ARB_LAST
};

// ===========================
// Float Slider

typedef struct float_slider_storage {
    int          is_during_drag;
    int          last_drag_pixel;  // Raw cursor pixel (x or y, per axis) at last processed frame of the drag
    arb_box_data current_style;
} float_slider_storage;

#define FLOAT_SLIDER_SCROLL_SPEED 0.6f  // fraction of (max-min) per second, per unit scroll_delta
static void float_slider_cursor_func(const arb_node_update_func_arguments* args) {
    const arb_float_slider_data*   dat = args->node_data;
    const arb_float_slider_style*  stl = dat->style;
    const arb_float_slider_target* tar = dat->target;
    float_slider_storage*          str = args->storage_data;

    int pixel_pos = stl->is_vertical ? args->mutable->cursor.position_y : args->mutable->cursor.position_x;

    // Scroll wheel nudges value while hovered, independent of drag/click state
    if (!stl->is_scroll_disabled && args->hovered && dat->target && 
        tar->max > tar->min && args->mutable->cursor.scroll_delta != 0.0f
    ) {
        float value_delta = 
            args->mutable->cursor.scroll_delta * args->delta_time
            * FLOAT_SLIDER_SCROLL_SPEED * (tar->max - tar->min);

        float new_value = *tar->target + value_delta;
        if (new_value < tar->min) new_value = tar->min;
        if (new_value > tar->max) new_value = tar->max;
        *tar->target = new_value;

        args->mutable->cursor.scroll_delta = 0.0f; // consume, don't leak to nodes underneath
    }

    int c_left_down = args->mutable->cursor.left_down;
    int p_left_down = args->previous->cursor.left_down;

    if (!c_left_down) {
        str->is_during_drag = 0;
        str->current_style  = args->hovered ? *stl->hovered_style : *stl->default_style;
        return;
    }

    if (!str->is_during_drag) {
        if (!(c_left_down && !p_left_down) || !args->hovered) return;
        str->is_during_drag  = 1;
        str->last_drag_pixel = pixel_pos;
        str->current_style   = *stl->pressed_style;
        args->mutable->cursor.left_down = 0;
        return;
    }

    str->current_style = *stl->pressed_style;

    if (dat->target && tar->max > tar->min) {
        int axis_extent = stl->is_vertical ? args->node_state->given_height : args->node_state->given_width;
        if (axis_extent > 0) {
            int   pixel_delta  = pixel_pos - str->last_drag_pixel;
            float signed_delta = (float)(stl->is_vertical ? -pixel_delta : pixel_delta);
            float value_delta  = signed_delta * (tar->max - tar->min) / (float)axis_extent;

            float new_value = *tar->target + value_delta;
            if (new_value < tar->min) new_value = tar->min;
            if (new_value > tar->max) new_value = tar->max;
            *tar->target = new_value;
        }
    }

    str->last_drag_pixel = pixel_pos;
    args->mutable->cursor.left_down = 0;
}

static void float_slider_transform_func(const arb_node_transform_func_arguments* args) {
    const arb_float_slider_data*   dat = args->node_data;
    const arb_float_slider_style*  stl = dat->style;
    const arb_float_slider_target* tar = dat->target;

    float percent = 0.0f;
    if (tar->max > tar->min && dat->target) {
        percent = (*tar->target - tar->min) / (tar->max - tar->min);
        if (percent < 0.0f) percent = 0.0f;
        if (percent > 1.0f) percent = 1.0f;
    }

    if (stl->is_vertical) {
        // Anchor at bottom edge, fill upward with value
        float extent    = (float)args->layout->given_height;
        float offset_px = (percent - 1.0f) * extent * 0.5f;
        *args->transform = arb_mat3x2_scale (*args->transform, 1.0f, percent);
        *args->transform = arb_mat3x2_offset(*args->transform, 0.0f, 2.0f * offset_px / (float)args->resolution_y);
    } else {
        // Anchor at left edge, fill rightward with value
        float extent    = (float)args->layout->given_width;
        float offset_px = (percent - 1.0f) * extent * 0.5f;
        *args->transform = arb_mat3x2_scale (*args->transform, percent, 1.0f);
        *args->transform = arb_mat3x2_offset(*args->transform, 2.0f * offset_px / (float)args->resolution_x, 0.0f);
    }
}

const arb_node arb_float_slider_structure[] = {
    // Push storage
    ARB_NODE(arb_storage_type, arb_flag_none, sizeof(float_slider_storage)),
    // Input field taking entire slider space (possibly bigger than the value box)
    ARB_NODE(arb_input_handle_type, arb_flag_none, float_slider_cursor_func),
    ARB_NODE(arb_input_call_type, arb_flag_ignore_max_width | arb_flag_ignore_max_height | arb_flag_instanced_data, 0),
    // Transform slider according to percent
    ARB_NODE(arb_transform_handle_type, arb_flag_none, float_slider_transform_func),
    ARB_NODE(arb_transform_call_type, arb_flag_instanced_data, 0),
    // Slider box taking all dim
    ARB_NODE(
        arb_box_type, 
        arb_flag_ignore_max_width | arb_flag_ignore_max_height | arb_flag_storaged_data, 
        offsetof(float_slider_storage, current_style)
    ),
    // Jump to child
    ARB_NODE(arb_indirect_type, arb_flag_instanced_data | arb_flag_indirected_data, offsetof(arb_float_slider_data, child))
};

// ===========================
// Fixed Text Box

typedef struct fixed_text_box_storage {
    int                     focused;
    size_t                  cursor_pos;
    arb_text_caret          caret_data;
    arb_text_data           text_data;
    arb_invalidation_data   invalidation;
    arb_invalidation_flag   consumable_flag;
} fixed_text_box_storage;

const arb_invalidation_flag fixed_text_box_invalidation_always_flag = arb_invalidation_flag_width_measure;

static void fixed_text_box_input_func(const arb_node_update_func_arguments* args) {
    const arb_fixed_text_box_data*   dat = args->node_data;
    const arb_fixed_text_box_style*  stl = dat->style;
    const arb_fixed_text_box_target* tar = dat->target;
    fixed_text_box_storage*          str = args->storage_data;

    arb_cursor_state crr = args->mutable->cursor;
    arb_cursor_state prv = args->previous->cursor;

    // Configure read pointers
    if (str->invalidation.flag_consumable_ptr != &str->consumable_flag) {
        str->consumable_flag = arb_invalidation_flag_text;
    }
    str->text_data.style   = stl->style;
    str->text_data.text    = tar->text;
    str->text_data.carets  = &str->caret_data;
    str->caret_data.count  = 0;
    str->caret_data.carets = &str->cursor_pos;
    str->invalidation.flag_consumable_ptr = &str->consumable_flag;
    str->invalidation.flag_always_ptr = &fixed_text_box_invalidation_always_flag;

    // OR-other invalidation flags
    if (dat->invalid) {
        *str->invalidation.flag_consumable_ptr |= *dat->invalid;
        *dat->invalid = arb_invalidation_flag_none;
    }

    // Detect mouse click to focus
    if (crr.left_down && !prv.left_down) {
        str->focused = args->hovered;
    }
    str->caret_data.count = str->focused ? 1 : 0;
    if (!str->focused) return;

    // Ensure cursor_pos stays within valid string bounds
    size_t text_len = strlen(tar->text);
    if (str->cursor_pos > text_len) {
        str->cursor_pos = text_len;
    }
    
    // Make changes in text
    char* input = (char*)args->mutable->typewriter.utf8_input;
    if (!input || !*input || tar->bytes == 0) return;
    str->consumable_flag = arb_invalidation_flag_text;  // Invalidate text on input
    while (*input) {
        if (*input == '\b') {
            if (str->cursor_pos > 0) {
                size_t prev_pos = str->cursor_pos;
                do {
                    str->cursor_pos--;
                } while (str->cursor_pos > 0 && ((unsigned char)tar->text[str->cursor_pos] & 0xC0) == 0x80);

                size_t deleted_bytes = prev_pos - str->cursor_pos;
                size_t tail_len = text_len - prev_pos;
                
                memmove(tar->text + str->cursor_pos, tar->text + prev_pos, tail_len + 1);
                text_len -= deleted_bytes;
            }
            input++;
            continue;
        }

        // Standard text insertion at cursor_pos
        const char* temp_ptr = input;
        decode_utf8(&temp_ptr); // Advance pointer over current UTF-8 sequence
        size_t char_bytes = (size_t)(temp_ptr - input);

        size_t available = tar->bytes - text_len - 1;
        if (char_bytes <= available) {
            size_t tail_len = text_len - str->cursor_pos;
            
            memmove(tar->text + str->cursor_pos + char_bytes, tar->text + str->cursor_pos, tail_len + 1);
            memcpy(tar->text + str->cursor_pos, input, char_bytes);
            
            str->cursor_pos += char_bytes;
            text_len += char_bytes;
        }
        input = (char*)temp_ptr;
    }

    args->mutable->typewriter.utf8_input = NULL;
}

const arb_node arb_fixed_text_box_structure[] = {
    // Push storage
    ARB_NODE(arb_storage_type, arb_flag_none, sizeof(fixed_text_box_storage)),
    // Invalidation
    ARB_NODE(arb_invalidation_type, arb_flag_storaged_data, offsetof(fixed_text_box_storage, invalidation)),
    // Focus button
    ARB_NODE(arb_input_handle_type, arb_flag_none, fixed_text_box_input_func),
    ARB_NODE(arb_input_call_type, arb_flag_ignore_max_width | arb_flag_ignore_max_height |  arb_flag_instanced_data, 0),
    ARB_NODE(arb_align_type, arb_flag_ignore_max_width | arb_flag_ignore_max_height, &(arb_align_data){ .horizontal_align = 0, .vertical_align = 0 }),
    // Text input
    ARB_NODE(arb_text_type, arb_flag_storaged_data, offsetof(fixed_text_box_storage, text_data)),
    ARB_LAST
};

#endif
