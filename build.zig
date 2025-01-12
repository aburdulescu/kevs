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

    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const strip = b.option(bool, "strip", "Strip the binary");

    const cli_name = "kevs";

    const cflags = [_][]const u8{
        "-std=c11",
        "-g",
        "-Wall",
        "-Wextra",
        "-Werror",
    };

    const cli_copts = std.Build.Module.AddCSourceFilesOptions{
        .files = &.{ "src/cli.c", "src/kevs.c" },
        .flags = &cflags,
    };

    const cli_exe = b.addExecutable(.{
        .name = cli_name,
        .target = target,
        .optimize = optimize,
        .strip = strip,
        .link_libc = true,
    });
    cli_exe.addCSourceFiles(cli_copts);
    b.installArtifact(cli_exe);

    const tester_exe = b.addExecutable(.{
        .name = "tester",
        .target = target,
        .root_source_file = b.path("scripts/tester.zig"),
    });
    b.installArtifact(tester_exe);

    const example_copts = std.Build.Module.AddCSourceFilesOptions{
        .files = &.{ "src/example.c", "src/kevs.c" },
        .flags = &cflags,
    };

    const example_exe = b.addExecutable(.{
        .name = "example",
        .target = target,
        .link_libc = true,
    });
    example_exe.addCSourceFiles(example_copts);
    b.installArtifact(example_exe);

    const unittests_copts = std.Build.Module.AddCSourceFilesOptions{
        .files = &.{ "src/unittests.c", "src/kevs.c" },
        .flags = &cflags,
    };

    const unittests_exe = b.addExecutable(.{
        .name = "unittests",
        .target = target,
        .link_libc = true,
    });
    unittests_exe.addCSourceFiles(unittests_copts);
    b.installArtifact(unittests_exe);

    const fmt_step = b.step("fmt", "Format zig source code");
    const fmt = b.addFmt(.{
        .paths = &.{
            "build.zig",
            "scripts/tester.zig",
        },
    });
    fmt_step.dependOn(&fmt.step);

    const release = b.step("release", "Make an upstream binary release");
    const release_targets = [_]std.Target.Query{
        .{ .os_tag = .linux, .cpu_arch = .aarch64 },
        .{ .os_tag = .linux, .cpu_arch = .x86_64 },
        .{ .os_tag = .macos, .cpu_arch = .aarch64 },
        .{ .os_tag = .macos, .cpu_arch = .x86_64 },
        .{ .os_tag = .windows, .cpu_arch = .x86 },
        .{ .os_tag = .windows, .cpu_arch = .x86_64 },
    };

    for (release_targets) |target_query| {
        const resolved_target = b.resolveTargetQuery(target_query);
        const t = resolved_target.result;

        const rel_optimize = if (optimize != .Debug) optimize else .ReleaseFast;

        const rel_exe = b.addExecutable(.{
            .name = "kevs",
            .target = resolved_target,
            .optimize = rel_optimize,
            .strip = true,
            .link_libc = true,
        });
        rel_exe.addCSourceFiles(cli_copts);

        const install = b.addInstallArtifact(rel_exe, .{});
        install.dest_dir = .prefix;
        install.dest_sub_path = b.fmt("{s}-{s}-{s}", .{
            rel_exe.name, @tagName(t.os.tag), @tagName(t.cpu.arch),
        });
        install.dest_sub_path = b.fmt("{s}/{s}-{s}-{s}", .{
            @tagName(rel_optimize),
            rel_exe.name,
            @tagName(t.os.tag),
            @tagName(t.cpu.arch),
        });

        release.dependOn(&install.step);
    }
}
