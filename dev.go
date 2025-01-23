package main

import (
	"bytes"
	"context"
	"flag"
	"fmt"
	"io"
	"io/fs"
	"os"
	"os/exec"
	"os/signal"
	"path/filepath"
	"strconv"
	"strings"
	"syscall"
	"time"
)

const (
	devOutDir = "dev-out"
)

var (
	buildDir                = flag.String("b", "b", "Path to build directory")
	update                  = flag.Bool("update", false, "Update expected output for integration tests with valid input")
	disableUnitTests        = flag.Bool("no-ut", false, "Disable unit tests")
	disableExample          = flag.Bool("no-ex", false, "Disable example")
	disableIntegrationTests = flag.Bool("no-it", false, "Disable integration tests")
	disableCodeCoverage     = flag.Bool("no-cc", false, "Disable code coverage")
	enableFuzzer            = flag.Bool("fuzz", false, "Run fuzzer")
	fuzzTime                = flag.Int("fuzz-time", 30, "Run fuzzer for that number of seconds")
	fuzzMaxLen              = flag.Int("fuzz-max-len", 8192, "Run fuzzer that max input size")
	osTag                   = flag.String("os", "linux", "Os tag: linux or windows")
)

var ctx context.Context

func main() {
	if err := mainErr(); err != nil {
		fmt.Fprintln(os.Stderr, "error:", err)
		os.Exit(1)
	}
}

func mainErr() error {
	flag.Parse()

	switch *osTag {
	case "linux", "windows":
	default:
		return fmt.Errorf("OS '%s' is not supported", *osTag)
	}

	*buildDir, _ = filepath.Abs(*buildDir)

	var cancel context.CancelFunc
	ctx, cancel = context.WithCancel(context.Background())

	sigc := make(chan os.Signal, 2)
	signal.Notify(sigc, syscall.SIGINT, syscall.SIGTERM)
	go func() {
		<-sigc
		cancel()
	}()

	if *enableFuzzer {
		return runFuzzer()
	}

	if !*disableUnitTests {
		if err := runUnitTests(); err != nil {
			return err
		}
	}
	if !*disableIntegrationTests {
		if err := runIntegrationTests(); err != nil {
			return err
		}
	}
	if !*disableExample {
		if err := runExample(); err != nil {
			return err
		}
	}

	globalResult.summary()

	return nil
}

type GlobalResult struct {
	total    int
	failed   []TestResult
	duration time.Duration
}

func (g *GlobalResult) add(name string, err error, dur time.Duration) {
	g.total++
	if err != nil {
		g.failed = append(g.failed, TestResult{name: name, err: err})
	}
	g.duration += dur
}

func (g GlobalResult) summary() {
	fmt.Printf("\nsummary:\nran %d tests in %s, %d failed\n", g.total, g.duration, len(g.failed))
	if len(g.failed) != 0 {
		fmt.Println("\nfailures:")
		for _, r := range g.failed {
			fmt.Println(r.name)
			fmt.Println(" ", r.err)
		}
		fmt.Print("\n")
	}
}

var globalResult GlobalResult

