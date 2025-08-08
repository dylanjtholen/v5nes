#include "main.h"
#include "agnes.h"
#include <stdio.h>

// Enable compiler optimizations
#pragma GCC optimize("O3")
#pragma GCC optimize("unroll-loops")

void initialize()
{
}

void disabled() {}

void competition_initialize() {}

void autonomous() {}

void display_pixel(int x, int y, agnes_color_t color)
{
    uint32_t color_val = (color.a << 24) | (color.r << 16) | (color.g << 8) | color.b;
    screen_set_pen(color_val);
    screen_draw_pixel(x + 112, y);
}

void get_input(agnes_input_t *input)
{
    input->a = controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_A);
    input->b = controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_B);
    input->select = controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_L1);
    input->start = controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_R1);
    input->up = controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_UP);
    input->down = controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_DOWN);
    input->left = controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_LEFT);
    input->right = controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_RIGHT);
}

void print(const char *text)
{
    controller_set_text(E_CONTROLLER_MASTER, 0, 0, text);
}

void opcontrol()
{
    print("Starting");


    FILE *rom = fopen("/usd/game.nes", "rb");
    if (!rom) {
        print("Failed to open game.nes");
        return;
    }

    fseek(rom, 0, SEEK_END);
    long game_data_size = ftell(rom);
    fseek(rom, 0, SEEK_SET);

    uint8_t *game_data = malloc(game_data_size);
    if (!game_data) {
        print("Failed to allocate memory for game data");
        fclose(rom);
        return;
    }

    fread(game_data, 1, game_data_size, rom);
    fclose(rom);

    agnes_t *agnes = agnes_make();
    if (!agnes) {
        print("Failed to create AGNES emulator");
        free(game_data);
        return;
    }
    
    if (!agnes_load_ines_data(agnes, game_data, game_data_size)) {
        print("Failed to load ROM data");
        agnes_destroy(agnes);
        free(game_data);
        return;
    }
    
    // Free ROM data after loading (AGNES keeps its own copy)
    free(game_data);
    game_data = NULL;
    
    // Allocate buffer for converted screen colors
    uint32_t *color_buffer = malloc(AGNES_SCREEN_WIDTH * AGNES_SCREEN_HEIGHT * sizeof(uint32_t));
    if (!color_buffer) {
        print("Failed to allocate color buffer");
        agnes_destroy(agnes);
        return;
    }
    
    // Pre-compute color lookup table for faster conversion
    uint32_t color_lookup[64];
    for (int i = 0; i < 64; i++) {
        agnes_color_t color = agnes_get_palette_color(i);
        color_lookup[i] = (color.a << 24) | (color.r << 16) | (color.g << 8) | color.b;
    }
    
    // Cache input to reduce polling overhead
    agnes_input_t input = {0};
    uint32_t frame_count = 0;
    uint32_t last_time = millis();
    const uint32_t target_frame_time = 1000 / 60; // 60 FPS target
    
    while (true) {
        // Frame rate limiting
        uint32_t current_time = millis();
        if (current_time - last_time < target_frame_time) {
            delay(1); // Small delay to prevent overwhelming the CPU
            continue;
        }
        last_time = current_time;
        
        // Only poll input every few frames to reduce overhead
        if (frame_count % 2 == 0) {
            get_input(&input);
        }
        frame_count++;

        agnes_set_input(agnes, &input, NULL);

        agnes_next_frame(agnes);

        // Convert palette buffer to RGB color buffer using lookup table
        uint8_t *screen_buffer = agnes_get_screen_buffer(agnes);
        const int buffer_size = AGNES_SCREEN_WIDTH * AGNES_SCREEN_HEIGHT;
        
        // Optimized conversion loop with manual unrolling
        for (int i = 0; i < buffer_size - 3; i += 4) {
            color_buffer[i]     = color_lookup[screen_buffer[i] & 0x3f];
            color_buffer[i + 1] = color_lookup[screen_buffer[i + 1] & 0x3f];
            color_buffer[i + 2] = color_lookup[screen_buffer[i + 2] & 0x3f];
            color_buffer[i + 3] = color_lookup[screen_buffer[i + 3] & 0x3f];
        }
        // Handle remaining pixels
        for (int i = buffer_size & ~3; i < buffer_size; i++) {
            color_buffer[i] = color_lookup[screen_buffer[i] & 0x3f];
        }
        
        // Use converted color buffer
        screen_copy_area(112, 0, AGNES_SCREEN_WIDTH - 1 + 112, AGNES_SCREEN_HEIGHT - 1, 
                        (uint8_t*)color_buffer, AGNES_SCREEN_WIDTH);
        
        /*
        // Old slow method - kept for reference
        for (int y = 0; y < AGNES_SCREEN_HEIGHT; y++) {
            for (int x = 0; x < AGNES_SCREEN_WIDTH; x++) {
                agnes_color_t c = agnes_get_screen_pixel(agnes, x, y);
                display_pixel(x, y, c);
            }
        }
        */
    }
    
    // Cleanup
    free(color_buffer);
    agnes_destroy(agnes);
}