# Arbor UI — Design Reference

This document covers how to create Arbor trees: node anatomy, how arrays of nodes form a tree, sizing rules, the node shortcut macros, instancing, the variable system, the storage system, and the invalidation system.

## 1. The Node

Every piece of UI is an `arb_node`:

```c
typedef struct arb_node {
    const arb_type* type;
    const uint32_t  flags;

    union {
        const void*   data;
        const size_t  data_offset;
    };
} arb_node;
```

- **type** — the behavior of the node (box, text, row, column, ...).
- **flags** — a bitfield that tweaks that behavior (combined with `|`).
- **data** — parameters for the type. This is a union: for most nodes it is a
  plain pointer to a data struct; when `arb_flag_instanced_data` or
  `arb_flag_storaged_data` is set, the *same bits* are instead read as
  `data_offset`, a byte offset into an instance or storage block (see
  *Instancing* and *Storage*).

A node never appears alone — nodes are always written as elements of an
`arb_node` array, and it is the array itself that encodes the tree shape.

## 2. Node Arrays Are Branches

Each `arb_node[]` array is **one branch of the UI tree, written out flat, top
to bottom**. Reading the array top to bottom is the same as walking down the
branch from parent to child.

```c
arb_node branch[] = {
    ARB_NODE(arb_box_type, ..., &box_data),   // Parent
    ARB_PADD(40),                             // Its child
    ARB_NODE(arb_box_type, ..., &box_data),   // That padding's child
    ARB_LAST                                  // Branch ends here
};
```

- **Single-child types** (box, text, padding, sizebox, depth, instance,
  invalidation, ...) always consume the very next array element as their one
  child. The branch keeps flowing downward through the array.
- **Array-child types** (row, column, overlay) do not take "the next
  element" as a single child — they read the following `arb_indirect_type`
  nodes, each one pointing off to a separate branch. Each indirect node
  becomes one container child.

`ARB_LAST` is the sentinel that closes a branch, so the interpreter knows not
to read past the end of the array.

### 2.1 Branching out with indirect nodes (IDIR)

Because a container needs *multiple* children, and an array can only flow
linearly, a container's children must each be their own branch — a separate
array — referenced through an `arb_indirect_type` node. This is what
`ARB_IDIR` produces:

```c
#define ARB_IDIR(argchild) (arb_node){  \
    .type   = &arb_indirect_type,       \
    .data   = (void*)(argchild)         \
}
```

The interpreter jumps into the array pointed to by the indirection node,
treats it as a branch on its own (with its own `ARB_LAST`), and returns once
that branch is fully consumed.

```c
arb_node leaf_branch[] = {
    ARB_NODE(arb_box_type, ..., &green_box),
    ARB_LAST
};

arb_node main_structure[] = {
    ARB_NODE(arb_box_type, ..., &background),
    ARB_PADD(40),
    ARB_NODE(arb_column_type, arb_flag_none, &column_data),
        ARB_IDIR(leaf_branch),  // Child 1: a separate branch
        ARB_IDIR(leaf_branch),  // Child 2: the *same* branch, reused
        ARB_IDIR(footer),       // Child 3: other branch
    ARB_LAST
};
```

A single branch array can be pointed to by more than one `IDIR` — it is just
data, so it is shared and reused freely.

> An indirection node may point to `NULL`, which causes the interpreter not
> to jump in at all. This matters because indirection nodes are also the
> mechanism used to inject icons, labels, or arbitrary caller content into
> prefabs such as buttons.

### 2.2 Inlining a branch with ARB_ELEM

Declaring and naming every small UI branch would be tedious. `ARB_ELEM`
inlines a branch directly at the call site — it creates a real, separate
array (an anonymous `arb_node[]`) and points to it through an indirect node.

```c
#define ARB_ELEM(...) (arb_node){   \
    .type   = &arb_indirect_type,   \
    .data   = (arb_node[]){         \
        __VA_ARGS__                 \
    }                               \
}
```

```c
arb_node main_structure[] = {
    ARB_NODE(arb_column_type, arb_flag_none, &column_data),
    ARB_ELEM(   // Inlined branch
        ARB_NODE(arb_box_type, ..., &box_a),
        ARB_LAST
    ),
    ARB_IDIR(separate_branch), // Mixing inline and named branches freely
    ARB_ELEM(   // Inlined branch
        ARB_NODE(arb_box_type, ..., &box_b),
        ARB_LAST
    ),
    ARB_LAST
};
```

