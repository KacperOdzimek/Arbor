#include "arbor/arbor.h"

void initial_size(int* width, int* height) {
    *width = 500; *height = 400; // Window Dims
}

void init()  {};
void frame() {};
void term()  {};

#define BUF_BYTES 128
char BUF[BUF_BYTES] = "";

static const arb_box_data background_style = {
    .rounding = 0,
    .tint = ARB_HEX("#2f2c2c")
};

static const arb_box_data border_style = {
    .rounding = 4,
    .tint = ARB_HEX("#6b3636")
};

static const arb_text_style text_style = {
    .size = 32, .font = "assets/roboto.ttf", .tint = ARB_HEX("#f2f4f6"),
};

arb_node main_structure[] = {
    ARB_NODE(arb_box_type, arb_flag_ignore_max_width | arb_flag_ignore_max_height, &background_style),
    ARB_NODE(arb_sizebox_type, arb_flag_none, &(arb_sizebox_data){
        .flag = arb_sizebox_overwrite_width_max | arb_sizebox_overwrite_height_max,
        .width.max = 160, .height.max = 40
    }),
    ARB_NODE(arb_box_type, arb_flag_none, &border_style),
    ARB_PADD(2),
    ARB_INST(&(arb_fixed_text_box_data){
        .style = &(arb_fixed_text_box_style){
            .style = &text_style
        },
        .target = &(arb_fixed_text_box_target){
            .bytes = BUF_BYTES,
            .text  = BUF
        }
    }),
    ARB_IDIR(arb_fixed_text_box_structure)
};