func runFuzzer() error {
	const (
		mainCorpusDir = "testdata/corpus"
	)
	var (
		fuzzOutDir    = filepath.Join(devOutDir, "fuzz")
		tempCorpusDir = filepath.Join(fuzzOutDir, "corpus")
		covProfile    = filepath.Join(fuzzOutDir, "coverage.profraw")
	)

	outBuf := new(bytes.Buffer)
	errBuf := new(bytes.Buffer)

	os.RemoveAll(fuzzOutDir)

	// add testdata to corpus
	{

		dirs := []string{"testdata/not_valid/", "testdata/valid/"}
		for _, dir := range dirs {
			exe := filepath.Join(*buildDir, "fuzzer")

			cmd := exec.CommandContext(ctx, exe, "-create_missing_dirs=1", "-merge=1", mainCorpusDir, dir)
			cmd.Stdout = outBuf
			cmd.Stderr = errBuf

			fmt.Printf("add %s to corpus ... ", dir)
			err := cmd.Run()
			fmt.Println("done")
			if err != nil {
				return err
			}

			// write logs
			outFile := filepath.Join(fuzzOutDir, "logs", dir, "out")
			errFile := filepath.Join(fuzzOutDir, "logs", dir, "err")
			os.MkdirAll(filepath.Dir(outFile), 0755)
			if err := os.WriteFile(outFile, outBuf.Bytes(), 0600); err != nil {
				return fmt.Errorf("failed to write stdout file: %w", err)
			}
			if err := os.WriteFile(errFile, errBuf.Bytes(), 0600); err != nil {
				return fmt.Errorf("failed to write stderr file: %w", err)
			}

			outBuf.Reset()
			errBuf.Reset()
		}
	}

	os.MkdirAll(tempCorpusDir, 0755)

	// add main corpus to temp
	{
		files, _ := os.ReadDir(mainCorpusDir)
		for _, file := range files {
			dstFile := filepath.Join(tempCorpusDir, file.Name())
			srcFile := filepath.Join(mainCorpusDir, file.Name())
			if err := copyFile(dstFile, srcFile); err != nil {
				return err
			}
		}
	}

	// run fuzzer
	{
		exe := filepath.Join(*buildDir, "fuzzer")

		cmd := exec.CommandContext(ctx,
			exe,
			"-max_total_time="+strconv.Itoa(*fuzzTime),
			"-create_missing_dirs=1",
			"-max_len="+strconv.Itoa(*fuzzMaxLen),
			"-artifact_prefix="+fuzzOutDir,
			tempCorpusDir,
		)
		cmd.Stdout = outBuf
		cmd.Stderr = errBuf
		cmd.Env = append(cmd.Env, "LLVM_PROFILE_FILE="+covProfile)

		fmt.Printf("run fuzzer for %s ... ", time.Duration(*fuzzTime)*time.Second)
		err := cmd.Run()
		fmt.Println("done")
		if err != nil {
			return err
		}

		// write logs
		outFile := filepath.Join(fuzzOutDir, "logs", "fuzzer", "out")
		errFile := filepath.Join(fuzzOutDir, "logs", "fuzzer", "err")
		os.MkdirAll(filepath.Dir(outFile), 0755)
		if err := os.WriteFile(outFile, outBuf.Bytes(), 0600); err != nil {
			return fmt.Errorf("failed to write stdout file: %w", err)
		}
		if err := os.WriteFile(errFile, errBuf.Bytes(), 0600); err != nil {
			return fmt.Errorf("failed to write stderr file: %w", err)
		}

		outBuf.Reset()
		errBuf.Reset()
	}

	os.RemoveAll(mainCorpusDir)

	// merge corpus
	{
		exe := filepath.Join(*buildDir, "fuzzer")

		cmd := exec.CommandContext(ctx, exe, "-create_missing_dirs=1", "-merge=1", mainCorpusDir, tempCorpusDir)
		cmd.Stdout = outBuf
		cmd.Stderr = errBuf

		fmt.Print("merge corpus ... ")
		err := cmd.Run()
		fmt.Println("done")
		if err != nil {
			return err
		}

		// write logs
		outFile := filepath.Join(fuzzOutDir, "logs", "merge", "out")
		errFile := filepath.Join(fuzzOutDir, "logs", "merge", "err")
		os.MkdirAll(filepath.Dir(outFile), 0755)
		if err := os.WriteFile(outFile, outBuf.Bytes(), 0600); err != nil {
			return fmt.Errorf("failed to write stdout file: %w", err)
		}
		if err := os.WriteFile(errFile, errBuf.Bytes(), 0600); err != nil {
			return fmt.Errorf("failed to write stderr file: %w", err)
		}

		outBuf.Reset()
		errBuf.Reset()
	}

	// generate coverage
	{
		profiles := []string{covProfile}
		bins := []string{filepath.Join(*buildDir, "fuzzer")}
		coverageOut := filepath.Join(devOutDir, "fuzz", "coverage")
		if err := generateCoverage(coverageOut, profiles, bins); err != nil {
			return err
		}
	}

	return nil
}