## 3. Sizing

Most nodes has no opinion about its own size beyond what its child
needs — it wraps its children. A bare box with no size-related flags renders
at 0×0, because it wraps nothing.

Four flags alter this behavior:

```
arb_flag_ignore_min_width   // Min width  of this node is set to 0
arb_flag_ignore_min_height  // Min height of this node is set to 0
arb_flag_ignore_max_width   // Max width  of this node is set to inf
arb_flag_ignore_max_height  // Max height of this node is set to inf
```

A box with both `ignore_max_*` flags set grows to fill whatever space its
parent gives it, rather than collapsing to its content.

`ignore_min_*` is mainly useful paired with clipboxes: a clipbox otherwise
wraps its children like anything else, so it never actually clips anything
unless it is allowed to be smaller than its content.

Two further flags apply *after* the `ignore_*` flags above have already run,
and collapse a node's range down to a single value rather than widening it:

```
arb_flag_set_max_to_min_width   // Sets max width  to (post-ignore) min width
arb_flag_set_max_to_min_height  // Sets max height to (post-ignore) min height
```

These can be used to pin size to minimal size. 
Note children to a node with such flag can still have desired max = inf,
leading to them filling entire given space. 
Consider this real-live example:
```c
// Column header measured width [some value, inf]
const arb_node node_header_structure[] = {
    // ...
    // Note arb_flag_ignore_max_width here!
    ARB_NODE(arb_box_type, arb_flag_instanced_data | arb_flag_ignore_max_width, offsetof(node_template_data, header_style)),
    // ...
};

// Column have arb_flag_set_max_to_min_width, so even though node_header_structure 
// wants to grow inf, it will only fill node_input_output_structure spanned width
ARB_NODE(arb_column_type, arb_flag_set_max_to_min_width, &(arb_column_data){
    .spacing = {.min = 8, .max = 8}
}),
ARB_IDIR(node_header_structure),        // Due to having arb_flag_ignore_max_width takes entire column width
ARB_IDIR(node_input_output_structure),  // Actually spans column width, have high min width and finite max
ARB_LAST
```

## 4. Shortcuts

Writing raw `(arb_node){ .type = ..., .data = ..., .flags = ... }` for every
node is tedious, so a small set of macros cover the common cases:

| Macro | Purpose |
|---|---|
| `ARB_NODE(type, flags, ...)` | Ordinary node: type + flags + data |
| `ARB_PADD(max_value)` | Uniform padding on all four sides, flexible |
| `ARB_IDIR(array)` | Indirect node pointing at an existing branch |
| `ARB_ELEM(...)` | Indirect node pointing at an inlined branch |
| `ARB_INST(...)` | Sets the instance pointer for the subtree below |
| `ARB_LAST` | Sentinel marking the end of a branch |

`ARB_NODE` writes through the `data_offset` member of the union (cast from
whatever is passed in), not `data` directly — this is valid because the two
members alias the same bits.

## 5. Pulling Data

Those are implementation functions to read node data and child. 
Read following chapters for explanation - this chapter is for reference, in case of doubt.

```c
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
```

## 6. Instancing

Instancing turns a branch into a reusable **prefab**, parametrized by a data
struct instead of hardcoded values.

- `arb_instance_type` / `ARB_INST(ptr)` sets "the current instance" for
  everything below it in the tree.
- Any node under it can add `arb_flag_instanced_data` and, instead of a
  normal data pointer, supply `offsetof(instance_struct, field)` — meaning
  "the real data lives at this byte offset inside the current enclosing
  instance."

```c
// Definition

typedef struct inventory_slot_data {
    arb_box_data slot_content;
} inventory_slot_data;

// Enter with inventory_slot_data instance
arb_node inventory_slot[] = {
    // This node has static data, unchanged by the instance
    ARB_NODE(arb_box_type, arb_flag_none, &(arb_box_data){
        .tint = ARB_HEX("#909390"), .rounding = 8
    }),
    // Same here
    ARB_PADD(4),
    // Instanced box data - pulled from the current instance structure
    ARB_NODE(arb_box_type, arb_flag_instanced_data | arb_flag_ignore_max_width | arb_flag_ignore_max_height,
        offsetof(inventory_slot_data, slot_content) // Data = offset in structure
    ),
    ARB_LAST
};

// Usage

arb_node a_bigger_ui[] = {
    // Sets current instance
    ARB_INST(&(inventory_slot_data){
        .slot_content = { .tint = ARB_HEX("#FFFFFF"), .rounding = 32 } // Arbitrary params
    }),
    // Enter prefab
    ARB_IDIR(inventory_slot)
};
```

