# Rendering Reference

## Transforms

UI elements dimensions are described using ``arb_mat3x2`` type. This type represents homogenous coordiantes transform matrix 3x3 with cut lower row, as it is always [0, 0, 1]. This type can be directly uploaded to GPU as it matches GLSL ``mat3x2`` ABI.

Indentity matrix:
```
1 0 0
0 1 0
[cut]
```
Represents box spanning entire screen. Smaller UI elements transforms, are derived from this, in sequence of translations and scale operations.
Adding to vertical offset (matrix 3rd columnd, 2nd row), moves data up - therefore coordinate system is Y+.

## Cache

Node layout and measurements are stored inside ``arb_cache``:

```c
arb_cache* arb_create_cache();
void arb_free_cache(arb_cache*);
```

This cache can be updated via:

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

Root being top of UI tree.
Returned ``arb_upload_access`` contains pointers to arbor render lists owned by cache - do not free, but can be read.
Important! Pointers inside ``arb_upload_access`` are valid until next cache_update!

## Upload Access

```c
// ===========================
// Upload Access

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

Rendering of Arbor UI shall look like this:
- Free old text buffers
- Alloc buffers and upload new text to GPU
- Upload clipboxes and draw calls
- Do render

Arbor leaves a lot of freedom to the user on how to do this, so here I will describe what I think is a good approach. But before that note on text layout and allocation.

## Text Layout

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

Arbor expects user to return a malloc-allocated array of laid out text glyphs. User can define glyphs to suit their own rendering system. An example glyph structure:

```c
typedef struct gpu_glyph {
    fnd_gfx_uv_2d   atlas_position;     // Font texture region
    float           off_x,  off_y;      // Top left offset
    float           size_x, size_y;     // Pixel size
} gpu_glyph;
```

It is impotant to define glyphs in pixel space - this allows unwrapped texts to stay valid during window resize.

## Text GPU Allocation

Storing GPU buffer handles per text node is ``arb_cache`` concern.
After cache update user may recive a ```arb_text_alloc_request```:

```c
typedef struct arb_text_alloc_request {
    void**  text_pointer_out;
    size_t  glyphs_count;
    void*   glyphs;
} arb_text_alloc_request;
```

Glyphs is a array earlier returned by ``arb_injection_text_layout`` - so it is in familar format.
The ``text_pointer_out`` is pointer to ``void*`` variable inside cache, designated to store a renderer side handle for this text allocation.

So when we receive such request we do:
```c
for req in access.ext_alloc_requests:
    *req.text_pointer_out = gpu_create_buffer(req.glyphs_count * sizeof(glyph))
    gpu_upload(*req.text_pointer_out, req.glyphs)
```

Even better if implementation subpartition single glyphs buffer:

```c
for req in access.text_alloc_requests:
    *req.text_pointer_out = find_suitable_partition(buffer, req.glyphs_count)
    gpu_upload_partition(buffer, *req.text_pointer_out, req.glyphs)
```

## Text GPU Free

If a text becomes unused, and cache garbage collector decides to free it, implementation receive ``arb_text_free_request``:
```c
typedef struct arb_text_free_request {
    void*   text_pointer;
} arb_text_free_request;
```
This text_pointer is the same renderer emitted when handling ``arb_text_alloc_request``.
Now renderer can free buffer or partition, created back then:
```c
for req in access.text_free_requests:
    gpu_free_buffer(req.text_pointer);
    or
    free_partition(req.text_buffer);
```

## Clipboxes

Since we got text ready on GPU, we need only to upload clipboxes and draw items.

```c
typedef struct arb_clipbox_request {
    arb_mat3x2  transform;  // Clipbox transform
} arb_clipbox_request;
```
Clipboxes may be stored in one SSBO - it is convenient as, draw requests index clipboxes via index [0, clipboxes count).

## Draw Items

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

Such structure describe a single draw call. 
Draw calls are ordered by depth, from deepest to the one on top.

If draw call is text draw call (``req.is_box_not_text`` == 0), then ``req.text.pointer`` is same variable as when text was allocated by ``arb_text_alloc_request``. Therefore we can get our buffer/partition handle again and render our text.

## Rendering In Single Draw Call

Rendering entire UI in single draw call is possible.
My renderer, explained below, can be find here, with shaders in second link:
https://github.com/KacperOdzimek/Compages/blob/main/include/compages/rendering/arbor_rendering.h
https://github.com/KacperOdzimek/Compages/tree/main/shaders/glsl/arbor_rendering

Lets define following types:

```c
typedef struct gpu_instance {
    int item;
    int glyph;
} gpu_instance;

typedef struct gpu_draw_item {
    arb_mat3x2      transform;
    fnd_gfx_uv_2d   atlas_position;
    int             texture_index;
    int             clipbox_index;
    uint32_t        shader_index;
    int             rounding_pixel;
    float           r, g, b, a;
} gpu_draw_item;

typedef struct gpu_clipbox {
    arb_mat3x2  transform;
} gpu_clipbox;
```

Lets generate a ``gpu_instance`` per box and per glyph - all glyphs are stored in one subpartitioner buffer, so we can get glyph per index. For boxes we emit instance with glyph == -1.

``gpu_draw_item`` is emitted per ``arb_draw_request`` with data from request. Textures here are accessed in bindless model, also via index, it is convinient.

Using all this data we can implement a vertex shader:
- Access draw item via instance index
- Transform quad according to draw item
- If glyph != -1
    - Access glyph via instance index
    - Transform quad further according to glyph transform
- Return position
- Return clipbox index

Then fragment shader:
- Access clipbox via clipbox index
- Check interpolated fragment position is in clipbox, else discard
- Color and return

## Extra Notes

### Depth

Decreasing depth means going 'into' the screen.
Depth index on draw requests can be ignored as those are already sorted by depth, from smallest (deepest) to greatest (in front).

### Clipboxing

Clipbox shall be handled in pixel/fragment shader to precisely clip for example, scrollbox contents - just discarding vertices wont work.
