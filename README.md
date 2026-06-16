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
- Thread vote aggregation for best-move selection (Stockfish-style)

### Evaluation — NNUE
- Architecture: (768 × 8KB → 1024) × 2 → 1 × 8ob, SCReLU activation
- 8 king buckets (horizontally mirrored), factorized weights
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

Windows binaries are provided on the [Releases](../../releases) page for several CPU architectures (BMI2, AVX2, SSE4). Linux users can build from source.

---

## Compilation

Requires a C++23 compiler. The default is `clang++`.

```bash
# Standard build
make -j$(nproc)

# For OpenBench
make openbench -j$(nproc)
```

Other Makefile targets: `release`, `debug`, `sanitize`, `perf`, `pgo`, `release-nolto`.

---

## Thanks

Special thanks to the authors of Vice, TSCP, and Gerbil for their public work that helped me understand the fundamentals. Also to the authors of Stockfish, Berserk, Obsidian, Clover, Halogen and many others whose open-source code continues to be an invaluable reference.

The M42 library was used early on for attack generation.
