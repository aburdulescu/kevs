const std = @import("std");

const usage =
    \\Usage: test [OPTIONS]
    \\
    \\Run tests.
    \\
    \\OPTIONS:
    \\  -help  print this message and exit
    \\  -exe   path to kevs CLI executable(default: zig-out/bin/kevs)
    \\  -out   path to write test output(default: test-out)
    \\
;

const stdout = std.io.getStdOut().writer();
const stderr = std.io.getStdErr().writer();

var gpa_instance = std.heap.GeneralPurposeAllocator(.{}){};
var gpa = gpa_instance.allocator();

var flags = Flags{
    .exe = "zig-out/bin/kevs",
    .out = "test-out",
};

pub fn main() !void {
    const args = (try std.process.argsAlloc(gpa))[1..];
    var i: usize = 0;
    while (i < args.len) {
        if (std.mem.eql(u8, args[i], "-help")) {
            try stdout.writeAll(usage);
            return;
        } else if (std.mem.eql(u8, args[i], "-exe")) {
            if (i + 1 == args.len) {
                try stderr.print("error: -exe needs a value\n", .{});
                std.process.exit(1);
            }
            flags.exe = args[i + 1];
            i += 1;
        } else if (std.mem.eql(u8, args[i], "-out")) {
            if (i + 1 == args.len) {
                try stderr.print("error: -out needs a value\n", .{});
                std.process.exit(1);
            }
            flags.out = args[i + 1];
            i += 1;
        } else {
            break;
        }
        i += 1;
    }

    const report = try runIntegrationTests(flags.exe);
    try report.summary();
}

const Flags = struct {
    exe: []const u8,
    out: []const u8,
};

fn runIntegrationTests(exe: []const u8) !TestReport {
    const root_path = "testdata";

    var dir = try std.fs.cwd().openDir(root_path, .{ .iterate = true });
    defer dir.close();

    var walker = try dir.walk(gpa);
    defer walker.deinit();

    var tests = std.ArrayList(IntegrationTest).init(gpa);
    defer tests.deinit();

    const file_ext = ".kevs";

    while (try walker.next()) |entry| {
        if (entry.kind != std.fs.File.Kind.file) {
            continue;
        }
        if (!std.mem.endsWith(u8, entry.basename, file_ext)) {
            continue;
        }

        const input = try std.fs.path.join(gpa, &[_][]const u8{ root_path, entry.path });

        // replace .kevs with .out
        const expected = try std.mem.concat(gpa, u8, &[_][]const u8{ input[0 .. input.len - file_ext.len], ".out" });

        // remove .kevs
        const name = (try gpa.dupe(u8, entry.path))[0 .. entry.path.len - file_ext.len];

        try tests.append(.{
            .name = name,
            .input = input,
            .expected = expected,
        });
    }

    var max_name: usize = 0;
    for (tests.items) |item| {
        if (item.name.len > max_name) {
            max_name = item.name.len;
        }
    }
    max_name += 2; // for 2 dots

    var dots = try gpa.alloc(u8, max_name);
    @memset(dots, '.');

    var timer = std.time.Timer.start() catch @panic("need timer to work");

    var report = TestReport{
        .total = 0,
        .duration = 0,
        .failed = std.ArrayList(TestResult).init(gpa),
    };

    for (tests.items) |item| {
        const start = timer.read();
        const result = try item.run(exe);
        const end = timer.read();
        const duration = end - start;
        try report.add(result, duration);
        const pretty = prettyTime(duration);
        try stdout.print("{s} {s} {s} {d:.3}{s}\n", .{
            item.name,
            dots[0 .. max_name - item.name.len],
            if (result.err) |_| "failed" else "passed",
            pretty.value,
            pretty.unit,
        });
    }

    return report;
}

