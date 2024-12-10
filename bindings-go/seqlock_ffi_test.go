package seqlock

import (
	"os"
	"sync"
	"sync/atomic"
	"testing"
	"time"
)

func TestSeqLockFFI(t *testing.T) {
	data := make([]byte, 7)
	lock := NewSeqLockFFI(data)

	if lock.Size() != 7 {
		t.Fatal("size should not be changed")
	}

	const Size = 7

	defer lock.Close()

	var wg sync.WaitGroup
	wg.Add(2)

	// writer
	var writerDone int64 = 0
	go func() {
		defer wg.Done()

		from := make([]byte, Size)
		for i := 0; i < 100; i++ {
			for j := 0; j < len(from); j++ {
				from[j] = byte(i & 127)
			}
			lock.Store(from)
			time.Sleep(10 * time.Millisecond)
		}
		atomic.StoreInt64(&writerDone, 1)
	}()

	// reader
	ok := true
	mask := make([]bool, 255)
	for i := 0; i < len(mask); i++ {
		mask[i] = false
	}
	go func() {
		defer wg.Done()

		to := make([]byte, Size)
	outer:
		for atomic.LoadInt64(&writerDone) == 0 {
			lock.Load(to)
			for i := 0; i < len(to)-1; i++ {
				if to[i] != to[i+1] {
					ok = false
					break outer
				}
			}
			mask[to[0]] = true
		}
	}()

	wg.Wait()

	if !ok {
		t.Fatal("invalid load")
	}

	ok = false
	for _, v := range mask {
		if v {
			ok = true
		}
	}
	if !ok {
		t.Fatal("invalid load")
	}
}

func TestSeqLockFFISharedSize(t *testing.T) {
	lock, err := NewSeqLockFFIShared("/somefilenam222", os.Getpagesize())
	if err != nil {
		t.Fatal(err)
	}
	defer lock.Close()

	if lock.Size() != 2*os.Getpagesize()-seqSize {
		t.Fatalf("invalid size, should be two pages instead of one")
	}
}

func TestSeqLockFFIShared(t *testing.T) {
	var wg sync.WaitGroup
	wg.Add(2)

	var (
		writerState int64 = 0 // 0: preparing 1: writing 2: done
		readerDone  int64 = 0
	)

	go func() {
		defer wg.Done()

		for atomic.LoadInt64(&writerState) == 0 {
		}

		lock, err := NewSeqLockFFIShared("/yesyesyes", os.Getpagesize())
		if err != nil {
			panic(err)
		}
		defer lock.Close()

		b := make([]byte, lock.Size())
		mask := make([]bool, 255)
		for i := 0; i < len(mask); i++ {
			mask[i] = false
		}

		for atomic.LoadInt64(&writerState) == 1 {
			lock.Load(b)
			for i := 0; i < len(b)-1; i++ {
				if b[i] != b[i+1] {
					panic("invalid load")
				}
				mask[b[i]] = true
			}
		}

		count := 0
		for _, v := range mask {
			if v {
				count++
			}
		}

		if count < 5 {
			panic("invalid load")
		}

		atomic.StoreInt64(&readerDone, 1)
	}()

	go func() {
		defer wg.Done()

		lock, err := NewSeqLockFFIShared("/yesyesyes", os.Getpagesize())
		if err != nil {
			panic(err)
		}
		defer lock.Close()

		atomic.StoreInt64(&writerState, 1)

		b := make([]byte, lock.Size())
		for i := 0; i < 100; i++ {
			for j := 0; j < len(b); j++ {
				b[j] = byte(i & 127)
			}
			lock.Store(b)
			time.Sleep(10 * time.Millisecond)
			lock.Load(b)
		}

		atomic.StoreInt64(&writerState, 2)

		for atomic.LoadInt64(&readerDone) == 0 {
		}

	}()

	wg.Wait()
}

func BenchmarkSeqLockFFIStoreLoad(b *testing.B) {
	lock, err := NewSeqLockFFIShared("/seqlock-ffi-bench", os.Getpagesize())
	if err != nil {
		b.Error(err)
	}
	defer lock.Close()

	buf := make([]byte, lock.Size())
	for i := 0; i < b.N; i++ {
		lock.Store(buf)
		lock.Load(buf)
	}
}

func BenchmarkSeqLockFFIStore(b *testing.B) {
	lock, err := NewSeqLockFFIShared("/seqlock-ffi-bench", os.Getpagesize())
	if err != nil {
		b.Error(err)
	}
	defer lock.Close()

	buf := make([]byte, lock.Size())
	for i := 0; i < b.N; i++ {
		lock.Store(buf)
	}
}

func BenchmarkSeqLockFFILoad(b *testing.B) {
	lock, err := NewSeqLockFFIShared("/seqlock-ffi-bench", os.Getpagesize())
	if err != nil {
		b.Error(err)
	}
	defer lock.Close()

	buf := make([]byte, lock.Size())
	for i := 0; i < b.N; i++ {
		lock.Load(buf)
	}
}