func runExample() error {
	exe := filepath.Join(*buildDir, "example")
	outBuf := new(bytes.Buffer)
	errBuf := new(bytes.Buffer)

	var args []string
	if *osTag == "windows" {
		args = append(args, "wine")
	}
	args = append(args, exe)

	cmd := exec.CommandContext(ctx, args[0], args[1:]...)
	cmd.Stdout = outBuf
	cmd.Stderr = errBuf

	fmt.Print("\nexample .. ")

	start := time.Now()
	err := cmd.Run()
	end := time.Now()
	dur := end.Sub(start)

	// write logs
	{
		outFile := filepath.Join(devOutDir, "example", "logs", "out")
		errFile := filepath.Join(devOutDir, "example", "logs", "err")
		os.MkdirAll(filepath.Dir(outFile), 0755)
		if err := os.WriteFile(outFile, outBuf.Bytes(), 0600); err != nil {
			return fmt.Errorf("failed to write stdout file: %w", err)
		}
		if err := os.WriteFile(errFile, errBuf.Bytes(), 0600); err != nil {
			return fmt.Errorf("failed to write stderr file: %w", err)
		}
	}

	if err == nil {
		fmt.Print("passed")
	} else {
		fmt.Print("failed")
	}
	fmt.Printf(" %s\n", dur)

	globalResult.add("example", err, dur)

	return nil
}

