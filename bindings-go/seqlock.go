package seqlock

// #cgo CXXFLAGS: -std=c++23
// #cgo LDFLAGS: ${SRCDIR}/libseqlock.a -lstdc++
// #include <stdlib.h>
// #include "ffi.h"
import "C"

import (
	"runtime"
	"unsafe"
)

type SeqLock struct {
	ptr    *C.struct_SingleWriterSeqLock
	size   int
	data   []byte          // optional
	pinner *runtime.Pinner // optional
}

func NewSeqLock(data []byte) *SeqLock {
	ptr := C.seqlock_single_writer_create((*C.char)(unsafe.Pointer(&data[0])), (C.size_t)(len(data)))
	size := int(ptr.shared_data_size)

	lock := &SeqLock{
		ptr:    ptr,
		size:   size,
		data:   data,
		pinner: &runtime.Pinner{},
	}
	// Pinning ensures the memory is not moved by the runtime throughout the SeqLock's lifetime, thus making the FFI calls safe.
	lock.pinner.Pin(&data[0])
	return lock
}

func NewSeqLockShared(filename string, size int) (*SeqLock, error) {
	cStr := C.CString(filename)
	defer C.free(unsafe.Pointer(cStr))

	ptr, err := C.seqlock_single_writer_create_shared(cStr, (C.size_t)(size))
	if err != nil {
		return nil, err
	}

	size = int(ptr.shared_data_size)

	return &SeqLock{
		ptr:  ptr,
		size: size,
	}, nil
}

func (l *SeqLock) Load(into []byte) error {
	addr := (*C.char)(unsafe.Pointer(&into[0]))
	var pinner runtime.Pinner
	pinner.Pin(addr)
	defer pinner.Unpin()

	_, err := C.seqlock_single_writer_load(l.ptr, addr, (C.size_t)(len(into)))
	return err
}

func (l *SeqLock) Store(from []byte) error {
	addr := (*C.char)(unsafe.Pointer(&from[0]))
	var pinner runtime.Pinner
	pinner.Pin(addr)
	defer pinner.Unpin()

	_, err := C.seqlock_single_writer_store(l.ptr, addr, (C.size_t)(len(from)))
	return err
}

func (l *SeqLock) Size() int {
	return l.size
}

func (l *SeqLock) Destroy() error {
	if l.pinner != nil {
		l.pinner.Unpin()
	}
	_, err := C.seqlock_single_writer_destroy(l.ptr)
	return err
}
