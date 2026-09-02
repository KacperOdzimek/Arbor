#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
    System Function - Forward
*/

void get_system_stats(int* cpu, int* ram, int* gpu, int* temp);

/*
    Arbor Section - Frontend
*/

#include "arbor/arbor.h"

static void refresh_on_clicked(void* payload);

static const arb_box_data style_bg = { .tint = ARB_HEX("#1b1e24") };

static const arb_box_data panel_cpu = { .tint = ARB_HEX("#233042"), .rounding = 18 };
static const arb_box_data panel_ram = { .tint = ARB_HEX("#243b2c"), .rounding = 18 };
static const arb_box_data panel_gpu = { .tint = ARB_HEX("#3d3320"), .rounding = 18 };
static const arb_box_data panel_temp = { .tint = ARB_HEX("#452626"), .rounding = 18 }; // Deep red for temp

static const arb_box_data style_refresh_btn_default = { .tint = ARB_HEX("#3a4150"), .rounding = 10 };
static const arb_box_data style_refresh_btn_hovered = { .tint = ARB_HEX("#4a5566"), .rounding = 10 };
static const arb_box_data style_refresh_btn_pressed = { .tint = ARB_HEX("#2c3038"), .rounding = 10 };

static const arb_button_style style_refresh_btn = {
    .default_style = &style_refresh_btn_default,
    .hovered_style = &style_refresh_btn_hovered,
    .pressed_style = &style_refresh_btn_pressed,
};

static const arb_text_style header_style = {
    .size = 32, .font = "assets/roboto.ttf", .tint = ARB_HEX("#f2f4f6")
};

static const arb_text_style title_style = {
    .size = 14, .font = "assets/roboto.ttf", .tint = ARB_HEX("#8a93a3")
};

static const arb_text_style stat_style = {
    .size = 30, .font = "assets/roboto.ttf", .tint = ARB_HEX("#e8ecef")
};

static const arb_text_style refresh_label_style = {
    .size = 16, .font = "assets/roboto.ttf", .tint = ARB_HEX("#e8ecef")
};

static const arb_text_data header_text = {
    .style = &header_style,
    .text = "System Monitor"
};

static const arb_sizebox_data button_size = {
    .flag  = arb_sizebox_overwrite_width_max | arb_sizebox_overwrite_height_max,
    .width.max = 400, .height.max = 200
};

static const arb_text_data title_cpu = { .style = &title_style, .text = "CPU" };
static const arb_text_data title_ram = { .style = &title_style, .text = "RAM" };
static const arb_text_data title_gpu = { .style = &title_style, .text = "GPU" };
static const arb_text_data title_temp = { .style = &title_style, .text = "TEMP" };

typedef struct stat_state {
    char                   text_buffer[16];
    arb_text_data          value_text;
    arb_invalidation_flag  value_flag;
} stat_state;

static stat_state cpu_stat  = { .value_text = { .style = &stat_style, .text = "--" } };
static stat_state ram_stat  = { .value_text = { .style = &stat_style, .text = "--" } };
static stat_state gpu_stat  = { .value_text = { .style = &stat_style, .text = "--" } };
static stat_state temp_stat = { .value_text = { .style = &stat_style, .text = "--" } };

static const arb_invalidation_data cpu_invalidation  = { .flag_consumable_ptr = &cpu_stat.value_flag };
static const arb_invalidation_data ram_invalidation  = { .flag_consumable_ptr = &ram_stat.value_flag };
static const arb_invalidation_data gpu_invalidation  = { .flag_consumable_ptr = &gpu_stat.value_flag };
static const arb_invalidation_data temp_invalidation = { .flag_consumable_ptr = &temp_stat.value_flag };

static char                  status_buffer[48] = "Idle - press Refresh";
static arb_text_data         status_text_data  = { .style = &title_style, .text = status_buffer };
static arb_invalidation_flag status_flag       = arb_invalidation_flag_none;
static const arb_invalidation_data status_invalidation = { .flag_consumable_ptr = &status_flag };

typedef struct stat_card_data {
    const arb_box_data*          panel_style;
    const arb_text_data*         title_text;
    const arb_text_data*         value_text;
    const arb_invalidation_data* value_invalidation;
} stat_card_data;

static const arb_node stat_card_structure[] = {
    ARB_NODE(arb_box_type, arb_flag_instanced_data | arb_flag_indirected_data, offsetof(stat_card_data, panel_style)),
    ARB_PADD(20),
    ARB_NODE(arb_column_type, arb_flag_none, &(arb_column_data){ .spacing = { .min = 6, .max = 6 } }),
        ARB_ELEM(
            ARB_NODE(arb_text_type, arb_flag_instanced_data | arb_flag_indirected_data, offsetof(stat_card_data, title_text)),
            ARB_LAST
        ),
        ARB_ELEM(
            ARB_NODE(arb_invalidation_type, arb_flag_instanced_data | arb_flag_indirected_data, offsetof(stat_card_data, value_invalidation)),
            ARB_NODE(arb_text_type, arb_flag_instanced_data | arb_flag_indirected_data, offsetof(stat_card_data, value_text)),
            ARB_LAST
        ),
    ARB_LAST
};