func runUnitTests() error {
	exe := filepath.Join(*buildDir, "unittests")
	outBuf := new(bytes.Buffer)
	errBuf := new(bytes.Buffer)

	covProfile := filepath.Join(devOutDir, "unit", "coverage", "coverage.profraw")

	var args []string
	if *osTag == "windows" {
		args = append(args, "wine")
	}
	args = append(args, exe)

	cmd := exec.CommandContext(ctx, args[0], args[1:]...)
	cmd.Stdout = outBuf
	cmd.Stderr = errBuf

	if !*disableCodeCoverage {
		cmd.Env = append(cmd.Env, "LLVM_PROFILE_FILE="+covProfile)
	}

	fmt.Print("unit tests .. ")

	start := time.Now()
	err := cmd.Run()
	end := time.Now()
	dur := end.Sub(start)

	// write logs
	{
		outFile := filepath.Join(devOutDir, "unit", "logs", "out")
		errFile := filepath.Join(devOutDir, "unit", "logs", "err")
		os.MkdirAll(filepath.Dir(outFile), 0755)
		if err := os.WriteFile(outFile, outBuf.Bytes(), 0600); err != nil {
			return fmt.Errorf("failed to write stdout file: %w", err)
		}
		if err := os.WriteFile(errFile, errBuf.Bytes(), 0600); err != nil {
			return fmt.Errorf("failed to write stderr file: %w", err)
		}
	}

	if err == nil {
		fmt.Print("passed")
	} else {
		fmt.Print("failed")
	}
	fmt.Printf(" %s\n\n", dur)

	globalResult.add("unittests", err, dur)

	// coverage
	if !*disableCodeCoverage {
		profiles := []string{covProfile}
		bins := []string{filepath.Join(*buildDir, "unittests")}
		coverageOut := filepath.Join(devOutDir, "unit", "coverage")
		if err := generateCoverage(coverageOut, profiles, bins); err != nil {
			return err
		}
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
	maxName += 2 // for 2 dots

	fmt.Println("integration tests:")
	for _, test := range tests {
		fmt.Printf("%s %s ", test.name, strings.Repeat(".", maxName-len(test.name)))
		start := time.Now()
		err := test.run()
		end := time.Now()
		dur := end.Sub(start)
		globalResult.add(test.name, err, dur)
		if err != nil {
			fmt.Print("failed")
		} else {
			fmt.Print("passed")
		}
		fmt.Printf("  %s\n", dur)
	}

	if !*disableCodeCoverage {
		var profiles []string
		profilesDir := filepath.Join(devOutDir, "int", "coverage", "profraw")
		err = filepath.WalkDir(profilesDir, func(path string, d fs.DirEntry, err error) error {
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
		out := filepath.Join(devOutDir, "int", "coverage")
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
	outBuf := new(bytes.Buffer)
	errBuf := new(bytes.Buffer)
	covProfile := filepath.Join(devOutDir, "int", "coverage", "profraw", t.name) + ".profraw"

	var args []string
	if *osTag == "windows" {
		args = append(args, "wine")
	}
	args = append(args, exe, "-abort", "-dump", "-free", t.input)

	cmd := exec.CommandContext(ctx, args[0], args[1:]...)
	cmd.Stdout = outBuf
	cmd.Stderr = errBuf

	if !*disableCodeCoverage {
		cmd.Env = append(cmd.Env, "LLVM_PROFILE_FILE="+covProfile)
	}

	err := cmd.Run()

	if *update {
		if err == nil {
			err = os.WriteFile(t.expected, outBuf.Bytes(), 0600)
			if err != nil {
				return fmt.Errorf("failed to write output file: %w", err)
			}
		}
	} else {
		// write logs
		outFile := filepath.Join(devOutDir, "int", "logs", t.name+".out")
		errFile := filepath.Join(devOutDir, "int", "logs", t.name+".err")
		os.MkdirAll(filepath.Dir(outFile), 0755)
		if err := os.WriteFile(outFile, outBuf.Bytes(), 0600); err != nil {
			return fmt.Errorf("failed to write stdout file: %w", err)
		}
		if err := os.WriteFile(errFile, errBuf.Bytes(), 0600); err != nil {
			return fmt.Errorf("failed to write stderr file: %w", err)
		}
	}

	if err != nil {
		return fmt.Errorf("failed to run command: %w", err)
	}

	data, err := os.ReadFile(t.expected)
	if err != nil {
		return fmt.Errorf("failed to read expected output file: %w", err)
	}

	haveLines := strings.Split(outBuf.String(), "\n")
	wantLines := strings.Split(string(data), "\n")

	if len(haveLines) != len(wantLines) {
		return fmt.Errorf("error: want %d lines, have %d lines\n", len(wantLines), len(haveLines))
	}
	for i := range wantLines {
		haveLine := strings.TrimSuffix(haveLines[i], "\r")
		if haveLine != wantLines[i] {
			return fmt.Errorf("error: line %d: want '%s', have '%s'\n", i+1, wantLines[i], haveLine)
		}
	}

	return nil
}

func (t IntegrationTest) runNotValid() error {
	exe := filepath.Join(*buildDir, "kevs")
	outBuf := new(bytes.Buffer)
	errBuf := new(bytes.Buffer)
	covProfile := filepath.Join(devOutDir, "int", "coverage", "profraw", t.name) + ".profraw"

	var args []string
	if *osTag == "windows" {
		args = append(args, "wine")
	}
	args = append(args, exe, "-no-err", "-free", t.input)

	cmd := exec.CommandContext(ctx, args[0], args[1:]...)
	cmd.Stdout = outBuf
	cmd.Stderr = errBuf

	if !*disableCodeCoverage {
		cmd.Env = append(cmd.Env, "LLVM_PROFILE_FILE="+covProfile)
	}

	err := cmd.Run()

	// write logs
	{
		outFile := filepath.Join(devOutDir, "int", "logs", t.name+".out")
		errFile := filepath.Join(devOutDir, "int", "logs", t.name+".err")
		os.MkdirAll(filepath.Dir(outFile), 0755)
		if err := os.WriteFile(outFile, outBuf.Bytes(), 0600); err != nil {
			return fmt.Errorf("failed to write stdout file: %w", err)
		}
		if err := os.WriteFile(errFile, errBuf.Bytes(), 0600); err != nil {
			return fmt.Errorf("failed to write stderr file: %w", err)
		}
	}

	if err != nil {
		return fmt.Errorf("failed to run command: %w", err)
	}

	data, err := os.ReadFile(t.expected)
	if err != nil {
		return fmt.Errorf("failed to read expected output file: %w", err)
	}

	haveLines := strings.Split(outBuf.String(), "\n")
	want := strings.TrimSuffix(string(data), "\n")

	for _, line := range haveLines {
		line = strings.TrimSuffix(line, "\r")
		if strings.Index(line, want) != -1 {
			return nil
		}
	}

	return fmt.Errorf("error: output does not contain '%s'\n", want)
}

func generateCoverage(outDir string, profiles []string, binaries []string) error {
	os.MkdirAll(outDir, 0755)

	dataFile := filepath.Join(outDir, "coverage.data")

	// merge raw profiles
	{
		args := []string{"merge", "-o", dataFile, "-sparse"}
		args = append(args, profiles...)
		cmd := exec.CommandContext(ctx, "llvm-profdata-14", args...)
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
		args = append(args, "src/kevs.c")
		cmd := exec.CommandContext(ctx, "llvm-cov-14", args...)
		cmd.Stdout = os.Stdout
		cmd.Stderr = os.Stderr
		if err := cmd.Run(); err != nil {
			return err
		}
	}

	return nil
}

func copyFile(dstPath, srcPath string) error {
	src, err := os.Open(srcPath)
	if err != nil {
		return err
	}
	defer src.Close()
	dst, err := os.Create(dstPath)
	if err != nil {
		return err
	}
	defer dst.Close()
	_, err = io.Copy(dst, src)
	if err != nil {
		return err
	}
	return dst.Sync()
}
