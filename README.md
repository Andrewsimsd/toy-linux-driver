# toy-linux-driver

A minimal end-to-end example showing:

- A Rust userspace application that continually writes fixed-size messages to `/dev/messagesender`.
- A Linux kernel toy platform driver (bound from device tree) that exposes `/dev/messagesender` and writes incoming values directly to a mapped MMIO register window.
- A PetaLinux device-tree include snippet (`.dtsi`) for integrating the device node.

## Message format

Each message is exactly 16 bytes and contains four little-endian `u32` values:

1. `a`
2. `b`
3. `c`
4. `d`

The Rust app starts with base value `0` and sends `{0,1,2,3}`, `{4,5,6,7}`, `{8,9,10,11}`, and so on.

The mapped register window for this toy device is 16 bytes total, with four 32-bit words at addresses `0x43c01000`, `0x43c01004`, `0x43c01008`, and `0x43c0100C`.


## Project layout

- `src/main.rs`: thin CLI entrypoint.
- `src/lib.rs`: message type, serialization, config parsing, and sender loop.
- `driver/messagesender.c`: platform driver + misc char device that maps the MMIO window from device-tree and writes four 32-bit values to offsets `0x0`, `0x4`, `0x8`, and `0xC` on each write.
- `driver/Makefile`: kernel Kbuild file for the module.
- `device-tree/messagesender.dtsi`: PetaLinux include snippet with MMIO region `0x43c01000..0x43c0100F` (word addresses `0x43c01000`, `0x43c01004`, `0x43c01008`, `0x43c0100C`).
- `Makefile`: top-level helper to build both app and driver.

## Build

Build both userspace app and kernel module:

```bash
make
```

Build Rust app only:

```bash
cargo build
```

Build kernel module only:

```bash
make driver KDIR=/path/to/kernel/build
```

## Run userspace sender

Run forever:

```bash
cargo run -- --delay-ms 100
```

Run finite test loop (10 writes):

```bash
cargo run -- --delay-ms 10 --iterations 10
```

## Load and unload kernel module

```bash
sudo insmod driver/messagesender.ko
ls -l /dev/messagesender
sudo rmmod messagesender
```

## PetaLinux device-tree integration

Include `device-tree/messagesender.dtsi` from your board or system device-tree and rebuild your PetaLinux image.

## Validation

```bash
cargo fmt --all -- --check
cargo clippy --all-targets --all-features -- -D warnings -W clippy::pedantic
cargo test --all-features
cargo test --doc
cargo doc --all-features --no-deps
```
