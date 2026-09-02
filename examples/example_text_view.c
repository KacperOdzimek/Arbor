#include "arbor/arbor.h"

void init()  {};
void frame() {};
void term()  {};

// UI
const arb_box_data bg_style = {
    .tint = ARB_HEX("#1E2127") // Dark background
};

const arb_box_data fg_outline = {
    .tint     = ARB_HEX("#8FA3B8"), // Soft blue-gray outline
    .rounding = 8
};

const arb_box_data fg_style = {
    .tint     = ARB_HEX("#2B313A"), // Main panel gray
    .rounding = 8
};

const arb_box_data default_style = {
    .tint     = ARB_HEX("#676e78"), // Default button
    .rounding = 8
};

const arb_box_data hovered_style = {
    .tint     = ARB_HEX("#626a75"), // Hover state
    .rounding = 8
};

const arb_box_data pressed_style = {
    .tint     = ARB_HEX("#7c838c"), // Pressed state
    .rounding = 8
};

const arb_sizebox_data bt_size = {
    .flag   = arb_sizebox_overwrite_all,
    .width  = (arb_length){0, ARB_INF_LENGTH, 1},
    .height = (arb_length){0, 40, 1}
};

const arb_text_style tx_style = {
    .size = 24,
    .font = "assets/roboto.ttf",
    .tint = ARB_HEX("#9c9898"),
};

arb_text_data tx_data = {
    .style = &tx_style,
    .text  = "Select Text",
};

// Set to all for init only
arb_invalidation_flag text_invalidation = arb_invalidation_flag_all;

extern const char pan_tadeusz[];
extern const char source_code[];

void button_set_text_func(void* payload) {
    tx_data.text = (const char*)payload;
    text_invalidation = arb_invalidation_flag_text; // Regenerate text
}

arb_node menu_tree[] = {
    ARB_NODE(arb_sizebox_type, arb_flag_none, &(arb_sizebox_data){
        .flag   = arb_sizebox_overwrite_all,
        .width  = (arb_length){200, 200, 1},
        .height = (arb_length){0, ARB_INF_LENGTH, 1}
    }),
    ARB_NODE(arb_box_type, arb_flag_none, &fg_outline), ARB_PADD(2),
    ARB_NODE(arb_box_type, arb_flag_ignore_max_width | arb_flag_ignore_max_height, &fg_style),
    ARB_PADD(20),
    ARB_NODE(arb_column_type, arb_flag_ignore_max_height, &(arb_column_data){
        .horizontal_align = 0, .spacing = (arb_length){0, 16, 1}
    }),
    ARB_ELEM(
        ARB_NODE(arb_box_type, arb_flag_none, &fg_outline), ARB_PADD(2),
        ARB_NODE(arb_sizebox_type, arb_flag_none, &bt_size),
        ARB_INST(&(arb_button_data){
            .style = &(arb_button_style){
                .default_style = &default_style,
                .hovered_style = &hovered_style,
                .pressed_style = &pressed_style
            },
            .target = &(arb_button_target){
                .on_released   = button_set_text_func,
                .payload       = (void*)&pan_tadeusz[0],
            },
            .child = &ARB_ELEM(
                ARB_NODE(arb_text_type, arb_flag_none, &(arb_text_data){
                    .style = &tx_style,
                    .text = "Pan Tadeusz",
                }),
                ARB_LAST
            )
        }),
        ARB_IDIR(arb_button_structure)
    ),
    ARB_ELEM(
        ARB_NODE(arb_box_type, arb_flag_none, &fg_outline), ARB_PADD(2),
        ARB_NODE(arb_sizebox_type, arb_flag_none, &bt_size),
        ARB_INST(&(arb_button_data){
            .style = &(arb_button_style){
                .default_style = &default_style,
                .hovered_style = &hovered_style,
                .pressed_style = &pressed_style
            },
            .target = &(arb_button_target){
                .on_released   = button_set_text_func,
                .payload       = (void*)&source_code[0],
            },
            .child = &ARB_ELEM(
                ARB_NODE(arb_text_type, arb_flag_none, &(arb_text_data){
                    .style = &tx_style,
                    .text = "Source Code",
                }),
                ARB_LAST
            )
        }),
        ARB_IDIR(arb_button_structure)
    ),
    ARB_LAST
};

arb_node text_view[] = {
    ARB_PADD(20),
    ARB_NODE(arb_invalidation_type, arb_flag_none, &(arb_invalidation_data){
        .flag_consumable_ptr = &text_invalidation
    }),
    ARB_NODE(arb_text_type, arb_flag_none, &tx_data),
    ARB_LAST
};

arb_node text_vertical_scroll[] = {
    ARB_INST(&(arb_scrollbox_data){
        .style = &(arb_scrollbox_style){
            .default_style = &default_style,
            .hovered_style = &hovered_style,
            .pressed_style = &pressed_style
        },
        .child = text_view
    }),
    ARB_IDIR(arb_vertical_scrollbox_structure)
};

