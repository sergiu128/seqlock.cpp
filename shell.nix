let
  nixpkgs = fetchTarball "https://github.com/NixOS/nixpkgs/tarball/nixos-24.05";
  pkgs = import nixpkgs { config = {}; overlays = []; };
in

pkgs.mkShellNoCC {
  packages = with pkgs; [
    gnumake
    zlib
    clang-tools_18
    clang_18
    llvm_18
    cmake
    ninja
  ];

  shellHook = ''
    export CXX=clang++
    export CC=clang
    export LIB_BUILD_PREFIX=$(pwd)/deps_build
    export LIB_INSTALL_PREFIX=$(pwd)/deps_install
    export USE_NIX=1
  '';
}
