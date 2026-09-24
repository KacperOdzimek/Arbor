#include "arbor/arbor.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <time.h>

/*
    Backend Forwards
*/

typedef struct item_def {
    const char*         name;           // Item name
    double              base_cost;      // Base cost of item
    double              growth;         // Cost multiplier per owned copy
    double              cps;            // Cookies per second, per copy
    double              click_bonus;    // Extra cookies per click, per copy
} item_def;
typedef struct shop_item {
    arb_button_data     button;
    arb_button_target   target;
    arb_text_data       name_text;
    arb_text_data       cost_text;
    arb_text_data       owned_text;
    arb_box_data        accent;
    const item_def*     def;
    int                 owned;
    char                cost_buf[128];
    char                owned_buf[16];
} shop_item;
static void on_cookie_clicked(void* payload);
static void on_unused(void* payload);
static char count_buf[48], cps_buf[64], click_buf[64];
#define ITEM_COUNT 7
static shop_item shop[ITEM_COUNT];

/*
    Frontend
*/

// ===========================
// Palette

#define COL_BG          ARB_HEX("#24150F")
#define COL_PANEL       ARB_HEX("#3A2618")
#define COL_CREAM       ARB_HEX("#FFF1DC")
#define COL_MUTED       ARB_HEX("#B39880")
#define COL_DIM         ARB_HEX("#8A7466")
#define COL_GOLD        ARB_HEX("#F2B84B")
#define COL_GREEN       ARB_HEX("#7BC24F")
#define COL_GREEN_TEXT  ARB_HEX("#B7E39A")
#define COL_COOKIE_RING ARB_HEX("#8B5A2B")
#define COL_ACCENT_OFF  ARB_HEX("#6B5546")

// ===========================
// Styles

static const arb_box_data box_panel = { 
    .tint = COL_PANEL, 
    .rounding = 16
    
};
static const arb_box_data box_invisible = {0};

static const arb_text_style st_title = {
    .size = 24, 
    .font = "assets/roboto.ttf",
    .tint = COL_GOLD
};

static const arb_text_style st_label = {
    .size = 16, 
    .font = "assets/roboto.ttf", 
    .tint = COL_MUTED
};

static const arb_sizebox_data size_item_row = {
    .flag = arb_sizebox_overwrite_all_height, .height = { .min = 64, .max = 64 },
};

// ===========================
// Widgets

static arb_button_data cookie_button = {
    .style = &(arb_button_style){
        .default_style = &box_invisible, .hovered_style = &box_invisible, .pressed_style = &box_invisible,
    },
    .target = &(arb_button_target){
        .on_clicked = on_cookie_clicked, .on_released = on_unused, .on_held = on_unused,
    },
};

// Instance with shop_item struct
const arb_node shop_label[] = {
    ARB_PADD(8),
    ARB_NODE(arb_row_type, arb_flag_none, &(arb_row_data){
        .vertical_align = 0.5f, .spacing = { .min = 10, .max = 10 }
    }),
        ARB_ELEM(   // Affordability bar
            ARB_NODE(arb_sizebox_type, arb_flag_none, &(arb_sizebox_data){
                .flag = arb_sizebox_overwrite_all_width, .width = { .min = 6, .max = 6 }
            }),
            ARB_NODE(
                arb_box_type, 
                arb_flag_instanced_data | arb_flag_ignore_max_width | arb_flag_ignore_max_height,
                offsetof(shop_item, accent)),
            ARB_LAST
        ),
        ARB_ELEM(   // Name + cost
            ARB_NODE(arb_column_type, arb_flag_none, &(arb_column_data){
                .horizontal_align = 0.0f, .spacing = { .min = 2, .max = 2 }
            }),
                ARB_ELEM(ARB_NODE(arb_text_type, arb_flag_instanced_data, offsetof(shop_item, name_text)), ARB_LAST),
                ARB_ELEM(ARB_NODE(arb_text_type, arb_flag_instanced_data, offsetof(shop_item, cost_text)), ARB_LAST),
            ARB_LAST
        ),
        ARB_ELEM(   // Owned count, pushed to the right edge
            ARB_NODE(
                arb_aling_type, 
                arb_flag_ignore_max_width | arb_flag_ignore_max_height, 
                &(arb_align_data){
                    .vertical_align = 0.5f, 
                    .horizontal_align = 1.0f
                }),
            ARB_NODE(arb_text_type, arb_flag_instanced_data, offsetof(shop_item, owned_text)),
            ARB_LAST
        ),
    ARB_LAST
};