const IntegrationTest = struct {
    const Self = @This();

    name: []const u8,
    input: []const u8,
    expected: []const u8,

    fn run(self: Self, exe: []const u8) !TestResult {
        const is_windows = if (std.mem.indexOf(u8, exe, "-windows-")) |_| true else false;
        if (std.mem.startsWith(u8, self.name, "valid")) {
            return self.runValid(exe, is_windows);
        } else if (std.mem.startsWith(u8, self.name, "not_valid")) {
            return self.runNotValid(exe, is_windows);
        } else {
            unreachable;
        }
    }

    fn runValid(self: Self, exe: []const u8, is_windows: bool) !TestResult {
        var args = std.ArrayList([]const u8).init(gpa);
        defer args.deinit();

        if (is_windows) {
            try args.append("wine");
        }

        try args.append(exe);
        try args.append("-abort");
        try args.append("-dump");
        try args.append(self.input);

        const result = try std.process.Child.run(.{
            .allocator = gpa,
            .argv = args.items,
        });
        defer gpa.free(result.stdout);
        defer gpa.free(result.stderr);

        switch (result.term) {
            .Exited => |code| {
                if (code != 0) {
                    try stderr.print("error: command returned code {d}\n", .{code});
                }
            },
            else => {
                try stderr.print("error: command terminated unexpectedly: {}\n", .{result.term});
                std.process.exit(1);
            },
        }

        // write logs
        {
            try std.fs.cwd().makePath(flags.out);

            var out_dir = try std.fs.cwd().openDir(flags.out, .{});
            defer out_dir.close();

            try out_dir.makePath(self.name);

            var test_dir = try out_dir.openDir(self.name, .{});
            defer test_dir.close();

            try test_dir.writeFile(.{ .sub_path = "out", .data = result.stdout });
            try test_dir.writeFile(.{ .sub_path = "err", .data = result.stderr });
        }

        var file = try std.fs.cwd().openFile(self.expected, .{ .mode = .read_only });
        defer file.close();
        const expected = try file.readToEndAlloc(gpa, std.math.maxInt(usize));
        defer gpa.free(expected);

        var wantLines = std.ArrayList([]const u8).init(gpa);
        defer wantLines.deinit();
        {
            var it = std.mem.splitSequence(u8, expected, "\n");
            while (it.next()) |line| {
                try wantLines.append(line);
            }
        }

        var haveLines = std.ArrayList([]const u8).init(gpa);
        defer haveLines.deinit();
        {
            var it = std.mem.splitSequence(u8, result.stdout, "\n");
            while (it.next()) |line| {
                try haveLines.append(std.mem.trimRight(u8, line, "\r"));
            }
        }

        if (haveLines.items.len != wantLines.items.len) {
            const err = try std.fmt.allocPrint(gpa, "want {d} lines, have {d}", .{ wantLines.items.len, haveLines.items.len });
            return .{
                .name = self.name,
                .err = err,
            };
        }

        for (wantLines.items, 0..) |want, i| {
            const have = haveLines.items[i];
            if (!std.mem.eql(u8, want, have)) {
                const err = try std.fmt.allocPrint(gpa, "line {d}: want '{s}', have '{s}'", .{ i + 1, want, have });
                return .{
                    .name = self.name,
                    .err = err,
                };
            }
        }

        return .{ .name = self.name, .err = null };
    }

    fn runNotValid(self: Self, exe: []const u8, is_windows: bool) !TestResult {
        var args = std.ArrayList([]const u8).init(gpa);
        defer args.deinit();

        if (is_windows) {
            try args.append("wine");
        }

        try args.append(exe);
        try args.append("-no-err");
        try args.append(self.input);

        const result = try std.process.Child.run(.{
            .allocator = gpa,
            .argv = args.items,
        });
        defer gpa.free(result.stdout);
        defer gpa.free(result.stderr);

        switch (result.term) {
            .Exited => |code| {
                if (code != 0) {
                    try stderr.print("error: command returned code {d}\n", .{code});
                }
            },
            else => {
                try stderr.print("error: command terminated unexpectedly: {}\n", .{result.term});
                std.process.exit(1);
            },
        }

        // write logs
        {
            try std.fs.cwd().makePath(flags.out);

            var out_dir = try std.fs.cwd().openDir(flags.out, .{});
            defer out_dir.close();

            try out_dir.makePath(self.name);

            var test_dir = try out_dir.openDir(self.name, .{});
            defer test_dir.close();

            try test_dir.writeFile(.{ .sub_path = "out", .data = result.stdout });
            try test_dir.writeFile(.{ .sub_path = "err", .data = result.stderr });
        }

        var file = try std.fs.cwd().openFile(self.expected, .{ .mode = .read_only });
        defer file.close();

        const expected = try file.readToEndAlloc(gpa, std.math.maxInt(usize));
        defer gpa.free(expected);

        const want = std.mem.trim(u8, expected, "\n");

        var haveLines = std.ArrayList([]const u8).init(gpa);
        defer haveLines.deinit();
        {
            var it = std.mem.splitSequence(u8, result.stdout, "\n");
            while (it.next()) |line| {
                try haveLines.append(std.mem.trimRight(u8, line, "\r"));
            }
        }

        var failed = true;
        for (haveLines.items) |have| {
            if (std.mem.indexOf(u8, have, want)) |_| {
                failed = false;
                break;
            }
        }

        if (failed) {
            const err = try std.fmt.allocPrint(gpa, "output does not contain '{s}'", .{want});
            return .{
                .name = self.name,
                .err = err,
            };
        }

        return .{ .name = self.name, .err = null };
    }
};

const TestReport = struct {
    const Self = @This();

    total: i32,
    duration: u64,
    failed: std.ArrayList(TestResult),

    fn add(self: *Self, tr: TestResult, duration: u64) !void {
        if (tr.err) |_| {
            try self.failed.append(tr);
        }
        self.total += 1;
        self.duration += duration;
    }

    fn summary(self: Self) !void {
        const pretty = prettyTime(self.duration);

        try stdout.print(
            "\nexe summary:\nran {d} tests in {d:.3}{s}, {d} failed\n",
            .{ self.total, pretty.value, pretty.unit, self.failed.items.len },
        );

        if (self.failed.items.len != 0) {
            try stdout.print("\nfailures:\n", .{});
            for (self.failed.items) |item| {
                try stdout.print("{s}: {s}\n", .{ item.name, item.err.? });
            }
        }
    }
};

const TestResult = struct {
    name: []const u8,
    err: ?[]const u8,
};

const PrettyTime = struct {
    value: f64,
    unit: []const u8,
};

fn prettyTime(v: u64) PrettyTime {
    if (v >= std.time.ns_per_s) {
        const vf: f64 = @floatFromInt(v);
        const df: f64 = @floatFromInt(std.time.ns_per_s);
        return .{ .value = vf / df, .unit = "s" };
    }
    if (v >= std.time.ns_per_ms) {
        const vf: f64 = @floatFromInt(v);
        const df: f64 = @floatFromInt(std.time.ns_per_ms);
        return .{ .value = vf / df, .unit = "ms" };
    }
    if (v >= std.time.ns_per_us) {
        const vf: f64 = @floatFromInt(v);
        const df: f64 = @floatFromInt(std.time.ns_per_us);
        return .{ .value = vf / df, .unit = "us" };
    }
    const vf: f64 = @floatFromInt(v);
    return .{ .value = vf, .unit = "ns" };
}