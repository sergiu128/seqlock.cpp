# Seqlock
A single/multi-writer multi-reader Sequential Lock implemented in C++23, with an accompanying Go wrapper. Only supports ARM64 and X86-64. Has optional support for multiple writers through a spinlock.

# Getting started
Start with `examples/`. Main code is in `seqlock/include/seqlock`.

## Building the C++ part
- Install nix.
- Invoke `nix-shell`.
- Run `./setup.sh`.
- Build with `cmake -GNinja -S . -B build; cmake --build build;`.
- Run tests with `./build/tests/run`.
- Build the release version with `cmake -GNinja -DCMAKE_BUILD_TYPE=Release -S . -B build_rel`.
- The static library will be in `build/seqlock/libseqlock.a`. You can install everything with `ninja install` while in `build/`.
- Check `examples/` and the files ending with `.test.cpp` and `.bm.cpp`.
- Benchmarks are `build/*.bm`. Make sure they're release targets before running them, as shown above.

### Go bindings
The module's path is `github.com/sergiu128/seqlock.cpp/bindings-go/seqlock`. See `bindings-go/examples/main.go` on how to use it. `libseqlock.a` (release target) and `ffi.h` are copied on each new release from the `cpp` part. 

# Credits
I found out about seqlocks from David Gross' [talk](https://www.youtube.com/watch?v=8uAW5FQtcvE) on C++ in trading. This project adds the right memory barriers on top of his implementation and provides a broader API while also adding support for multiple writers.