arb_node text_hortizontal_scroll[] = {
    ARB_INST(&(arb_scrollbox_data){
        .style = &(arb_scrollbox_style){
            .default_style = &default_style,
            .hovered_style = &hovered_style,
            .pressed_style = &pressed_style
        },
        .child = text_vertical_scroll
    }),
    ARB_IDIR(arb_horizontal_scrollbox_structure)
};

arb_node text_tree[] = {
    ARB_NODE(arb_box_type, arb_flag_none, &fg_outline),
    ARB_PADD(2),
    ARB_NODE(arb_box_type, arb_flag_ignore_max_width | arb_flag_ignore_max_height, &fg_style),
    ARB_PADD(10),
    ARB_IDIR(text_vertical_scroll)
};

arb_node main_structure[] = {
    ARB_NODE(arb_box_type, arb_flag_ignore_min_width | arb_flag_ignore_max_height, &bg_style),
    ARB_PADD(20),
    ARB_NODE(arb_row_type, arb_flag_none, &(arb_row_data){
        .spacing = (arb_length){10, 20, 1}
    }),
    ARB_IDIR(menu_tree),
    ARB_IDIR(text_tree),
    ARB_LAST
};

// Textes

const char pan_tadeusz[] =
    u8"Litwo! Ojczyzno moja! ty jesteś jak zdrowie;\n"
    u8"Ile cię trzeba cenić, ten tylko się dowie,\n"
    u8"Kto cię stracił. Dziś piękność twą w całej ozdobie\n"
    u8"Widzę i opisuję, bo tęsknię po tobie.\n"
    u8"\n"
    u8"Panno Święta, co Jasnej bronisz Częstochowy\n"
    u8"I w Ostrej świecisz Bramie! Ty, co gród zamkowy\n"
    u8"Nowogródzki ochraniasz z jego wiernym ludem!\n"
    u8"Jak mnie dziecko do zdrowia powróciłaś cudem\n"
    u8"(Gdy od płaczącej matki pod Twoją opiekę\n"
    u8"Ofiarowany, martwą podniosłem powiekę;\n"
    u8"I zaraz mogłem pieszo do Twych świątyń progu\n"
    u8"Iść za wrócone życie podziękować Bogu),\n"
    u8"Tak nas powrócisz cudem na Ojczyzny łono.\n"
    u8"\n"
    u8"Tymczasem przenoś moją duszę utęsknioną\n"
    u8"Do tych pagórków leśnych, do tych łąk zielonych,\n"
    u8"Szeroko nad błękitnym Niemnem rozciągnionych;\n"
    u8"Do tych pól malowanych zbożem rozmaitem,\n"
    u8"Wyzłacanych pszenicą, posrebrzanych żytem;\n"
    u8"Gdzie bursztynowy świerzop, gryka jak śnieg biała,\n"
    u8"Gdzie panieńskim rumieńcem dzięcielina pała,\n"
    u8"A wszystko przepasane, jakby wstęgą, miedzą\n"
    u8"Zieloną, na niej z rzadka ciche grusze siedzą.";

