const std = @import("std");

const c = @cImport({
    @cInclude("kevs.h");
});

const stdout = std.io.getStdOut().writer();
const stderr = std.io.getStdErr().writer();

pub fn main() !void {
    const file = "../example.kevs";

    var buf: [8192]u8 = undefined;
    const data = try std.fs.cwd().readFile(file, &buf);
    buf[data.len] = 0;
    const content = @as([*:0]const u8, @ptrCast(data));

    var root: c.Table = undefined;
    const ok = c.table_parse(
        &root,
        .{},
        c.str_from_cstr(file),
        c.str_from_cstr(content),
    );
    if (!ok) {
        try stderr.print("error: failed to parse\n", .{});
    }
    defer c.table_free(&root);
}
