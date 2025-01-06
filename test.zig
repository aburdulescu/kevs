const std = @import("std");

pub fn main() !void {
    try runIntegrationTests();
}

fn runIntegrationTests() !void {
    const stdout = std.io.getStdOut().writer();
    // const stderr = std.io.getStdErr().writer();

    var arena_instance = std.heap.ArenaAllocator.init(std.heap.page_allocator);
    defer arena_instance.deinit();
    const arena = arena_instance.allocator();

    const root_path = "testdata";

    var dir = try std.fs.cwd().openDir(root_path, .{ .iterate = true });
    defer dir.close();

    var walker = try dir.walk(arena);
    defer walker.deinit();

    var tests = std.ArrayList(IntegrationTest).init(arena);

    while (try walker.next()) |entry| {
        if (entry.kind != std.fs.File.Kind.file) {
            continue;
        }
        if (!std.mem.endsWith(u8, entry.basename, ".kevs")) {
            continue;
        }

        try stdout.print("{s}\n", .{entry.path});

        try tests.append(.{
            .input = entry.path,
            .expected = entry.path,
        });
    }
}

const IntegrationTest = struct {
    input: []const u8,
    expected: []const u8,
};
