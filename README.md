# Zangdar
A UCI chess engine written in C++23.

This project is somewhat a hobby, it will serve to learn the ropes of chess programming.
I also play chess, very humbly; and I was always curious of how the programs work.

I began some years ago, but without time, it never went far. I had only a move generator, a alpha-beta search, and that's almost all.
I have now enough time to spend on the program. By luck, reading the forums, I discovered Vice, and the videos. Thay are great, and very instructive.
So well, Im following the lessons, and Zangdar came to life. I found also the site from Bruce Moreland that explain a lot of things.

Why Zangdar ? Well look for the Naheulbeuk dungeon !!

I would like to thank specially the authors of Vice, TSCP, Gerbil. They helped me a lot understand several aspects of programmation.
I also use the M42 library for generating attacks; and took inspiration from the Libchess library. 

Since then, I learned a lot, and Zangdar became a formidable opponent. It is now in the first 100 programs, and this represents a big achievement. I would never thought come this far some years ago.

It has the following features :

+ **Language** 
  - Written in C++23

+ **Board** 
  - Magic Bitboard

+ **Search**
  - Aspiration Window
  - Iterative Deepening
  - Alpha-Beta, Fail Soft
  - Quiescence
  - Killer Heuristic
  - Transposition Table, with 4 buckets
  - Null Move 
  - Late Move Pruning
  - Razoring
  - Internal Iterative Deepening
  - Uses several Histories

+ **Move Ordering**
  - MVVVLA
  - Capture History

+ **Parallelism**
  - can use several threads

+ **Evaluation**
  - Since version 3, Zangdar uses a network based on NNUE. 
 
+ **NNUE**
  - The network is relatively simple, 768 Hidden Layer, no bucket.
  - The first one was created with the HCE version of Zangdar. Successives versions were created with the precedent version.

+ **Syzygy Tables**
  - Use of endgame tables is possible; you must use the UCI option. You also need a SSD to work correctly.
 
+ **Communication**
  - UCI
  - Options to change Hash size, Threads number, Syzygy tables usage, Move Overhead.
    
+ **Usage**
  - The program is only an engine, so it needs one interface (Arena, Banksgui...).
  - I provide binaries for Windows. I develop with Linux, but as binaries are not portable, I don't distribute it. A Makefile is given and should work without too much work. All are compiled in static, so you don't need extrernal libraries. 

+ **Compilation**
  - You must have a C++ compiler that use at least C++23. I use now MSYS2 for Windows 11, that compiles with clang++.
I provide also binaries for several architectures.

+ **Old Releases**
  - As I was very ignorant of git, the first repositery was extremely bad done. So I decided to redo it, and uses git tools. But the ancient one is still here, named Zangdar_0.
