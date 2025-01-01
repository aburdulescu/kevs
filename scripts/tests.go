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
	"time"
)

// TODO: run unittests
// TODO: collect and generate coverage
// TODO: run fuzzer
// TODO: use run CLI on fuzzer corpus?

const (
	testsOutDir = "tests-out"
)

var (
	buildDir = flag.String("b", "b", "Path to build directory")
	update   = flag.Bool("update", false, "Update expected output for integration tests with valid input")
	coverage = flag.Bool("coverage", false, "Generate code coverage")
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

	if err := runIntegrationTests(); err != nil {
		return err
	}

	return nil
}

func runIntegrationTests() error {
	var tests []IntegrationTest
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
		name = strings.TrimPrefix(name, "testdata/")
		expected := filepath.Join("testdata", name+".out")

		if !*update {
			if _, err := os.Stat(expected); err != nil {
				return fmt.Errorf("cannot find file with expected output for '%s'", path)
			}
		}

		tests = append(tests, IntegrationTest{
			name:     name,
			input:    path,
			expected: expected,
		})

		return nil
	})
	if err != nil {
		return err
	}

	maxName := 0
	for _, test := range tests {
		if len(test.name) > maxName {
			maxName = len(test.name)
		}
	}

	var total time.Duration

	var failed []TestResult

	fmt.Println("integration tests:")
	for _, test := range tests {
		fmt.Printf("%s %s ", test.name, strings.Repeat(".", maxName-len(test.name)))
		start := time.Now()
		err := test.run()
		end := time.Now()
		if err != nil {
			fmt.Print("failed")
			failed = append(failed, TestResult{name: test.name, err: err})
		} else {
			fmt.Print("passed")
		}
		e := end.Sub(start)
		total += e
		fmt.Printf("  %s\n", e)
	}

	fmt.Printf("\nsummary:\nran %d tests in %s, %d failed\n", len(tests), total, len(failed))

	if len(failed) != 0 {
		fmt.Println("\nfailures:")

		for _, r := range failed {
			fmt.Println(r.name)
			fmt.Println(" ", r.err)
		}
		fmt.Print("\n")

		return fmt.Errorf("tests failed")
	}

	if *coverage {
		var profiles []string
		profilesDir := filepath.Join(testsOutDir, "int", "coverage-out", "profraw")
		err := filepath.WalkDir(profilesDir, func(path string, d fs.DirEntry, err error) error {
			if err != nil {
				return err
			}
			profiles = append(profiles, path)
			return nil
		})
		if err != nil {
			return err
		}

		bins := []string{filepath.Join(*buildDir, "kevs")}
		out := filepath.Join(testsOutDir, "int", "coverage-out")
		if err := generateCoverage(out, profiles, bins); err != nil {
			return err
		}
	}

	return nil
}

type TestResult struct {
	name string
	err  error
}

type IntegrationTest struct {
	name     string
	input    string
	expected string
}

func (t IntegrationTest) run() error {
	if strings.HasPrefix(t.name, "valid") {
		return t.runValid()
	} else if strings.HasPrefix(t.name, "not_valid") {
		return t.runNotValid()
	} else {
		return fmt.Errorf("unexpected test name: %s", t.name)
	}
}

func (t IntegrationTest) runValid() error {
	exe := filepath.Join(*buildDir, "kevs")
	out := new(bytes.Buffer)

	cmd := exec.Command(exe, "-abort", "-dump", t.input)
	cmd.Stdout = out
	if *coverage {
		covProfile := filepath.Join(testsOutDir, "int", "coverage-out", "profraw", t.name) + ".profraw"
		cmd.Env = append(cmd.Env, "LLVM_PROFILE_FILE="+covProfile)
	}

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

func (t IntegrationTest) runNotValid() error {
	exe := filepath.Join(*buildDir, "kevs")
	out := new(bytes.Buffer)

	cmd := exec.Command(exe, "-no-err", t.input)
	cmd.Stdout = out
	if *coverage {
		covProfile := filepath.Join(testsOutDir, "int", "coverage-out", "profraw", t.name) + ".profraw"
		cmd.Env = append(cmd.Env, "LLVM_PROFILE_FILE="+covProfile)
	}

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

func generateCoverage(outDir string, profiles []string, binaries []string) error {
	dataFile := filepath.Join(outDir, "coverage.data")

	// merge raw profiles
	{
		args := []string{"merge", "-o", dataFile, "-sparse"}
		args = append(args, profiles...)
		cmd := exec.Command("llvm-profdata-14", args...)
		cmd.Stdout = os.Stdout
		cmd.Stderr = os.Stderr
		if err := cmd.Run(); err != nil {
			return err
		}
	}

	// generate HTML report
	{
		reportDir := filepath.Join(outDir, "html")
		args := []string{
			"show",
			"-instr-profile", dataFile,
			"-output-dir", reportDir,
			"-format=html",
			"-show-branches=percent",
			"-show-line-counts=true",
			"-show-regions=false",
		}
		args = append(args, binaries...)
		args = append(args, "kevs.c")
		cmd := exec.Command("llvm-cov-14", args...)
		cmd.Stdout = os.Stdout
		cmd.Stderr = os.Stderr
		if err := cmd.Run(); err != nil {
			return err
		}
	}

	return nil
}
