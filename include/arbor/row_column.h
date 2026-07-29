#ifndef ARBOR_ROW_COLUMN_H
#define ARBOR_ROW_COLUMN_H

#include "arbor/architecture.h"

// Layouts children in a row, left to right
// Data is arb_row_data, array children
extern const arb_type arb_row_type;
typedef struct arb_row_data {
    float           vertical_align;     // 0 - align top,  0.5 - align center, 1.0 - align bottom, other values also work
    arb_length      spacing;            // spacing between children
} arb_row_data;

// Layouts children in a column, top to down
// Data is arb_column_data, array children
extern const arb_type arb_column_type;
typedef struct arb_column_data {
    float           horizontal_align;   // 0 - align left,  0.5 - align center, 1.0 - align right, other values also work
    arb_length      spacing;            // spacing between children
} arb_column_data;

#endif

#ifdef ARBOR_ROW_COLUMN_IMPL

// Row

void row_width_measure(
    void*                   node_data,
    arb_node_layout_state*  node_state,
    size_t                  children_count,
    arb_node_layout_state** children_states
) {
    const arb_row_data* data = (const arb_row_data*)node_data;
    arb_length          own  = {0, 0, 0.0f};

    for (size_t i = 0; i < children_count; ++i) {
        arb_length child = children_states[i]->measured_width;
        own.min += child.min; own.max += child.max;
    }

    size_t spaces = children_count ? children_count - 1 : 0;
    own.min += spaces * data->spacing.min;

    if (own.max != arb_inf_length && data->spacing.max != arb_inf_length) own.max += spaces * data->spacing.max;
    else own.max = arb_inf_length;

    if (own.min != own.max) own.flex = 1.0f;
    node_state->measured_width = own;
}

void row_width_distribute(
    void*                   node_data,
    arb_node_layout_state*  node_state,
    size_t                  children_count,
    arb_node_layout_state** children_states
) {
    const arb_row_data* data = (const arb_row_data*)node_data;

    // Find spaces count
    size_t spaces_count = children_count ? children_count - 1 : 0;

    // Minimal pass
    int   used_width = 0;
    float flexsum    = 0.0f;
    int   spacing    = data->spacing.min;
    for (size_t i = 0; i < children_count; ++i) {
        flexsum += children_states[i]->measured_width.flex;
        children_states[i]->given_width = children_states[i]->measured_width.min;
        used_width += children_states[i]->given_width;
    }
    flexsum += data->spacing.flex;
    used_width += spaces_count * data->spacing.min;

    // Divide extra space
    int left_width = node_state->given_width - used_width;
    if (left_width < 0) left_width = 0;
    while (left_width) {
        float next_flexsum = 0.0f;
        int   partitioned  = 0;
        
        // add to spacing
        if (spacing < data->spacing.max) {
            int gain = (int)(left_width * (data->spacing.flex / flexsum));
            if (spaces_count) gain /= (int)spaces_count;
            gain = limit_length_gain(spacing, data->spacing, gain);

            spacing     += gain; 
            partitioned += gain * spaces_count;

            if (spacing != data->spacing.max) next_flexsum += data->spacing.flex;
        }

        // add to children
        for (size_t i = 0; i < children_count; ++i) {
            arb_length  m = children_states[i]->measured_width;
            int* assigned = &children_states[i]->given_width;
            if (*assigned == m.max) continue;   // maxed

            int gain = (int)(left_width * (m.flex / flexsum));
            gain = limit_length_gain(*assigned, m, gain);

            *assigned   += gain;
            partitioned += gain;

            if (*assigned != m.max) next_flexsum += m.flex;
        }

        // if failed to divide the space, break
        if (partitioned == 0) break;

        left_width -= partitioned;
        flexsum     = next_flexsum;
    }

    // Position children in horizontal axis
    int cursor_x = -node_state->given_width / 2;
    for (size_t i = 0; i < children_count; ++i) {
        arb_node_layout_state* child = children_states[i];
        cursor_x += child->given_width / 2;
        child->hori_offset = cursor_x;
        cursor_x += child->given_width / 2 + spacing;
    }
}

void row_position(
    void*                   node_data,
    arb_node_layout_state*  node_state,
    size_t                  children_count,
    arb_node_layout_state** children_states
) {
    const arb_row_data* data = (const arb_row_data*)node_data;

    // Position children in vertial axis
    for (size_t i = 0; i < children_count; ++i) {
        arb_node_layout_state* child = children_states[i];
        child->vert_offset = (node_state->given_height - child->given_height) * (0.5f - data->vertical_align);
    }
}

