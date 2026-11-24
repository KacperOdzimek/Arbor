#include "arbor/arbor.h"

// Style

static const arb_box_data background_style = {
    .rounding = 0,
    .tint = ARB_HEX("#2f2c2c")
};

static const arb_box_data border_style = {
    .rounding = 4,
    .tint = ARB_HEX("#6b3636")
};

static const arb_box_data scroll_default = {
    .rounding = 4,
    .tint = ARB_HEX("#898181")
};

static const arb_box_data scroll_hovered = {
    .rounding = 4,
    .tint = ARB_HEX("#ab9d9d")
};

static const arb_box_data scroll_pressed = {
    .rounding = 4,
    .tint = ARB_HEX("#d5c2c2")
};

static const arb_scrollbox_style scrollbox_style = {
    .default_style = &scroll_default,
    .hovered_style = &scroll_hovered,
    .pressed_style = &scroll_pressed
};

static const arb_float_slider_style slider_style = {
    .default_style = &scroll_default,
    .hovered_style = &scroll_hovered,
    .pressed_style = &scroll_pressed
};

// Logic

#define BUF_BYTES 1024
char BUF[BUF_BYTES] = "";

float font_size = 18;

static const arb_float_slider_target font_slider_target = {
    .min    = 9,
    .max    = 36,
    .target = &font_size
};

static arb_text_style text_style = {
    .size = 18, .font = "assets/roboto.ttf", .tint = ARB_HEX("#f2f4f6"),
};

static arb_invalidation_flag font_invalidator = arb_invalidation_flag_none;

void frame() {
    if (text_style.size != (int)font_size) {
        text_style.size = (int)font_size;
        font_invalidator = arb_invalidation_flag_text;
    }
};

void initial_size(int* width, int* height) {
    *width = 500; *height = 400; // Window Dims
}
void init()  {};
void term()  {};

// Structure

static const arb_node typing_box[] = {
    ARB_NODE(arb_box_type, arb_flag_none, &border_style),
    ARB_PADD(8),
    ARB_INST(&(arb_fixed_text_box_data){
        .style = &(arb_fixed_text_box_style){
            .style = &text_style
        },
        .target = &(arb_fixed_text_box_target){
            .bytes = BUF_BYTES,
            .text  = BUF
        },
        .invalid = &font_invalidator
    }),
    ARB_IDIR(arb_fixed_text_box_structure)
};

static const arb_node hori_scroll[] = {
    ARB_INST(&(arb_scrollbox_data){
        .style = &scrollbox_style,
        .child = typing_box
    }),
    ARB_IDIR(arb_horizontal_scrollbox_structure)
};

static const arb_node vert_scroll[] = {
    ARB_INST(&(arb_scrollbox_data){
        .child = hori_scroll,
        .style = &scrollbox_style
    }),
    ARB_IDIR(arb_vertical_scrollbox_structure)
};

static const arb_node menu_row[] = {
    ARB_NODE(arb_sizebox_type, arb_flag_none, &(arb_sizebox_data){
        .flag   = arb_sizebox_overwrite_all_height,
        .height = {20, 20, 1}
    }),
    ARB_NODE(arb_box_type, arb_flag_none, &border_style),
    ARB_INST(&(arb_float_slider_data){
        .child  = NULL,
        .style  = &slider_style,
        .target = &font_slider_target
    }),
    ARB_IDIR(arb_float_slider_structure)
};

arb_node main_structure[] = {
    ARB_NODE(arb_box_type, arb_flag_ignore_max_width | arb_flag_ignore_max_height, &background_style),
    ARB_PADD(16),
    ARB_NODE(arb_column_type, arb_flag_none, &(arb_column_data){
        .spacing = {16, 16, 0}
    }),
    ARB_IDIR(menu_row),
    ARB_IDIR(vert_scroll),
    ARB_LAST
};
