#include "mbed.h"

// for peripheral
#include "stm32f769i_discovery.h"
#include "stm32f769i_discovery_lcd.h"
#include "stm32f769i_discovery_sdram.h"
#include "stm32f769i_discovery_ts.h"
#include "stm32f769i_discovery_sd.h"
#include "stm32f769i_discovery_audio.h"

// for system
#include "stm32f7xx_hal_flash.h"
#include "core_cm7.h"

// for emulator
#include "rust_nes_emulator.h"

// EMU_WORK on DTCM 64K
#define EMU_WORK_SIZE (64*1024) // 64K
MBED_ALIGN(32) uint8_t emuWorkBuffer[EMU_WORK_SIZE];

// user specified values
#define PRINT_MESSAGE_HEIGHT          (20)
#define SDRAM_BASE_ADDR               (SDRAM_DEVICE_ADDR)       //  0xc000_0000
#define SDRAM_SIZE                    (SDRAM_DEVICE_SIZE)       // ~0xc100_0000(16MByte)
#define DISPLAY_WIDTH                 (OTM8009A_800X480_WIDTH)  // 800
#define DISPLAY_HEIGHT                (OTM8009A_800X480_HEIGHT) // 480

void initSystem(void) {
    // Enabled I/D Cache, Prefetch Buffer
    __HAL_FLASH_ART_ENABLE();
    __HAL_FLASH_PREFETCH_BUFFER_ENABLE();
    SCB_EnableDCache();
    SCB_EnableICache();
    __DMB();
}

void initLcd(void) {
    BSP_LCD_Init();
    BSP_LCD_LayerDefaultInit(0, SDRAM_BASE_ADDR);
    BSP_LCD_Clear(LCD_COLOR_BLACK);
    BSP_LCD_FillRect(0, 0, BSP_LCD_GetXSize(), 40);
    BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
    BSP_LCD_SetBackColor(LCD_COLOR_BLACK);
    BSP_LCD_SetFont(&Font24);
}

bool initSdram(void) {
    return (BSP_SDRAM_Init() == SDRAM_OK);
}

bool initTouchscreen(void) {
    return BSP_TS_Init(BSP_LCD_GetXSize(), BSP_LCD_GetYSize()) == TS_OK;
}

bool initSd(void) {
    return (BSP_SD_Init() == MSD_OK);
}

bool readRomFromSd(uint8_t* buffer) {
    // TODO: FATFS support
    const uint32_t readBlockAddr = 0;
    const uint32_t readBytes = 64 * 1024; // 64KByte
    const uint32_t blockSize = 512;
    const uint32_t numOfReadBlocks = readBytes / blockSize;
    const uint32_t readTimeout = 1000;

    // read datas
    if (MSD_OK != BSP_SD_ReadBlocks(reinterpret_cast<uint32_t*>(buffer), readBlockAddr, numOfReadBlocks, readTimeout)) {
        return false;
    }

    // .NES Header Pre-check
    if ((buffer[0] != 0x4e) || (buffer[1] != 0x45) || (buffer[2] != 0x53) || (buffer[3] != 0x1a)) {
        return false;
    }

    // success
    return true;
}

