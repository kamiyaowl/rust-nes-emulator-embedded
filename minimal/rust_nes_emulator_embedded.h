#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <new>

static const uintptr_t EMBEDDED_EMULATOR_NUM_OF_COLOR = 4;

static const uintptr_t EMBEDDED_EMULATOR_VISIBLE_SCREEN_HEIGHT = 240;

static const uintptr_t EMBEDDED_EMULATOR_VISIBLE_SCREEN_WIDTH = 256;

enum class CpuInterrupt : uint8_t {
  NMI,
  RESET,
  IRQ,
  BRK,
  NONE,
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

/// CPUを1cycエミュレーションします
uint8_t EmbeddedEmulator_EmulateCpu(uint8_t *raw_cpu_ptr, uint8_t *raw_system_ptr);

/// PPUをエミュレーションします。cpu cycを基準に進めます
/// `cpu_cyc`: cpuでエミュレーション経過済で、PPU側に未反映のCPU Cycle数合計
CpuInterrupt EmbeddedEmulator_EmulatePpu(uint8_t *raw_ppu_ptr,
                                         uint8_t *raw_system_ptr,
                                         uintptr_t cpu_cycle,
                                         uint8_t (*fb)[EMBEDDED_EMULATOR_VISIBLE_SCREEN_HEIGHT][EMBEDDED_EMULATOR_VISIBLE_SCREEN_WIDTH][EMBEDDED_EMULATOR_NUM_OF_COLOR]);

/// 画面全体を描画するのに必要なCPU Cylceを返します
uintptr_t EmbeddedEmulator_GetCpuCyclePerFrame();

/// Cpuのデータ構造に必要なサイズを返します
uintptr_t EmbeddedEmulator_GetCpuDataSize();

/// Ppuのデータ構造に必要なサイズを返します
uintptr_t EmbeddedEmulator_GetPpuDataSize();

/// SubSystemのデータ構造に必要なサイズを返します
uintptr_t EmbeddedEmulator_GetSystemDataSize();

/// Cpuの構造体を初期化します
void EmbeddedEmulator_InitCpu(uint8_t *raw_ptr);

/// Ppuの構造体を初期化します
void EmbeddedEmulator_InitPpu(uint8_t *raw_ptr);

/// Systemの構造体を初期化します
void EmbeddedEmulator_InitSystem(uint8_t *raw_ptr);

/// CPUに特定の割り込みを送信します
void EmbeddedEmulator_InterruptCpu(uint8_t *raw_cpu_ptr,
                                   uint8_t *raw_system_ptr,
                                   CpuInterrupt interrupt);

/// ROMを読み込みます
/// 成功した場合はtrueが返ります。実行中のエミュレートは中止して、Resetをかけてください
bool EmbeddedEmulator_LoadRom(uint8_t *raw_system_ptr,
                              const uint8_t *rom_ptr);

/// エミュレータをリセットします
/// 各種変数の初期化後、RESET割り込みが行われます
void EmbeddedEmulator_Reset(uint8_t *raw_cpu_ptr, uint8_t *raw_system_ptr, uint8_t *raw_ppu_ptr);

/// キー入力を反映
/// `player_num` - Player番号, 0 or 1
void EmbeddedEmulator_UpdateKey(uint8_t *raw_system_ptr, uint32_t player_num, KeyEvent key);

void EmbeddedEmulator_init();

/// .nesファイルを読み込みます
/// `bin_ptr` - nesファイルのバイナリの先頭ポインタ
bool EmbeddedEmulator_load(const uint8_t *bin_ptr);

/// エミュレータをリセットします
/// カセットの中身はリセットしないので実機のリセット相当の処理です
void EmbeddedEmulator_reset();

/// キー入力します
void EmbeddedEmulator_update_key(KeyEvent key);

/// 描画領域1面分更新します
void EmbeddedEmulator_update_screen(uint8_t (*fb)[EMBEDDED_EMULATOR_VISIBLE_SCREEN_HEIGHT][EMBEDDED_EMULATOR_VISIBLE_SCREEN_WIDTH][EMBEDDED_EMULATOR_NUM_OF_COLOR]);

void rust_eh_personality();

} // extern "C"