#define SHOP_ROW(i)                                                    \
    ARB_ELEM(                                                          \
        ARB_NODE(arb_sizebox_type, arb_flag_none, &size_item_row),     \
        ARB_INST(&shop[i]),                                            \
        ARB_IDIR(arb_button_structure),                                \
        ARB_LAST                                                       \
    )

static const arb_node shop_list[] = {
    ARB_NODE(arb_column_type, arb_flag_none, &(arb_column_data){
        .horizontal_align = 0.0f, .spacing = { .min = 8, .max = 8 } }),
        SHOP_ROW(0), SHOP_ROW(1), SHOP_ROW(2), SHOP_ROW(3),
        SHOP_ROW(4), SHOP_ROW(5), SHOP_ROW(6),
    ARB_LAST
};

static arb_scrollbox_data shop_scroll = {
    .style = &(arb_scrollbox_style){
        .default_style = &(arb_box_data){ .tint = ARB_HEX("#7A5A44"), .rounding = 5 },
        .hovered_style = &(arb_box_data){ .tint = ARB_HEX("#93705A"), .rounding = 5 },
        .pressed_style = &(arb_box_data){ .tint = ARB_HEX("#B08A70"), .rounding = 5 },
    },
    .child = shop_list,
};

// ===========================
// Tree

static const arb_node left_panel[] = {
    ARB_NODE(arb_sizebox_type, arb_flag_none, &(arb_sizebox_data){
        .flag = arb_sizebox_overwrite_all_width, .width = { .min = 330, .max = 330 }
    }),
    ARB_NODE(arb_box_type, arb_flag_ignore_max_width | arb_flag_ignore_max_height, &box_panel),
    ARB_PADD(14),
    ARB_NODE(arb_column_type, arb_flag_none, &(arb_column_data){
        .horizontal_align = 0.5f, .spacing = { .min = 10, .max = 10 }
    }),
        ARB_ELEM(
            ARB_NODE(arb_text_type, arb_flag_none, &(arb_text_data){ 
                &st_title, "Cookie Bakery" 
            }), 
            ARB_LAST
        ),
        ARB_ELEM(   // HUD
            ARB_NODE(arb_column_type, arb_flag_none, &(arb_column_data){
                .horizontal_align = 0.5f, .spacing = { .min = 2, .max = 2 }
            }),
                ARB_ELEM(
                    ARB_NODE(arb_text_type, arb_flag_none, &(arb_text_data){
                        &(arb_text_style){ 
                            .size = 46, .font = "assets/roboto.ttf", .tint = COL_CREAM
                        }, count_buf
                    }), 
                    ARB_LAST
                ),
                ARB_ELEM(ARB_NODE(arb_text_type, arb_flag_none, &(arb_text_data){ &st_label, "cookies" }), ARB_LAST),
                ARB_ELEM(ARB_NODE(arb_text_type, arb_flag_none, &(arb_text_data){ &st_label, cps_buf }),   ARB_LAST),
                ARB_ELEM(ARB_NODE(arb_text_type, arb_flag_none, &(arb_text_data){ &st_label, click_buf }), ARB_LAST),
            ARB_LAST
        ),
        ARB_ELEM(   // The cookie button
            ARB_NODE(arb_sizebox_type, arb_flag_none, &(arb_sizebox_data){
                .flag   = arb_sizebox_overwrite_all,
                .width  = { .min = 236, .max = 236 },
                .height = { .min = 236, .max = 236 }
            }),
            ARB_NODE(arb_box_type, arb_flag_ignore_max_width | arb_flag_ignore_max_height, &(arb_box_data){
                .tint = COL_COOKIE_RING, .image = "assets/cookie.png"
            }),
            ARB_PADD(8),
            ARB_INST(&cookie_button),
            ARB_IDIR(arb_button_structure),
            ARB_LAST
        ),
        ARB_ELEM(
            ARB_NODE(arb_text_type, arb_flag_none, &(arb_text_data){
                &(arb_text_style){
                    .size = 14, 
                    .font = "assets/roboto.ttf", 
                    .tint = COL_DIM 
                }, "Click the cookie to bake!"}
            ), 
            ARB_LAST
        ),
    ARB_LAST
};