static const stat_card_data cpu_card  = { &panel_cpu,  &title_cpu,  &cpu_stat.value_text,  &cpu_invalidation };
static const stat_card_data ram_card  = { &panel_ram,  &title_ram,  &ram_stat.value_text,  &ram_invalidation };
static const stat_card_data gpu_card  = { &panel_gpu,  &title_gpu,  &gpu_stat.value_text,  &gpu_invalidation };
static const stat_card_data temp_card = { &panel_temp, &title_temp, &temp_stat.value_text, &temp_invalidation };

static const arb_button_target refresh_target = { .on_clicked = refresh_on_clicked };

static const arb_text_data refresh_label_data = {
    .style = &refresh_label_style,
    .text = "Refresh"
};

static const arb_node refresh_label_branch[] = {
    ARB_PADD(14),
    ARB_NODE(arb_text_type, arb_flag_none, &refresh_label_data),
    ARB_LAST
};

static const arb_button_data refresh_button_data = {
    .style  = &style_refresh_btn,
    .target = &refresh_target,
    .child  = refresh_label_branch,
};

arb_node main_structure[] = {
    ARB_NODE(arb_box_type, arb_flag_ignore_max_width | arb_flag_ignore_max_height, &style_bg),
    ARB_PADD(48),
    ARB_NODE(arb_column_type, arb_flag_none, &(arb_column_data){ .spacing = { .min = 24, .max = 24 } }),
        ARB_ELEM(
            ARB_NODE(arb_text_type, arb_flag_none, &header_text),
            ARB_LAST
        ),
        ARB_ELEM(
            ARB_NODE(arb_row_type, arb_flag_none, &(arb_row_data){ .spacing = { .min = 16, .max = 16 } }),
            ARB_ELEM( ARB_INST(&cpu_card),  ARB_IDIR(stat_card_structure), ARB_LAST ),
            ARB_ELEM( ARB_INST(&ram_card),  ARB_IDIR(stat_card_structure), ARB_LAST ),
            ARB_ELEM( ARB_INST(&gpu_card),  ARB_IDIR(stat_card_structure), ARB_LAST ),
            ARB_ELEM( ARB_INST(&temp_card), ARB_IDIR(stat_card_structure), ARB_LAST ),
            ARB_LAST
        ),
        ARB_ELEM(
            ARB_NODE(arb_invalidation_type, arb_flag_none, &status_invalidation),
            ARB_NODE(arb_text_type, arb_flag_none, &status_text_data),
            ARB_LAST
        ),
        ARB_ELEM(
            ARB_NODE(arb_sizebox_type, arb_flag_none, &button_size),
            ARB_INST(&refresh_button_data),
            ARB_IDIR(arb_button_structure),
            ARB_LAST
        ),
    ARB_LAST
};

static int refresh_count = 0;

static void update_stats(void) {
    int cpu = 0, ram = 0, gpu = 0, temp = 0;
    get_system_stats(&cpu, &ram, &gpu, &temp);

    snprintf(cpu_stat.text_buffer,  sizeof(cpu_stat.text_buffer),  "%d%%", cpu);
    snprintf(ram_stat.text_buffer,  sizeof(ram_stat.text_buffer),  "%d%%", ram);
    snprintf(gpu_stat.text_buffer,  sizeof(gpu_stat.text_buffer),  "%d%%", gpu);
    snprintf(temp_stat.text_buffer, sizeof(temp_stat.text_buffer), "%d C", temp); 

    cpu_stat.value_text.text  = cpu_stat.text_buffer;
    ram_stat.value_text.text  = ram_stat.text_buffer;
    gpu_stat.value_text.text  = gpu_stat.text_buffer;
    temp_stat.value_text.text = temp_stat.text_buffer;

    cpu_stat.value_flag  = arb_invalidation_flag_text;
    ram_stat.value_flag  = arb_invalidation_flag_text;
    gpu_stat.value_flag  = arb_invalidation_flag_text;
    temp_stat.value_flag = arb_invalidation_flag_text;

    refresh_count++;
    snprintf(status_buffer, sizeof(status_buffer), "Updated - refresh #%d", refresh_count);
    status_flag = arb_invalidation_flag_text;
}

static void refresh_on_clicked(void* payload) {
    (void)payload;
    update_stats();
}

void initial_size(int* width, int* height) {
    *width = 500; *height = 400; // Anything
}

void init(void) {
    update_stats(); 
}

void frame(void) {
    static int ticks_until_next_update = 90;

    if (--ticks_until_next_update <= 0) {
        ticks_until_next_update = 90;
        update_stats();
    }
}

void term(void) {
}

/*
    System Section - Backend
*/

#if defined(_WIN32)
#include <windows.h>

static ULARGE_INTEGER file_time_to_ularge(const FILETIME* ft) {
    ULARGE_INTEGER li;
    li.LowPart = ft->dwLowDateTime;
    li.HighPart = ft->dwHighDateTime;
    return li;
}

