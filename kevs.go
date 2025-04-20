package main

import (
	"flag"
	"fmt"
	"os"
	"strings"
)

func main() {
	if err := mainErr(); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
}

func mainErr() error {
	flag.Bool("abort", false, "Abort on error")
	flag.Parse()

	if flag.NArg() == 0 {
		return fmt.Errorf("need file")
	}

	file := flag.Arg(0)

	data, err := os.ReadFile(file)
	if err != nil {
		return err
	}

	t, err := parse(Params{
		File:         file,
		Content:      string(data),
		AbortOnError: true,
	})
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
	File         string
	Content      string
	AbortOnError bool
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
		self.errorf("comment does not end with newline")
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

func (self *scanner) errorf(format string, args ...any) {
	self.err = fmt.Errorf("%s:%d: error: scan: %s", self.params.File, self.line, fmt.Sprintf(format, args...))

	if self.params.AbortOnError {
		panic(self.err)
	}
}

func indexAny(s, chars string) (rune, int) {
	i := strings.IndexAny(s, chars)
	if i == -1 {
		return 0, -1
	}
	for _, r := range chars {
		i := strings.IndexRune(s, r)
		if i != -1 {
			return r, i
		}
	}
	return 0, -1
}

func (self *scanner) scan_key() bool {
	c, end := indexAny(self.params.Content, "=\n")
	if end == -1 || c != kKeyValSep {
		self.errorf("key-value pair is missing separator")
		return false
	}
	self.append(tokenKindKey, end)
	if len(self.tokens[len(self.tokens)-1].value) == 0 {
		self.errorf("empty key")
		return false
	}
	return true
}

func (self *scanner) scan_value() bool {
	self.trim_space()
	ok := false
	if self.expect(kListBegin) {
		ok = self.scan_list_value()
	} else if self.expect(kTableBegin) {
		ok = self.scan_table_value()
	} else if self.expect(kStringBegin) {
		ok = self.scan_string_value()
	} else if self.expect(kRawStringBegin) {
		ok = self.scan_raw_string()
	} else {
		ok = self.scan_int_or_bool_value()
	}
	if !ok {
		return false
	}
	if !self.scan_delim(kKeyValEnd) {
		self.errorf("value does not end with semicolon")
		return false
	}
	return true
}

func (self *scanner) scan_delim(c byte) bool {
	if !self.expect(c) {
		return false
	}
	self.append_delim()
	return true
}

func (self *scanner) scan_string_value() bool {
	// advance past leading quote
	s := self.params.Content[1:]

	for {
		// search for trailing quote
		i := strings.IndexByte(s, kStringBegin)

		if i == -1 {
			self.errorf("string value does not end with quote")
			return false
		}

		// get previous char
		prev := s[i-1]

		// advance
		s = s[i+1:]

		// stop if quote is not escaped
		if prev != '\\' {
			break
		}
	}

	// calculate the end, includes trailing quote
	end := len(s)

	self.append(tokenKindValue, end)

	return true
}

func (self *scanner) scan_raw_string() bool {
	end :=
		strings.IndexByte(self.params.Content[1:], kRawStringBegin)
	if end == -1 {
		self.errorf("raw string value does not end with backtick")
		return false
	}

	// +2 for leading and trailing quotes
	self.append(tokenKindValue, end+2)

	// count newlines in raw string to keep line count accurate
	self.line += strings.Count(self.tokens[len(self.tokens)-1].value, "\n")

	return true
}

func (self *scanner) scan_int_or_bool_value() bool {
	// search for all possible value endings
	// if semicolon(or none of them) is not found => error
	c, end := indexAny(self.params.Content, ";]}\n")
	if end == -1 || c != kKeyValEnd {
		self.errorf("integer or boolean value does not end with semicolon")
		return false
	}
	self.append(tokenKindValue, end)
	return true
}

func (self *scanner) scan_list_value() bool {
	self.append_delim()
	for {
		self.trim_space()
		if len(self.params.Content) == 0 {
			self.errorf("end of input without list end")
			return false
		}
		if self.expect('\n') {
			if !self.scan_newline() {
				return false
			}
			continue
		}
		if self.expect(kCommentBegin) {
			if !self.scan_comment() {
				return false
			}
			continue
		}
		if self.expect(kListEnd) {
			self.append_delim()
			return true
		}
		if !self.scan_value() {
			return false
		}
		if self.expect(kListEnd) {
			self.append_delim()
			return true
		}
	}
	return true
}

func (self *scanner) scan_table_value() bool {
	self.append_delim()
	for {
		self.trim_space()
		if len(self.params.Content) == 0 {
			self.errorf("end of input without table end")
			return false
		}
		if self.expect('\n') {
			if !self.scan_newline() {
				return false
			}
			continue
		}
		if self.expect(kCommentBegin) {
			if !self.scan_comment() {
				return false
			}
			continue
		}
		if self.expect(kTableEnd) {
			self.append_delim()
			return true
		}
		if !self.scan_key_value() {
			return false
		}
		if self.expect(kTableEnd) {
			self.append_delim()
			return true
		}
	}
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

func (self *scanner) append(kind tokenKind, end int) {
	val := self.params.Content[:end]
	val = strings.TrimRight(val, spaces)

	self.tokens = append(self.tokens, token{
		kind:  kind,
		value: val,
		line:  self.line,
	})

	self.advance(end)
}
