package seqlock

import (
	"fmt"
	"math"
	"os"
	"syscall"
)

const seqSize = 64 // matches the C++ code

func GetFileSize(fd int) (int, error) {
	stat := syscall.Stat_t{}
	if err := syscall.Fstat(int(fd), &stat); err != nil {
		return 0, fmt.Errorf("Could not fstat err=%v", err)
	}
	return int(stat.Size), nil
}

func RoundToPageSize(size int) (int, error) {
	pageSize := os.Getpagesize()
	if pageSize == 0 {
		return size, fmt.Errorf("Fatal: system's page size is 0")
	}
	if size == 0 {
		return pageSize, nil
	}
	if remainder := size % pageSize; remainder > 0 {
		if size > math.MaxInt-(pageSize-remainder) {
			return size, fmt.Errorf(
				"Size %d exceeds allowable limits when rounded to page size %d.",
				size,
				pageSize,
			)
		}
		size += pageSize - remainder
	}
	return size, nil
}
