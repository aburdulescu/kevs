const std = @import("std");

const stdout = std.io.getStdOut().writer();
const stderr = std.io.getStdErr().writer();

var gpa_instance = std.heap.GeneralPurposeAllocator(.{}){};
var gpa = gpa_instance.allocator();

var gr = GlobalResult{
    .total = 0,
    .duration = 0,
};

pub fn main() !void {
    try runIntegrationTests();
    try gr.summary();
}

fn runIntegrationTests() !void {
    const root_path = "testdata";

    var dir = try std.fs.cwd().openDir(root_path, .{ .iterate = true });
    defer dir.close();

    var walker = try dir.walk(gpa);
    defer walker.deinit();

    var tests = std.ArrayList(IntegrationTest).init(gpa);

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

    for (tests.items) |item| {
        const start = timer.read();
        const status = if (item.run()) |_| "passed" else |_| "failed";
        const end = timer.read();
        const duration = end - start;

        gr.add(duration);

        const pretty = prettyTime(duration);

        try stdout.print("{s} {s} {s} {d}{s}\n", .{ item.name, dots[0 .. max_name - item.name.len], status, pretty.value, pretty.unit });
    }
}

const IntegrationTest = struct {
    const Self = @This();

    name: []const u8,
    input: []const u8,
    expected: []const u8,

    fn run(self: Self) !void {
        if (std.mem.startsWith(u8, self.name, "valid")) {
            var args = std.ArrayList([]const u8).init(gpa);
            try args.append("./zig-out/bin/kevs");
            try args.append("-abort");
            try args.append("-dump");
            try args.append(self.input);

            var child = std.process.Child.init(args.items, gpa);
            child.stdout_behavior = std.process.Child.StdIo.Pipe;
            child.stderr_behavior = std.process.Child.StdIo.Pipe;

            const term = try child.spawnAndWait();
            if (term != .Exited) {
                try stderr.print("error: command terminated unexpectedly\n", .{});
                std.process.exit(1);
            }

            // TODO: check stdout against expected
        } else if (std.mem.startsWith(u8, self.name, "not_valid")) {
            var args = std.ArrayList([]const u8).init(gpa);
            try args.append("./zig-out/bin/kevs");
            try args.append("-dump");
            try args.append(self.input);

            var child = std.process.Child.init(args.items, gpa);
            child.stdout_behavior = std.process.Child.StdIo.Pipe;
            child.stderr_behavior = std.process.Child.StdIo.Pipe;

            const term = try child.spawnAndWait();
            if (term != .Exited) {
                try stderr.print("error: command terminated unexpectedly\n", .{});
                std.process.exit(1);
            }

            // TODO: check stdout against expected
        } else {
            unreachable;
        }
    }
};

const GlobalResult = struct {
    const Self = @This();

    total: i32,
    duration: u64,

    fn add(self: *Self, duration: u64) void {
        self.total += 1;
        self.duration += duration;
    }

    fn summary(self: Self) !void {
        const pretty = prettyTime(self.duration);
        try stdout.print("\nsummary:\nran {d} tests in {d}{s}\n", .{ self.total, pretty.value, pretty.unit });
    }
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