const char source_code[] =
    u8"arb_box_data bg_style = {\n"
    u8"    .tint = ARB_HEX(\"#373030\")\n"
    u8"};\n"
    u8"\n"
    u8"arb_box_data fg_style = {\n"
    u8"    .tint     = ARB_HEX(\"#5e5050\"),\n"
    u8"    .rounding = 16\n"
    u8"};\n"
    u8"\n"
    u8"arb_sizebox_data bt_size = {\n"
    u8"    .flag   = arb_sizebox_overwrite_all,\n"
    u8"    .width  = (arb_length){0, ARB_INF_LENGTH, 1},\n"
    u8"    .height = (arb_length){0, 40, 1}\n"
    u8"};\n"
    u8"\n"
    u8"#define DEFAULT_STYLE (arb_box_data){.tint = ARB_HEX(\"#373030\"), .rounding = 8}\n"
    u8"#define HOVERED_STYLE (arb_box_data){.tint = ARB_HEX(\"#7b6c6c\"), .rounding = 8}\n"
    u8"#define PRESSED_STYLE (arb_box_data){.tint = ARB_HEX(\"#897777\"), .rounding = 8}\n"
    u8"\n"
    u8"arb_text_data tx_data = {\n"
    u8"    .size = 24,\n"
    u8"    .font = \"Roboto\",\n"
    u8"    .tint = ARB_HEX(\"#9c9898\"),\n"
    u8"    .text = \"Select Text\",\n"
    u8"};\n"
    u8"\n"
    u8"arb_invalidation_data tx_invalidation = {\n"
    u8"    .flag_consumable = arb_invalidation_flag_all    // Init only, then maunal\n"
    u8"};\n"
    u8"\n"
    u8"extern const char pan_tadeusz[];\n"
    u8"extern const char source_code[];\n"
    u8"\n"
    u8"void button_set_text_func(void* payload) {\n"
    u8"    tx_data.text = (const char*)payload;\n"
    u8"    tx_invalidation.flag_consumable = arb_invalidation_flag_text; // Regenerate text\n"
    u8"}\n"
    u8"\n"
    u8"arb_node menu_tree[] = {\n"
    u8"    ARB_NODE(arb_sizebox_type, arb_flag_none, &(arb_sizebox_data){\n"
    u8"        .flag   = arb_sizebox_overwrite_all,\n"
    u8"        .width  = (arb_length){200, 200, 1},\n"
    u8"        .height = (arb_length){0, ARB_INF_LENGTH, 1}\n"
    u8"    }),\n"
    u8"    ARB_NODE(arb_box_type, arb_flag_ignore_max_width | arb_flag_ignore_max_height, &fg_style),\n"
    u8"    ARB_PADD(20),\n"
    u8"    ARB_NODE(arb_column_type, arb_flag_ignore_max_height, &(arb_column_data){\n"
    u8"        .horizontal_align = 0, .spacing = (arb_length){0, 16, 1}\n"
    u8"    }),\n"
    u8"    ARB_ELEM(\n"
    u8"        ARB_NODE(arb_sizebox_type, arb_flag_none, &bt_size),\n"
    u8"        ARB_INST(&(arb_button_data){\n"
    u8"            .default_style = DEFAULT_STYLE,\n"
    u8"            .hovered_style = HOVERED_STYLE,\n"
    u8"            .pressed_style = PRESSED_STYLE,\n"
    u8"            .on_released   = button_set_text_func,\n"
    u8"            .payload       = (void*)&pan_tadeusz[0],\n"
    u8"            .child = ARB_SINGLE(arb_text_type, arb_flag_none, &(arb_text_data){\n"
    u8"                .size = 24,\n"
    u8"                .font = \"Roboto\",\n"
    u8"                .tint = ARB_HEX(\"#9c9898\"),\n"
    u8"                .text = \"Pan Tadeusz\",\n"
    u8"            })\n"
    u8"        }),\n"
    u8"        ARB_IDIR(arb_button_structure)\n"
    u8"    ),\n"
    u8"    ARB_ELEM(\n"
    u8"        ARB_NODE(arb_sizebox_type, arb_flag_none, &bt_size),\n"
    u8"        ARB_INST(&(arb_button_data){\n"
    u8"            .default_style = DEFAULT_STYLE,\n"
    u8"            .hovered_style = HOVERED_STYLE,\n"
    u8"            .pressed_style = PRESSED_STYLE,\n"
    u8"            .on_released   = button_set_text_func,\n"
    u8"            .payload       = (void*)&source_code[0],\n"
    u8"            .child = ARB_SINGLE(arb_text_type, arb_flag_none, &(arb_text_data){\n"
    u8"                .size = 24,\n"
    u8"                .font = \"Roboto\",\n"
    u8"                .tint = ARB_HEX(\"#9c9898\"),\n"
    u8"                .text = \"Source Code\",\n"
    u8"            })\n"
    u8"        }),\n"
    u8"        ARB_IDIR(arb_button_structure)\n"
    u8"    ),\n"
    u8"    ARB_LAST\n"
    u8"};\n"
    u8"\n"
    u8"arb_node text_tree[] = {\n"
    u8"    ARB_NODE(arb_box_type, arb_flag_ignore_max_width | arb_flag_ignore_max_height, &fg_style),\n"
    u8"    ARB_PADD(10),\n"
    u8"    ARB_INST(&(arb_scrollbox_data){\n"
    u8"        .default_style = DEFAULT_STYLE,\n"
    u8"        .hovered_style = HOVERED_STYLE,\n"
    u8"        .pressed_style = PRESSED_STYLE,\n"
    u8"        .child = &ARB_ELEM(\n"
    u8"            ARB_PADD(20),\n"
    u8"            ARB_NODE(arb_invalidation_type, arb_flag_none, &tx_invalidation),\n"
    u8"            ARB_NODE(arb_text_type, arb_flag_none, &tx_data),\n"
    u8"            ARB_LAST\n"
    u8"        )\n"
    u8"    }),\n"
    u8"    ARB_IDIR(arb_vertical_scrollbox_structure)\n"
    u8"};\n"
    u8"\n"
    u8"arb_node main_structure[] = {\n"
    u8"    ARB_NODE(arb_box_type, arb_flag_ignore_min_width | arb_flag_ignore_max_height, &bg_style),\n"
    u8"    ARB_PADD(20),\n"
    u8"    ARB_NODE(arb_row_type, arb_flag_none, &(arb_row_data){\n"
    u8"        .spacing = (arb_length){10, 20, 1}\n"
    u8"    }),\n"
    u8"    ARB_IDIR(menu_tree),\n"
    u8"    ARB_IDIR(text_tree),\n"
    u8"    ARB_LAST\n"
    u8"};\n";
