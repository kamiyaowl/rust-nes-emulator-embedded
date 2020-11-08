#![no_std]
#![feature(lang_items, core_intrinsics)]

use core::intrinsics;
use core::mem;
use core::panic::PanicInfo;

#[panic_handler]
#[no_mangle]
pub fn panic(_info: &PanicInfo) -> ! {
    unsafe { intrinsics::abort() }
}

#[lang = "eh_personality"]
#[no_mangle]
pub extern "C" fn rust_eh_personality() {}

extern crate rust_nes_emulator;
use rust_nes_emulator::prelude::*;

pub const EMBEDDED_EMULATOR_NUM_OF_COLOR: usize = 4;
pub const EMBEDDED_EMULATOR_VISIBLE_SCREEN_WIDTH: usize = 256;
pub const EMBEDDED_EMULATOR_VISIBLE_SCREEN_HEIGHT: usize = 240;

// 泣く泣くの策、structをそのままcに公開できなかったので
static mut EMULATOR: Option<EmbeddedEmulator> = None;

#[repr(u8)]
pub enum KeyEvent {
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
}

#[repr(u8)]
pub enum CpuInterrupt {
    NMI,
    RESET,
    IRQ,
    BRK,
    NONE,
}

/// 配列への参照を構造体への参照に変換します
/// `raw_ptr` - 構造体の参照先。 ARM向けを考慮すると、4byte alignした位置に配置されていることが望ましい
unsafe fn get_struct_ptr<T>(raw_ptr: &mut u8) -> &mut T {
    core::mem::transmute::<&mut u8, &mut T>(raw_ptr)
}

/// 配列への参照に対し、特定の構造体とみなして初期化します
/// `raw_ptr` - 構造体の参照先。 ARM向けを考慮すると、4byte alignした位置に配置されていることが望ましい
unsafe fn init_struct_ptr<T: Default>(raw_ptr: &mut u8) {
    let data_ptr = get_struct_ptr::<T>(raw_ptr);
    *data_ptr = T::default();
}

/// 画面全体を描画するのに必要なCPU Cylceを返します
#[no_mangle]
pub unsafe extern "C" fn EmbeddedEmulator_GetCpuCyclePerFrame() -> usize {
    CYCLE_PER_DRAW_FRAME
}

/// Cpuのデータ構造に必要なサイズを返します
#[no_mangle]
pub unsafe extern "C" fn EmbeddedEmulator_GetCpuDataSize() -> usize {
    mem::size_of::<Cpu>()
}

/// SubSystemのデータ構造に必要なサイズを返します
#[no_mangle]
pub unsafe extern "C" fn EmbeddedEmulator_GetSystemDataSize() -> usize {
    mem::size_of::<System>()
}

/// Ppuのデータ構造に必要なサイズを返します
#[no_mangle]
pub unsafe extern "C" fn EmbeddedEmulator_GetPpuDataSize() -> usize {
    mem::size_of::<Ppu>()
}

/// Cpuの構造体を初期化します
#[no_mangle]
pub unsafe extern "C" fn EmbeddedEmulator_InitCpu(raw_ptr: &mut u8) {
    init_struct_ptr::<Cpu>(raw_ptr);
}

/// Systemの構造体を初期化します
#[no_mangle]
pub unsafe extern "C" fn EmbeddedEmulator_InitSystem(raw_ptr: &mut u8) {
    init_struct_ptr::<System>(raw_ptr);
}

/// Ppuの構造体を初期化します
#[no_mangle]
pub unsafe extern "C" fn EmbeddedEmulator_InitPpu(raw_ptr: &mut u8) {
    init_struct_ptr::<Ppu>(raw_ptr);
}

/// CPUに特定の割り込みを送信します
#[no_mangle]
pub unsafe extern "C" fn EmbeddedEmulator_InterruptCpu(
    raw_cpu_ptr: &mut u8,
    raw_system_ptr: &mut u8,
    interrupt: CpuInterrupt,
) {
    let cpu_ptr = get_struct_ptr::<Cpu>(raw_cpu_ptr);
    let system_ptr = get_struct_ptr::<System>(raw_system_ptr);
    let irq = match interrupt {
        CpuInterrupt::NMI => Interrupt::NMI,
        CpuInterrupt::RESET => Interrupt::RESET,
        CpuInterrupt::IRQ => Interrupt::IRQ,
        CpuInterrupt::BRK => Interrupt::BRK,
        _ => Interrupt::RESET, // とりあえずResetにしておく
    };
    (*cpu_ptr).interrupt(&mut *system_ptr, irq);
}

