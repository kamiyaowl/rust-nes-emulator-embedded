#include <iostream>
#include <cstdint>
#include <cstdlib>

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
    const float scale = 2;
    const uint32_t screenWidth = static_cast<int>(EMBEDDED_EMULATOR_VISIBLE_SCREEN_WIDTH * scale);
    const uint32_t screenHeight = static_cast<int>(EMBEDDED_EMULATOR_VISIBLE_SCREEN_HEIGHT * scale);
    const uint32_t fps = 0;

    InitWindow(screenWidth, screenHeight, "rust-nes-emulator-embedded");
    if (fps > 0) {
        SetTargetFPS(fps);
    }

    // FrameBuffer Image
    Image fbImg = { fb, EMBEDDED_EMULATOR_VISIBLE_SCREEN_WIDTH, EMBEDDED_EMULATOR_VISIBLE_SCREEN_WIDTH, 1, UNCOMPRESSED_R8G8B8A8 };
    Texture2D fbTexture = LoadTextureFromImage(fbImg);

    // Main game loop
    while (!WindowShouldClose())
    {
        // Emulate and update framebuffer
        EmbeddedEmulator_update_screen(&fb);
        Color* fbPtr = reinterpret_cast<Color*>(fb); // Color* fbPtr = GetImageData(fbImg); free(fbPtr);
        UpdateTexture(fbTexture, fbPtr);

        // Draw
        BeginDrawing();
        {
            DrawTextureEx(fbTexture, Vector2{ 0, 0 }, 0, scale, WHITE);
            DrawFPS(10, 10);
        }
        EndDrawing();
    }

    // Finalize
    UnloadTexture(fbTexture);
    UnloadImage(fbImg);
    CloseWindow();

    return 0;
}