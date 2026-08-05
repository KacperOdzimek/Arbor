# Arbor UI — Design Reference

This document covers how to create Arbor trees; Node anatomy, how arrays of
nodes form a tree, sizing rules, the node shortcut macros, instancing, and the
storage system.

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

- **type** — which behavior this node has (box, text, row, column, ...).
- **flags** — a bitfield that tweaks that behavior (combine with `|`).
- **data** — parameters for the type. This is a union: for most nodes it's a
  plain pointer to a data struct; when `arb_flag_instanced_data` or
  `arb_flag_storaged_data` is set, the *same bits* are instead read as
  `data_offset`, a byte offset into an instance or storage block (see *Instancing* and *Storage*).

A node never appears alone — nodes are always written as elements of an
`arb_node` array, and it's the array itself that encodes the tree shape.

## 2. Node Arrays Are Branches

Think of each `arb_node[]` array as a **one branch of the UI tree, written out flat, top to bottom**. Reading the array top to
bottom is the same as walking down the branch from parent to child.

```c
arb_node branch[] = {
    ARB_NODE(arb_box_type, ..., &box_data),   // Parent
    ARB_PADD(40),                             // Its child
    ARB_NODE(arb_box_type, ..., &box_data),   // That padding's child
    ARB_LAST                                  // Branch ends here
};
```

Whether the *next element in the array* is treated as "my single child" or
"a signpost to somewhere else" depends entirely on the current node's type:

- **Single-child types** (box, text, padding, sizebox, depth, instance,
  invalidation, ...) always consume the very next array element as their one
  child. The branch just keeps flowing downward through the array.
- **Array-child types** (row, column, overlay) don't take "the next element"
  as a single child — they read following `arb_indirect_type` nodes, each
  one pointing off to a separate branch. Each indirect node becomes one
  container child.

`ARB_LAST` is the sentinel that closes a branch, so the interpreter knows not
to read past the end of the array.

### 2.1 Branching out with indirect nodes (IDIR)

Because a container needs *multiple* children, and an
array can only flow linearly, a container's children must each be their own
branch — a separate array — referenced through an `arb_indirect_type` node.
This is what `ARB_IDIR` produces:

```c
#define ARB_IDIR(argchild) (arb_node){  \
    .type   = &arb_indirect_type,       \
    .data   = (void*)(argchild)         \
}
```

The interpreter jumps into array pointed by indirection node, treats it as a branch on its own (with its own `ARB_LAST`), and comes back once that branch is fully
consumed.

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

A single branch array can be pointed to by more than one `IDIR` — it's just
data, so it's shared/reused freely.

> Indirection node may point to NULL - this cause the interpreter to not jump in. It is important, because soon, we will use indireciton nodes to inject icons/labels to structures like buttons etc.

### 2.2 Inlining a branch with ARB_ELEM

