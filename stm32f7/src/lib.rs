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

pub const EMBEDDED_EMULATOR_PLAYER_0: u32 = 0;
pub const EMBEDDED_EMULATOR_PLAYER_1: u32 = 1;

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

#[repr(u8)]
pub enum DrawPioxelFormat {
    RGBA8888,
    BGRA8888,
    ARGB8888,
}

/// 配列への参照を任意の型への参照に変換します
/// `raw_ref` - 参照先。 ARM向けを考慮すると、4byte alignした位置に配置されていることが望ましい
unsafe fn convert_ref<T>(raw_ref: &mut u8) -> &mut T {
    core::mem::transmute::<&mut u8, &mut T>(raw_ref)
}

/// 配列への参照に対し、特定の構造体とみなして初期化します
/// `raw_ref` - 構造体の参照先。 ARM向けを考慮すると、4byte alignした位置に配置されていることが望ましい
unsafe fn init_struct_ref<T: Default>(raw_ref: &mut u8) {
    let data_ref = convert_ref::<T>(raw_ref);
    *data_ref = T::default();
}

/// 画面全体を描画するのに必要なCPU Cylceを返します
#[no_mangle]
pub unsafe extern "C" fn EmbeddedEmulator_GetCpuCyclePerFrame() -> usize {
    CYCLE_PER_DRAW_FRAME
}

/// 1行描画するのに必要なCPU Cylceを返します
#[no_mangle]
pub unsafe extern "C" fn EmbeddedEmulator_GetCpuCyclePerLine() -> usize {
    CPU_CYCLE_PER_LINE
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
pub unsafe extern "C" fn EmbeddedEmulator_InitCpu(raw_ref: &mut u8) {
    init_struct_ref::<Cpu>(raw_ref);
}

/// Systemの構造体を初期化します
#[no_mangle]
pub unsafe extern "C" fn EmbeddedEmulator_InitSystem(raw_ref: &mut u8) {
    init_struct_ref::<System>(raw_ref);
}

/// Ppuの構造体を初期化します
#[no_mangle]
pub unsafe extern "C" fn EmbeddedEmulator_InitPpu(raw_ref: &mut u8) {
    init_struct_ref::<Ppu>(raw_ref);
}

/// Ppuの描画設定を更新します
#[no_mangle]
pub unsafe extern "C" fn EmbeddedEmulator_SetPpuDrawOption(
    raw_ppu_ref: &mut u8,
    fb_width: u32,
    fb_height: u32,
    offset_x: i32,
    offset_y: i32,
    scale: u32,
    draw_pixel_format: DrawPioxelFormat,
) {
    let ppu_ref = convert_ref::<Ppu>(raw_ppu_ref);
    let pixel_format = match draw_pixel_format {
        DrawPioxelFormat::RGBA8888 => PixelFormat::RGBA8888,
        DrawPioxelFormat::BGRA8888 => PixelFormat::BGRA8888,
        DrawPioxelFormat::ARGB8888 => PixelFormat::ARGB8888,
    };
    (*ppu_ref).draw_option.fb_width = fb_width;
    (*ppu_ref).draw_option.fb_height = fb_height;
    (*ppu_ref).draw_option.offset_x = offset_x;
    (*ppu_ref).draw_option.offset_y = offset_y;
    (*ppu_ref).draw_option.scale = scale;
    (*ppu_ref).draw_option.pixel_format = pixel_format;
}

/// CPUに特定の割り込みを送信します
#[no_mangle]
pub unsafe extern "C" fn EmbeddedEmulator_InterruptCpu(
    raw_cpu_ref: &mut u8,
    raw_system_ref: &mut u8,
    interrupt: CpuInterrupt,
) {
    let cpu_ref = convert_ref::<Cpu>(raw_cpu_ref);
    let system_ref = convert_ref::<System>(raw_system_ref);
    let irq = match interrupt {
        CpuInterrupt::NMI => Interrupt::NMI,
        CpuInterrupt::RESET => Interrupt::RESET,
        CpuInterrupt::IRQ => Interrupt::IRQ,
        CpuInterrupt::BRK => Interrupt::BRK,
        _ => Interrupt::RESET, // とりあえずResetにしておく
    };
    (*cpu_ref).interrupt(&mut *system_ref, irq);
}

/// エミュレータをリセットします
/// 各種変数の初期化後、RESET割り込みが行われます
#[no_mangle]
pub unsafe extern "C" fn EmbeddedEmulator_Reset(
    raw_cpu_ref: &mut u8,
    raw_system_ref: &mut u8,
    raw_ppu_ref: &mut u8,
) {
    let cpu_ref = convert_ref::<Cpu>(raw_cpu_ref);
    let system_ref = convert_ref::<System>(raw_system_ref);
    let ppu_ref = convert_ref::<Ppu>(raw_ppu_ref);
    (*cpu_ref).reset();
    (*system_ref).reset();
    (*ppu_ref).reset();
    (*cpu_ref).interrupt(&mut *system_ref, Interrupt::RESET);
}

/// ROMを読み込みます
/// 成功した場合はtrueが返ります。実行中のエミュレートは中止して、Resetをかけてください
#[no_mangle]
pub unsafe extern "C" fn EmbeddedEmulator_LoadRom(
    raw_system_ref: &mut u8,
    rom_ref: *const u8,
) -> bool {
    let system_ref = convert_ref::<System>(raw_system_ref);
    (*system_ref)
        .cassette
        .from_ines_binary(|addr: usize| *rom_ref.offset(addr as isize))
}

/// CPUを1stepエミュレーションします
#[no_mangle]
pub unsafe extern "C" fn EmbeddedEmulator_EmulateCpu(
    raw_cpu_ref: &mut u8,
    raw_system_ref: &mut u8,
) -> u8 {
    let cpu_ref = convert_ref::<Cpu>(raw_cpu_ref);
    let system_ref = convert_ref::<System>(raw_system_ref);

    (*cpu_ref).step(&mut (*system_ref))
}

/// PPUをエミュレーションします。cpu cycを基準にlineごとに進めます
/// `cpu_cyc`: cpuでエミュレーション経過済で、PPU側に未反映のCPU Cycle数合計
#[no_mangle]
pub unsafe extern "C" fn EmbeddedEmulator_EmulatePpu(
    raw_ppu_ref: &mut u8,
    raw_system_ref: &mut u8,
    fb_ptr: *mut u8,
    cpu_cycle: usize,
) -> CpuInterrupt {
    let ppu_ref = convert_ref::<Ppu>(raw_ppu_ref);
    let system_ref = convert_ref::<System>(raw_system_ref);

    match (*ppu_ref).step(cpu_cycle, &mut (*system_ref), fb_ptr) {
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
    raw_system_ref: &mut u8,
    player_num: u32,
    key: KeyEvent,
) {
    let system_ref = convert_ref::<System>(raw_system_ref);
    let p = match player_num {
        0 => &mut (*system_ref).pad1,
        1 => &mut (*system_ref).pad2,
        _ => &mut (*system_ref).pad1, // とりあえず1Pにしておく
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
