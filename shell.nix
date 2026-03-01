{pkgs ? import <nixpkgs> {}}:
pkgs.mkShell {
  name = "nanocut-control-dev";
  nativeBuildInputs = with pkgs; [
    pkg-config
    cmake
    xxd
  ];
  buildInputs = with pkgs; [
    libGL
    zlib
    libGLU
    glfw
    (python314.withPackages (ps: with ps; [
      mkdocs
      mkdocs-material
      grip
      pillow
      numpy
      scipy
    ]))
  ];
}
