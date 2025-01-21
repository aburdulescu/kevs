package main

// #cgo LDFLAGS: -static
// #include "kevs.h"
import "C"

import (
	"os"
)

func main() {
	const file = "../example.kevs"

	data, err := os.ReadFile(file)
	if err != nil {
		panic(err)
	}

	var errBuf [8193]byte

	var root C.Table
	if err := C.table_parse(
		&root,
		C.Params{
			file:        C.str_from_cstr(C.CString(file)),
			content:     C.str_from_cstr(C.CString(string(data))),
			err_buf:     C.CString(string(errBuf[:])),
			err_buf_len: C.size_t(len(errBuf) - 1),
		},
	); err != nil {
		panic(C.GoString(err))
	}
	defer C.table_free(&root)

	var stringEscaped *C.char
	if err := C.table_string(root, C.CString("string_escaped"), &stringEscaped); err != nil {
		panic(C.GoString(err))
	}

	var table2 C.Table
	if err := C.table_table(root, C.CString("table2"), &table2); err != nil {
		panic(C.GoString(err))
	}

	var name *C.char
	if err := C.table_string(table2, C.CString("name"), &name); err != nil {
		panic(C.GoString(err))
	}

	var age C.int64_t
	if err := C.table_int(table2, C.CString("age"), &age); err != nil {
		panic(C.GoString(err))
	}

	println("string_escaped:", C.GoString(stringEscaped))
	println("name:", C.GoString(name))
	println("age:", age)
}
