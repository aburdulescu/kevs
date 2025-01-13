const std = @import("std");
const builtin = @import("builtin");

pub fn build(b: *std.Build) void {
    comptime {
        const needed = "0.13.0";
        const current = builtin.zig_version;
        const needed_vers = std.SemanticVersion.parse(needed) catch unreachable;
        if (current.order(needed_vers) != .eq) {
            @compileError(std.fmt.comptimePrint("Your zig version is not supported, need version {s}", .{needed}));
        }
    }

    const optimize = b.standardOptimizeOption(.{});
    const strip = b.option(bool, "strip", "Strip the binary") orelse switch (optimize) {
        .Debug, .ReleaseSafe => false,
        else => true,
    };

    const fmt_step = b.step("fmt", "Format zig source code");
    const fmt = b.addFmt(.{
        .paths = &.{
            "build.zig",
        },
    });
    fmt_step.dependOn(&fmt.step);

    const targets = [_]std.Target.Query{
        .{ .os_tag = .linux, .cpu_arch = .aarch64 },
        .{ .os_tag = .linux, .cpu_arch = .x86_64 },
        .{ .os_tag = .macos, .cpu_arch = .aarch64 },
        .{ .os_tag = .macos, .cpu_arch = .x86_64 },
        .{ .os_tag = .windows, .cpu_arch = .x86 },
        .{ .os_tag = .windows, .cpu_arch = .x86_64 },
    };

    for (targets) |query| {
        const target = b.resolveTargetQuery(query);
        const t = target.result;

        const exe = b.addExecutable(.{
            .name = "kevs",
            .target = target,
            .optimize = optimize,
            .strip = strip,
            .link_libc = true,
        });
        exe.addCSourceFiles(.{
            .files = &.{ "src/cli.c", "src/kevs.c" },
            .flags = &.{ "-std=c11", "-g", "-Wall", "-Wextra", "-Werror" },
        });

        const install = b.addInstallArtifact(exe, .{});
        install.dest_dir = .prefix;
        install.dest_sub_path = b.fmt("{s}/{s}-{s}-{s}", .{
            @tagName(optimize),
            exe.name,
            @tagName(t.os.tag),
            @tagName(t.cpu.arch),
        });
        b.getInstallStep().dependOn(&install.step);
    }
}
