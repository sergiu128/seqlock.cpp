package seqlock

import (
	"fmt"
	"sync/atomic"
	"syscall"
	"unsafe"
)

func mapNew(fd int, name string, size int) ([]byte, error) {
	if err := syscall.Ftruncate(int(fd), int64(size)); err != nil {
		return nil, fmt.Errorf("Could not truncate file %s to size %d err=%v", name, size, err)
	}

	b, err := syscall.Mmap(fd, 0, size, syscall.PROT_READ|syscall.PROT_WRITE, syscall.MAP_SHARED)
	if err != nil {
		return nil, fmt.Errorf("Could not mmap new file %s of size %d err=%v", name, size, err)
	}
	return b, nil
}

func mapExisting(fd int, name string, size int) ([]byte, error) {
	if actualSize, err := GetFileSize(fd); err != nil || actualSize != size {
		if err != nil {
			return nil, err
		}
		return nil, fmt.Errorf(
			"Ftruncate failed: actual size = %d != %d = desired size",
			actualSize,
			size,
		)
	}

	b, err := syscall.Mmap(fd, 0, size, syscall.PROT_READ|syscall.PROT_WRITE, syscall.MAP_SHARED)
	if err != nil {
		return nil, fmt.Errorf("Could not mmap existing file %s of size %d err=%v", name, size, err)
	}
	return b, nil
}

type memoryRegion struct {
	whole []byte

	seq  *uint64 // == whole[:8]
	data []byte  // == whole[8:]
}

type SeqLockNative struct {
	region    memoryRegion
	isCreator bool
	name      string
	size      int
}

func NewSeqLockNativeShared(name string, size int) (*SeqLockNative, error) {
	if len(name) <= 0 {
		return nil, fmt.Errorf("Cannot create a file with an empty name.")
	}
	max_length := syscall.NAME_MAX
	// TODO also modify the c++ code: on linux '/' is needed but not on macOS.
	// if name[0] != '/' {
	// 	name = "/" + name
	// 	max_length--
	// }
	if len(name) > max_length {
		return nil, fmt.Errorf("Filename %s larger than %d.", name, max_length)
	}

	size += 8
	if roundedSize, err := RoundToPageSize(size); err != nil {
		return nil, err
	} else {
		size = roundedSize
	}

	fdptr, _, errno := syscall.Syscall(
		syscall.SYS_SHM_OPEN,
		uintptr(unsafe.Pointer(&name)),
		syscall.O_CREAT|syscall.O_EXCL|syscall.O_RDWR,
		syscall.S_IRUSR|syscall.S_IWUSR|syscall.S_IRGRP|syscall.S_IWGRP,
	)
	var (
		fd               = int(fdptr)
		isCreator        = false
		b         []byte = nil
		mapErr    error  = nil
	)

	if fd >= 0 {
		isCreator = true
		b, mapErr = mapNew(fd, name, size)
	} else if errno == syscall.EEXIST {
		isCreator = false

		errno = 0
		fdptr, _, errno = syscall.Syscall(
			syscall.SYS_SHM_OPEN,
			uintptr(unsafe.Pointer(&name)),
			syscall.O_RDWR,
			syscall.S_IRUSR|syscall.S_IWUSR|syscall.S_IRGRP|syscall.S_IWGRP,
		)
		fd = int(fdptr)
		if fd < 0 {
			var err error
			err = errno
			return nil, err
		}

		b, mapErr = mapExisting(fd, name, size)
	} else {
		var err error
		err = errno
		return nil, err
	}

	syscall.Close(fd)
	if mapErr != nil {
		if isCreator {
			syscall.Syscall(syscall.SYS_SHM_UNLINK, uintptr(unsafe.Pointer(&name)), 0, 0)
		}
		return nil, mapErr
	}

	if len(b) != size {
		return nil, fmt.Errorf("Mapped memory size = %d != %d = desired size", len(b), size)
	}

	return &SeqLockNative{
		region: memoryRegion{
			whole: b,
			seq:   (*uint64)(unsafe.Pointer(&b[0])),
			data:  unsafe.Slice(&b[8], len(b)-8),
		},
		isCreator: isCreator,
		name:      name,
		size:      size,
	}, nil
}

func (s *SeqLockNative) Close() {
	if s.region.whole != nil {
		syscall.Munmap(s.region.whole)
		s.region.whole = nil

		if s.isCreator {
			syscall.Syscall(syscall.SYS_SHM_UNLINK, uintptr(unsafe.Pointer(&s.name)), 0, 0)
		}
	}
}

func (s *SeqLockNative) StoreFn(fn func(data []byte)) {
	atomic.AddUint64(s.region.seq, 1)
	fn(s.region.data)
	atomic.AddUint64(s.region.seq, 1)
}

func (s *SeqLockNative) Load(into []byte) bool {
	seqBefore := atomic.LoadUint64(s.region.seq)
	if seqBefore%2 == 0 {
		copy(into, s.region.data)
		seqAfter := atomic.LoadUint64(s.region.seq)
		return seqBefore == seqAfter
	}
	return false
}

func (s *SeqLockNative) Store(from []byte) {
	s.StoreFn(func(data []byte) {
		copy(data, from)
	})
}

func (s *SeqLockNative) Size() int {
	return s.size - 8
}
