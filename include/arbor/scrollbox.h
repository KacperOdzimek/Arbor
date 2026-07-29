#ifndef ARBOR_SCROLLBOX_H
#define ARBOR_SCROLLBOX_H

#include "arbor/architecture.h"

extern const arb_node arb_vertical_scrollbox_structure[];
extern const arb_node arb_horizontal_scrollbox_structure[];
typedef struct arb_scrollbox_data {
    // Config
    arb_box_data    default_style;
    arb_box_data    hovered_style;
    arb_box_data    pressed_style;
    const arb_node* child;

    // State
    int             position;
    arb_box_data    current_handle_style;
    int             handle_drag;
    int             display_height;
    int             content_height;
    int             last_content_offset;
} arb_scrollbox_data;

#endif // ARBOR_SCROLLBOX_H

#ifdef ARBOR_SCROLLBOX_IMPL

// Vertical Scrollbox

static const float scroll_speed_vertical = 2500;

static void vertical_scrollbox_scroll_cursor_func(void* node_data, arb_node_cursor_input* node_input) {
    arb_scrollbox_data* data = node_data;
    if (node_input->hovered) {
        float pixel_change = node_input->mutable_state->scroll_delta * node_input->delta_time * scroll_speed_vertical;
        data->position -= pixel_change;
    }
}

static void vertical_scrollbox_transform_func(void* node_data, arb_mat3* transform, int resolution_x, int resolution_y) {
    arb_scrollbox_data* data = node_data;

    // Calculate offset
    int offset_to_align = -data->content_height / 2;  // start offseting from align - hardcoded top
    int total_offset    = offset_to_align + data->position;

    // No scrolling needed
    if (data->content_height <= data->display_height) {
        total_offset  = 0;
        data->position = 0;
    } 
    // Clamp
    else {
        int max_offset = (data->content_height - data->display_height) / 2;
        if (total_offset >  max_offset) {
            total_offset = max_offset;
            data->position = max_offset - offset_to_align;
        }
        if (total_offset < -max_offset) {
            total_offset = -max_offset;
            data->position = -max_offset - offset_to_align;
        }
    }

    // Offset transform
    transform->m[2][1] += 2 * (float)total_offset / resolution_y;
    data->last_content_offset = total_offset;

    // Calculate handle size
    float diplayed_portion = data->content_height ? (float)data->display_height / data->content_height : 0.0f;
    float handle_height    = data->display_height * diplayed_portion;
    if (handle_height > data->display_height) handle_height = data->display_height;
}

void vertical_scrollbox_position(void* node_data, arb_node_layout_state* node_state, size_t children_count, arb_node_layout_state** children_states) {
    // Do not position child, as it's transform is dynamic not static
    // Ensure static offset is 0
    arb_overlay_position_func(node_data, node_state, children_count, children_states);

    // Probe height
    arb_scrollbox_data* data = node_data;
    data->display_height = node_state->given_height;
    data->content_height = node_state->measured_height.max;
}

// Special type to offset content and probe height given and measured
static const arb_type vertical_scrollbox_scroller_type = {
    ARB_TYPE_OVERLAY_INIT,
    .position   = vertical_scrollbox_position,
    .transform  = vertical_scrollbox_transform_func
};

static void vertical_scrollbox_handle_transform_func(void* node_data, arb_mat3* transform, int resolution_x, int resolution_y) {
    arb_scrollbox_data* data = node_data;

    if (!data->content_height) {
        *transform = (arb_mat3){0}; return;
    }

    // Find handle height as a fraction of displayed height
    float visible_fraction = (float)data->display_height / data->content_height;
    if (visible_fraction > 1.0f) visible_fraction = 1.0f; // clamp

    // Find handle height
    int height = data->display_height * visible_fraction;
    if (height > data->content_height) height = data->content_height;

    // Position handle
    int handle_offset = 0;
    if (visible_fraction >= 1.0f) {
        handle_offset = 0;
    }
    else {
        // Find current lerp alpha of content between ends
        float begin = (data->content_height - data->display_height) / 2;
        float end   = -begin;
        float alpha = (data->last_content_offset - begin) / (end -  begin);

        // Apply alpha to handle movement
        begin = -(data->display_height / 2) + (height / 2);
        end   = -begin;
        handle_offset = begin + (end - begin) * alpha;
    }

    // Find vertical scale
    float sy = (float)height / data->display_height;

    // Apply to transform
    transform->m[2][1] += 2 * (float)handle_offset / resolution_y;
    transform->m[0][1] *= sy;
    transform->m[1][1] *= sy;
}

