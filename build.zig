const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const strip = b.option(bool, "strip", "strip the binary");

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

    const release = b.step("release", "make an upstream binary release");
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
        const rel_exe = b.addExecutable(.{
            .name = "kevs",
            .target = resolved_target,
            .optimize = .ReleaseFast,
            .strip = true,
            .link_libc = true,
        });
        rel_exe.addCSourceFiles(c_opts);

        const install = b.addInstallArtifact(rel_exe, .{});
        install.dest_dir = .prefix;
        install.dest_sub_path = b.fmt("{s}-{s}-{s}", .{
            rel_exe.name, @tagName(t.os.tag), @tagName(t.cpu.arch),
        });

        release.dependOn(&install.step);
    }
}
