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
)

var (
	buildDir       = flag.String("build-dir", "b", "Path to build directory")
	skipNotValid   = flag.Bool("skip-not-valid", false, "Skip not valid tests")
	skipValid      = flag.Bool("skip-valid", false, "Skip valid tests")
	updateExpected = flag.Bool("update", false, "Update expected output")
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

	if !*skipValid {
		if err := runValid(); err != nil {
			return err
		}
	}

	if !*skipNotValid {

	}

	return nil
}

type Test struct {
	name     string
	input    string
	expected string
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

		name := strings.TrimSuffix(filepath.Base(path), ".kevs")
		expected := filepath.Join("testdata", "valid", name+".out")

		if !*updateExpected {
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

	out := new(bytes.Buffer)

	for _, test := range tests {
		fmt.Println(test.name)

		exe := filepath.Join(*buildDir, "kevs")

		out.Reset()

		cmd := exec.Command(exe, "-abort", "-dump", test.input)
		cmd.Stdout = out

		if err := cmd.Run(); err != nil {
			fmt.Println("error:", err)
			continue
		}

		if *updateExpected {
			err := os.WriteFile(test.expected, out.Bytes(), 0600)
			if err != nil {
				fmt.Println("error:", err)
				continue
			}
		} else {
			data, err := os.ReadFile(test.expected)
			if err != nil {
				fmt.Println("error:", err)
				continue
			}
			haveLines := strings.Split(out.String(), "\n")
			wantLines := strings.Split(string(data), "\n")
			if len(haveLines) != len(wantLines) {
				fmt.Printf("error: want %d lines, have %d lines\n", len(wantLines), len(haveLines))
				continue
			}
			for i := range wantLines {
				if haveLines[i] != wantLines[i] {
					fmt.Printf("error: line %d: want '%s', have '%s'\n", i+1, wantLines[i], haveLines[i])
					continue
				}
			}
		}
	}

	return nil
}