static void vertical_scrollbox_handle_cursor_func(void* node_data, arb_node_cursor_input* node_input) {
    arb_scrollbox_data* data = node_data;

    // Reset style
    data->current_handle_style = data->default_style;

    // Set style to hovered if hovered
    if (node_input->hovered) data->current_handle_style = data->hovered_style;

    // Scroll by draging handle
    int left_pressed = node_input->mutable_state->left_down;
    if (left_pressed) {
        int cursor_y = node_input->mutable_state->position_y;
        if (data->handle_drag != -1) {                         // Was dragged
            int pixels_change = data->handle_drag - cursor_y;  // Calculate pixel movement within handle
            pixels_change *= (data->content_height / data->display_height); // Calculate pixel movement within content

            data->position -= pixels_change;
            data->current_handle_style = data->pressed_style;

            data->handle_drag = cursor_y;
            node_input->mutable_state->left_down = 0;   // Consume left click
        }
        else if (node_input->hovered) {
            int c_left_pressed = node_input->mutable_state->left_down;
            int p_left_pressed = node_input->prev_raw_state->left_down;
            if (!(c_left_pressed && !p_left_pressed)) return; // Avoid accidental drag, require new click inside handle    
            data->handle_drag = cursor_y;
            node_input->mutable_state->left_down = 0;   // Consume left click
        }
    }
    else data->handle_drag = -1;
}

// Special type to apply handle transform and receive cursor events
static const arb_type vertical_scrollbox_handle_type = {
    ARB_TYPE_OVERLAY_INIT,
    .transform  = vertical_scrollbox_handle_transform_func,
    .cursor     = vertical_scrollbox_handle_cursor_func
};

const arb_node vertical_scrollbox_main_body[] = {
    {   // Scroller Node
        .type  = &vertical_scrollbox_scroller_type,
        .flags = arb_flag_instanced_data | arb_flag_ignore_min_height,
        .child = &vertical_scrollbox_main_body[1],
        .data_offset = 0, // Scrollbox data itself 
    },
    {   // Child
        .type  = &arb_indirect_type,
        .flags = arb_flag_instanced_child,
        .child_offset = offsetof(arb_scrollbox_data, child)
    }
};

const arb_node vertical_scrollbox_handle[] = {
    {   // Require handle width
        .type  = &arb_sizebox_type,
        .child = &vertical_scrollbox_handle[1],
        .data  = &(arb_sizebox_data){
            .flag  = arb_sizebox_overwrite_all_width,
            .width = (arb_length){16, 16, 1}
        },
    },
    {   // Handle Node
        .type  = &vertical_scrollbox_handle_type,
        .child = &vertical_scrollbox_handle[2],
        .flags = arb_flag_instanced_data,
        .data_offset = 0 // Scrollbox data itself
    },
    {   // Handle visual
        .type  = &arb_box_type,
        .flags = arb_flag_instanced_data | arb_flag_ignore_max_width | arb_flag_ignore_max_height,
        .data_offset = offsetof(arb_scrollbox_data, current_handle_style)
    }
};

const arb_node arb_vertical_scrollbox_structure[] = {
    {   // Clipbox
        .type  = &arb_indirect_type,
        .flags = arb_flag_clipbox | arb_flag_ignore_min_height,
        .child = &arb_vertical_scrollbox_structure[1],
    },
    {   // Handle for scroll input
        .type  = &arb_cursor_handle_type,
        .data  = vertical_scrollbox_scroll_cursor_func,
        .child = &arb_vertical_scrollbox_structure[2]
    },
    {   // Scroll Input
        .type  = &arb_cursor_call_type,
        .flags = arb_flag_instanced_data,
        .child = &arb_vertical_scrollbox_structure[3],
        .data_offset = 0 // Scrollbox data itself
    },
    {   // Row content-handle
        .type  = &arb_row_type,
        .child = &arb_vertical_scrollbox_structure[4],
        .data  = &(arb_row_data){
            .spacing        = (arb_length){0, 16, 1},
            .vertical_align = 0.5
        }
    },
    {   // Content
        .type  = &arb_indirect_type,
        .child = vertical_scrollbox_main_body,
    },
    {   // Handle
        .type  = &arb_indirect_type,
        .child = vertical_scrollbox_handle,
    },
    ARB_ARRAY_END
};

