# Arbor UI — Contents Reference

This is a catalog of all node types, and predefinied structures in arbor.

## 1. Architectural Node Types

These implement tree structure and state mechanisms rather than anything
visible on screen.

### `arb_instance_type`
Sets the current instance pointer for its subtree. Data is a pointer (or
offset) to an arbitrary instance struct; nodes below can pull fields out of
it with `arb_flag_instanced_data`. Single child. No data struct of its own
— `ARB_INST(ptr)` is the shortcut. See ``design_reference/instancing``.

### `arb_storage_type`
Opens an owned, cache-persistent block of `uint64_t(data)` bytes for its
subtree, zero-initialized on first allocation. Nodes below read/write
fields in it with `arb_flag_storaged_data`. Single child. Data is just the
byte size to allocate (e.g. `sizeof(button_storage)`).  See ``design_reference/storage``.

### `arb_invalidation_type`
A relayout gate: the subtree below only reruns the layout stages named by
its dirty flag; otherwise last frame's layout is reused. Single child.

```c
typedef struct arb_invalidation_data {
    arb_invalidation_flag* flag_always_ptr;      // nullable, checked every frame, never auto-cleared
    arb_invalidation_flag* flag_consumable_ptr;  // nullable, checked once, auto-cleared after firing
} arb_invalidation_data;
```

`arb_invalidation_flag` values (`text`, `width_measure`, `width_distribute`,
`height_measure`, `height_distribute`, `position`, `none`, `all`) are
ordered by pipeline stage — triggering an earlier one cascades through
every later stage.

### `arb_indirect_type`
Points to a separate branch (another `arb_node[]`), letting an array-child
type reach multiple children, or letting a prefab expose a slot for
caller-supplied content. Data is a pointer to that branch's first node, or
`NULL` to skip. `ARB_IDIR(array)` and `ARB_ELEM(...)` are the shortcuts for
named and inlined branches respectively.

### `arb_variable_type`
Stores its data to a cache-local variable for its subtree, propagating
down-tree only. Descendant nodes can then read that value as their own data
source with `arb_flag_variable_data`, the same offset mechanism instancing
and storage use. See ``design_referende/variable``. Single child.

## 2. Rendering Node Types

### `arb_depth_type`
Offsets the subtree's depth index. Decreasing depth means going further
into the screen. Single child.

```c
typedef struct arb_depth_data {
    short depth_change;
} arb_depth_data;
```

### `arb_box_type`
The rectangular render primitive. Single child.

```c
typedef struct arb_box_data {
    arb_color   tint;      // box color
    const char* image;     // image name/path, may be NULL
    float       rounding;  // pixel corner rounding radius
    uint32_t    shader;    // shader effect index
} arb_box_data;
```

### `arb_text_type`
The text render primitive. Single child. Styling (size, font, tint, shader)
is factored into a separate `arb_text_style` struct, referenced by pointer,
so many text nodes can share one style; only the text pointer itself is
expected to vary node to node.

```c
typedef struct arb_text_style {
    unsigned int            size;   // Font size
    const char*             font;   // Font name/path
    arb_color               tint;   // Text color modifier
    uint32_t                shader; // Shader effect index
} arb_text_style;

typedef struct arb_text_data {
    const arb_text_style*   style;  // Text style; must not be NULL
    const char*             text;   // Text pointer
} arb_text_data;
```

## 3. Layout Node Types

### `arb_overlay_type`
Stacks children on top of one another — first child deepest, rendered
first. No data. Array children.

### `arb_padding_type`
Insets its single child by a flexible amount on each side.

```c
typedef struct arb_padding_data {
    arb_length left, right, top, bottom;
} arb_padding_data;
```

`ARB_PADD(max_value)` is the shortcut for uniform, flexing padding on all
four sides.

### `arb_row_type`
Lays children left to right. Array children.

```c
typedef struct arb_row_data {
    float      vertical_align; // 0 top, 0.5 center, 1.0 bottom
    arb_length spacing;        // spacing between children
} arb_row_data;
```

### `arb_column_type`
Lays children top to bottom. Array children.

```c
typedef struct arb_column_data {
    float      horizontal_align; // 0 left, 0.5 center, 1.0 right
    arb_length spacing;          // spacing between children
} arb_column_data;
```

