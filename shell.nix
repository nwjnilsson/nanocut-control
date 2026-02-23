{pkgs ? import <nixpkgs> {}}:
pkgs.stdenv.mkDerivation {
  name = "nanocut-control-dev";
  nativeBuildInputs = with pkgs; [
    pkg-config
    cmake
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
    ]))
  ];
}
