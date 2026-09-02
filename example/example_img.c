#include <arbor/arbor.h>

arb_node main_structure[] = {
    ARB_NODE(arb_box_type, arb_flag_ignore_max_width | arb_flag_ignore_max_height, &(arb_box_data){
        .tint = ARB_HEX("#00FF00"),
        .image = "/home/kacper/Projects/Arbor/example/rendered.png"
    }),
    ARB_LAST
};
