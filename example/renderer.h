#ifndef RENDERER_H
#define RENDERER_H

#include "arbor/arbor.h"

// Initializes entire renderer
// Returns 0 on failure
int arbor_renderer_init(
    const char* font_path,
    float       font_pixel_height,
    const char* vertex_shader_path,
    const char* fragment_shader_path
);

// Releases all renderer GPU resources
void arbor_renderer_shutdown(void);

// A per-frame render function
void arbor_renderer_draw_frame(arb_upload_access access, int width, int height);

#endif // RENDERER_H
