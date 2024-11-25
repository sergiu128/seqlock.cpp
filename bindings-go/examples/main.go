// Showcases how to use a seqlock to share data between two processes.
package main

import (
	"flag"
	"log"
	"time"

	"github.com/sergiu128/seqlock.cpp/bindings-go/seqlock"
)

var (
	filename = flag.String("f", "/seqlock-go-example-rw", "Shared memory name. Must start with /")
	size     = flag.Int("s", 4096, "Shared memory size. Will be rounded up to the nearest page size.")
	writer   = flag.Bool("w", false, "If true, the program writes to shared memory. If false, it reads from shared memory.")
	interval = flag.Duration("i", time.Second, "Interval at which to write")
)

func main() {
	flag.Parse()

	lck, err := seqlock.NewSeqLockShared(*filename, *size)
	if err != nil {
		log.Panic(err)
	}
	defer func() {
		if err := lck.Destroy(); err != nil {
			log.Panic(err)
		}
	}()

	if *writer {
		runWriter(lck)
	} else {
		runReader(lck)
	}

	log.Printf("created seqlock protected shared memory of size %dB", lck.Size())
	log.Print("bye :)")
}

func runWriter(lck *seqlock.SeqLock) {
	log.Print("running writer")

	var (
		b      [128]byte
		ticker = time.NewTicker(*interval)
		value  = 0
	)

	update := func(v byte) {
		for i := 0; i < len(b); i++ {
			b[i] = v
		}
	}
	update(0)

	for {
		select {
		case <-ticker.C:
			value++
			value %= 128
			update(byte(value))

			start := time.Now()
			if err := lck.Store(b[:]); err != nil {
				log.Panic(err)
			}
			diff := time.Now().Sub(start)
			log.Printf("wrote %d in %s", value, diff)

		}
	}
}

func runReader(lck *seqlock.SeqLock) {
	log.Print("running reader")

	var (
		b        [128]byte
		lastRead int = -1
	)
	for {
		start := time.Now()
		if err := lck.Load(b[:]); err != nil {
			log.Panic(err)
		}
		diff := time.Now().Sub(start)
		for i := 0; i < len(b)-1; i++ {
			if b[i] != b[i+1] {
				log.Panic("invalid read")
			}
		}

		if lastRead != int(b[0]) {
			lastRead = int(b[0])
			log.Printf("read %d in %s", lastRead, diff)
		}
	}
}
