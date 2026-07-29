#ifndef ARBOR_PADDING_H
#define ARBOR_PADDING_H

#include "arbor/architecture.h"

typedef enum arb_sizebox_overwrite_flag {
    arb_sizebox_overwrite_none        = 0,
    arb_sizebox_overwrite_all         = 255,
    arb_sizebox_overwrite_all_width   = 7,
    arb_sizebox_overwrite_all_height  = 56,

    arb_sizebox_overwrite_width_min   = 1 << 0,
    arb_sizebox_overwrite_width_max   = 1 << 1,
    arb_sizebox_overwrite_width_flex  = 1 << 2,

    arb_sizebox_overwrite_height_min  = 1 << 3,
    arb_sizebox_overwrite_height_max  = 1 << 4,
    arb_sizebox_overwrite_height_flex = 1 << 5
} arb_sizebox_overwrite_flag;

// During layout, overwrites selected fields with provided values
// Data is arb_sizebox_data, single child
extern const arb_type arb_sizebox_type;
typedef struct arb_sizebox_data {
    arb_sizebox_overwrite_flag  flag;
    arb_length                  width;
    arb_length                  height;    
} arb_sizebox_data;

#endif // ARBOR_PADDING_H

#ifdef ARBOR_PADDING_IMPL

void sizebox_width_measure(
    void*                   node_data,
    arb_node_layout_state*  node_state,
    size_t                  children_count,
    arb_node_layout_state** children_states
) {
    const arb_sizebox_data* data = node_data;
    arb_overlay_width_measure_func(node_data, node_state, children_count, children_states);
    if (data->flag & arb_sizebox_overwrite_width_min)   node_state->measured_width.min   = data->width.min;
    if (data->flag & arb_sizebox_overwrite_width_max)   node_state->measured_width.max   = data->width.max;
    if (data->flag & arb_sizebox_overwrite_width_flex)  node_state->measured_width.flex  = data->width.flex;
}

void sizebox_height_measure(
    void*                   node_data,
    arb_node_layout_state*  node_state,
    size_t                  children_count,
    arb_node_layout_state** children_states
) {
    const arb_sizebox_data* data = node_data;
    arb_overlay_height_measure_func(node_data, node_state, children_count, children_states);
    if (data->flag & arb_sizebox_overwrite_height_min)  node_state->measured_height.min  = data->height.min;
    if (data->flag & arb_sizebox_overwrite_height_max)  node_state->measured_height.max  = data->height.max;
    if (data->flag & arb_sizebox_overwrite_height_flex) node_state->measured_height.flex = data->height.flex;
}

const arb_type arb_sizebox_type = {
    .array_child        = 0,
    .width_measure      = sizebox_width_measure,
    .width_distribute   = arb_overlay_width_distribute_func,
    .height_measure     = sizebox_height_measure,
    .height_distribute  = arb_overlay_height_distribute_func,
    .position           = arb_overlay_position_func,
    .transform          = NULL
};

#endif // ARBOR_PADDING_IMPL
