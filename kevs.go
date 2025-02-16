package main

func main() {
}

type ValueKind uint8

const (
	ValueKindUndefined ValueKind = iota
	ValueKindString
	ValueKindInteger
	ValueKindBoolean
	ValueKindList
	ValueKindTable
)

type Value struct {
	Kind ValueKind
	Data any
}

type KeyValue struct {
	Key   string
	Value Value
}

type Table []KeyValue

func parse(file string) (Table, error) {
	return nil, nil
}
