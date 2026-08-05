# Arbor UI — Design Reference

This document covers how to create Arbor trees: node anatomy, how arrays of
nodes form a tree, sizing rules, the node shortcut macros, instancing, the
storage system, and the invalidation system.

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

Whether the *next element in the array* is treated as "the single child" or
"a signpost to somewhere else" depends entirely on the current node's type:

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

By default a node has no opinion about its own size beyond what its child
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

## 4. Shortcuts

Writing raw `(arb_node){ .type = ..., .data = ..., .flags = ... }` for every
node is tedious, so a small set of macros cover the common cases:

| Macro | Purpose |
|---|---|
| `ARB_NODE(type, flags, ...)` | Ordinary node: type + flags + data |
| `ARB_PADD(max_value)` | Uniform padding on all four sides, flexible |
| `ARB_SINGLE(type, flags, ...)` | A one-node branch, pre-terminated with `ARB_LAST` |
| `ARB_IDIR(array)` | Indirect node pointing at an existing branch |
| `ARB_ELEM(...)` | Indirect node pointing at an inlined branch |
| `ARB_INST(...)` | Sets the instance pointer for the subtree below |
| `ARB_LAST` | Sentinel marking the end of a branch |

`ARB_NODE` writes through the `data_offset` member of the union (cast from
whatever is passed in), not `data` directly — this is valid because the two
members alias the same bits.

## 5. Instancing

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

arb_node inventory_slot[] = {
    // This node has static data, unchanged by the instance
    ARB_NODE(arb_box_type, arb_flag_ignore_max_width | arb_flag_ignore_max_height, &(arb_box_data){
        .tint = ARB_HEX("#909390"), .rounding = 8
    }),
    // Same here
    ARB_PADD(4),
    // Instanced box data - pulled from the current instance structure
    ARB_NODE(arb_box_type, arb_flag_ignore_max_width | arb_flag_ignore_max_height | arb_flag_instanced_data,
        offsetof(inventory_slot_data, slot_content) // Data = offset in structure
    ),
    ARB_LAST
};

// Usage

arb_node a_bigger_ui[] = {
    // Sets current instance
    ARB_INST(&inventory_slot_data{
        .content = { whatever }
    }),
    // Enter prefab
    ARB_IDIR(inventory_slot)
};
```

> Instances can nest: an instanced node inside an already-instanced subtree
> reads from *its own* instance struct, so a row of slots can offer each
> slot its own `inventory_slot_data` out of a larger `inventory_row_data`.

Because `arb_indirect_type` can also be instanced, a prefab can even expose
a "slot" for the caller's own children:

```c
typedef struct scrollbox_data {
    arb_node* scrolled_child;
} scrollbox_data;

ARB_NODE(arb_indirect_type, arb_flag_instanced_data, offsetof(scrollbox_data, scrolled_child))
```

> This is a special case: normally, instancing causes a branch read inside
> the instance structure directly; the Arbor implementation adds an
> additional indirection in this case, allowing a pointer stored in the
> structure to be followed instead.

## 6. Storage

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

## 7. Invalidation

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

## 8. Worked Example: Button

The button prefab combines instancing (the caller's styling and callbacks)
with storage (the button's own pressed/idle state) and the cursor system.

**Public data — what a caller instances the structure with:**

```c
typedef struct arb_button_data {
    void*           payload;
    arb_button_func on_clicked;
    arb_button_func on_released;
    arb_button_func on_held;
    arb_box_data    default_style;
    arb_box_data    hovered_style;
    arb_box_data    pressed_style;
    const arb_node* child;
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
`storage_data` already resolved — no offset math is needed at this level):

```c
static void button_cursor_func(void* node_data, void* storage_data, arb_node_cursor_input* node_input) {
    arb_button_data* data = node_data;
    button_storage*  stor = storage_data;
    // ... reads data->on_clicked / data->pressed_style etc,
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
    ARB_NODE(arb_indirect_type, arb_flag_instanced_data, offsetof(arb_button_data, child))
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
5. Finally, an `arb_indirect_type` with `arb_flag_instanced_data` reads
   `offsetof(arb_button_data, child)` — this is the child-injection pattern
   described in *Instancing*: the caller's `data->child` branch is spliced
   in as this node's child, letting a button wrap arbitrary caller content
   (a label, an icon, ...).

This example demonstrates most of Arbor's mechanisms working together.

## 9. Good Practices

A larger example ties these mechanisms together in a way worth analyzing
directly:

```c
arb_box_data bg_style = { .tint = ARB_HEX("#373030") };
arb_box_data fg_style = { .tint = ARB_HEX("#5e5050"), .rounding = 16 };

arb_text_data tx_data = {
    .size = 24, .font = "Roboto", .tint = ARB_HEX("#b7b0b0"), .text = "Select Text",
};

arb_invalidation_flag text_invalidation_flag = arb_invalidation_flag_all;

void button_set_text_func(void* payload) {
    tx_data.text = (const char*)payload;
    text_invalidation_flag = arb_invalidation_flag_text; // Regenerate text
}
```

Several habits are worth naming explicitly:

- **Trees are templates, not state.** The `arb_node` arrays themselves can stay `const` and ``static`` —
  they describe structure, not content. Anything that actually needs to
  change frame to frame (`tx_data.text` above) lives in ordinary externally
  declared memory the tree points to, not inside the tree itself.
- **Pair mutable state with an invalidation flag.** `tx_data` and
  `text_invalidation_flag` are declared together and updated together:
  `button_set_text_func` mutates the data first, then sets the consumable
  flag so the cache knows to redo exactly the affected pass on the next
  update. Mutating data that a tree points to is only meaningful once the
  corresponding invalidation flag is raised.
- **Share style data by pointer.** `bg_style` and `fg_style` are each
  declared once and pointed to from wherever that style is needed — since
  node data is just a pointer, nothing prevents many unrelated nodes from
  reading the same style struct.
- **Decouple callbacks from data with a generic payload.** A single
  `button_set_text_func` serves every button in the menu; each instance
  only supplies a different `payload` pointer. This avoids writing one
  callback per button for what is structurally the same action.
