# Arbor UI — Integration Reference

This document specifies how a host application uses Arbor system and
consumes its output.

## 1. Transforms

UI element placement is described using the `arb_mat3x2` type: a 3x3
homogeneous transform matrix with its lower row omitted, since it is always
`[0, 0, 1]`. The type is laid out to match the GLSL `mat3x2` ABI and can be
uploaded to the GPU directly, without conversion.

The identity matrix:

```
1 0 0
0 1 0
[cut]
```

represents a box spanning the entire screen. All other UI element transforms
are derived from this root transform through a sequence of translation and
scale operations.

The vertical axis is Y+: increasing the vertical offset (matrix column 3, row 2) moves content upward.

## 2. Cache

Node layout and measurement state is stored inside an `arb_cache`:

```c
arb_cache* arb_create_cache();
void arb_free_cache(arb_cache*);
```

The cache is advanced with:

```c
arb_upload_access arb_cache_update(
    arb_cache*          cache,
    const arb_node*     root,
    int                 resolution_x,
    int                 resolution_y,
    arb_cursor_state    cursor_state,
    float               delta_time
);
```

`root` is the top node of the UI tree for this cache.

## 3. Multithreading & Synchronization

`arb_cache` owns all mutable UI state: layout results, text GPU handles,
storage blocks, and invalidation flags all live inside the cache instance.

- A single `arb_cache` is not internally synchronized. Concurrent calls to
  `arb_cache_update` on the same cache, or concurrent access to a
  previously returned `arb_upload_access` from another thread, is undefined
  behavior. The host must serialize all access to a given cache — for
  example, by confining a cache to a single thread, or by guarding it with
  an external lock.
- Independent `arb_cache` instances do not share state and can be updated
  concurrently on separate threads without synchronization against each
  other.
- Arbor is only internally hermetic: it guarantees consistency of the state
  it owns (the cache), not of anything reached through it. Instance
  pointers, storage callbacks, and cursor/button callbacks all execute
  synchronously during `arb_cache_update`, but if any of them read or write
  application-owned state (for example, a payload pointed to by
  `arb_button_data`, or a global counter incremented by `on_clicked`), the
  host is responsible for synchronizing that state itself. Arbor's
  guarantees stop at the boundary of the cache; anything a callback touches
  outside it is the host's concern.

## 4. Upload Access

```c
typedef struct arb_upload_access {
    uint32_t                        resolution_x;
    uint32_t                        resolution_y;

    size_t                          text_free_count;
    const arb_text_free_request*    text_free_requests;

    size_t                          text_alloc_count;
    const arb_text_alloc_request*   text_alloc_requests;

    size_t                          clipboxes_count;
    const arb_clipbox_request*      clipboxes_requests;

    size_t                          draws_count;
    const arb_draw_request*         draws_requests;
} arb_upload_access;
```

The `arb_upload_access` returned from ``arb_cache_update`` contains pointers into render lists owned by the cache. 
These pointers must not be freed by the caller, but may be read. 
They remain valid only until the next call to `arb_cache_update` on the same cache — after that call, all previously returned pointers are invalidated and must not be dereferenced.

Each `arb_cache_update` call is expected to be followed by, in order:

1. Free GPU resources for `text_free_requests`.
2. Allocate GPU resources for and upload `text_alloc_requests`.
3. Upload `clipboxes_requests` and `draws_requests`.
4. Issue rendering.

Arbor does not mandate a specific renderer architecture beyond this
ordering. Section 9 documents valid implementation approach.

## 5. Text Layout

```c
void arb_injection_text_layout(
    const arb_text_data*    text_data,          // Text data to layout
    int                     width_constrain,    // Given width, 0 == unlimited width
    size_t*                 out_count,          // Out count of glyphs
    void**                  out_glyphs,         // Glyphs malloc'ated array, shall be NULL if count == 0
    int*                    out_width,          // Out pixel width of text box
    int*                    out_height          // Out pixel height of text box
);
```

This injection function must be implemented by the host application. It must
return a `malloc`-allocated array of laid-out glyphs, in an
application-defined structure suited to the host's rendering system. 

An example of glyph structure:

```c
typedef struct gpu_glyph {
    uv_2d   atlas_position;     // Font texture region
    float   off_x,  off_y;      // Top left offset
    float   size_x, size_y;     // Pixel size
} gpu_glyph;
```

Glyphs must be defined in pixel space. This is required so that already
laid-out, unwrapped text remains valid across a window resize.

## 6. Text GPU Allocation

Storage of per-text-node GPU buffer handles is the responsibility of
`arb_cache`. Following a cache update, the host may receive
`arb_text_alloc_request` entries:

