CC = g++
CFLAGS = 

.PHONY: run
run: build
	./main.out

.PHONY: run-release
run-release: build
	./main.out

.PHONY: clean
clean:
	cargo clean
	rm -f ./main.out

.PHONY: build
build: build-rust
	rustup run nightly cargo build
	g++ -o main.out main.cpp ./target/debug/librust_nes_emulator_minimal.so

.PHONY: build-release
build-release: build-rust-release
	g++ -O3 -o main.out main.cpp ./target/release/librust_nes_emulator_minimal.so

.PHONY: build-rust
build-rust:
	rustup run nightly cargo build

.PHONY: build-rust-release
build-rust-release:
	rustup run nightly cargo build --release
