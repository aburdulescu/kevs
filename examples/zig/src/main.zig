const std = @import("std");

const c = @cImport({
    @cInclude("kevs.h");
});

pub fn main() !void {
    const file = "../example.kevs";

    // read file
    var buf: [8192]u8 = undefined;
    const data = try std.fs.cwd().readFile(file, &buf);
    buf[data.len] = 0; // add null terminator
    const content_ptr = @as([*:0]const u8, @ptrCast(data));

    var root: c.Table = .{};
    const ok = c.table_parse(
        &root,
        .{},
        c.str_from_cstr(file),
        c.str_from_cstr(content_ptr),
    );
    if (!ok) {
        try std.debug.panic("parse failed\n", .{});
        return;
    }
    defer c.table_free(&root);

    var string_escaped: [*c]u8 = null;
    {
        const err = c.table_string(root, "string_escaped", &string_escaped);
        if (err != null) {
            try std.debug.panic("{s}\n", .{err});
            return;
        }
    }

    var table2: c.Table = .{};
    {
        const err = c.table_table(root, "table2", &table2);
        if (err != null) {
            try std.debug.panic("{s}\n", .{err});
            return;
        }
    }

    var name: [*c]u8 = null;
    {
        const err = c.table_string(table2, "name", &name);
        if (err != null) {
            try std.debug.panic("{s}\n", .{err});
            return;
        }
    }

    var age: i64 = 0;
    {
        const err = c.table_int(table2, "age", &age);
        if (err != null) {
            try std.debug.panic("{s}\n", .{err});
            return;
        }
    }

    std.debug.print("string_escaped: {s}\n", .{string_escaped});
    std.debug.print("name: {s}\n", .{name});
    std.debug.print("age: {d}\n", .{age});
}
