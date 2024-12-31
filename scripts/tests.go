package main

import (
	"bytes"
	"flag"
	"fmt"
	"io/fs"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"text/tabwriter"
	"time"
)

var (
	buildDir = flag.String("b", "b", "Path to build directory")
	update   = flag.Bool("update", false, "Update expected output")
	verbose  = flag.Bool("v", false, "Print test output")
)

func main() {
	if err := mainErr(); err != nil {
		fmt.Fprintln(os.Stderr, "error:", err)
		os.Exit(1)
	}
}

func mainErr() error {
	flag.Parse()

	*buildDir, _ = filepath.Abs(*buildDir)

	if err := runValid(); err != nil {
		return err
	}

	return nil
}

func runValid() error {
	var tests []Test
	err := filepath.WalkDir("testdata/valid", func(path string, d fs.DirEntry, err error) error {
		if err != nil {
			return err
		}

		if d.IsDir() {
			return nil
		}
		if !strings.HasSuffix(path, ".kevs") {
			return nil
		}

		name := strings.TrimSuffix(path, ".kevs")
		expected := name + ".out"

		if !*update {
			if _, err := os.Stat(expected); err != nil {
				return fmt.Errorf("cannot find file with expected output for '%s'", path)
			}
		}

		tests = append(tests, Test{
			name:     name,
			input:    path,
			expected: expected,
		})

		return nil
	})
	if err != nil {
		return err
	}

	var total time.Duration
	var nfailed int

	w := tabwriter.NewWriter(os.Stdout, 0, 2, 2, ' ', 0)

	for _, test := range tests {
		fmt.Fprintf(w, "%s\t", test.name)
		start := time.Now()
		err := test.run()
		end := time.Now()
		if err != nil {
			fmt.Fprint(w, "failed")
			nfailed++
		} else {
			fmt.Fprint(w, "passed")
		}
		e := end.Sub(start)
		total += e
		fmt.Fprintf(w, "\t%s\n", e)
	}

	w.Flush()

	fmt.Printf("summary: ran %d tests in %s, %d failed\n", len(tests), total, nfailed)

	if nfailed != 0 {
		return fmt.Errorf("tests failed")
	}

	return nil
}

func runNotValid() error {
	var tests []Test
	err := filepath.WalkDir("testdata/not_valid", func(path string, d fs.DirEntry, err error) error {
		if err != nil {
			return err
		}

		if d.IsDir() {
			return nil
		}
		if !strings.HasSuffix(path, ".kevs") {
			return nil
		}

		name := strings.TrimSuffix(path, ".kevs")
		expected := name + ".out"

		if !*update {
			if _, err := os.Stat(expected); err != nil {
				return fmt.Errorf("cannot find file with expected output for '%s'", path)
			}
		}

		tests = append(tests, Test{
			name:     name,
			input:    path,
			expected: expected,
		})

		return nil
	})
	if err != nil {
		return err
	}

	var total time.Duration
	var nfailed int

	w := tabwriter.NewWriter(os.Stdout, 0, 2, 2, ' ', 0)

	for _, test := range tests {
		fmt.Fprintf(w, "%s\t", test.name)
		start := time.Now()
		err := test.run()
		end := time.Now()
		if err != nil {
			fmt.Fprint(w, "failed")
			nfailed++
		} else {
			fmt.Fprint(w, "passed")
		}
		e := end.Sub(start)
		total += e
		fmt.Fprintf(w, "\t%s\n", e)
	}

	w.Flush()

	fmt.Printf("summary: ran %d tests in %s, %d failed\n", len(tests), total, nfailed)

	if nfailed != 0 {
		return fmt.Errorf("tests failed")
	}

	return nil
}

type Test struct {
	name     string
	input    string
	expected string
	out      bytes.Buffer
}

func (t Test) run() error {
	exe := filepath.Join(*buildDir, "kevs")
	cmd := exec.Command(exe, "-abort", "-dump", t.input)
	cmd.Stdout = &t.out
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("failed to run command: %w", err)
	}
	if *update {
		err := os.WriteFile(t.expected, t.out.Bytes(), 0600)
		if err != nil {
			return fmt.Errorf("failed to write output file: %w", err)
		}
	}
	return t.checkValid()
}

func (t Test) checkValid() error {
	data, err := os.ReadFile(t.expected)
	if err != nil {
		return fmt.Errorf("failed to read expected output file: %w", err)
	}
	haveLines := strings.Split(t.out.String(), "\n")
	wantLines := strings.Split(string(data), "\n")
	if len(haveLines) != len(wantLines) {
		return fmt.Errorf("error: want %d lines, have %d lines\n", len(wantLines), len(haveLines))
	}
	for i := range wantLines {
		if haveLines[i] != wantLines[i] {
			return fmt.Errorf("error: line %d: want '%s', have '%s'\n", i+1, wantLines[i], haveLines[i])
		}
	}
	return nil
}