> Instances can nest: an instanced node inside an already-instanced subtree
> can read from parent instance struct.   
> This can be used to nest prefabs in 
> prefabs, eg. a row of slots can offer each
> slot its own `inventory_slot_data` out of a larger `inventory_row_data`.

Because `arb_indirect_type` can also be instanced, a prefab can even expose
a "slot" for the caller's own children:

```c
typedef struct scrollbox_data {
    arb_node* scrolled_child;
} scrollbox_data;

// Data indirected, as scrolled_child is a pointer, not the value (arb_nodes array) itself; More on that later
ARB_NODE(arb_indirect_type, arb_flag_instanced_data | arb_flag_indirected_data, offsetof(scrollbox_data, scrolled_child))
```

## 8. Storage

Instancing supplies read-only parameters from the caller. **Storage** is the
other half: it gives a subtree a private block of *mutable, cache-owned*
memory that persists frame to frame — the mechanism needed for state such as
"is this button currently pressed".

- `arb_storage_type` allocates a block of `data` bytes (the size is passed
  as node data, e.g. `sizeof(button_storage)`).
- Any node under it can add `arb_flag_storaged_data` and supply
  `offsetof(storage_struct, field)` instead of a normal data pointer — the
  same offset mechanism as instancing, but reading from *storage* instead of
  the *instance*.
- Cursor/layout/render callbacks all receive `storage_data` directly, so
  application code (e.g. a cursor handler) can read and mutate it in place
  without going through the offset mechanism at all.

The allocated memory block persists across frames. When first allocated,
the block is zero-initialized.

## 9. Variable

Variable is a mechanism similar to instance:
* ``arb_variable_type`` sets subtree variable for own data
* ``arb_flag_variable_data`` enables read from variable pointer, just like instance

There is a great difference between them though: Nodes are identified as (node, instance) - therefore, for example storage node at same address, with same instance, will return same memory block, despite variable being different.

Variable shall be used locally, to implement more complex memory accesses.
Consider example where our instance is:
```c
typedef struct parent_instance {
    const child_instance* instance;
    const arb_node*       child;
} parent_instance;
```
And we want to enter ``parent_instance->child`` with given ``parent_instance->instance``. Changing instance to child instance, disqualifies us from reading child pointer - variable can help here.

```c
// Instance is parent_instance
// Store instance + 0 in variable
ARB_NODE(arb_variable_type, arb_flag_instanced_data, 0), 
 // Enter instance->instance pointer
ARB_NODE(arb_instance_type, arb_flag_instanced_data | arb_flag_indirected_data, offsetof(parent_instance, instance)),
// Read from overwritten parent instance via variable
ARB_NODE(arb_indirect_type, arb_flag_variable_data | arb_flag_indirected_data, offsetof(parent_instance, child))
```

## 10. Indirection

As You might have already seen ``arb_flag_indirected_data`` is used to make intepreter walk one more pointer, allowing us to store pointers to structures in our instances. This is usefull when we want to keep state outside instance, eg. we share same ``background style`` across multiple buttons.

```c
typedef struct an_instance {
    arb_box_data  local_style;  // Access without indirected; Local to instance
    arb_box_data* extern_style; // Access with indirection; Extern to instance
} an_instance;
```

## 11. Invalidation

Layout is not free, and most of a tree does not change from one frame to
the next. `arb_invalidation_type` is a single-childed gate placed above a
subtree: the subtree's layout is only rebuilt when the invalidation node is
marked with a relevant dirty flag; otherwise the previous frame's layout
results for that subtree are reused unchanged.

```c
typedef struct arb_invalidation_data {
    arb_invalidation_flag* flag_always_ptr;         // Nullable
    arb_invalidation_flag* flag_consumable_ptr;     // Nullable
} arb_invalidation_data;

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
```

Both pointers are nullable and serve different purposes:

- `flag_always_ptr` points to a flag that is checked every frame but never
  cleared by the engine. It is meant for subtrees whose relayout condition
  is itself persistent state the application manages elsewhere.
- `flag_consumable_ptr` points to a flag that is checked once, and if it
  triggers a pass, is reset to `arb_invalidation_flag_none` automatically
  afterward. It is meant for one-shot triggers: "the text changed, relayout
  once," without the application needing to remember to clear the flag.

