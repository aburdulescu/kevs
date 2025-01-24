with (import <nixpkgs> {});

mkShell {
  buildInputs = [
    pkgs.gcc
    pkgs.clang
    pkgs.llvm
    pkgs.ninja
    pkgs.cmake
    pkgs.gdb
    pkgs.valgrind
    pkgs.go
    pkgs.zig
  ];

  shellHook = ''
    alias ems='emacs -Q -nw --load ~/.files/dotemacs'
  '';
}