/// エミュレータをリセットします
/// 各種変数の初期化後、RESET割り込みが行われます
#[no_mangle]
pub unsafe extern "C" fn EmbeddedEmulator_Reset(
    raw_cpu_ptr: &mut u8,
    raw_system_ptr: &mut u8,
    raw_ppu_ptr: &mut u8
) {
    let cpu_ptr = get_struct_ptr::<Cpu>(raw_cpu_ptr);
    let system_ptr = get_struct_ptr::<System>(raw_system_ptr);
    let ppu_ptr = get_struct_ptr::<Ppu>(raw_ppu_ptr);
    (*cpu_ptr).reset();
    (*system_ptr).reset();
    (*ppu_ptr).reset();
    (*cpu_ptr).interrupt(&mut *system_ptr, Interrupt::RESET);
}

/// ROMを読み込みます
/// 成功した場合はtrueが返ります。実行中のエミュレートは中止して、Resetをかけてください
#[no_mangle]
pub unsafe extern "C" fn EmbeddedEmulator_LoadRom(
    raw_system_ptr: &mut u8,
    rom_ptr: *const u8,
) -> bool {
    let system_ptr = get_struct_ptr::<System>(raw_system_ptr);
    (*system_ptr)
        .cassette
        .from_ines_binary(|addr: usize| *rom_ptr.offset(addr as isize))
}

/// CPUを1cycエミュレーションします
#[no_mangle]
pub unsafe extern "C" fn EmbeddedEmulator_EmulateCpu(
    raw_cpu_ptr: &mut u8,
    raw_system_ptr: &mut u8,
) -> u8 {
    let cpu_ptr = get_struct_ptr::<Cpu>(raw_cpu_ptr);
    let system_ptr = get_struct_ptr::<System>(raw_system_ptr);

    (*cpu_ptr).step(&mut (*system_ptr))
}

/// PPUをエミュレーションします。cpu cycを基準に進めます
/// `cpu_cyc`: cpuでエミュレーション経過済で、PPU側に未反映のCPU Cycle数合計
#[no_mangle]
pub unsafe extern "C" fn EmbeddedEmulator_EmulatePpu(
    raw_ppu_ptr: &mut u8,
    raw_system_ptr: &mut u8,
    cpu_cycle: usize,
    fb: &mut [[[u8; EMBEDDED_EMULATOR_NUM_OF_COLOR]; EMBEDDED_EMULATOR_VISIBLE_SCREEN_WIDTH];
             EMBEDDED_EMULATOR_VISIBLE_SCREEN_HEIGHT],
) -> CpuInterrupt {
    let ppu_ptr = get_struct_ptr::<Ppu>(raw_ppu_ptr);
    let system_ptr = get_struct_ptr::<System>(raw_system_ptr);

    match (*ppu_ptr).step(cpu_cycle, &mut (*system_ptr), fb) {
        Some(Interrupt::NMI) => CpuInterrupt::NMI,
        Some(Interrupt::RESET) => CpuInterrupt::RESET,
        Some(Interrupt::IRQ) => CpuInterrupt::IRQ,
        Some(Interrupt::BRK) => CpuInterrupt::BRK,
        None => CpuInterrupt::NONE,
    }
}

/// キー入力を反映
/// `player_num` - Player番号, 0 or 1
#[no_mangle]
pub unsafe extern "C" fn EmbeddedEmulator_UpdateKey(
    raw_system_ptr: &mut u8,
    player_num: u32,
    key: KeyEvent,
) {
    let system_ptr = get_struct_ptr::<System>(raw_system_ptr);
    let p = match player_num {
        0 => &mut (*system_ptr).pad1,
        1 => &mut (*system_ptr).pad2,
        _ => &mut (*system_ptr).pad1, // とりあえず1Pにしておく
    };

    match key {
        KeyEvent::PressA => p.push_button(PadButton::A),
        KeyEvent::PressB => p.push_button(PadButton::B),
        KeyEvent::PressSelect => p.push_button(PadButton::Select),
        KeyEvent::PressStart => p.push_button(PadButton::Start),
        KeyEvent::PressUp => p.push_button(PadButton::Up),
        KeyEvent::PressDown => p.push_button(PadButton::Down),
        KeyEvent::PressLeft => p.push_button(PadButton::Left),
        KeyEvent::PressRight => p.push_button(PadButton::Right),

        KeyEvent::ReleaseA => p.release_button(PadButton::A),
        KeyEvent::ReleaseB => p.release_button(PadButton::B),
        KeyEvent::ReleaseSelect => p.release_button(PadButton::Select),
        KeyEvent::ReleaseStart => p.release_button(PadButton::Start),
        KeyEvent::ReleaseUp => p.release_button(PadButton::Up),
        KeyEvent::ReleaseDown => p.release_button(PadButton::Down),
        KeyEvent::ReleaseLeft => p.release_button(PadButton::Left),
        KeyEvent::ReleaseRight => p.release_button(PadButton::Right),
    };
}

