{
  description = "Mitsuba 3.71 renderer build environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-24.05";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };
        python = pkgs.python312;
      in
      {
        devShells.default = pkgs.mkShell {
          nativeBuildInputs = with pkgs; [
            cmake
            ninja
            gcc13
            pkg-config
          ];

          buildInputs = with pkgs; [
            llvm_17
            libffi
            zlib
            python

	    # images
            libpng
            libjpeg
            openexr
            imagemagick

	    # ray tracing
            tbb
            embree
          ];

          CMAKE_GENERATOR = "Ninja";
          DRJIT_LIBLLVM_PATH = "${pkgs.llvm_17.lib}/lib/libLLVM.so";
        };
      }
    );
}
