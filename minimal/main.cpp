#include <iostream>
#include <fstream>

#include <cstdint>
#include <cstdlib>

#include "raylib.h"
#include "rust_nes_emulator_embedded.h"

uint8_t fb[EMBEDDED_EMULATOR_VISIBLE_SCREEN_HEIGHT][EMBEDDED_EMULATOR_VISIBLE_SCREEN_WIDTH][EMBEDDED_EMULATOR_NUM_OF_COLOR];

int main(int argc, char* argv[])
{
    // parse command line args
    if (argc < 2) {
        std::cout << "game [rom_path] [scale*] [fps*]" << std::endl
                  << " - rom_path: .nes ROM file path (required) " << std::endl
                  << " - scale: screen scale. (default 2)" << std::endl
                  << " - fps: frame per seconds. If 0 is specified, no control is given. (default 60)" << std::endl;
        return 0;
    }
    const char* romPath = argv[1];
    const float scale = (argc > 2) ? std::stof(argv[2]) : 2.0;
    const uint32_t fps = (argc > 3) ? std::stoi(argv[3]) : 60;

    // Emulator initialize
    std::cout << "INFO: Init emulator" << std::endl;
    EmbeddedEmulator_init();

    // Open rom file
    std::cout << "INFO: Load rom binary '" << romPath << "'" << std::endl;
    std::ifstream ifs(romPath, std::ios::binary | std::ios::in);
    if (!ifs) {
        std::cout << "ERROR: Failed to read '" << romPath << "'" << std::endl;
        return -1;
    }

    // Get rom size
    ifs.seekg(0, std::ios::end);
    const int romSize = ifs.tellg();
    if (!romSize) {
        std::cout << "ERROR: ROM size is zero" << std::endl;
        ifs.close();
        return -1;
    }

    // Read rom
    uint8_t* romBuf = new uint8_t[romSize];
    ifs.seekg(0, std::ios::beg);
    ifs.read((char*)romBuf, romSize);
    ifs.close();

    // Parse rom header and parepare, reset
    if (!EmbeddedEmulator_load()) {
        std::cout << "ERROR: failed to parse rom binary" << std::endl;
        delete[] romBuf;
        return -1;
    }

    // Screen Initialize
    const uint32_t screenWidth = static_cast<int>(EMBEDDED_EMULATOR_VISIBLE_SCREEN_WIDTH * scale);
    const uint32_t screenHeight = static_cast<int>(EMBEDDED_EMULATOR_VISIBLE_SCREEN_HEIGHT * scale);

    std::cout << "INFO: Init window" << std::endl;
    InitWindow(screenWidth, screenHeight, "rust-nes-emulator-embedded");
    if (fps > 0) {
        SetTargetFPS(fps);
    }

    // FrameBuffer Image
    Image fbImg = { fb, EMBEDDED_EMULATOR_VISIBLE_SCREEN_WIDTH, EMBEDDED_EMULATOR_VISIBLE_SCREEN_WIDTH, 1, UNCOMPRESSED_R8G8B8A8 };
    Texture2D fbTexture = LoadTextureFromImage(fbImg);

    // Main game loop
    std::cout << "INFO: Start emulation" << std::endl;
    while (!WindowShouldClose())
    {
        // Emulate and update framebuffer
        EmbeddedEmulator_update_screen(&fb);
        Color* fbPtr = reinterpret_cast<Color*>(fb);
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
    std::cout << "INFO: Finalize" << std::endl;
    UnloadTexture(fbTexture);
    CloseWindow();
    delete[] romBuf;

    std::cout << "INFO: Exit" << std::endl;
    return 0;
}