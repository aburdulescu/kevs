package main

// #cgo LDFLAGS: -static
// #include "kevs.h"
import "C"

import (
	"os"
)

func main() {
	data, err := os.ReadFile("../example.kevs")
	if err != nil {
		panic(err)
	}

	var root C.Table
	ctx := C.Context{}
	file := C.str_from_cstr(C.CString("example.kevs"))
	content := C.str_from_cstr(C.CString(string(data)))

	ok := C.table_parse(&root, ctx, file, content)
	if !ok {
		panic("parse failed")
	}
	defer C.table_free(&root)

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

	println("name:", C.GoString(name))
	println("age:", age)
}
