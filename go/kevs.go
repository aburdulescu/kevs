package main

import (
	"flag"
	"fmt"
	"os"
	"strings"
)

var (
	abortOnError = flag.Bool("abort", false, "Abort when encountering an error")
	dump         = flag.Bool("dump", false, "Print keys and values, or tokens if -scan is active")
	onlyScan     = flag.Bool("scan", false, "Run only the scanner")
	_            = flag.Bool("free", false, "dummy")
	noErr        = flag.Bool("no-err", false, "Exit with code 0 even if an error was encountered")
)

func main() {
	if err := mainErr(); err != nil {
		fmt.Println(err)
		if *noErr {
			os.Exit(0)
		} else {
			os.Exit(1)
		}
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

	params := Params{
		File:         file,
		Content:      string(data),
		AbortOnError: *abortOnError,
	}

	tokens, err := scan(params)
	if err != nil {
		return err
	}

	if *dump && *onlyScan {
		for _, tok := range tokens {
			fmt.Println(tokenkind_str(tok.kind), tok.value)
		}
	}

	if *onlyScan {
		return nil
	}

	root, err := parse(params, tokens)
	if err != nil {
		return err
	}

	if *dump {
		root.dump()
	}

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

type List []Value

type ValueData struct {
	List    List
	Table   Table
	String  string
	Integer int64
	Boolean bool
}

type Value struct {
	Kind ValueKind
	Data ValueData
}

type KeyValue struct {
	Key   string
	Value Value
}

type Table []KeyValue

func Parse(params Params) (Table, error) {
	tokens, err := scan(params)
	if err != nil {
		return nil, err
	}
	return parse(params, tokens)
}

type tokenKind uint8

const (
	tokenKindUndefined tokenKind = iota
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
		switch {
		case s.expect('\n'):
			ok = s.scan_newline()
		case s.expect(kCommentBegin):
			ok = s.scan_comment()
		default:
			ok = s.scan_key_value()
		}
		if !ok {
			return nil, s.err
		}
	}

	return s.tokens, nil
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
	return self.scan_value()
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
	switch {
	case self.expect(kListBegin):
		ok = self.scan_list_value()
	case self.expect(kTableBegin):
		ok = self.scan_table_value()
	case self.expect(kStringBegin):
		ok = self.scan_string_value()
	case self.expect(kRawStringBegin):
		ok = self.scan_raw_string()
	default:
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
	end := 1
	s := self.params.Content

	for {
		// search for trailing quote
		i := strings.IndexByte(s[end:], kStringBegin)
		if i == -1 {
			self.errorf("string value does not end with quote")
			return false
		}

		// advance
		end += i + 1

		// stop if quote is not escaped
		if prev := s[end-2]; prev != '\\' {
			break
		}
	}

	self.append(tokenKindValue, end)

	return true
}

func (self *scanner) scan_raw_string() bool {
	end := strings.IndexByte(self.params.Content[1:], kRawStringBegin)
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

type parser struct {
	params Params
	tokens []token
	table  Table
	i      int
	err    error
}

func parse(params Params, tokens []token) (Table, error) {
	p := parser{
		params: params,
		tokens: tokens,
	}

	for p.i < len(tokens) {
		kv, ok := p.parse_key_value(p.table)
		if !ok {
			return nil, p.err
		}
		p.table = append(p.table, *kv)
	}

	return p.table, nil
}

func (self *parser) parse_key_value(parent Table) (*KeyValue, bool) {
	key, ok := self.parse_key(parent)
	if !ok {
		return nil, false
	}

	if !self.parse_delim(kKeyValSep) {
		self.errorf("missing key value separator")
		return nil, false
	}

	val, ok := self.parse_value()
	if !ok {
		return nil, false
	}

	out := &KeyValue{
		Key:   key,
		Value: *val,
	}

	return out, true
}

func (self *parser) parse_key(parent Table) (string, bool) {
	if !self.expect(tokenKindKey) {
		self.errorf("expected key token")
		return "", false
	}

	tok := self.get()

	if !is_identifier(tok.value) {
		self.errorf("key is not a valid identifier: '%s'", tok.value)
		return "", false
	}

	// check if key is unique
	for _, kv := range parent {
		if kv.Key == tok.value {
			self.errorf("key '%s' is not unique for current table", tok.value)
			return "", false
		}
	}

	key := tok.value

	self.pop()

	return key, true
}

func (self *parser) parse_value() (*Value, bool) {
	var ok bool
	var out *Value

	switch {
	case self.expect_delim(kListBegin):
		out, ok = self.parse_list_value()
	case self.expect_delim(kTableBegin):
		out, ok = self.parse_table_value()
	default:
		out, ok = self.parse_simple_value()
	}
	if !ok {
		return nil, false
	}

	if !self.parse_delim(kKeyValEnd) {
		self.errorf("missing key value end")
		return nil, false
	}

	return out, true
}

func (self *parser) parse_list_value() (*Value, bool) {
	out := &Value{
		Kind: ValueKindList,
	}

	self.pop()

	for {
		if self.parse_delim(kListEnd) {
			return out, true
		}

		v, ok := self.parse_value()
		if !ok {
			return nil, false
		}
		out.Data.List = append(out.Data.List, *v)

		if self.parse_delim(kListEnd) {
			return out, true
		}
	}
}

func (self *parser) parse_table_value() (*Value, bool) {
	out := &Value{
		Kind: ValueKindTable,
	}

	self.pop()

	for {
		if self.parse_delim(kTableEnd) {
			return out, true
		}

		kv, ok := self.parse_key_value(out.Data.Table)
		if !ok {
			return nil, false
		}
		out.Data.Table = append(out.Data.Table, *kv)

		if self.parse_delim(kTableEnd) {
			return out, true
		}
	}
}

func (self *parser) parse_simple_value() (*Value, bool) {
	if !self.expect(tokenKindValue) {
		self.errorf("expected value token")
		return nil, false
	}

	val := self.get().value

	ok := true
	out := &Value{}

	switch {
	case val[0] == kStringBegin:
		data, err := normString(val[1 : len(val)-1])
		if err != nil {
			self.errorf("could not normalize string: %s", err)
			return nil, false
		}
		out.Kind = ValueKindString
		out.Data.String = data

	case val[0] == kRawStringBegin:
		out.Kind = ValueKindString
		out.Data.String = val[1 : len(val)-1]

	case val == "true":
		out.Kind = ValueKindBoolean
		out.Data.Boolean = true

	case val == "false":
		out.Kind = ValueKindBoolean
		out.Data.Boolean = false

	default:
		i, err := str_to_int(val, 0)
		if err != nil {
			self.errorf("value '%s' is not an integer: %s", val, err)
			ok = false
		} else {
			out.Kind = ValueKindInteger
			out.Data.Integer = i
		}
	}

	self.pop()

	return out, ok
}

func normString(s string) (string, error) {
	dst := strings.Builder{}

	for i := 0; i < len(s); {
		if s[i] == '\\' {
			i++
			switch s[i] {
			case 'a':
				dst.WriteByte('\a')
				i++
			case 'b':
				dst.WriteByte('\b')
				i++
			case 'f':
				dst.WriteByte('\f')
				i++
			case 'n':
				dst.WriteByte('\n')
				i++
			case 'r':
				dst.WriteByte('\r')
				i++
			case 't':
				dst.WriteByte('\t')
				i++
			case 'v':
				dst.WriteByte('\v')
				i++
			case '"':
				dst.WriteByte('"')
				i++
			case '\\':
				dst.WriteByte('\\')
				i++

			case 'u':
				i++

				if (i + 4) > len(s) {
					return "", fmt.Errorf("\\u must be followed by 4 hex digits: \\uXXXX")
				}

				code, err := str_to_uint(s[i:i+4], 16)
				if err != nil {
					return "", err
				}
				i += 4

				utf8 := ucs_to_utf8(code)
				if utf8 == nil {
					return "", fmt.Errorf("could not encode Unicode code point to UTF-8")
				}
				dst.Write(utf8)

			case 'U':
				i++

				if (i + 8) > len(s) {
					return "", fmt.Errorf("\\U must be followed by 8 hex digits: \\UXXXXXXXX")
				}

				code, err := str_to_uint(s[i:i+8], 16)
				if err != nil {
					return "", err
				}
				i += 8

				utf8 := ucs_to_utf8(code)
				if utf8 == nil {
					return "", fmt.Errorf("could not encode Unicode code point to UTF-8")
				}
				dst.Write(utf8)

			default:
				return "", fmt.Errorf("unknown escape sequence")

			}
		} else {
			dst.WriteByte(s[i])
			i++
		}
	}

	return dst.String(), nil
}

// Convert UCS code point to UTF-8
func ucs_to_utf8(code uint64) []byte {
	// Code points in the surrogate range are not valid for UTF-8.
	if 0xd800 <= code && code <= 0xdfff {
		return nil
	}

	var out []byte

	// 0x00000000 - 0x0000007F:
	// 0xxxxxxx
	if code <= 0x0000007F {
		out = append(out, byte(code))
		return out
	}

	// 0x00000080 - 0x000007FF:
	// 110xxxxx 10xxxxxx
	if code <= 0x000007FF {
		out = append(out, byte(0xc0|(code>>6)))
		out = append(out, byte(0x80|(code&0x3f)))
		return out
	}

	// 0x00000800 - 0x0000FFFF:
	// 1110xxxx 10xxxxxx 10xxxxxx
	if code <= 0x0000FFFF {
		out = append(out, byte(0xe0|(code>>12)))
		out = append(out, byte(0x80|((code>>6)&0x3f)))
		out = append(out, byte(0x80|(code&0x3f)))
		return out
	}

	// 0x00010000 - 0x0010FFFF:
	// 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
	if code <= 0x0010FFFF {
		out = append(out, byte(0xf0|(code>>18)))
		out = append(out, byte(0x80|((code>>12)&0x3f)))
		out = append(out, byte(0x80|((code>>6)&0x3f)))
		out = append(out, byte(0x80|(code&0x3f)))
		return out
	}

	return nil
}

func (self *parser) parse_delim(c byte) bool {
	if !self.expect_delim(c) {
		return false
	}
	self.pop()
	return true
}

func (self parser) expect_delim(delim byte) bool {
	if !self.expect(tokenKindDelim) {
		return false
	}
	return self.get().value == string(delim)
}

func (self *parser) errorf(format string, args ...any) {
	self.err = fmt.Errorf("%s:%d: error: parse: %s", self.params.File, self.get().line, fmt.Sprintf(format, args...))

	if self.params.AbortOnError {
		panic(self.err)
	}
}

func is_digit(c byte) bool { return c >= '0' && c <= '9' }

func lower(c byte) byte { return (c | ('x' - 'X')) }

func is_letter(c byte) bool {
	return lower(c) >= 'a' && lower(c) <= 'z'
}

func is_identifier(s string) bool {
	c := s[0]
	if c != '_' && !is_letter(c) {
		return false
	}
	for i := 1; i < len(s); i++ {
		if !is_digit(s[i]) && !is_letter(s[i]) && s[i] != '_' {
			return false
		}
	}
	return true
}

func (self parser) get() token {
	return self.tokens[self.i]
}

func (self *parser) pop() { self.i++ }

func (self parser) expect(kind tokenKind) bool {
	if self.i >= len(self.tokens) {
		self.errorf("expected token '%s', have nothing", tokenkind_str(kind))
		return false
	}
	return self.get().kind == kind
}

func tokenkind_str(v tokenKind) string {
	switch v {
	case tokenKindUndefined:
		return "undefined"
	case tokenKindKey:
		return "key"
	case tokenKindDelim:
		return "delim"
	case tokenKindValue:
		return "value"
	default:
		return "unknown"
	}
}

func (self Table) dump() {
	for _, kv := range self {
		switch kv.Value.Kind {
		case ValueKindTable:
			fmt.Printf("%s %s\n", kv.Key, valuekind_str(kv.Value.Kind))
			kv.Value.Data.Table.dump()

		case ValueKindList:
			fmt.Printf("%s %s\n", kv.Key, valuekind_str(kv.Value.Kind))
			kv.Value.Data.List.dump()

		case ValueKindString:
			fmt.Printf("%s %s '%s'\n", kv.Key, valuekind_str(kv.Value.Kind), kv.Value.Data.String)

		case ValueKindBoolean:
			fmt.Printf("%s %s %v\n", kv.Key, valuekind_str(kv.Value.Kind), kv.Value.Data.Boolean)

		case ValueKindInteger:
			fmt.Printf("%s %s %d\n", kv.Key, valuekind_str(kv.Value.Kind), kv.Value.Data.Integer)

		case ValueKindUndefined:
			fmt.Printf("%s %s\n", kv.Key, valuekind_str(kv.Value.Kind))

		default:
			fmt.Printf("%s %s\n", kv.Key, valuekind_str(kv.Value.Kind))

		}
	}
}

func valuekind_str(v ValueKind) string {
	switch v {
	case ValueKindUndefined:
		return "undefined"
	case ValueKindString:
		return "string"
	case ValueKindInteger:
		return "integer"
	case ValueKindBoolean:
		return "boolean"
	case ValueKindList:
		return "list"
	case ValueKindTable:
		return "table"
	default:
		return "unknown"
	}
}

func (self List) dump() {
	for _, v := range self {
		switch v.Kind {
		case ValueKindTable:
			fmt.Printf("%s\n", valuekind_str(v.Kind))
			v.Data.Table.dump()

		case ValueKindList:
			fmt.Printf("%s\n", valuekind_str(v.Kind))
			v.Data.List.dump()

		case ValueKindString:
			fmt.Printf("%s '%s'\n", valuekind_str(v.Kind), v.Data.String)

		case ValueKindBoolean:
			fmt.Printf("%s %v\n", valuekind_str(v.Kind), v.Data.Boolean)

		case ValueKindInteger:
			fmt.Printf("%s %d\n", valuekind_str(v.Kind), v.Data.Integer)

		case ValueKindUndefined:
			fmt.Printf("%s\n", valuekind_str(v.Kind))

		default:
			fmt.Printf("%s\n", valuekind_str(v.Kind))

		}
	}
}

func str_to_uint(s string, base uint64) (uint64, error) {
	if len(s) == 0 {
		return 0, fmt.Errorf("empty input")
	}

	if base == 0 {
		if s[0] == '0' {
			// stop if 0
			if len(s) == 1 {
				return 0, nil
			}
			if len(s) < 3 {
				return 0, fmt.Errorf("leading 0 requires at least 2 more chars")
			}
			switch s[1] {
			case 'x':
				base = 16
				s = s[2:]
			case 'o':
				base = 8
				s = s[2:]
			case 'b':
				base = 2
				s = s[2:]
			default:
				return 0, fmt.Errorf("invalid base char, must be 'x', 'o' or 'b'")
			}
		} else {
			base = 10
		}
	} else {
		if base != 2 && base != 8 && base != 16 {
			return 0, fmt.Errorf("invalid base")
		}
	}

	const max = 1<<64 - 1

	// cutoff is the smallest number such that cutoff*base > max.
	cutoff := max/base + 1

	n := uint64(0)
	for i := 0; i < len(s); i++ {
		c := s[i]

		d := uint64(0)
		switch {
		case is_digit(c):
			d = uint64(c - '0')
		case is_letter(c):
			d = uint64(lower(c) - 'a' + 10)
		default:
			return 0, fmt.Errorf("invalid char, must be a letter or a digit")
		}

		if d >= base {
			return 0, fmt.Errorf("invalid digit, bigger than base")
		}

		if n >= cutoff {
			return 0, fmt.Errorf("invalid input, mul overflows")
		}
		n *= base

		n1 := n + d
		if n1 < n || n1 > max {
			return 0, fmt.Errorf("invalid input, add overflows")
		}
		n = n1
	}

	return n, nil
}

func str_to_int(s string, base uint64) (int64, error) {
	if len(s) == 0 {
		return 0, fmt.Errorf("empty input")
	}

	neg := false
	if s[0] == '+' {
		s = s[1:]
	} else if s[0] == '-' {
		neg = true
		s = s[1:]
	}

	un, err := str_to_uint(s, base)
	if err != nil {
		return 0, err
	}

	max := uint64(1) << 63

	if !neg && un >= max {
		return 0, fmt.Errorf("invalid input, overflows max value")
	}
	if neg && un > max {
		return 0, fmt.Errorf("invalid input, underflows min value")
	}

	//nolint overflow checked above
	n := int64(un)
	if neg && n >= 0 {
		n = -n
	}

	return n, nil
}