static const arb_node shop_panel[] = {
    ARB_NODE(arb_box_type, arb_flag_ignore_max_width | arb_flag_ignore_max_height, &box_panel),
    ARB_PADD(14),
    ARB_NODE(arb_column_type, arb_flag_none, &(arb_column_data){
        .horizontal_align = 0.0f, 
        .spacing = { .min = 10, .max = 10 }
    }),
        ARB_ELEM(
            ARB_NODE(arb_text_type, arb_flag_none, &(arb_text_data){&st_title, "Bakery Upgrades"}), 
            ARB_LAST
        ),
        ARB_ELEM(
            ARB_INST(&shop_scroll),
            ARB_IDIR(arb_vertical_scrollbox_structure),
            ARB_LAST
        ),
    ARB_LAST
};

const arb_node main_structure[] = {
    ARB_NODE(
        arb_box_type, 
        arb_flag_ignore_max_width | arb_flag_ignore_max_height, 
        &(arb_box_data){ .tint = COL_BG }
    ),
    ARB_PADD(16),
    ARB_NODE(
        arb_row_type, arb_flag_none, 
        &(arb_row_data){.vertical_align = 0.0f, .spacing = { .min = 16, .max = 16 }
    }),
        ARB_IDIR(left_panel),
        ARB_IDIR(shop_panel),
    ARB_LAST
};

/*
    Backend
*/

// ===========================
// Model

static const item_def defs[ITEM_COUNT] = {
    { "Cursor",        15,      1.15, 0.1,  0 },
    { "Sturdy Finger", 50,      1.60, 0,    1 },
    { "Grandma",       100,     1.15, 1,    0 },
    { "Farm",          1100,    1.15, 8,    0 },
    { "Mine",          12000,   1.15, 47,   0 },
    { "Factory",       130000,  1.15, 260,  0 },
    { "Portal",        1400000, 1.15, 1400, 0 },
};

static double cookies     = 0;
static double cps         = 0;
static double click_power = 1;
static double last_time   = 0;

// HUD text buffers (rewritten every frame)
static char count_buf[48], cps_buf[64], click_buf[64];

// ===========================
// Styles the logic swaps at runtime (must have static storage: init() keeps pointers to them)

static const arb_box_data box_accent_on  = { .tint = COL_GREEN,      .rounding = 3 };
static const arb_box_data box_accent_off = { .tint = COL_ACCENT_OFF, .rounding = 3 };

static const arb_text_style st_item_name = { .size = 18, .font = "assets/roboto.ttf", .tint = COL_CREAM };
static const arb_text_style st_item_dim  = { .size = 18, .font = "assets/roboto.ttf", .tint = COL_DIM };
static const arb_text_style st_cost_ok   = { .size = 14, .font = "assets/roboto.ttf", .tint = COL_GREEN_TEXT };
static const arb_text_style st_cost_dim  = { .size = 14, .font = "assets/roboto.ttf", .tint = COL_DIM };
static const arb_text_style st_owned     = { .size = 30, .font = "assets/roboto.ttf", .tint = COL_GOLD };

static const arb_button_style item_button_style = {
    .default_style = &(arb_box_data){ .tint = ARB_HEX("#553A2A"), .rounding = 10 },
    .hovered_style = &(arb_box_data){ .tint = ARB_HEX("#684734"), .rounding = 10 },
    .pressed_style = &(arb_box_data){ .tint = ARB_HEX("#452E21"), .rounding = 10 },
};

