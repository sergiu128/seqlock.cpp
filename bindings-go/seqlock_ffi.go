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

type SeqLockFFI struct {
	ptr    *C.struct_SingleWriterSeqLock
	size   int
	data   []byte          // optional
	pinner *runtime.Pinner // optional
}

func NewSeqLockFFI(data []byte) *SeqLockFFI {
	ptr := C.seqlock_single_writer_create((*C.char)(unsafe.Pointer(&data[0])), (C.size_t)(len(data)))
	size := int(ptr.shared_data_size)

	lock := &SeqLockFFI{
		ptr:    ptr,
		size:   size,
		data:   data,
		pinner: &runtime.Pinner{},
	}
	// Pinning ensures the memory is not moved by the runtime throughout the SeqLockFFI's lifetime, thus making the FFI calls safe.
	lock.pinner.Pin(&data[0])
	return lock
}

func NewSeqLockFFIShared(filename string, size int) (*SeqLockFFI, error) {
	cStr := C.CString(filename)
	defer C.free(unsafe.Pointer(cStr))

	ptr, err := C.seqlock_single_writer_create_shared(cStr, (C.size_t)(size))
	if err != nil {
		return nil, err
	}

	size = int(ptr.shared_data_size)

	return &SeqLockFFI{
		ptr:  ptr,
		size: size,
	}, nil
}

func (l *SeqLockFFI) Load(into []byte) error {
	addr := (*C.char)(unsafe.Pointer(&into[0]))
	var pinner runtime.Pinner
	pinner.Pin(addr)
	defer pinner.Unpin()

	_, err := C.seqlock_single_writer_load(l.ptr, addr, (C.size_t)(len(into)))
	return err
}

func (l *SeqLockFFI) Store(from []byte) error {
	addr := (*C.char)(unsafe.Pointer(&from[0]))
	var pinner runtime.Pinner
	pinner.Pin(addr)
	defer pinner.Unpin()

	_, err := C.seqlock_single_writer_store(l.ptr, addr, (C.size_t)(len(from)))
	return err
}

func (l *SeqLockFFI) Size() int {
	return l.size
}

func (l *SeqLockFFI) Close() error {
	if l.pinner != nil {
		l.pinner.Unpin()
	}
	_, err := C.seqlock_single_writer_destroy(l.ptr)
	return err
}
