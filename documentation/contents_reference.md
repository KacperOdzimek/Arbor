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
The text render primitive. Single child.

```c
typedef struct arb_text_data {
    unsigned int size;   // font size
    const char*  font;   // font name/path
    const char*  text;   // text pointer
    arb_color    tint;   // text color modifier
    uint32_t     shader; // shader effect index
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

```c
typedef struct arb_button_data {
    void*               payload;
    arb_button_func     on_clicked;
    arb_button_func     on_released;
    arb_button_func     on_held;
    const arb_box_data* default_style;
    const arb_box_data* hovered_style;
    const arb_box_data* pressed_style;
    const arb_node*     child;
} arb_button_data;
```

`on_clicked`/`on_released`/`on_held` are each `void(*)(void* payload)`,
called with `data->payload`. `child` is spliced in via an instanced
indirect node, so a button can wrap a label, an icon, or any other branch.

### `arb_vertical_scrollbox_structure` / `arb_horizontal_scrollbox_structure`
Scrollable containers for their respective axes, sharing the same data
shape as the button (styled states plus an injected child):

```c
typedef struct arb_scrollbox_data {
    const arb_box_data* default_style;
    const arb_box_data* hovered_style;
    const arb_box_data* pressed_style;
    const arb_node*     child;
} arb_scrollbox_data;
```

The three style fields describe the scrollbar/track appearance in its
idle, hovered, and pressed states; `child` is the scrolled content,
injected the same way as the button's.
