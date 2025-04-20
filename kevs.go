package main

import (
	"flag"
	"fmt"
	"os"
	"strings"
)

func main() {
	if err := mainErr(); err != nil {
		fmt.Fprintln(os.Stderr, "error:", err)
		os.Exit(1)
	}
}

func mainErr() error {
	flag.Parse()

	if flag.NArg() == 0 {
		return fmt.Errorf("need file")
	}

	file := flag.Arg(0)

	data, err := os.ReadFile(file)
	if err != nil {
		return err
	}

	t, err := parse(Params{File: file, Content: string(data)})
	if err != nil {
		return err
	}

	fmt.Println(len(t))

	return nil
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

func parse(params Params) (Table, error) {
	tokens, err := scan(params)
	if err != nil {
		return nil, err
	}

	for _, tok := range tokens {
		fmt.Println(tok)
	}

	return nil, nil
}

type tokenKind uint8

const (
	tokenKindUndefined tokenKind = 0
	tokenKindKey
	tokenKindDelim
	tokenKindValue
)

type token struct {
	value string
	kind  tokenKind
	line  int
}

type Params struct {
	File    string
	Content string
}

type scanner struct {
	params Params
	tokens []token
	line   int
	err    error
}

const (
	kKeyValSep      = '='
	kKeyValEnd      = ';'
	kCommentBegin   = '#'
	kStringBegin    = '"'
	kRawStringBegin = '`'
	kListBegin      = '['
	kListEnd        = ']'
	kTableBegin     = '{'
	kTableEnd       = '}'

	spaces = " \t"
)

func scan(params Params) ([]token, error) {
	s := scanner{
		params: params,
		line:   1,
	}

	for len(s.params.Content) != 0 {
		s.trim_space()
		ok := false
		if s.expect('\n') {
			ok = s.scan_newline()
		} else if s.expect(kCommentBegin) {
			ok = s.scan_comment()
		} else {
			ok = s.scan_key_value()
		}
		if !ok {
			return nil, s.err
		}
	}

	return nil, nil
}

func (self *scanner) trim_space() {
	self.params.Content = strings.TrimLeft(self.params.Content, spaces)
}

func (self *scanner) expect(c byte) bool {
	if len(self.params.Content) == 0 {
		return false
	}
	return self.params.Content[0] == c
}

func (self *scanner) scan_newline() bool {
	self.line++
	self.advance(1)
	return true
}

func (self *scanner) advance(n int) {
	self.params.Content = self.params.Content[n:]
}

func (self *scanner) scan_comment() bool {
	newline := strings.IndexByte(self.params.Content, '\n')
	if newline == -1 {
		self.scan_errorf("comment does not end with newline")
		return false
	}
	self.advance(newline)
	return true
}

func (self *scanner) scan_key_value() bool {
	if !self.scan_key() {
		return false
	}

	// separator check done in scan_key, no need to check again
	self.append_delim()

	if !self.scan_value() {
		return false
	}

	return true
}

func (self *scanner) scan_errorf(format string, args ...any) {
	self.err = fmt.Errorf("%s:%d: error: scan: %s", self.params.File, self.line, fmt.Sprintf(format, args))
}

func (self *scanner) scan_key() bool {
	return true
}

func (self *scanner) scan_value() bool {
	return true
}

func (self *scanner) append_delim() {
	self.tokens = append(self.tokens, token{
		kind:  tokenKindDelim,
		value: self.params.Content[0:1],
		line:  self.line,
	})
	self.advance(1)
}
