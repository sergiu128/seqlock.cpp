package seqlock

import (
	"os"
	"sync"
	"testing"
	"time"
)

func TestSeqLockNative(t *testing.T) {
	w, err := NewSeqLockNativeShared("sharedobject4", 4096)
	if err != nil {
		t.Fatal(err)
	}
	defer w.Close()

	w.StoreFn(func(b []byte) {
		for i := 0; i < len(b); i++ {
			b[i] = 0
		}
	})

	r, err := NewSeqLockNativeShared("sharedobject4", 4096)
	if err != nil {
		t.Fatal(err)
	}
	defer r.Close()

	var (
		done = make(chan struct{}, 1)
		wg   sync.WaitGroup
	)

	wg.Add(2)
	go func() {
		defer wg.Done()
		c := 0
	outer:
		for {
			select {
			case <-done:
				break outer
			default:
				w.StoreFn(func(b []byte) {
					for j := range b {
						b[j] = byte(c & 127)
					}
					c++
				})
				time.Sleep(10 * time.Microsecond)
			}
		}
	}()

	go func() {
		defer wg.Done()
		var (
			reads = 0
			freq  [128]int
		)
		for i := range freq {
			freq[i] = 0
		}

		for reads < 1024*128 {
			for i := 0; i < 1024; i++ {
				b := make([]byte, 1024)
				success := r.Load(b)
				if success {
					reads++

					for i := 1; i < len(b); i++ {
						if b[i] != b[i-1] {
							panic("invalid read")
						}
					}
					freq[int(b[i])]++
				}
			}
		}

		for n, c := range freq {
			if n > 0 && c == 0 {
				t.Fatalf("Invalid load, missed %d", n)
			}
		}

		done <- struct{}{}
	}()

	wg.Wait()
}

func TestSeqLockNativeSharedSize(t *testing.T) {
	lock, err := NewSeqLockNativeShared("somefilenam222", os.Getpagesize())
	if err != nil {
		t.Fatal(err)
	}
	defer lock.Close()

	if lock.Size() != 2*os.Getpagesize()-8 {
		t.Fatalf("invalid size, should be two pages instead of one")
	}
}

func BenchmarkSeqLockNativeStoreLoad(b *testing.B) {
	lock, err := NewSeqLockNativeShared("seqlock-native-bench", os.Getpagesize())
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

func BenchmarkSeqLockNativeStore(b *testing.B) {
	lock, err := NewSeqLockNativeShared("/seqlock-native-bench", os.Getpagesize())
	if err != nil {
		b.Error(err)
	}
	defer lock.Close()

	buf := make([]byte, lock.Size())
	for i := 0; i < b.N; i++ {
		lock.Store(buf)
	}
}

func BenchmarkSeqLockNativeLoad(b *testing.B) {
	lock, err := NewSeqLockNativeShared("/seqlock-native-bench", os.Getpagesize())
	if err != nil {
		b.Error(err)
	}
	defer lock.Close()

	buf := make([]byte, lock.Size())
	for i := 0; i < b.N; i++ {
		lock.Load(buf)
	}
}