```c
typedef struct arb_text_alloc_request {
    void**  text_pointer_out;
    size_t  glyphs_count;
    void*   glyphs;
} arb_text_alloc_request;
```

`glyphs` is the array previously returned by `arb_injection_text_layout`,
and is therefore in the application-defined glyph format. `text_pointer_out`
points at a `void*` slot owned by the cache, reserved to hold the
renderer-side handle for this text allocation.

A minimal handling loop:

```c
for req in access.text_alloc_requests:
    *req.text_pointer_out = gpu_create_buffer(req.glyphs_count * sizeof(glyph))
    gpu_upload(*req.text_pointer_out, req.glyphs)
```

An implementation may instead subpartition a single shared glyph buffer:

```c
for req in access.text_alloc_requests:
    *req.text_pointer_out = find_suitable_partition(buffer, req.glyphs_count)
    gpu_upload_partition(buffer, *req.text_pointer_out, req.glyphs)
```

## 7. Text GPU Free

When a text allocation becomes unused and the cache's garbage collector
reclaims it, the host receives an `arb_text_free_request`:

```c
typedef struct arb_text_free_request {
    void*   text_pointer;
} arb_text_free_request;
```

`text_pointer` is the same value the host previously wrote when handling the
corresponding `arb_text_alloc_request`. The host is responsible for
releasing the associated buffer or partition:

```c
for req in access.text_free_requests:
    gpu_free_buffer(req.text_pointer);
    // or
    free_partition(req.text_pointer);
```

## 8. Clipboxes

```c
typedef struct arb_clipbox_request {
    arb_mat3x2  transform;  // Clipbox transform
} arb_clipbox_request;
```

Clipboxes may be stored in a single SSBO. Draw requests reference clipboxes by index  `[0, clipboxes_count)`.

## 9. Draw Items

```c
typedef struct arb_draw_request {
    arb_mat3x2  transform;          // Drawn box transform
    int         clip_index;         // Clip index from clipboxes requests, -1 means no clip
    short       depth_index;        // Depth index
    char        is_box_not_text;    // Whether to read union.box or union.text
    union {
        struct {
            arb_box_data    data;   // Rendered box data
        } box;
        struct {
            void**          pointer;// Text renderer-handle storage variable pointer
            arb_text_data   data;   // Rendered text data
        } text;
    };
} arb_draw_request;
```

Each entry describes a single draw call. Draw requests are pre-sorted by
depth, from deepest to frontmost.

For text draw requests (`is_box_not_text == 0`), `text.pointer` is the same
storage slot populated during the corresponding `arb_text_alloc_request`,
and can be dereferenced to retrieve the buffer/partition handle for
rendering.

## 10. Single Draw Call Rendering

The full upload access can be rendered in a single draw call. A reference
implementation, with accompanying shaders, is available at:

- https://github.com/KacperOdzimek/Compages/blob/main/include/compages/rendering/arbor_rendering.h
- https://github.com/KacperOdzimek/Compages/tree/main/shaders/glsl/arbor_rendering

The following supporting types can be defined:

```c
typedef struct gpu_instance {
    int item;
    int glyph;
} gpu_instance;

typedef struct gpu_draw_item {
    arb_mat3x2  transform;
    uv_2d       atlas_position;
    int         texture_index;
    int         clipbox_index;
    uint32_t    shader_index;
    int         rounding_pixel;
    float       r, g, b, a;
} gpu_draw_item;

typedef struct gpu_clipbox {
    arb_mat3x2  transform;
} gpu_clipbox;
```

One `gpu_instance` is emitted per box and per glyph. All glyphs are stored
in a single subpartitioned buffer, addressable by index. Boxes emit an
instance with `glyph == -1`.

One `gpu_draw_item` is emitted per `arb_draw_request`, populated from that
request's data. Textures are accessed in a bindless model, also by index.

Given this data, a vertex shader can implement:

- Access the draw item via instance index.
- Transform the quad according to the draw item.
- If `glyph != -1`:
  - Access the glyph via instance index.
  - Transform the quad further according to the glyph transform.
- Output position and clipbox index.

And a fragment shader:

- Access the clipbox via clipbox index.
- Discard the fragment if the interpolated position falls outside the
  clipbox.
- Otherwise output color.

## 11. Depth

Decreasing depth means going further into the screen. `depth_index` on draw
requests can be ignored for ordering purposes, since requests are already
pre-sorted by depth, from smallest (deepest) to greatest (frontmost).

## 12. Clipboxing

Clipboxes must be evaluated in the fragment shader to clip content
precisely — for example, scrollbox contents. Discarding at the vertex
stage is not sufficient for this purpose.
