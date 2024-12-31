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

	if err := run(); err != nil {
		return err
	}

	return nil
}

func run() error {
	var tests []Test
	err := filepath.WalkDir("testdata", func(path string, d fs.DirEntry, err error) error {
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

	var failed []TestResult

	w := tabwriter.NewWriter(os.Stdout, 0, 2, 2, ' ', 0)

	for _, test := range tests {
		fmt.Fprintf(w, "%s\t", test.name)
		start := time.Now()
		err := test.run()
		end := time.Now()
		if err != nil {
			fmt.Fprint(w, "failed")
			failed = append(failed, TestResult{name: test.name, err: err})
		} else {
			fmt.Fprint(w, "passed")
		}
		e := end.Sub(start)
		total += e
		fmt.Fprintf(w, "\t%s\n", e)
	}

	w.Flush()

	fmt.Printf("\nsummary: ran %d tests in %s, %d failed\n", len(tests), total, len(failed))

	if len(failed) != 0 {
		fmt.Println("\nfailures:")

		for _, r := range failed {
			fmt.Println("  ", r.name, r.err)
		}
		fmt.Print("\n")

		return fmt.Errorf("tests failed")
	}

	return nil
}

type TestResult struct {
	name string
	err  error
}

type Test struct {
	name     string
	input    string
	expected string
}

func (t Test) run() error {
	if strings.HasPrefix(t.name, "testdata/valid") {
		return t.runValid()
	} else if strings.HasPrefix(t.name, "testdata/not_valid") {
		return t.runNotValid()
	} else {
		return fmt.Errorf("unexpected test name: %s", t.name)
	}
}

func (t Test) runValid() error {
	exe := filepath.Join(*buildDir, "kevs")
	out := new(bytes.Buffer)

	cmd := exec.Command(exe, "-abort", "-dump", t.input)
	cmd.Stdout = out

	if err := cmd.Run(); err != nil {
		return fmt.Errorf("failed to run command: %w", err)
	}

	if *update {
		err := os.WriteFile(t.expected, out.Bytes(), 0600)
		if err != nil {
			return fmt.Errorf("failed to write output file: %w", err)
		}
	}

	data, err := os.ReadFile(t.expected)
	if err != nil {
		return fmt.Errorf("failed to read expected output file: %w", err)
	}

	haveLines := strings.Split(out.String(), "\n")
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

func (t Test) runNotValid() error {
	exe := filepath.Join(*buildDir, "kevs")
	out := new(bytes.Buffer)

	cmd := exec.Command(exe, "-no-err", t.input)
	cmd.Stdout = out

	if err := cmd.Run(); err != nil {
		return fmt.Errorf("failed to run command: %w", err)
	}

	data, err := os.ReadFile(t.expected)
	if err != nil {
		return fmt.Errorf("failed to read expected output file: %w", err)
	}

	haveLines := strings.Split(out.String(), "\n")
	want := strings.TrimSuffix(string(data), "\n")

	for _, line := range haveLines {
		if strings.Index(line, want) != -1 {
			return nil
		}
	}

	return fmt.Errorf("error: output does not contain '%s'\n", want)
}
