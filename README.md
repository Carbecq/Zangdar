# Zangdar

A UCI chess engine written in C++23, featuring NNUE evaluation, Lazy SMP, and Syzygy tablebase support.

Why *Zangdar*? Look up the Naheulbeuk dungeon!

---

## Features

### Board
- Magic Bitboard move generation

### Search
- Iterative Deepening with Aspiration Windows
- Alpha-Beta (Fail Soft) + Quiescence Search
- Transposition Table (4 buckets)
- Null Move Pruning (with Hindsight NMP)
- Late Move Reductions (LMR) with Hindsight Extension/Reduction
- Futility Pruning, Reverse Futility Pruning, Razoring
- ProbCut
- Singular Extensions
- Internal Iterative Reduction
- SEE-based move pruning (weighted by history)
- History Pruning
- Correction History (pawn + non-pawn, gravity formula)
- Killer Heuristic
- QS Futility Pruning

### Move Ordering
- MVV-LVA
- Capture History (threat-indexed)
- Quiet History, Continuation History

### Parallelism (Lazy SMP)
- Multi-threaded search sharing a lockless transposition table
- Thread vote aggregation for best-move selection

### Evaluation — NNUE
- Architecture: (768 × 16KB → 1024) × 2 → 1 × 8ob, SCReLU activation
- 16 king buckets (horizontally mirrored), factorized weights
- Trained with [Bullet](https://github.com/jnlt3/bullet); network embedded in the binary
- Self-generated training data from successive engine versions (bootstrapped iteratively)

### Endgame Tablebases
- Syzygy tablebase support (3–6 pieces)
- Configurable via UCI option

### Communication
- Full UCI protocol
- Options: Hash size, Threads, Syzygy path, Move Overhead

---

## Usage

Zangdar is a pure engine — it requires a chess GUI (Arena, BanksGUI, CuteChess, etc.).

Binaries are provided on the [Releases](../../releases) page. All of them are
built with **profile-guided optimization** (PGO) and are **statically linked**:
no runtime DLL on Windows, no glibc/libstdc++ version requirement on Linux —
just download and run.

| Platform                | Builds                   | Notes                                                     |
|-------------------------|--------------------------|-----------------------------------------------------------|
| Windows (x86-64)        | `sse2`, `avx2`, `bmi2`   | No Visual C++ / MinGW runtime needed.                     |
| Linux (x86-64)          | `sse2`, `avx2`, `bmi2`   | Runs on any distribution, however old.                    |
| Linux ARM64 (aarch64)   | `arm64`                  | Raspberry Pi 4/5, Ampere, AWS Graviton, Apple Silicon VMs. |

**Which x86-64 build should I pick?** `avx2` unless you know otherwise — it
covers essentially every CPU from 2013 on, Intel and AMD alike. Take `bmi2` only
on Intel Haswell+ or AMD Zen 3+; it is *slower* than `avx2` on Zen 1/2, whose
PEXT is microcoded. `sse2` is the fallback for pre-2013 machines and for VMs
that expose no AVX.

Building from source is described below, and remains the best option for
squeezing out the last few percent (`ARCH=native`).

---

## Compilation

Requires a **C++23 compiler**. The default is `clang++` (GCC is also supported via `CXX=g++`).
The default target is `openbench`, so a bare `make` produces a ready-to-use binary.

Building also needs GNU make driving a **Unix shell** — the recipes rely on
`rm`, `mv`, `cp`, `mkdir`, `find`, `test` and `curl`. That is the case on Linux,
on macOS, and on Windows under MSYS2; it is not the case under `cmd.exe`.

```bash
# Standard build (auto-detects the host CPU)
make -j$(nproc)

# Explicit compiler / architecture
make -j$(nproc) CXX=g++ ARCH=avx2
```

The network file is embedded in the binary at compile time. It is downloaded
automatically on the first build (`curl` required); you can also fetch it
beforehand with `make download-net`, or point to a local file with `EVALFILE=<path>`.

### Build variables

Variables are passed on the `make` command line, e.g. `make ARCH=avx2 CXX=g++ STATIC=yes`.

| Variable     | Values / example                    | Description                                                        |
|--------------|-------------------------------------|--------------------------------------------------------------------|
| `ARCH`       | `native` (default), `sse2`, `avx2`, `bmi2`, `avx512`, `arm64` | Target CPU architecture (see table below). |
| `CXX`        | `clang++` (default), `g++`, `aarch64-linux-gnu-g++` | C++ compiler.                            |
| `STATIC`     | `yes`                               | Statically link the binary (Linux; always static on Windows).      |
| `COMP`       | `mingw`                             | Force Windows target mode. Only needed when cross-compiling from Linux — under MSYS2 it is detected automatically. |
| `EVALFILE`   | `<path to .bin>`                    | Override the embedded NNUE network.                                |
| `EXTRA_DEFS` | `-DUSE_TUNING`, `-DDEBUG_LOG`, …    | Extra preprocessor defines.                                        |
| `EXE`        | `<output path>`                     | Output binary path (used by OpenBench).                            |

### Architectures (`ARCH=`)

The scale mirrors Stockfish's. Each target corresponds to a real functional
difference (NNUE SIMD kernel and/or PEXT attacks), not gratuitous granularity.

| `ARCH`   | SIMD kernel        | Attacks | Target hardware                                                                 |
|----------|--------------------|---------|---------------------------------------------------------------------------------|
| `native` | auto (host)        | auto    | Compile-for-yourself; uses `-march=native`. **Default.**                        |
| `sse2`   | SSE2 (128-bit)     | magics  | Old x86 (~2008–2013) and VMs without AVX. Requires POPCNT (CPU ≥ 2008).         |
| `avx2`   | AVX2 (256-bit)     | magics  | The 2013+ standard: Intel Haswell+, **and** AMD Zen 1/2.                         |
| `bmi2`   | AVX2 (256-bit)     | PEXT    | **Only** Intel Haswell+ and AMD Zen 3+. ⚠️ Slower than `avx2` on Zen 1/2 (microcoded PEXT). |
| `avx512` | AVX-512 (512-bit)  | PEXT    | Zen 4/5, Intel server/HEDT (Ice Lake-SP+).                                       |
| `arm64`  | NEON (128-bit)     | magics  | Linux aarch64 (ARM64). Cross-compiled; NEON kernel ≈ SSE2 width, bit-exact.      |

> **Which one?** On a machine you compile on yourself, keep `ARCH=native`.
> For distributing a portable Linux binary, `avx2` covers virtually all x86-64
> hardware from 2013 on. Use `bmi2`/`avx512` only for the CPUs listed above —
> `bmi2` is a *trap* on Zen 1/2 (their PEXT is microcoded, ~100+ cycles).

### Cross-compiling for ARM64 (aarch64)

Install the cross toolchain (Debian/Ubuntu), then build a static binary:

```bash
sudo apt install crossbuild-essential-arm64
make -j$(nproc) ARCH=arm64 CXX=aarch64-linux-gnu-g++ STATIC=yes
```

The result is a statically-linked aarch64 ELF that runs on any ARMv8-A machine
(Raspberry Pi, Graviton, Apple Silicon under Linux, …). The NNUE inference is
bit-exact with the x86 SIMD builds. To test it on an x86 host without ARM
hardware: `sudo apt install qemu-user-static && qemu-aarch64-static ./<binary> bench`.

### Building on Windows (MSYS2)

Windows builds are made **natively under [MSYS2](https://www.msys2.org/)**, in
the **CLANG64** environment — same compiler as the Linux builds, so both sides
stay comparable.

```bash
# in the MSYS2 CLANG64 shell
pacman -S mingw-w64-clang-x86_64-clang mingw-w64-clang-x86_64-lld make
make -j$(nproc) release ARCH=avx2
```

Nothing else to pass: Windows defines the `OS=Windows_NT` environment variable,
which the Makefile picks up to switch on the `.exe` suffix, the automatic
`-static` link and the 16 MB stack (on Windows the stack size is written into
the executable at link time; on Linux it comes from `ulimit -s`).

MSYS2 ships several environments, all targeting the mingw-w64 runtime — i.e.
plain native Windows executables, neither MSVC nor Cygwin. `CLANG64` pairs clang
with **libc++**, whereas `MINGW64`/`UCRT64` pair gcc with libstdc++; any of them
works, they simply produce different binaries. Check that the result is
self-contained (with libc++, `-static` must also absorb `libc++`/`libunwind`):

```bash
ldd Zangdar-7-avx2-clang-rel.exe    # should list only system DLLs
```

> Cross-compiling from Linux with `x86_64-w64-mingw32-g++` also works
> (`COMP=mingw` then forces Windows mode), but it means maintaining a second,
> gcc-based toolchain for an equivalent result. Should you go that way, install
> the **`-posix`** variant: the `win32` one has no `<thread>`/`<mutex>` in its
> C++ library, which Lazy SMP requires.

### Producing the release binaries

Released binaries use the `pgo` target, not `release`. Each build must be
preceded by `make clean` (see the warning below), and the result moved out of
the way — `clean` deletes every `Zangdar-*`, including the binary produced by
the previous iteration.

> **PGO runs the instrumented binary** to collect the profile, so the build
> machine must be able to *execute* the target instruction set. A CPU can always
> build for its own subset — a Zen 3 covers `sse2`, `avx2` and `bmi2` — but
> `avx512` requires an actual AVX-512 machine, and `arm64` requires either ARM
> hardware or qemu (below). `-j` does not help here: the `pgo` target compiles
> everything in a single compiler invocation.

```bash
mkdir -p dist

# Linux x86-64, static            (on any x86-64 machine, e.g. Zen 3)
for a in sse2 avx2 bmi2; do
  make clean && make pgo ARCH=$a STATIC=yes && mv Zangdar-*-$a-* dist/
done

# Linux ARM64, static             (sudo apt install crossbuild-essential-arm64 qemu-user-static)
make clean && make pgo ARCH=arm64 CXX=aarch64-linux-gnu-g++ STATIC=yes
mv Zangdar-*arm64* dist/

# Windows x86-64, static          (in the MSYS2 CLANG64 shell)
for a in sse2 avx2 bmi2; do
  make clean && make pgo ARCH=$a && mv Zangdar-*.exe dist/
done

# Windows AVX-512                 (MSYS2 CLANG64, on an AVX-512 machine)
make clean && make pgo ARCH=avx512 && mv Zangdar-*avx512*.exe dist/
```

`avx512` is released for Windows only, simply because the AVX-512 machine
available here runs Windows. Linux users on Ice Lake-SP+/Zen 4+ get the same
result by building that target themselves.

Cross-building the ARM64 profile works because `qemu-user-static` registers
itself with `binfmt_misc`: the instrumented aarch64 binary then runs
transparently on the x86 host, and the `pgo` target needs no adjustment. The
profile is collected under emulation, which preserves branch and block-frequency
counters — what GCC's PGO actually consumes — but not cache or timing behaviour,
so expect slightly less benefit than a profile gathered on real ARM hardware.

Sanity check before publishing — a build must not contain instructions its
target cannot execute:

```bash
objdump -d dist/Zangdar-*-sse2-* | grep -cwE 'pext|vpmaddwd'   # must be 0
objdump -d dist/Zangdar-*-avx2-* | grep -cw pext               # must be 0
```

(Counting `%ymm`/`%zmm` in a *static* binary is not meaningful: the C library
ships several `memcpy`/`strlen` variants and dispatches at runtime.)

### Build targets

| Target             | Purpose                                                                       |
|--------------------|-------------------------------------------------------------------------------|
| `openbench`        | Optimized release build (**default**). Used by OpenBench.                     |
| `release`          | Same flags as `openbench`, renamed output.                                    |
| `pgo`              | Profile-Guided Optimization (runs a bench to collect the profile). **Used for all released binaries.** |
| `debug`            | `-O2 -g` with the full warning set.                                           |
| `perf`             | Release + debug symbols, no frame-pointer omission (for `perf record`).       |
| `sanitize`         | ASan + UBSan (abort on first error).                                          |
| `sanitize-thread`  | ThreadSanitizer (data-race detection for Lazy SMP).                           |
| `release-nolto`    | Release without LTO (isolates UB exposed by inter-module optimization).       |
| `release-autoinit` | `release-nolto` + zero-init of stack variables (bench-determinism check).     |
| `release-valgrind` | `release-nolto` + `-g`, no strip (for `valgrind --tool=memcheck`).            |
| `release-bounds`   | `release-nolto` + UBSan bounds/object-size + `_FORTIFY_SOURCE=3`.             |
| `clean`            | Remove objects, `.d` dependency files and **all** binaries (every `ARCH`).    |
| `mrproper`         | `clean` + remove any remaining generated executables.                        |
| `download-net`     | Download the NNUE network file only.                                          |

> **Header dependencies are tracked** (`-MMD -MP`): editing a header — or
> switching branches — recompiles exactly the affected objects, and deleting a
> header no longer breaks the build.
>
> ⚠️ **But object files are shared across architectures.** After changing
> `ARCH`, `CXX`, `STATIC` or `EXTRA_DEFS`, run `make clean` **first**. Make only
> compares `.cpp`/`.o` timestamps and cannot see that the flags changed: without
> a clean, `make ARCH=avx2` after a `native` build silently relinks the native
> objects into a binary *named* `avx2` — one that carries the compiling CPU's
> instructions (PEXT, AVX-512, …) and crashes with `SIGILL` on the hardware that
> target is meant to serve.

---

## Thanks

Special thanks to the authors of Vice, TSCP, and Gerbil for their public work that helped me understand the fundamentals. Also to the authors of Stockfish, Berserk, Obsidian, Clover, Halogen and many others whose open-source code continues to be an invaluable reference.

The M42 library was used early on for attack generation.