// Horizontal Scrollbox

static const float scroll_speed_horizontal = 3500;

static void horizontal_scrollbox_scroll_cursor_func(void* node_data, arb_node_cursor_input* node_input) {
    arb_scrollbox_data* data = node_data;
    if (node_input->hovered) {
        float pixel_change = node_input->mutable_state->scroll_delta * node_input->delta_time * scroll_speed_horizontal;
        data->position += pixel_change;
    }
}

static void horizontal_scrollbox_transform_func(void* node_data, arb_mat3* transform, int resolution_x, int resolution_y) {
    arb_scrollbox_data* data = node_data;

    // Calculate offset
    int offset_to_align = -data->content_height / 2;  // start offseting from align - hardcoded left
    int total_offset    = offset_to_align + data->position;

    // No scrolling needed
    if (data->content_height <= data->display_height) {
        total_offset  = 0;
        data->position = 0;
    } 
    // Clamp
    else {
        int max_offset = (data->content_height - data->display_height) / 2;
        if (total_offset >  max_offset) {
            total_offset = max_offset;
            data->position = max_offset - offset_to_align;
        }
        if (total_offset < -max_offset) {
            total_offset = -max_offset;
            data->position = -max_offset - offset_to_align;
        }
    }

    // Offset transform
    transform->m[2][0] += 2 * (float)total_offset / resolution_x;
    data->last_content_offset = total_offset;

    // Calculate handle size
    float diplayed_portion = data->content_height ? (float)data->display_height / data->content_height : 0.0f;
    float handle_width     = data->display_height * diplayed_portion;
    if (handle_width > data->display_height) handle_width = data->display_height;
}

void horizontal_scrollbox_position(void* node_data, arb_node_layout_state* node_state, size_t children_count, arb_node_layout_state** children_states) {
    // Do not position child, as it's transform is dynamic not static
    // Ensure static offset is 0
    arb_overlay_position_func(node_data, node_state, children_count, children_states);

    // Probe width
    arb_scrollbox_data* data = node_data;
    data->display_height = node_state->given_width;
    data->content_height = node_state->measured_width.max;
}

// Special type to offset content and probe width given and measured
static const arb_type horizontal_scrollbox_scroller_type = {
    ARB_TYPE_OVERLAY_INIT,
    .position   = horizontal_scrollbox_position,
    .transform  = horizontal_scrollbox_transform_func
};

static void horizontal_scrollbox_handle_transform_func(void* node_data, arb_mat3* transform, int resolution_x, int resolution_y) {
    arb_scrollbox_data* data = node_data;

    if (!data->content_height) {
        *transform = (arb_mat3){0}; return;
    }

    // Find handle width as a fraction of displayed width
    float visible_fraction = (float)data->display_height / data->content_height;
    if (visible_fraction > 1.0f) visible_fraction = 1.0f; // clamp

    // Find handle width
    int width = data->display_height * visible_fraction;
    if (width > data->content_height) width = data->content_height;

    // Position handle
    int handle_offset = 0;
    if (visible_fraction >= 1.0f) {
        handle_offset = 0;
    }
    else {
        // Find current lerp alpha of content between ends
        float begin = (data->content_height - data->display_height) / 2;
        float end   = -begin;
        float alpha = (data->last_content_offset - begin) / (end -  begin);

        // Apply alpha to handle movement
        begin = -(data->display_height / 2) + (width / 2);
        end   = -begin;
        handle_offset = begin + (end - begin) * alpha;
    }

    // Find horizontal scale
    float sx = (float)width / data->display_height;

    // Apply to transform
    transform->m[2][0] += 2 * (float)handle_offset / resolution_x;
    transform->m[0][0] *= sx;
    transform->m[1][0] *= sx;
}