void get_system_stats(int* cpu, int* ram, int* gpu, int* temp) {
    static ULARGE_INTEGER prev_idle, prev_kernel, prev_user;
    static int first_call = 1;

    // --- RAM ---
    MEMORYSTATUSEX mem_info;
    mem_info.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&mem_info)) {
        *ram = (int)mem_info.dwMemoryLoad;
    } else {
        *ram = 0;
    }

    // --- CPU ---
    FILETIME ft_idle, ft_kernel, ft_user;
    if (GetSystemTimes(&ft_idle, &ft_kernel, &ft_user)) {
        ULARGE_INTEGER idle = file_time_to_ularge(&ft_idle);
        ULARGE_INTEGER kernel = file_time_to_ularge(&ft_kernel);
        ULARGE_INTEGER user = file_time_to_ularge(&ft_user);

        if (first_call) {
            *cpu = 0; 
            first_call = 0;
        } else {
            ULONGLONG diff_idle = idle.QuadPart - prev_idle.QuadPart;
            ULONGLONG diff_kernel = kernel.QuadPart - prev_kernel.QuadPart;
            ULONGLONG diff_user = user.QuadPart - prev_user.QuadPart;
            ULONGLONG diff_total = diff_kernel + diff_user;

            if (diff_total > 0) {
                *cpu = (int)((diff_total - diff_idle) * 100ULL / diff_total);
            } else {
                *cpu = 0;
            }
        }
        prev_idle = idle;
        prev_kernel = kernel;
        prev_user = user;
    } else {
        *cpu = 0;
    }

    // --- GPU & TEMP (Windows limitation) ---
    // Universal GPU and Temp reading on Windows in pure C requires huge external SDKs 
    // (DXGI, NVML, or WMI COM setup). Fallback to 0 for now.
    *gpu = 0; 
    *temp = 0;
}

#elif defined(__linux__)

void get_system_stats(int* cpu, int* ram, int* gpu, int* temp) {
    // --- RAM ---
    FILE* meminfo = fopen("/proc/meminfo", "r");
    if (meminfo) {
        char line[256];
        long long total = 0, available = 0;
        while (fgets(line, sizeof(line), meminfo)) {
            if (strncmp(line, "MemTotal:", 9) == 0) sscanf(line, "MemTotal: %lld", &total);
            else if (strncmp(line, "MemAvailable:", 13) == 0) sscanf(line, "MemAvailable: %lld", &available);
        }
        fclose(meminfo);
        if (total > 0) {
            *ram = (int)(((total - available) * 100LL) / total);
        } else {
            *ram = 0;
        }
    } else {
        *ram = 0;
    }

    // --- CPU ---
    static unsigned long long prev_idle = 0, prev_total = 0;
    FILE* stat = fopen("/proc/stat", "r");
    if (stat) {
        char line[256];
        if (fgets(line, sizeof(line), stat)) {
            unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
            if (sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu %llu", 
                       &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal) >= 4) {
                
                unsigned long long total_idle = idle + iowait;
                unsigned long long non_idle = user + nice + system + irq + softirq + steal;
                unsigned long long total = total_idle + non_idle;

                if (prev_total != 0 && total > prev_total) {
                    unsigned long long total_diff = total - prev_total;
                    unsigned long long idle_diff = total_idle - prev_idle;
                    *cpu = (int)(((total_diff - idle_diff) * 100ULL) / total_diff);
                } else {
                    *cpu = 0; 
                }
                prev_idle = total_idle;
                prev_total = total;
            }
        }
        fclose(stat);
    } else {
        *cpu = 0;
    }

    // --- GPU ---
    // Try AMD / standard DRM sysfs first
    FILE* amd_gpu = fopen("/sys/class/drm/card0/device/gpu_busy_percent", "r");
    if (amd_gpu) {
        if (fscanf(amd_gpu, "%d", gpu) != 1) *gpu = 0;
        fclose(amd_gpu);
    } else {
        // Fallback to NVIDIA via command line query
        FILE* nv = popen("nvidia-smi --query-gpu=utilization.gpu --format=csv,noheader,nounits 2>/dev/null", "r");
        if (nv) {
            if (fscanf(nv, "%d", gpu) != 1) *gpu = 0;
            pclose(nv);
        } else {
            *gpu = 0;
        }
    }

    // --- TEMPERATURE ---
    // Typically found at thermal_zone0
    FILE* thermal = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (thermal) {
        long long t = 0;
        if (fscanf(thermal, "%lld", &t) == 1) {
            *temp = (int)(t / 1000); // Usually represented in millidegrees C
        } else {
            *temp = 0;
        }
        fclose(thermal);
    } else {
        *temp = 0;
    }
}

#else
// Unsupported OS fallback
void get_system_stats(int* cpu, int* ram, int* gpu, int* temp) {
    *cpu = 0; *ram = 0; *gpu = 0; *temp = 0;
}
#endif