const arb_type arb_row_type = {
    .array_child        = 1,
    .width_measure      = row_width_measure,
    .width_distribute   = row_width_distribute,
    .height_measure     = arb_overlay_height_measure_func,
    .height_distribute  = arb_overlay_height_distribute_func,
    .position           = row_position,
    .transform          = NULL
};

// Column

void column_height_measure(
    void*                   node_data,
    arb_node_layout_state*  node_state,
    size_t                  children_count,
    arb_node_layout_state** children_states
) {
    const arb_column_data* data = (const arb_column_data*)node_data;
    arb_length             own  = {0, 0, 0.0f};

    for (size_t i = 0; i < children_count; ++i) {
        arb_length child = children_states[i]->measured_height;
        own.min += child.min; own.max += child.max;
    }

    size_t spaces = children_count ? children_count - 1 : 0;
    own.min += spaces * data->spacing.min;

    if (own.max != arb_inf_length && data->spacing.max != arb_inf_length) own.max += spaces * data->spacing.max;
    else own.max = arb_inf_length;

    if (own.min != own.max) own.flex = 1.0f;
    node_state->measured_height = own;
}

void column_height_distribute(
    void*                   node_data,
    arb_node_layout_state*  node_state,
    size_t                  children_count,
    arb_node_layout_state** children_states
) {
    const arb_column_data* data = (const arb_column_data*)node_data;

    // Find spaces count
    size_t spaces_count = children_count ? children_count - 1 : 0;

    // Minimal pass
    int   used_height = 0;
    float flexsum     = 0.0f;
    int   spacing     = data->spacing.min;
    for (size_t i = 0; i < children_count; ++i) {
        flexsum += children_states[i]->measured_height.flex;
        children_states[i]->given_height = children_states[i]->measured_height.min;
        used_height += children_states[i]->given_height;
    }
    flexsum += data->spacing.flex;
    used_height += spaces_count * data->spacing.min;

    // Divide extra space
    int left_height = node_state->given_height - used_height;
    if (left_height < 0) left_height = 0;
    while (left_height) {
        float next_flexsum = 0.0f;
        int   partitioned  = 0;

        // add to spacing
        if (spacing < data->spacing.max) {
            int gain = (int)(left_height * (data->spacing.flex / flexsum));
            if (spaces_count) gain /= (int)spaces_count;
            gain = limit_length_gain(spacing, data->spacing, gain);

            spacing     += gain;
            partitioned += gain * spaces_count;

            if (spacing != data->spacing.max) next_flexsum += data->spacing.flex;
        }

        // add to children
        for (size_t i = 0; i < children_count; ++i) {
            arb_length  m = children_states[i]->measured_height;
            int* assigned = &children_states[i]->given_height;
            if (*assigned == m.max) continue;   // maxed

            int gain = (int)(left_height * (m.flex / flexsum));
            gain = limit_length_gain(*assigned, m, gain);

            *assigned   += gain;
            partitioned += gain;

            if (*assigned != m.max) next_flexsum += m.flex;
        }

        // if failed to divide the space, break
        if (partitioned == 0) break;

        left_height -= partitioned;
        flexsum      = next_flexsum;
    }

    // Position children in vertical axis
    int cursor_y = node_state->given_height / 2;
    for (size_t i = 0; i < children_count; i++) {
        arb_node_layout_state* child = children_states[i];
        cursor_y -= child->given_height / 2;
        child->vert_offset = cursor_y;
        cursor_y -= child->given_height / 2 + spacing;
    }
}

void column_position(
    void*                   node_data,
    arb_node_layout_state*  node_state,
    size_t                  children_count,
    arb_node_layout_state** children_states
) {
    const arb_column_data* data = (const arb_column_data*)node_data;

    // Position children in horizontal axis
    for (size_t i = 0; i < children_count; ++i) {
        arb_node_layout_state* child = children_states[i];
        child->hori_offset = (node_state->given_width  - child->given_width)  * (data->horizontal_align - 0.5f);
    }
}

const arb_type arb_column_type = {
    .array_child        = 1,
    .width_measure      = arb_overlay_width_measure_func,
    .width_distribute   = arb_overlay_width_distribute_func,
    .height_measure     = column_height_measure,
    .height_distribute  = column_height_distribute,
    .position           = column_position,
    .transform          = NULL
};

#endif // ARBOR_ROW_COLUMN_IMPL
