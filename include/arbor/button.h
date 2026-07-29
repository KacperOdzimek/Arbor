#ifndef ARBOR_BUTTON_H
#define ARBOR_BUTTON_H

#include "arbor/architecture.h"

typedef void(arb_button_func_signature)(
    void* payload
);
typedef arb_button_func_signature* arb_button_func;

// Instance this with arb_button_data for button structure
// This button tries to fill entire given space (flex = 1, max = inf)
// On events calls callbacks from data if provided
extern const arb_node arb_button_structure[];
typedef struct arb_button_data {
    // Config
    void*           payload;
    arb_button_func on_clicked;
    arb_button_func on_released;
    arb_button_func on_held;
    arb_box_data    default_style;
    arb_box_data    hovered_style;
    arb_box_data    pressed_style;
    const arb_node* child;

    // State
    arb_box_data    current_style;
    unsigned char   pressed;
} arb_button_data;

#endif

#ifdef ARBOR_BUTTON_IMPL

static void button_cursor_func(void* node_data, arb_node_cursor_input* node_input) {
    arb_button_data* data = node_data;

    arb_cursor_state crr = *node_input->mutable_state;
    arb_cursor_state prv = *node_input->prev_raw_state;

    char just_pressed  = crr.left_down  && !prv.left_down;
    char just_released = !crr.left_down && prv.left_down;

    if (just_pressed && node_input->hovered) {  // press started
        data->pressed = 1;
        data->current_style = data->pressed_style;
        if (data->on_clicked) data->on_clicked(data->payload);
        crr.left_down = 0;
    }
    else if (crr.left_down && data->pressed) {    // held
        data->current_style = data->pressed_style;
        if (data->on_held) data->on_held(data->payload);
        crr.left_down = 0;
    }
    else if (just_released && data->pressed) {   // released
        data->pressed = 0;
        if (node_input->hovered) data->current_style = data->hovered_style;
        else data->current_style = data->default_style;
        if (data->on_released)  data->on_released(data->payload);
    }
    else if (node_input->hovered) data->current_style = data->hovered_style;    // hover
    else data->current_style = data->default_style;  // idle
}

const arb_node arb_button_structure[] = {
    {   // Set handle to button func
        .type   = &arb_cursor_handle_type,
        .data   = button_cursor_func,
        .child  = &arb_button_structure[1]
    },
    {   // Do logic
        .type   = &arb_cursor_call_type,
        .flags  = arb_flag_instanced_data,
        .child  = &arb_button_structure[2],
        .data_offset = 0,   // Instance itself
    },
    {   // Box, style = hitbox auxilary current style
        .type   = &arb_box_type,
        .flags  = arb_flag_instanced_data | arb_flag_instanced_child | arb_flag_ignore_max_width | arb_flag_ignore_max_height,
        .data_offset  = offsetof(arb_button_data, current_style),
        .child_offset = offsetof(arb_button_data, child)
    }
};

#endif // ARBOR_BUTTON_IMPL
