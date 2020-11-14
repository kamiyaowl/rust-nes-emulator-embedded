#include <iostream>
#include <fstream>
#include <functional>
#include <map>
#include <tuple>

#include <cstdint>
#include <cstdlib>

#include "raylib.h"
#include "rust_nes_emulator_embedded.h"


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
    const uint32_t scale = (argc > 2) ? std::stoi(argv[2]) : 2;
    const uint32_t fps  = (argc > 3) ? std::stoi(argv[3]) : 60;

    const uint32_t offsetX = 0;
    const uint32_t offsetY = 0;
    const uint32_t screenWidth  = EMBEDDED_EMULATOR_VISIBLE_SCREEN_WIDTH * scale;
    const uint32_t screenHeight = EMBEDDED_EMULATOR_VISIBLE_SCREEN_HEIGHT * scale;

    // Allocate workarea
    // In this application, there is no constraint on allocate, so allocate a continuous area
    const uint32_t fbDataSize     = screenWidth * screenHeight * EMBEDDED_EMULATOR_NUM_OF_COLOR;
    const uint32_t cpuDataSize    = EmbeddedEmulator_GetCpuDataSize();
    const uint32_t systemDataSize = EmbeddedEmulator_GetSystemDataSize();
    const uint32_t ppuDataSize    = EmbeddedEmulator_GetPpuDataSize();
    std::cout << "INFO: Allocate buffer" << std::endl
              << " - FB     : " << fbDataSize << " bytes" << std::endl
              << " - Cpu    : " << cpuDataSize << " bytes" << std::endl
              << " - System : " << systemDataSize << " bytes" << std::endl
              << " - Ppu    : " << ppuDataSize << " bytes" << std::endl;

    uint8_t* workBuf   = new uint8_t[fbDataSize + cpuDataSize + systemDataSize + ppuDataSize];
    uint8_t* fbBuf     = &workBuf[0];
    uint8_t* cpuBuf    = &workBuf[fbDataSize];
    uint8_t* systemBuf = &workBuf[fbDataSize + cpuDataSize];
    uint8_t* ppuBuf    = &workBuf[fbDataSize + cpuDataSize + systemDataSize];

    // Emulator initialize
    std::cout << "INFO: Init emulator" << std::endl;
    EmbeddedEmulator_InitCpu(cpuBuf);
    EmbeddedEmulator_InitSystem(systemBuf);
    EmbeddedEmulator_InitPpu(ppuBuf);
    EmbeddedEmulator_SetPpuDrawOption(ppuBuf, screenWidth, screenHeight, offsetX, offsetY, scale, DrawPioxelFormat::RGBA8888);

    // Open rom file
    std::cout << "INFO: Load rom binary '" << romPath << "'" << std::endl;
    std::ifstream ifs(romPath, std::ios::binary | std::ios::in);
    if (!ifs) {
        std::cout << "ERROR: Failed to read '" << romPath << "'" << std::endl;
        delete[] workBuf;
        return -1;
    }

    // Get rom size
    ifs.seekg(0, std::ios::end);
    const int romSize = ifs.tellg();
    if (!romSize) {
        std::cout << "ERROR: ROM size is zero" << std::endl;
        ifs.close();
        delete[] workBuf;
        return -1;
    }

    // Read rom
    std::cout << "INFO: Allocate ROM buffer " << romSize << " bytes" << std::endl;
    uint8_t* romBuf = new uint8_t[romSize]; // another workarea
    ifs.seekg(0, std::ios::beg);
    ifs.read((char*)romBuf, romSize);
    ifs.close();

    // Parse rom header and parepare, reset
    const bool isLoad = EmbeddedEmulator_LoadRom(systemBuf, romBuf);
    if (!isLoad) {
        std::cout << "ERROR: failed to parse rom binary" << std::endl;
        delete[] romBuf;
        delete[] workBuf;
        return -1;
    }

    // Reset
    std::cout << "INFO: Reset" << std::endl;
    EmbeddedEmulator_Reset(cpuBuf, systemBuf, ppuBuf);

    // Screen Initialize
    std::cout << "INFO: Init window" << std::endl;
    InitWindow(screenWidth, screenHeight, "rust-nes-emulator-embedded");
    if (fps > 0) {
        SetTargetFPS(fps);
    }

    // FrameBuffer Image
    Image fbImg = { fbBuf, static_cast<int>(screenWidth), static_cast<int>(screenHeight), 1, UNCOMPRESSED_R8G8B8A8 };
    Texture2D fbTexture = LoadTextureFromImage(fbImg);

    // Key mapping
    const std::map<int, std::tuple<KeyEvent, KeyEvent>> keyMaps {
        { KEY_J, { KeyEvent::PressA, KeyEvent::ReleaseA } },
        { KEY_K, { KeyEvent::PressB, KeyEvent::ReleaseB } },
        { KEY_U, { KeyEvent::PressSelect, KeyEvent::ReleaseSelect } },
        { KEY_I, { KeyEvent::PressStart, KeyEvent::ReleaseStart } },
        { KEY_W, { KeyEvent::PressUp, KeyEvent::ReleaseUp } },
        { KEY_S, { KeyEvent::PressDown, KeyEvent::ReleaseDown } },
        { KEY_A, { KeyEvent::PressLeft, KeyEvent::ReleaseLeft } },
        { KEY_D, { KeyEvent::PressRight, KeyEvent::ReleaseRight } },
    };

    // Main game loop
    // Since there are keystrokes, the main thread should be the CPU thread.
    const uint32_t cyclePerFrame = EmbeddedEmulator_GetCpuCyclePerFrame();
    std::cout << "INFO: Start emulation" << std::endl;
    while (!WindowShouldClose())
    {
        // Input
        for (const auto& [key, value]: keyMaps) {
            if (IsKeyPressed(key)) {
                EmbeddedEmulator_UpdateKey(systemBuf, EMBEDDED_EMULATOR_PLAYER_0, std::get<0>(value));
            }
            if (IsKeyReleased(key)) {
                EmbeddedEmulator_UpdateKey(systemBuf, EMBEDDED_EMULATOR_PLAYER_0, std::get<1>(value));
            }
        }
        if (IsKeyReleased(KEY_R)) {
            std::cout << "INFO: Reset" << std::endl;
            EmbeddedEmulator_Reset(cpuBuf, systemBuf, ppuBuf);
        }

        // Emulate cpu/ppu
        for(uint32_t cycleSum = 0; cycleSum < cyclePerFrame; ) {
            // emulate cpu
            const uint32_t cyc = EmbeddedEmulator_EmulateCpu(cpuBuf, systemBuf);
            cycleSum += cyc;
            // emulate ppu
            const CpuInterrupt irq = EmbeddedEmulator_EmulatePpu(ppuBuf, systemBuf, fbBuf, cyc);
            // Interrupt from ppu
            if (irq != CpuInterrupt::NONE) {
                EmbeddedEmulator_InterruptCpu(cpuBuf, systemBuf, irq);
            }
        }

        // Draw
        Color* fbPtr = reinterpret_cast<Color*>(fbBuf);
        UpdateTexture(fbTexture, fbPtr);

        BeginDrawing();
        {
            // The scale of the image is newly implemented on the Rust side.
            // DrawTextureEx(fbTexture, Vector2{ 0, 0 }, 0, scale, WHITE);
            DrawTextureEx(fbTexture, Vector2{ 0, 0 }, 0, 1, WHITE);

            DrawFPS(10, 10);
        }
        EndDrawing();
    }

    // Finalize
    std::cout << "INFO: Finalize" << std::endl;
    UnloadTexture(fbTexture);
    CloseWindow();
    delete[] romBuf;
    delete[] workBuf;

    std::cout << "INFO: Exit" << std::endl;
    return 0;
}