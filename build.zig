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

    const exe_name = "kevs";

    const c_opts = .{
        .files = &.{
            "kevs_cli.c",
            "kevs.c",
        },
        .flags = &.{
            "-std=c11",
            "-g",
            "-Wall",
            "-Wextra",
            "-Werror",
        },
    };

    const exe = b.addExecutable(.{
        .name = exe_name,
        .target = target,
        .optimize = optimize,
        .strip = strip,
        .link_libc = true,
    });
    exe.addCSourceFiles(c_opts);
    b.installArtifact(exe);

    const release = b.step("release", "Make an upstream binary release");
    const release_targets = [_]std.Target.Query{
        .{
            .cpu_arch = .aarch64,
            .os_tag = .linux,
        },
        .{
            .cpu_arch = .x86_64,
            .os_tag = .linux,
        },
        .{
            .cpu_arch = .aarch64,
            .os_tag = .macos,
        },
        .{
            .cpu_arch = .x86_64,
            .os_tag = .macos,
        },
        .{
            .cpu_arch = .x86,
            .os_tag = .windows,
        },
        .{
            .cpu_arch = .x86_64,
            .os_tag = .windows,
        },
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
        rel_exe.addCSourceFiles(c_opts);

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

    const test_exe = b.addExecutable(.{
        .name = "test",
        .target = target,
        .root_source_file = b.path("test.zig"),
    });
    b.installArtifact(test_exe);

    const test_run = b.addRunArtifact(test_exe);

    const test_step = b.step("test", "Run tests");
    test_step.dependOn(&exe.step); // TODO: this does not compile the exe before test
    test_step.dependOn(&test_run.step);

    const fmt_step = b.step("fmt", "Format zig source code");
    const fmt = b.addFmt(.{
        .paths = &.{
            "build.zig",
            "test.zig",
        },
    });
    fmt_step.dependOn(&fmt.step);
}
