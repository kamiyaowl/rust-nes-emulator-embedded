#include <iostream>
#include <fstream>
#include <functional>
#include <map>
#include <tuple>
#include <thread>

#include <cstdint>
#include <cstdlib>

#include "raylib.h"
#include "rust_nes_emulator_embedded.h"

// Shared variables
// Assumption that ppuUncosumedCycles are placed between different CPUs where they are not affected by the cache
uint8_t* fbBuf = NULL;
uint8_t* cpuBuf = NULL;
uint8_t* systemBuf = NULL;
uint8_t* ppuBuf = NULL;
uint32_t cpuCyc = 0;
uint32_t ppuCyc = 0;
CpuInterrupt interrupt = CpuInterrupt::NONE; // When rewriting from the PPU side, make sure that the rewrite is not none.

// PPU thread control variables
volatile bool isPause = true;
volatile bool hasPaused = false;
volatile bool isAbort = false;

void runPpu(void) {
    const uint32_t cyclePerLine = EmbeddedEmulator_GetCpuCyclePerLine();
    while (!isAbort) {
        // pause control
        if (isPause) {
            hasPaused = true;
            continue;
        }
        hasPaused = false;

        // Emulate ppu
        // On the real machine, you should be able to remove the loop and emulate it indefinitely.
        const uint32_t fetchCpuCyc = cpuCyc; // buffered
        if (fetchCpuCyc != ppuCyc) {
            // Even if it's overflowing, it's still calculated correctly until it goes around one round.
            const uint32_t diffCyc = fetchCpuCyc - ppuCyc;
            const uint32_t loopCount = diffCyc / cyclePerLine;
            for (uint32_t i = 0; i < loopCount; i++) {
                const CpuInterrupt irq = EmbeddedEmulator_EmulatePpu(ppuBuf, systemBuf, fbBuf, cyclePerLine);
                // Interrupt event
                if ((interrupt == CpuInterrupt::NONE) && (irq != CpuInterrupt::NONE)) {
                    interrupt = irq;
                    std::cout << "DEBUG: Interrupt " << static_cast<uint32_t>(irq) << std::endl;
                }
                // Update latest cyc
                ppuCyc += cyclePerLine;
            }
        }

    }
    // aborted
}

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
    const float scale   = (argc > 2) ? std::stof(argv[2]) : 2.0;
    const uint32_t fps  = (argc > 3) ? std::stoi(argv[3]) : 60;

    // Allocate workarea
    // In this application, there is no constraint on allocate, so allocate a contiguous area
    const uint32_t fbDataSize     = EMBEDDED_EMULATOR_VISIBLE_SCREEN_WIDTH * EMBEDDED_EMULATOR_VISIBLE_SCREEN_HEIGHT * EMBEDDED_EMULATOR_NUM_OF_COLOR;
    const uint32_t cpuDataSize    = EmbeddedEmulator_GetCpuDataSize();
    const uint32_t systemDataSize = EmbeddedEmulator_GetSystemDataSize();
    const uint32_t ppuDataSize    = EmbeddedEmulator_GetPpuDataSize();
    std::cout << "INFO: Allocate buffer" << std::endl
              << " - FB     : " << fbDataSize << " bytes" << std::endl
              << " - Cpu    : " << cpuDataSize << " bytes" << std::endl
              << " - System : " << systemDataSize << " bytes" << std::endl
              << " - Ppu    : " << ppuDataSize << " bytes" << std::endl;

    uint8_t* workBuf = new uint8_t[fbDataSize + cpuDataSize + systemDataSize + ppuDataSize];
    fbBuf     = &workBuf[0];
    cpuBuf    = &workBuf[fbDataSize];
    systemBuf = &workBuf[fbDataSize + cpuDataSize];
    ppuBuf    = &workBuf[fbDataSize + cpuDataSize + systemDataSize];

    // Emulator initialize
    std::cout << "INFO: Init emulator" << std::endl;
    EmbeddedEmulator_InitCpu(cpuBuf);
    EmbeddedEmulator_InitSystem(systemBuf);
    EmbeddedEmulator_InitPpu(ppuBuf);

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
    const uint32_t screenWidth  = static_cast<int>(EMBEDDED_EMULATOR_VISIBLE_SCREEN_WIDTH * scale);
    const uint32_t screenHeight = static_cast<int>(EMBEDDED_EMULATOR_VISIBLE_SCREEN_HEIGHT * scale);

    std::cout << "INFO: Init window" << std::endl;
    InitWindow(screenWidth, screenHeight, "rust-nes-emulator-embedded");
    if (fps > 0) {
        SetTargetFPS(fps);
    }

    // FrameBuffer Image
    Image fbImg = { fbBuf, EMBEDDED_EMULATOR_VISIBLE_SCREEN_WIDTH, EMBEDDED_EMULATOR_VISIBLE_SCREEN_WIDTH, 1, UNCOMPRESSED_R8G8B8A8 };
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

    // Start ppu thread
    std::cout << "INFO: Start ppu thread" << std::endl;
    std::thread ppuThread(runPpu);
    isPause = false;

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
            // pause ppu thread
            isPause = true;
            while (!hasPaused) {
                // wait for pause
            }
            // reset
            EmbeddedEmulator_Reset(cpuBuf, systemBuf, ppuBuf);
            // resume ppu thread
            isPause = false;
        }

        // Emulate cpu
        // On the real machine, you should be able to remove the loop and emulate it indefinitely.
        for(uint32_t cycleSum = 0; cycleSum < cyclePerFrame; ) {
            const uint32_t cyc = EmbeddedEmulator_EmulateCpu(cpuBuf, systemBuf);
            cycleSum += cyc;
            cpuCyc += cyc;
        }
        // Receive Interrupt
        if (interrupt != CpuInterrupt::NONE) {
            EmbeddedEmulator_InterruptCpu(cpuBuf, systemBuf, interrupt);
            // Release interrupt lock
            interrupt = CpuInterrupt::NONE;
        }

        // Draw
        Color* fbPtr = reinterpret_cast<Color*>(fbBuf);
        UpdateTexture(fbTexture, fbPtr);

        BeginDrawing();
        {
            DrawTextureEx(fbTexture, Vector2{ 0, 0 }, 0, scale, WHITE);
            DrawFPS(10, 10);
        }
        EndDrawing();
    }

    // Abort ppu thread
    std::cout << "INFO: Abort ppu thread" << std::endl;
    isAbort = true;
    ppuThread.join();

    // Finalize
    std::cout << "INFO: Finalize" << std::endl;
    UnloadTexture(fbTexture);
    CloseWindow();
    delete[] romBuf;
    delete[] workBuf;

    std::cout << "INFO: Exit" << std::endl;
    return 0;
}