pub struct EmbeddedEmulator {
    pub cpu: Cpu,
    pub cpu_sys: System,
    pub ppu: Ppu,
}

impl Default for EmbeddedEmulator {
    fn default() -> Self {
        Self {
            cpu: Cpu::default(),
            cpu_sys: System::default(),
            ppu: Ppu::default(),
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn EmbeddedEmulator_init() {
    EMULATOR = Some(EmbeddedEmulator::default());
}

/// エミュレータをリセットします
/// カセットの中身はリセットしないので実機のリセット相当の処理です
#[no_mangle]
pub unsafe extern "C" fn EmbeddedEmulator_reset() {
    if let Some(ref mut emu) = EMULATOR {
        emu.cpu.reset();
        emu.cpu_sys.reset();
        emu.ppu.reset();
        emu.cpu.interrupt(&mut emu.cpu_sys, Interrupt::RESET);
    }
}

/// .nesファイルを読み込みます
/// `bin_ptr` - nesファイルのバイナリの先頭ポインタ
#[no_mangle]
pub unsafe extern "C" fn EmbeddedEmulator_load(bin_ptr: *const u8) -> bool {
    if let Some(ref mut emu) = EMULATOR {
        let success = emu
            .cpu_sys
            .cassette
            .from_ines_binary(|addr: usize| *bin_ptr.offset(addr as isize));
        if success {
            EmbeddedEmulator_reset();
        }
        success
    } else {
        false
    }
}

/// 描画領域1面分更新します
#[no_mangle]
pub unsafe extern "C" fn EmbeddedEmulator_update_screen(
    fb: &mut [[[u8; EMBEDDED_EMULATOR_NUM_OF_COLOR]; EMBEDDED_EMULATOR_VISIBLE_SCREEN_WIDTH];
             EMBEDDED_EMULATOR_VISIBLE_SCREEN_HEIGHT],
) {
    if let Some(ref mut emu) = EMULATOR {
        let mut total_cycle: usize = 0;
        while total_cycle < CYCLE_PER_DRAW_FRAME {
            let cpu_cycle = usize::from(emu.cpu.step(&mut emu.cpu_sys));
            if let Some(interrupt) = emu.ppu.step(cpu_cycle, &mut emu.cpu_sys, fb) {
                emu.cpu.interrupt(&mut emu.cpu_sys, interrupt);
            }
            total_cycle = total_cycle + cpu_cycle;
        }
    }
}

/// キー入力します
#[no_mangle]
pub unsafe extern "C" fn EmbeddedEmulator_update_key(key: KeyEvent) {
    if let Some(ref mut emu) = EMULATOR {
        match key {
            KeyEvent::PressA => emu.cpu_sys.pad1.push_button(PadButton::A),
            KeyEvent::PressB => emu.cpu_sys.pad1.push_button(PadButton::B),
            KeyEvent::PressSelect => emu.cpu_sys.pad1.push_button(PadButton::Select),
            KeyEvent::PressStart => emu.cpu_sys.pad1.push_button(PadButton::Start),
            KeyEvent::PressUp => emu.cpu_sys.pad1.push_button(PadButton::Up),
            KeyEvent::PressDown => emu.cpu_sys.pad1.push_button(PadButton::Down),
            KeyEvent::PressLeft => emu.cpu_sys.pad1.push_button(PadButton::Left),
            KeyEvent::PressRight => emu.cpu_sys.pad1.push_button(PadButton::Right),

            KeyEvent::ReleaseA => emu.cpu_sys.pad1.release_button(PadButton::A),
            KeyEvent::ReleaseB => emu.cpu_sys.pad1.release_button(PadButton::B),
            KeyEvent::ReleaseSelect => emu.cpu_sys.pad1.release_button(PadButton::Select),
            KeyEvent::ReleaseStart => emu.cpu_sys.pad1.release_button(PadButton::Start),
            KeyEvent::ReleaseUp => emu.cpu_sys.pad1.release_button(PadButton::Up),
            KeyEvent::ReleaseDown => emu.cpu_sys.pad1.release_button(PadButton::Down),
            KeyEvent::ReleaseLeft => emu.cpu_sys.pad1.release_button(PadButton::Left),
            KeyEvent::ReleaseRight => emu.cpu_sys.pad1.release_button(PadButton::Right),
        }
    }
}
