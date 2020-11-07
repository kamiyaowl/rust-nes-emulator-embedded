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

    // FrameBuffer Image
    Image fbImg = { fb, EMBEDDED_EMULATOR_VISIBLE_SCREEN_WIDTH, EMBEDDED_EMULATOR_VISIBLE_SCREEN_WIDTH, 1, UNCOMPRESSED_R8G8B8A8 };
    Texture2D fbTexture = LoadTextureFromImage(fbImg);

    // Main game loop
    while (!WindowShouldClose())
    {
        // Update
        EmbeddedEmulator_update_screen(&fb);
        Color* fbPtr = reinterpret_cast<Color*>(fb);
        UpdateTexture(fbTexture, fbPtr);

        // Draw
        BeginDrawing();
        {
            ClearBackground(BLACK);
            DrawTexture(fbTexture, 0, 0, WHITE);
            DrawFPS(10, screenHeight - 20);
        }
        EndDrawing();
    }

    // Finalize
    CloseWindow();

    return 0;
}