The flag values are ordered by pipeline stage. Enabling an earlier stage
also causes every later stage to run: setting `arb_invalidation_flag_text`
implies text layout, which invalidates measured width, which cascades
through width distribution, height measurement, height distribution, and
position, since each later stage depends on the outputs of the one before
it. `arb_invalidation_flag_all` runs every stage; `arb_invalidation_flag_none`
runs none.

## 12. Worked Example: Button

The button prefab combines instancing (the caller's styling and callbacks)
with storage (the button's own pressed/idle state) and the cursor system.

**Public data — what a caller instances the structure with.** The style
(shared, likely-`const` styling data) and target (per-instance callbacks and
payload) are split into their own structs, both referenced by pointer, so
that many buttons can share one style without duplicating it:

```c
typedef struct arb_button_style {
    const arb_box_data* default_style;   // Button style when not touched
    const arb_box_data* hovered_style;   // Button style when hovered
    const arb_box_data* pressed_style;   // Button style when pressed
} arb_button_style;

typedef struct arb_button_target {
    arb_button_func on_clicked;   // On button first pressed
    arb_button_func on_released;  // On button release frame
    arb_button_func on_held;      // Every frame, while pressed
    void*           payload;      // Pointer passed to arb_button_funcs
} arb_button_target;

typedef struct arb_button_data {
    const arb_button_style*  style;   // Must not be NULL
    const arb_button_target* target;  // Must not be NULL
    const arb_node*          child;   // Button child, overlay on button
} arb_button_data;
```

**Private state — what the button keeps for itself, invisible to the caller:**

```c
typedef struct button_storage {
    int          pressed;
    arb_box_data current;
} button_storage;
```

**The cursor handler** reads and writes storage directly, and reads the
instance data as an ordinary pointer (it is handed `node_data` and
`storage_data` already resolved — no offset math is needed at this level).
It also receives this node's resolved `layout` and the screen resolution,
mirroring `transform`, even though this particular handler has no need of
either:

```c
static void button_cursor_func(
    const void* node_data, void* storage_data, arb_node_cursor_input* node_input,
    const arb_node_layout_state* layout, int resolution_x, int resolution_y
) {
    const arb_button_data*   data   = node_data;
    const arb_button_style*  style  = data->style;
    const arb_button_target* target = data->target;
    button_storage*          stor   = storage_data;
    // ... reads style->pressed_style / target->on_clicked etc,
    //     writes stor->pressed / stor->current
}
```

**The structure itself:**

```c
const arb_node arb_button_structure[] = {
    ARB_NODE(arb_storage_type,       arb_flag_none, sizeof(button_storage)),
    ARB_NODE(arb_cursor_handle_type, arb_flag_none, button_cursor_func),
    ARB_NODE(arb_cursor_call_type,   arb_flag_instanced_data, 0),
    ARB_NODE(arb_box_type, arb_flag_storaged_data | arb_flag_ignore_max_width | arb_flag_ignore_max_height,
        offsetof(button_storage, current)),
    ARB_NODE(arb_indirect_type, arb_flag_instanced_data | arb_flag_indirected_data, offsetof(arb_button_data, child))
};
```

Reading it as a branch, top to bottom:

1. `arb_storage_type` opens `sizeof(button_storage)` bytes of persistent
   memory for everything below it — this is where `pressed` and `current`
   live from now on.
2. `arb_cursor_handle_type` registers `button_cursor_func` as the callback
   that fires for cursor input anywhere in this subtree.
3. `arb_cursor_call_type` is the actual hit-testable input node. Its data is
   `arb_flag_instanced_data` with offset `0`, which causes the `data`
   pointer passed to the cursor callback to be the instance struct itself
   (instance pointer + 0 = instance pointer).
4. The **box** is the visible part of the button. It uses
   `arb_flag_storaged_data` to pull `current` out of storage — so its color
   is whatever the cursor handler last decided (idle/hovered/pressed style),
   not something set once by the caller.
5. Finally, an `arb_indirect_type` reads `offsetof(arb_button_data, child)`
   out of the instance — this is the child-injection pattern described in
   *Instancing*. Because `data->child` is itself a *pointer* to a branch
   (not an inline branch), this node also carries
   `arb_flag_indirected_data`, so `get_node_data` walks one further pointer
   after the instance offset, landing on the caller's actual branch. This
   lets a button wrap arbitrary caller content (a label, an icon, ...).

This example demonstrates most of Arbor's mechanisms working together.