### `arb_sizebox_type`
Overwrites selected width/height fields during layout with fixed values,
regardless of what the child would otherwise measure. Single child.

```c
typedef struct arb_sizebox_data {
    arb_sizebox_overwrite_flag flag;
    arb_length                 width;
    arb_length                 height;
} arb_sizebox_data;
```

`flag` selects which of the six sub-fields (width min/max/flex, height
min/max/flex) are overwritten; `arb_sizebox_overwrite_all_width`,
`arb_sizebox_overwrite_all_height`, and `arb_sizebox_overwrite_all` are
provided as combined masks.

### `arb_aling_type`
Aligns content within the space this node is given, applied in the
position pass. Single child. Intended to be paired with
`arb_flag_ignore_max_width`/`height` on this node so there is spare space
to align within.

```c
typedef struct arb_align_data {
    float vertical_align;   // 0 top, 0.5 center, 1.0 bottom
    float horizontal_align; // 0 left, 0.5 center, 1.0 right
} arb_align_data;
```

## 4. Cursor Node Types

These add input handling to a subtree without writing a new `arb_type`.

### `arb_cursor_handle_type`
Sets the `arb_node_cursor_func` callback that descendant
`arb_cursor_call_type` nodes will invoke. Data is that function pointer.

### `arb_cursor_call_type`
Creates an actual hit-testable input box, which calls back into the
nearest enclosing `arb_cursor_handle_type`'s callback.

## 5. Transform Node Types

The render-side counterpart to the cursor handle/call pair, for adding
transform logic to a subtree without a new `arb_type`.

### `arb_transform_handle_type`
Sets the `arb_node_render_func` callback used by descendant
`arb_transform_call_type` nodes. Data is that function pointer.

### `arb_transform_call_type`
Creates a transform box that invokes the nearest enclosing
`arb_transform_handle_type`'s callback during the render pass.

## 6. Predefined Structures

Complete, ready-to-instance node structures, built from the types above. Use in pattern:
```c
// Set instance to structure data
ARB_INST(&(arb_structure_data){
    ...
}),
// Enter structure
ARB_IDIR(arb_structure)
```

### `arb_button_structure`
Combines storage (pressed/idle state), the cursor system, and instancing
(caller styling and callbacks) into a  clickable box that wraps
arbitrary caller content. Fills all given space (`flex = 1`, `max = inf`).
Styling and callbacks/payload are split into their own `style`/`target`
structs, each referenced by pointer, so many buttons can share one style or
one target independently:

```c
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
```

`on_clicked`/`on_released`/`on_held` are each `void(*)(void* payload)`,
called with `target->payload`. `child` is spliced in via an instanced,
indirected indirect node, so a button can wrap a label, an icon, or any
other branch.

### `arb_vertical_scrollbox_structure` / `arb_horizontal_scrollbox_structure`
Scrollable containers for their respective axes, sharing the same
style/child data shape as the button:

```c
typedef struct arb_scrollbox_style {
    const arb_box_data*             default_style;      // Scrollbox handle style when not touched
    const arb_box_data*             hovered_style;      // Scrollbox handle style when hovered
    const arb_box_data*             pressed_style;      // Scrollbox handle style when pressed
} arb_scrollbox_style;

typedef struct arb_scrollbox_data {
    const arb_scrollbox_style*      style;              // Scrollbox style pointer; Must not be NULL
    const arb_node*                 child;              // Scrollbox scrolled child
} arb_scrollbox_data;
```

The three style fields (inside `style`) describe the scrollbar/track
appearance in its idle, hovered, and pressed states; `child` is the
scrolled content, injected the same way as the button's.

### `arb_float_slider_structure`
A draggable, scroll-adjustable fill-slider bound to a `float` target,
combining storage (drag state and current style), the cursor system (drag
and scroll-wheel handling), and the transform system (scaling the visual
fill to the current value). Fills all given space (`flex = 1`, `max = inf`).

```c
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
```

Dragging (or scrolling, unless `is_scroll_disabled`) moves `*target->target`
between `target->min` and `target->max`; the slider's own visual fill is
scaled to match via the transform system, anchored at the bottom edge when
`is_vertical`, or the left edge otherwise. `child` is injected the same way
as the button's.