// Defined in the frontend
extern const arb_node shop_label[];

// ===========================
// Helpers

static double now_seconds(void) {
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static double item_cost(const shop_item* it) {
    return ceil(it->def->base_cost * pow(it->def->growth, it->owned));
}

static void recompute_rates(void) {
    cps = 0;
    click_power = 1;
    for (int i = 0; i < ITEM_COUNT; i++) {
        cps         += shop[i].owned * shop[i].def->cps;
        click_power += shop[i].owned * shop[i].def->click_bonus;
    }
}

// 999 -> "999", 1234 -> "1.23 K". `decimals` only applies below 1000.
static void fmt_number(char* out, size_t cap, double v, int decimals) {
    static const char* const suffix[] = { "", "K", "M", "B", "T", "Qa", "Qi" };
    int i = 0;
    while (v >= 1000.0 && i < 6) { v /= 1000.0; i++; }

    if (i == 0) snprintf(out, cap, "%.*f", fabs(v - round(v)) > 1e-9 ? decimals : 0, v);
    else        snprintf(out, cap, "%.2f %s", v, suffix[i]);
}

// ===========================
// Callbacks

static void on_cookie_clicked(void* payload) {
    (void)payload;
    cookies += click_power;
}

static void on_buy(void* payload) {
    shop_item* it   = payload;
    double     cost = item_cost(it);
    if (cookies < cost) return;
    cookies -= cost;
    it->owned++;
    recompute_rates();
}

static void on_unused(void* payload) { (void)payload; }

// ===========================
// Per-frame updates

static void update_hud(void) {
    char num[48];

    fmt_number(count_buf, sizeof count_buf, floor(cookies), 0);

    fmt_number(num, sizeof num, cps, 1);
    snprintf(cps_buf, sizeof cps_buf, "per second: %s", num);

    fmt_number(num, sizeof num, click_power, 0);
    snprintf(click_buf, sizeof click_buf, "per click: %s", num);
}

static void update_item(shop_item* it) {
    double price = item_cost(it);
    int    can   = cookies >= price;
    char   cost[32], gain[32];

    fmt_number(cost, sizeof cost, price, 0);
    if (it->def->click_bonus > 0) {
        snprintf(gain, sizeof gain, "+%d per click", (int)it->def->click_bonus);
    } else {
        char n[24];
        fmt_number(n, sizeof n, it->def->cps, 1);
        snprintf(gain, sizeof gain, "+%s /s", n);
    }
    snprintf(it->cost_buf,  sizeof it->cost_buf,  "%s cookies  |  %s", cost, gain);
    snprintf(it->owned_buf, sizeof it->owned_buf, "%d", it->owned);

    it->name_text.style = can ? &st_item_name : &st_item_dim;
    it->cost_text.style = can ? &st_cost_ok   : &st_cost_dim;
    it->accent          = can ? box_accent_on : box_accent_off;
}

// ===========================
// Lifecycle

void init() {
    for (int i = 0; i < ITEM_COUNT; i++) {
        shop_item* it = &shop[i];

        it->def        = &defs[i];
        it->target     = (arb_button_target){
            .on_clicked = on_buy, .on_released = on_unused, .on_held = on_unused, .payload = it,
        };
        it->button     = (arb_button_data){
            .style = &item_button_style, .target = &it->target, .child = shop_label,
        };
        it->name_text  = (arb_text_data){ &st_item_name, it->def->name };
        it->cost_text  = (arb_text_data){ &st_cost_dim,  it->cost_buf };
        it->owned_text = (arb_text_data){ &st_owned,     it->owned_buf };
        it->accent     = box_accent_off;
    }

    recompute_rates();
    last_time = now_seconds();
}

void term() {
}

void frame() {
    double now = now_seconds();
    double dt  = now - last_time;
    last_time  = now;
    if (dt > 1.0) dt = 1.0;

    cookies += cps * dt;

    update_hud();
    for (int i = 0; i < ITEM_COUNT; i++) update_item(&shop[i]);
}

void initial_size(int* width, int* height) {
    *width = 880; *height = 560;
}
