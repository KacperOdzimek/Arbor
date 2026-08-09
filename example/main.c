// ===========================
// Note! The renderer is just a example!
// Images are not supported (lot of bookeeping)
// Only font is supported
// (Arbor itself allows infinite, as font/image is stored as string for renderer to resolve)

// ===========================
// Driver Application
// loads opengl, creates window, reads input, issues rendering

#include "deps/glad.h"
#include <GLFW/glfw3.h>
#include <stdio.h>

#include "arbor/arbor.h"
#include "user_interface.h"
#include "renderer.h"

static float g_scroll_delta_x = 0.0f;
static float g_scroll_delta_y = 0.0f;

// GLFW scroll callback function
static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    (void)window;
    g_scroll_delta_x += (float)xoffset;
    g_scroll_delta_y += (float)yoffset;
}

GLFWwindow* window;
arb_cache*  ui_cache;

void create_window_load_opengl() {
    if (!glfwInit()) return;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(1920, 1080, "Arbor UI - GL Host", NULL, NULL);
    if (!window) {
        glfwTerminate(); return;
    }

    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glfwSetScrollCallback(window, scroll_callback);
}

void terminate() {
    arbor_renderer_shutdown();
    arb_free_cache(ui_cache);
    glfwTerminate();
}

int load_source_code(const char *filename, char *buffer, size_t buffer_size);
int main() {
    // Window and Opengl
    create_window_load_opengl();
    if (!window) goto _fail;

    // Renderer Inner Objects
    if (!arbor_renderer_init("assets/roboto.ttf", 64.0f, "shaders/shader.vert", "shaders/shader.frag")) goto _fail;

    // Here we create UI cache, which will hold our UI geometry and render lists
    ui_cache = arb_create_cache();
    if (!ui_cache) goto _fail;

    // Load source code to display in UI
    load_source_code("renderer.c", source_code, SOURCE_CODE_SIZE_MAX);

    // Looping until user wishes to close the window
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();   // Load window input

        // Load current window resolution
        int width, height; glfwGetFramebufferSize(window, &width, &height);

        // Load current cursor position
        double cursor_x, cursor_y; glfwGetCursorPos(window, &cursor_x, &cursor_y);

        // Create cursor state, which will tell our arb cache about cursor actions
        // We are using scroll delta from GLFW scroll callback
        // The cursor input is handled inside cache_update
        arb_cursor_state cursor_state = {
            .left_down    = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT)  == GLFW_PRESS,
            .right_down   = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS,
            .position_x   = (int)cursor_x,
            .position_y   = (int)cursor_y,
            .scroll_delta = g_scroll_delta_y
        };

        // Reset scroll deltas
        g_scroll_delta_x = 0.0f;
        g_scroll_delta_y = 0.0f;

        // Here we update our UI geometry
        // Returned access contains pointers to render lists
        arb_upload_access access = arb_cache_update(
            ui_cache, main_structure, width, height, cursor_state, 0.016f
        );

        // Issue rendering
        arbor_renderer_draw_frame(access, width, height);

        // Present new rendered frame to user
        glfwSwapBuffers(window);
    }

    return 0;
_fail: terminate(); return -1;
}

// ============================
// Implementation Building
// Here we build our single header libraries

// Build arbor
#define ARBOR_IMPL
#include "arbor/arbor.h"

// Build stb truetype
#define STB_TRUETYPE_IMPLEMENTATION
#include "deps/stb_truetype.h"

// Build stb image
#define STB_IMAGE_IMPLEMENTATION
#include "deps/stb_image.h"

// ============================
// Load Helper

int load_source_code(const char *filename, char *buffer, size_t buffer_size) {
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        return -1;
    }

    size_t bytes_read = fread(buffer, 1, buffer_size - 1, file);

    if (ferror(file)) {
        fclose(file);
        return -1;
    }

    buffer[bytes_read] = '\0';

    fclose(file);

    return (int)bytes_read;
}
