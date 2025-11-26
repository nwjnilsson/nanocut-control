{pkgs ? import <nixpkgs> {}}:
pkgs.stdenv.mkDerivation {
  name = "nanocut-control-dev";
  nativeBuildInputs = with pkgs; [
    pkg-config
  ];
  buildInputs = with pkgs; [
    libGL
    zlib
    libGLU
    glfw
  ];
}
