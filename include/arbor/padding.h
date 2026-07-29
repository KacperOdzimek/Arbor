#ifndef ARBOR_PADDING_H
#define ARBOR_PADDING_H

#include "arbor/architecture.h"

// Padds child inside self
// Data is arb_padding_data, single child
extern const arb_type arb_padding_type;
typedef struct arb_padding_data {
    arb_length left, right, top, bottom;
} arb_padding_data;

#endif // ARBOR_PADDING_H

#ifdef ARBOR_PADDING_IMPL

static inline int padding_distribute_length(
    int* a, arb_length al,
    int* b, arb_length bl,
    int* c, arb_length cl,
    int remaining
) {
    for (int pass = 0; pass < 3 && remaining > 0; pass++) {
        float tf = 0.0f;
        if (*a < al.max) tf += al.flex;
        if (*b < bl.max) tf += bl.flex;
        if (*c < cl.max) tf += cl.flex;
        if (tf <= 0.0f) break;

        int ga = (*a < al.max && al.flex > 0.0f) ? limit_length_gain(*a, al, (int)((float)remaining * al.flex / tf)) : 0;
        int gb = (*b < bl.max && bl.flex > 0.0f) ? limit_length_gain(*b, bl, (int)((float)remaining * bl.flex / tf)) : 0;
        int gc = (*c < cl.max && cl.flex > 0.0f) ? limit_length_gain(*c, cl, (int)((float)remaining * cl.flex / tf)) : 0;

        if (ga + gb + gc == 0) break; // all remaining too small after int cast
        *a += ga; *b += gb; *c += gc;
        remaining -= ga + gb + gc;
    }
    return remaining;
}

void padding_width_measure(
    void*                   node_data,
    arb_node_layout_state*  node_state,
    size_t                  children_count,
    arb_node_layout_state** children_states
) {
    const arb_padding_data* data = node_data;
    arb_length own = {0, 0, 0.0f};
    
    int child_min = 0, child_max = 0;
    if (children_count > 0) {
        child_min = children_states[0]->measured_width.min;
        child_max = children_states[0]->measured_width.max;
    }

    int w_min = data->left.min + child_min + data->right.min;
    int w_max = data->left.max + child_max + data->right.max;

    node_state->measured_width = (arb_length){
        .min  = w_min,
        .max  = w_max,
        .flex = (w_min != w_max) ? 1.0f : 0.0f,
    };
}

void padding_width_distribute(
    void*                   node_data,
    arb_node_layout_state*  node_state,
    size_t                  children_count,
    arb_node_layout_state** children_states
) {
    if (children_count == 0) return;
    const arb_padding_data* data = node_data;
    arb_node_layout_state* child = children_states[0];

    // Give every element its minimum
    int left_w  = data->left.min;
    int right_w = data->right.min;
    int child_w = child->measured_width.min;

    // Divide remaining space, give leftover to child
    int remaining = node_state->given_width - left_w - right_w - child_w;
    if (remaining > 0) {
        remaining = padding_distribute_length(
            &left_w, data->left, &right_w, data->right, &child_w, child->measured_width, remaining
        ); child_w += limit_length_gain(child_w, child->measured_width, remaining);
    }

    // Assign child width and position
    child->given_width = child_w;
    child->hori_offset = (left_w - right_w) / 2;
}

void padding_height_measure(
    void*                   node_data,
    arb_node_layout_state*  node_state,
    size_t                  children_count,
    arb_node_layout_state** children_states
) {
    const arb_padding_data* data = node_data;
    int child_min = 0, child_max = 0;
    if (children_count > 0) {
        child_min = children_states[0]->measured_height.min;
        child_max = children_states[0]->measured_height.max;
    }

    int h_min = data->top.min + child_min + data->bottom.min;
    int h_max = data->top.max + child_max + data->bottom.max;

    node_state->measured_height = (arb_length){
        .min  = h_min,
        .max  = h_max,
        .flex = (h_min != h_max) ? 1.0f : 0.0f,
    };
}

void padding_height_distribute(
    void*                   node_data,
    arb_node_layout_state*  node_state,
    size_t                  children_count,
    arb_node_layout_state** children_states
) {
    if (children_count == 0) return;
    const arb_padding_data* data = node_data;
    arb_node_layout_state* child = children_states[0];

    // Give every element its minimum
    int top_h    = data->top.min;
    int bottom_h = data->bottom.min;
    int child_h  = child->measured_height.min;

    // Divide remaining space, give leftover to child
    int remaining = node_state->given_height - top_h - bottom_h - child_h;
    if (remaining > 0) {
        remaining = padding_distribute_length(
            &top_h, data->top, &bottom_h, data->bottom, &child_h, child->measured_height, remaining
        ); child_h += limit_length_gain(child_h, child->measured_height, remaining);
    }

    // Assign child width and position
    child->given_height = child_h;
    child->vert_offset  = (bottom_h - top_h) / 2;
}

const arb_type arb_padding_type = {
    .array_child        = 0,
    .width_measure      = padding_width_measure,
    .width_distribute   = padding_width_distribute,
    .height_measure     = padding_height_measure,
    .height_distribute  = padding_height_distribute,
    .position           = NULL,
    .transform          = NULL
};

#endif // ARBOR_PADDING_IMPL