int main(void) {
    // Allocate SDRAM Buffer
    // Frame Buffer:
    // - [0xc000_0000 - 0xc017_6fff]
    // - [0xc017_7000 - 0xc02e_dfff]
    // General Buffer:
    // - [0xc02e_e000 - ]
    const uint32_t frameBufferSize   = (DISPLAY_WIDTH * DISPLAY_HEIGHT * EMBEDDED_EMULATOR_NUM_OF_COLOR); // 138800byte
    uint8_t* frameBuffer0Ptr   = reinterpret_cast<uint8_t*>(SDRAM_BASE_ADDR);
    uint8_t* frameBuffer1Ptr   = frameBuffer0Ptr + frameBufferSize;

    uint8_t* generalBufferPtr  = frameBuffer1Ptr + frameBufferSize;
    const uint32_t generalBufferSize = (SDRAM_SIZE - reinterpret_cast<uint32_t>(generalBufferPtr));
    uint8_t* romBuf = generalBufferPtr;

    // work data
    char msg[128];
    uint32_t messageLine = 0;

    // Init core peripheral
    initSystem();
    initLcd();

    // Print startup info
    BSP_LCD_DisplayStringAt(0, (messageLine++ * PRINT_MESSAGE_HEIGHT), (uint8_t *)"kamiyaowl/rust-nes-emulator-embedded", LEFT_MODE);
    sprintf(msg, "[DEBUG] SystemCoreClock: %ld MHz", SystemCoreClock / 1000000ul);
    BSP_LCD_DisplayStringAt(0, (messageLine++ * PRINT_MESSAGE_HEIGHT), (uint8_t *)msg, LEFT_MODE);

    // Init other peripherals
    BSP_LCD_DisplayStringAt(0, (messageLine++ * PRINT_MESSAGE_HEIGHT), (uint8_t *)"[INFO ] Init SDRAM", LEFT_MODE);
    if(!initSdram()) {
        BSP_LCD_DisplayStringAt(0, (messageLine++ * PRINT_MESSAGE_HEIGHT), (uint8_t *)"[ERROR] FAILED", LEFT_MODE);
        while(1);
    }

    BSP_LCD_DisplayStringAt(0, (messageLine++ * PRINT_MESSAGE_HEIGHT), (uint8_t *)"[INFO ] Init Touchscreen", LEFT_MODE);
    if (!initTouchscreen()) {
        BSP_LCD_DisplayStringAt(0, (messageLine++ * PRINT_MESSAGE_HEIGHT), (uint8_t *)"[ERROR] FAILED", LEFT_MODE);
        while(1);
    }   

    BSP_LCD_DisplayStringAt(0, (messageLine++ * PRINT_MESSAGE_HEIGHT), (uint8_t *)"[INFO ] Init SD", LEFT_MODE);
    if (!initSd()) {
        BSP_LCD_DisplayStringAt(0, (messageLine++ * PRINT_MESSAGE_HEIGHT), (uint8_t *)"[ERROR] FAILED", LEFT_MODE);
        while(1);
    }   
    
    // Read binary from SD Card
    BSP_LCD_DisplayStringAt(0, (messageLine++ * PRINT_MESSAGE_HEIGHT), (uint8_t *)"[INFO ] Read ROM from SD", LEFT_MODE);
    if (!readRomFromSd(generalBufferPtr)) {
        BSP_LCD_DisplayStringAt(0, (messageLine++ * PRINT_MESSAGE_HEIGHT), (uint8_t *)"[ERROR] FAILED", LEFT_MODE);
        // Show HexDump
        for (uint32_t i = 0; i < 0x10; i++) {
            const uint32_t offset = i * 0x10;
            sprintf(
                msg, 
                "%08x: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x ", 
                generalBufferPtr + offset,
                generalBufferPtr[offset + 0x00], generalBufferPtr[offset + 0x01], generalBufferPtr[offset + 0x02], generalBufferPtr[offset + 0x03], 
                generalBufferPtr[offset + 0x04], generalBufferPtr[offset + 0x05], generalBufferPtr[offset + 0x06], generalBufferPtr[offset + 0x07], 
                generalBufferPtr[offset + 0x08], generalBufferPtr[offset + 0x09], generalBufferPtr[offset + 0x0a], generalBufferPtr[offset + 0x0b], 
                generalBufferPtr[offset + 0x0c], generalBufferPtr[offset + 0x0d], generalBufferPtr[offset + 0x0e], generalBufferPtr[offset + 0x0f]
            );
            BSP_LCD_DisplayStringAt(0, (messageLine++ * PRINT_MESSAGE_HEIGHT), (uint8_t *)msg, LEFT_MODE);
        }
        while(1);
    }

    // Assign Emu work buffer
    const uint32_t cpuDataSize    = EmbeddedEmulator_GetCpuDataSize();
    const uint32_t systemDataSize = EmbeddedEmulator_GetSystemDataSize();
    const uint32_t ppuDataSize    = EmbeddedEmulator_GetPpuDataSize();
    uint8_t* cpuBuf    = &emuWorkBuffer[0];
    uint8_t* systemBuf = &emuWorkBuffer[cpuDataSize];
    uint8_t* ppuBuf    = &emuWorkBuffer[cpuDataSize + systemDataSize];

    // Init emulator
    const uint32_t scale = 2;
    const uint32_t fps   = 60; // TODO: Support

    const uint32_t screenWidth  = DISPLAY_WIDTH;
    const uint32_t screenHeight = DISPLAY_HEIGHT;
    const uint32_t offsetX = (screenWidth / 2) - (EMBEDDED_EMULATOR_VISIBLE_SCREEN_WIDTH * scale / 2);
    const uint32_t offsetY = 0;

    EmbeddedEmulator_InitCpu(cpuBuf);
    EmbeddedEmulator_InitSystem(systemBuf);
    EmbeddedEmulator_InitPpu(ppuBuf);
    EmbeddedEmulator_SetPpuDrawOption(ppuBuf, screenWidth, screenHeight, offsetX, offsetY, scale, DrawPioxelFormat::BGRA8888);

    // Load ROM
    BSP_LCD_DisplayStringAt(0, (messageLine++ * PRINT_MESSAGE_HEIGHT), (uint8_t *)"[INFO ] Load ROM", LEFT_MODE);

    const bool isLoad = EmbeddedEmulator_LoadRom(systemBuf, romBuf);
    if (!isLoad) {
        BSP_LCD_DisplayStringAt(0, (messageLine++ * PRINT_MESSAGE_HEIGHT), (uint8_t *)"[ERROR] FAILED", LEFT_MODE);
        while(1);
    }

    // Reset
    BSP_LCD_DisplayStringAt(0, (messageLine++ * PRINT_MESSAGE_HEIGHT), (uint8_t *)"[INFO ] Reset Emulator", LEFT_MODE);
    EmbeddedEmulator_Reset(cpuBuf, systemBuf, ppuBuf);

    // Start Emulation
    BSP_LCD_DisplayStringAt(0, (messageLine++ * PRINT_MESSAGE_HEIGHT), (uint8_t *)"[INFO ] Start Emulation", LEFT_MODE);
    wait_ms(1000);
    BSP_LCD_Clear(LCD_COLOR_BLACK);
    const uint32_t cyclePerFrame = EmbeddedEmulator_GetCpuCyclePerFrame();
    while(1) {
        // TODO: Input
        // for (const auto& [key, value]: keyMaps) {
        //     if (IsKeyPressed(key)) {
        //         EmbeddedEmulator_UpdateKey(systemBuf, EMBEDDED_EMULATOR_PLAYER_0, std::get<0>(value));
        //     }
        //     if (IsKeyReleased(key)) {
        //         EmbeddedEmulator_UpdateKey(systemBuf, EMBEDDED_EMULATOR_PLAYER_0, std::get<1>(value));
        //     }
        // }
        // if (IsKeyReleased(KEY_R)) {
        //     std::cout << "INFO: Reset" << std::endl;
        //     EmbeddedEmulator_Reset(cpuBuf, systemBuf, ppuBuf);
        // }

        // Emulate cpu/ppu
        for(uint32_t cycleSum = 0; cycleSum < cyclePerFrame; ) {
            // emulate cpu
            const uint32_t cyc = EmbeddedEmulator_EmulateCpu(cpuBuf, systemBuf);
            cycleSum += cyc;
            // emulate ppu
            const CpuInterrupt irq = EmbeddedEmulator_EmulatePpu(ppuBuf, systemBuf, frameBuffer0Ptr, cyc);
            // Interrupt from ppu
            if (irq != CpuInterrupt::NONE) {
                EmbeddedEmulator_InterruptCpu(cpuBuf, systemBuf, irq);
            }
        }
    }
}