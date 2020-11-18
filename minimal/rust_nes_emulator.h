#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <new>

static const uintptr_t EMBEDDED_EMULATOR_NUM_OF_COLOR = 4;

static const uint32_t EMBEDDED_EMULATOR_PLAYER_0 = 0;

static const uint32_t EMBEDDED_EMULATOR_PLAYER_1 = 1;

static const uintptr_t EMBEDDED_EMULATOR_VISIBLE_SCREEN_HEIGHT = 240;

static const uintptr_t EMBEDDED_EMULATOR_VISIBLE_SCREEN_WIDTH = 256;

enum class CpuInterrupt : uint8_t {
  NMI,
  RESET,
  IRQ,
  BRK,
  NONE,
};

enum class DrawPioxelFormat : uint8_t {
  RGBA8888,
  BGRA8888,
  ARGB8888,
};

enum class KeyEvent : uint8_t {
  PressA,
  PressB,
  PressSelect,
  PressStart,
  PressUp,
  PressDown,
  PressLeft,
  PressRight,
  ReleaseA,
  ReleaseB,
  ReleaseSelect,
  ReleaseStart,
  ReleaseUp,
  ReleaseDown,
  ReleaseLeft,
  ReleaseRight,
};

extern "C" {

/// CPUを1stepエミュレーションします
uint8_t EmbeddedEmulator_EmulateCpu(uint8_t *raw_cpu_ref, uint8_t *raw_system_ref);

/// PPUをエミュレーションします。cpu cycを基準にlineごとに進めます
/// `cpu_cyc`: cpuでエミュレーション経過済で、PPU側に未反映のCPU Cycle数合計
CpuInterrupt EmbeddedEmulator_EmulatePpu(uint8_t *raw_ppu_ref,
                                         uint8_t *raw_system_ref,
                                         uint8_t *fb_ptr,
                                         uintptr_t cpu_cycle);

/// 画面全体を描画するのに必要なCPU Cylceを返します
uintptr_t EmbeddedEmulator_GetCpuCyclePerFrame();

/// 1行描画するのに必要なCPU Cylceを返します
uintptr_t EmbeddedEmulator_GetCpuCyclePerLine();

/// Cpuのデータ構造に必要なサイズを返します
uintptr_t EmbeddedEmulator_GetCpuDataSize();

/// Ppuのデータ構造に必要なサイズを返します
uintptr_t EmbeddedEmulator_GetPpuDataSize();

/// SubSystemのデータ構造に必要なサイズを返します
uintptr_t EmbeddedEmulator_GetSystemDataSize();

/// Cpuの構造体を初期化します
void EmbeddedEmulator_InitCpu(uint8_t *raw_ref);

/// Ppuの構造体を初期化します
void EmbeddedEmulator_InitPpu(uint8_t *raw_ref);

/// Systemの構造体を初期化します
void EmbeddedEmulator_InitSystem(uint8_t *raw_ref);

/// CPUに特定の割り込みを送信します
void EmbeddedEmulator_InterruptCpu(uint8_t *raw_cpu_ref,
                                   uint8_t *raw_system_ref,
                                   CpuInterrupt interrupt);

/// ROMを読み込みます
/// 成功した場合はtrueが返ります。実行中のエミュレートは中止して、Resetをかけてください
bool EmbeddedEmulator_LoadRom(uint8_t *raw_system_ref,
                              const uint8_t *rom_ref);

/// エミュレータをリセットします
/// 各種変数の初期化後、RESET割り込みが行われます
void EmbeddedEmulator_Reset(uint8_t *raw_cpu_ref, uint8_t *raw_system_ref, uint8_t *raw_ppu_ref);

/// Ppuの描画設定を更新します
void EmbeddedEmulator_SetPpuDrawOption(uint8_t *raw_ppu_ref,
                                       uint32_t fb_width,
                                       uint32_t fb_height,
                                       int32_t offset_x,
                                       int32_t offset_y,
                                       uint32_t scale,
                                       DrawPioxelFormat draw_pixel_format);

/// キー入力を反映
/// `player_num` - Player番号, 0 or 1
void EmbeddedEmulator_UpdateKey(uint8_t *raw_system_ref, uint32_t player_num, KeyEvent key);

void rust_eh_personality();

} // extern "C"
