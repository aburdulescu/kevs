package main

import (
	"flag"
	"fmt"
	"os"

	"github.com/aburdulescu/kevs/src/go"
)

var (
	abortOnError = flag.Bool("abort", false, "Abort when encountering an error")
	dump         = flag.Bool("dump", false, "Print keys and values, or tokens if -scan is active")
	onlyScan     = flag.Bool("scan", false, "Run only the scanner")
	_            = flag.Bool("free", false, "Not used")
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

	params := kevs.Params{
		File:         file,
		Content:      string(data),
		AbortOnError: *abortOnError,
	}

	tokens, err := kevs.Scan(params)
	if err != nil {
		return err
	}

	if *dump && *onlyScan {
		for _, tok := range tokens {
			fmt.Println(tok.Kind, tok.Value)
		}
	}

	if *onlyScan {
		return nil
	}

	root, err := kevs.ParseTokens(params, tokens)
	if err != nil {
		return err
	}

	if *dump {
		root.Dump()
	}

	return nil
}