static void horizontal_scrollbox_handle_cursor_func(void* node_data, arb_node_cursor_input* node_input) {
    arb_scrollbox_data* data = node_data;

    // Reset style
    data->current_handle_style = data->default_style;

    // Set style to hovered if hovered
    if (node_input->hovered) data->current_handle_style = data->hovered_style;

    // Scroll by draging handle
    int left_pressed = node_input->mutable_state->left_down;
    if (left_pressed) {
        int cursor_x = node_input->mutable_state->position_x;
        if (data->handle_drag != -1) {                         // Was dragged
            int pixels_change = data->handle_drag - cursor_x;  // Calculate pixel movement within handle
            pixels_change *= (data->content_height / data->display_height); // Calculate pixel movement within content

            data->position += pixels_change;
            data->current_handle_style = data->pressed_style;

            data->handle_drag = cursor_x;
            node_input->mutable_state->left_down = 0;   // Consume left click
        }
        else if (node_input->hovered) {
            int c_left_pressed = node_input->mutable_state->left_down;
            int p_left_pressed = node_input->prev_raw_state->left_down;
            if (!(c_left_pressed && !p_left_pressed)) return; // Avoid accidental drag, require new click inside handle    
            data->handle_drag = cursor_x;
            node_input->mutable_state->left_down = 0;   // Consume left click
        }
    }
    else data->handle_drag = -1;
}

// Special type to apply handle transform and receive cursor events
static const arb_type horizontal_scrollbox_handle_type = {
    ARB_TYPE_OVERLAY_INIT,
    .transform  = horizontal_scrollbox_handle_transform_func,
    .cursor     = horizontal_scrollbox_handle_cursor_func
};

const arb_node horizontal_scrollbox_main_body[] = {
    {   // Scroller Node
        .type  = &horizontal_scrollbox_scroller_type,
        .flags = arb_flag_instanced_data | arb_flag_ignore_min_width,
        .child = &horizontal_scrollbox_main_body[1],
        .data_offset = 0, // Scrollbox data itself 
    },
    {   // Child
        .type  = &arb_indirect_type,
        .flags = arb_flag_instanced_child,
        .child_offset = offsetof(arb_scrollbox_data, child)
    }
};

const arb_node horizontal_scrollbox_handle[] = {
    {   // Require handle height
        .type  = &arb_sizebox_type,
        .child = &horizontal_scrollbox_handle[1],
        .data  = &(arb_sizebox_data){
            .flag   = arb_sizebox_overwrite_all_height,
            .height = (arb_length){16, 16, 1}
        },
    },
    {   // Handle Node
        .type  = &horizontal_scrollbox_handle_type,
        .child = &horizontal_scrollbox_handle[2],
        .flags = arb_flag_instanced_data,
        .data_offset = 0 // Scrollbox data itself
    },
    {   // Handle visual
        .type  = &arb_box_type,
        .flags = arb_flag_instanced_data | arb_flag_ignore_max_width | arb_flag_ignore_max_height,
        .data_offset = offsetof(arb_scrollbox_data, current_handle_style)
    }
};

const arb_node arb_horizontal_scrollbox_structure[] = {
    {   // Clipbox
        .type  = &arb_indirect_type,
        .flags = arb_flag_clipbox | arb_flag_ignore_min_width,
        .child = &arb_horizontal_scrollbox_structure[1],
    },
    {   // Handle for scroll input
        .type  = &arb_cursor_handle_type,
        .data  = horizontal_scrollbox_scroll_cursor_func,
        .child = &arb_horizontal_scrollbox_structure[2]
    },
    {   // Scroll Input
        .type  = &arb_cursor_call_type,
        .flags = arb_flag_instanced_data,
        .child = &arb_horizontal_scrollbox_structure[3],
        .data_offset = 0 // Scrollbox data itself
    },
    {   // Column content-handle
        .type  = &arb_column_type,
        .child = &arb_horizontal_scrollbox_structure[4],
        .data  = &(arb_column_data){
            .spacing          = (arb_length){0, 16, 1},
            .horizontal_align = 0.5
        }
    },
    {   // Content
        .type  = &arb_indirect_type,
        .child = horizontal_scrollbox_main_body,
    },
    {   // Handle
        .type  = &arb_indirect_type,
        .child = horizontal_scrollbox_handle,
    },
    ARB_ARRAY_END
};

#endif // ARBOR_SCROLLBOX_IMPL