Declaring and naming each UI branch would be tedious. `ARB_ELEM` inlines a branch directly at the call site — it creates a real, separate array (an anonymous `arb_node[]`) and points to it through an indirect node.

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
    ARB_ELEM(   // inlined branch
        ARB_NODE(arb_box_type, ..., &box_a), 
        ARB_LAST
    ),
    ARB_IDIR(separate_branch), // mix inline and named branches freely
    ARB_ELEM(   // inlined branch
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

Four flags allow altering this behavior:

```
arb_flag_ignore_min_width   // Min width  of this node is set to 0
arb_flag_ignore_min_height  // Min height of this node is set to 0
arb_flag_ignore_max_width   // Max width  of this node is set to inf
arb_flag_ignore_max_height  // Max height of this node is set to inf
```

A box with both `ignore_max_*` flags set will grow to fill whatever space its
parent gives it, rather than collapsing to its content.

`ignore_min_*` is mainly useful paired with clipboxes: a clipbox otherwise wraps its children like anything else, so it never actually clips anything, unless it's allowed to be smaller than its content.

## 4. Shortcuts

Writing raw `(arb_node){ .type = ..., .data = ..., .flags = ... }` for every
node is tedious, so a small set of macros cover the common cases:

| Macro | Purpose |
|---|---|
| `ARB_NODE(type, flags, ...)` | Ordinary node: type + flags + data |
| `ARB_PADD(max_value)` | Uniform padding on all four sides, flexible |
| `ARB_SINGLE(type, flags, ...)` | A one-node branch, pre-terminated with `ARB_LAST` |
| `ARB_IDIR(array)` | Indirect node pointing at an existing branch |
| `ARB_ELEM(...)` | Indirect node pointing at inlined branch |
| `ARB_INST(...)` | Sets the instance pointer for the subtree below |
| `ARB_LAST` | Sentinel marking the end of a branch |

Note that `ARB_NODE` writes through the `data_offset` member of the union
(cast from whatever user pass in), not `data` directly — this is fine because
the two members alias the same bits.

## 5. Instancing

Instancing turns a branch into a reusable **prefab**, parametrized by a data
struct instead of hardcoded values.

- `arb_instance_type` / `ARB_INST(ptr)` sets "the current instance" for
  everything below it in the tree.
- Any node under it can add `arb_flag_instanced_data` and, instead of a
  normal data pointer, give an `offsetof(instance_struct, field)` — meaning "my data lives at this byte offset inside the current enclosing instance".

```c
// Definition

typedef struct inventory_slot_data {
    arb_box_data slot_content;
} inventory_slot_data;

arb_node inventory_slot[] = {
    // This node have static data, unchanged by instance
    ARB_NODE(arb_box_type, arb_flag_ignore_max_width | arb_flag_ignore_max_height, &(arb_box_data){
        .tint = ARB_HEX("#909390"), .rounding = 8
    }),
    // Same here
    ARB_PADD(4),
    // Instanced box data - pulled from current instance structure
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

> Note: Instances can nest: an instanced node inside an already-instanced subtree reads from *its own* instance struct, so a row of slots can offer each slot its own `inventory_slot_data` out of a larger `inventory_row_data`.

Because `arb_indirect_type` can also be instanced, a prefab can even expose
a "slot" for the caller's own children:

```c
typedef struct scrollbox_data {
    arb_node* scrolled_child;
} scrollbox_data;

ARB_NODE(arb_indirect_type, arb_flag_instanced_data, offsetof(scrollbox_data, scrolled_child))
```

> This is in fact a special case - normaly instancing would cause a branch read inside the instance structure - arbor implementation add additional indirection it this case, allowing pointer in structure.

## 6. Storage

Instancing supplies read-only parameters from the caller. **Storage** is the
other half: it gives a subtree a private block of *mutable, cache-owned*
memory that persists frame to frame — the thing user needs for state like
"is this button currently pressed".

- `arb_storage_type` allocates a block of `data` bytes (user passes the size, e.g. `sizeof(button_storage)` as storage node data)
- Any node under it can add `arb_flag_storaged_data` and give
  `offsetof(storage_struct, field)` instead of a normal data pointer — same
  offset trick as instancing, but reading from *storage* instead of the
  *instance*.
- Cursor/layout/render callbacks all receive `storage_data` directly, so
  application code (e.g. a cursor handler) can read and mutate it in place
  without going through the offset mechanism at all.

The allocated memory block is persistent through the frames. When allocated the structure is 0-initialized.

## 7. Worked Example: Button

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

**The cursor handler** reads/writes storage directly and reads the instance
data as an ordinary pointer (it's handed `node_data` and `storage_data`
already resolved — no offset math needed at this level):

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
3. `arb_cursor_call_type` is the actual hit-testable input node. Note its
   data is `arb_flag_instanced_data` with offset `0` - this cause ``data`` pointer passed to cursor callback to be instance struct itself (instance ptr + 0 = instance ptr)
4. The **box** is the visible part of the button. It uses
   `arb_flag_storaged_data` to pull `current` out of storage — so its color
   is whatever the cursor handler last decided (idle/hovered/pressed style),
   not something set once by the caller.
5. Finally, an `arb_indirect_type` with `arb_flag_instanced_data` reads
   `offsetof(arb_button_data, child)` — this is the child injection patter, described in ``instancing`` paragraph: The caller's `data->child` branch is spliced in as this node's child,
   letting a button wrap arbitrary caller content (a label, an icon, ...).

This example show most of Arbor mechanisms.
