# Seqlock

A Sequential Lock implemented in C++20, with an accompanying Go wrapper. Only supports ARM64 and X86-64. The C++ library can be linked statically. The Go library can be found at github.com/sergiu128/seqlock.cpp/bindings/seqlock.go.

# Credits

I found out about seqlocks from David Gross' [talk](https://www.youtube.com/watch?v=8uAW5FQtcvE) on C++ in trading. This project adds the right memory barriers on top of his implementation and provides a nicer API.
