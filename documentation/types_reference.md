# Arbor UI — Types Reference

This document explains how to implement a custom ``arb_type``.
Also the layout pipeline of Arbor, because it is required to do so.

## 1. What a Type Is

A type is just a struct of function pointers:

```c
typedef struct arb_type {
    int array_child;

    arb_node_layout_func    width_measure;
    arb_node_layout_func    width_distribute;
    arb_node_layout_func    height_measure;
    arb_node_layout_func    height_distribute;
    arb_node_layout_func    position;

    arb_node_render_func    transform;
    arb_node_cursor_func    cursor;
} arb_type;
```

Any function not needed can be left `NULL` — the engine simply skips
calling it. A purely visual, non-interactive box, for instance, has no
reason to define `cursor`.

`array_child` is the one non-function field, and it's what decides how the
node's children are found while walking the tree (see the design reference,
section 2): `0` means "the very next array element is the single child", `1`
means "a sequence of indirect nodes following the node are its children".

## 2. The Five Layout Stages

Layout happens in five passes over the tree, always in this order:

| Stage | Direction | Reads | Writes |
|---|---|---|---|
| `width_measure` | bottom-up | children's measured widths | own measured width |
| `width_distribute` | top-down | own measured width, own given width | children's given widths |
| `height_measure` | bottom-up | children's measured heights (widths already given) | own measured height |
| `height_distribute` | top-down | own measured height, own given height | children's given heights |
| `position` | top-down | all widths/heights, given | children's offsets |

The split between width and height passes exists because a lot of content
(text being the obvious case) has a height that depends on the width it's
given — so all widths are settled across the whole tree before any height is
measured.

Each callback receives:

```c
typedef void(arb_node_layout_func_signature)(
    const void*             node_data,
    void*                   storage_data,
    arb_node_layout_state*  node_state,
    size_t                  children_count,
    arb_node_layout_state** children_states
);
```

- `node_data` — the node's data, already resolved (instance/storage offset
  math has already happened before this function is called; a plain pointer
  to the relevant struct is provided directly).
- `storage_data` — pointer to whatever storage block is open for this
  subtree, or `NULL` if none is. (This is always the case, and has nothing
  to do with `arb_flag_storaged_data`.)
- `node_state` — the node's own `arb_node_layout_state`, described below.
  Measure stages write into it; distribute/position stages both read from
  it (`given_width`/`given_height`, set by the parent).
- `children_count` / `children_states` — for single-child types this is `0`
  or `1`; for array-child types it's however many indirect children were
  found, in order.

```c
typedef struct arb_node_layout_state {
    arb_length measured_width;   // Desired width  (min/max/flex)
    arb_length measured_height;  // Desired height (min/max/flex)
    int        given_width;      // Width actually granted by parent
    int        given_height;     // Height actually granted by parent
    int        hori_offset;      // This node's center, relative to parent's center
    int        vert_offset;      // This node's center, relative to parent's center
} arb_node_layout_state;
```

### 2.1 Width measure pass (bottom-up)

A measure function's task is to combine the node's children width
measurements (`children_states[i]->measured_width`), in regard to this
type's requirements, and write `node_state->measured_width`. This is where
`arb_length{min, max, flex}` gets built — `min`/`max` bound how small/large
the node is willing to go, `flex` says how eagerly extra space should be
taken if it's offered later.

### 2.2 Width distribute pass (top-down)

By the time `width_distribute` runs, the node's `node_state->given_width`
has already been set by its parent's distribute pass. The node type's task
is to distribute this width between children, by writing
`children_states[i]->given_width`.

For a single child this is often trivial (give it everything, or everything
minus some fixed amount). For an array-child type distributing space among
several flexible children, this is where a flex-sharing algorithm would be
implemented — proportionally handing out any width beyond each child's
minimum according to their `flex`, without exceeding any child's `max`.

### 2.3 Height measure pass (bottom-up)

Same as the width measure pass. Note all widths are already assigned, so
they can be included in calculations (for example, calculating text height
given a width constraint).

### 2.4 Height distribute pass (top-down)

Same as the width distribute pass, just for height.

### 2.5 Position pass (top-down)

By now every node in the tree has a final `given_width`/`given_height`. This
pass only sets `hori_offset`/`vert_offset` — how far each child's center lies
from this node's center. A node with a single child that fills all given
space usually just leaves the offset at `0`. A node that aligns or spaces
multiple children (rows, columns, overlays) computes each child's offset
here.

## 3. Predefined Functions and ARB_TYPE_OVERLAY_INIT

Most single- and array-child types only need to customize *one* axis of
behavior and are otherwise happy to behave like a plain overlay on the
other four stages — a row, for instance, only really has something new to
say about `width_measure`, `width_distribute` and `position`; 
it's `height_measure` and `height_distribute` are identical to what an overlay would already do (wrap to the largest child, then hand every child the full given size).

A set of predefined functions capture that common overlay behavior so it
doesn't need to be reimplemented per type:

```c
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
```

And a shortcut macro to pull all four non-positioning ones in at once:

```c
#define ARB_TYPE_OVERLAY_INIT \
    .width_measure      = arb_overlay_width_measure_func,       \
    .width_distribute   = arb_overlay_width_distribute_func,    \
    .height_measure     = arb_overlay_height_measure_func,      \
    .height_distribute  = arb_overlay_height_distribute_func
```

## 4. Rendering: `transform`

```c
typedef void(arb_node_render_func_signature)(
    const void*                     node_data,
    void*                           storage_data,
    arb_mat3x2*                     transform,
    const arb_node_layout_state*    layout,
    int                             resolution_x,
    int                             resolution_y
);
```

This runs top-down, once layout is fully resolved. `transform` arrives as
the transform inherited from the parent; it can be mutated in place (scale,
offset, etc.) before it's handed further down to the node's children.
`layout` is this node's own, already-resolved `arb_node_layout_state`,
provided so a render function can read final sizes/offsets (e.g. to scale a
fill relative to `given_width`/`given_height`) without redoing layout work.
`resolution_x`/`resolution_y` are the current screen resolution, needed to
convert a pixel-space offset into the clip-space `transform` operates in.

This is the intended way of creating animations, as layout passes are
supposed to be blocked from updating too often by the invalidation system —
rendering happens every frame.

## 5. Input: `cursor`

```c
typedef void(arb_node_cursor_func_signature)(
    const void*                     node_data,
    void*                           storage_data,
    arb_node_cursor_input*          node_input,
    const arb_node_layout_state*    layout,
    int                             resolution_x,
    int                             resolution_y
);
```

Setting the cursor func causes the node to push an input box every frame,
just as `arb_cursor_call_type` would. As with `transform`, `layout` and
`resolution_x`/`resolution_y` give the callback this node's resolved layout
state and the current screen resolution, e.g. to convert a raw cursor pixel
position into a fraction of the node's own width/height (as a slider drag
would need).

## 6. Examples

For example, search for row/column or padding in the Arbor implementation.
