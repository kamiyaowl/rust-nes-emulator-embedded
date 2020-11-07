#include <iostream>
#include <cstdint>

#include "raylib.h"
#include "rust_nes_emulator_embedded.h"

uint8_t fb[EMBEDDED_EMULATOR_VISIBLE_SCREEN_HEIGHT][EMBEDDED_EMULATOR_VISIBLE_SCREEN_WIDTH][EMBEDDED_EMULATOR_NUM_OF_COLOR];

int main(int argc, char* argv[])
{
    // Emulator initialize
    EmbeddedEmulator_init();
    if (EmbeddedEmulator_load() == false) {
        std::cout << "emulator load error" << std::endl;
        return 1;
    }

    // Screen Initialize
    const uint32_t scale = 2;
    const uint32_t screenWidth = EMBEDDED_EMULATOR_VISIBLE_SCREEN_WIDTH * scale;
    const uint32_t screenHeight = EMBEDDED_EMULATOR_VISIBLE_SCREEN_HEIGHT * scale;
    const uint32_t fps = 60;

    InitWindow(screenWidth, screenHeight, "rust-nes-emulator-embedded");
    SetTargetFPS(fps);

    // Main game loop
    while (!WindowShouldClose())
    {
        // Update
        EmbeddedEmulator_update_screen(&fb);

        // Draw
        BeginDrawing();
        ClearBackground(BLACK);
        for(uint32_t j = 0 ; j < EMBEDDED_EMULATOR_VISIBLE_SCREEN_HEIGHT ; ++j) {
            for(uint32_t i = 0 ; i < EMBEDDED_EMULATOR_VISIBLE_SCREEN_WIDTH ; ++i) {
                Color c = {
                    .r = fb[j][i][0],
                    .g = fb[j][i][1],
                    .b = fb[j][i][2],
                    .a = 255,
                };
                const uint32_t x = i * scale;
                const uint32_t y = j * scale;
                DrawRectangle(x, y, scale, scale, c);
            }
        }
        DrawFPS(10, 10);
        EndDrawing();
    }

    // Finalize
    CloseWindow();

    return 0;
}