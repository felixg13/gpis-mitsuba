{
  description = "Mitsuba 3.7 renderer build environment";
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-24.05";
    flake-utils.url = "github:numtide/flake-utils";
  };
  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs {
          inherit system;
          config.allowUnfree = true;
        };
        
        python = pkgs.python312;
        pythonPackages = python.pkgs;
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
            # GCC runtime libraries
            stdenv.cc.cc.lib
            
            # LLVM for Dr.Jit backend
            llvm_17
            libffi
            libxml2
            ncurses
            zlib
            
            # Python 3.12
            python
            pythonPackages.numpy
            pythonPackages.pillow
            
            # System libraries
            libpng
            libjpeg
            openexr
            
            # CUDA (optional)
            cudaPackages.cuda_nvcc
            cudaPackages.cudatoolkit
            
            # Additional dependencies
            tbb
            embree
          ];
          shellHook = ''
            export CMAKE_GENERATOR=Ninja
            
            # Ensure libraries are found
            export LIBRARY_PATH="${pkgs.stdenv.cc.cc.lib}/lib:$LIBRARY_PATH"
            export LD_LIBRARY_PATH="${pkgs.stdenv.cc.cc.lib}/lib:${pkgs.llvm_17.lib}/lib:$LD_LIBRARY_PATH"
            
            # CRITICAL: Tell Dr.Jit where to find LLVM
            export DRJIT_LIBLLVM_PATH="${pkgs.llvm_17.lib}/lib/libLLVM.so"
            
            # Python environment
            export PYTHONPATH="${python}/${python.sitePackages}:$PYTHONPATH"
            
            echo "Mitsuba 3.7 development environment"
            echo "===================================="
            echo "GCC version: $(gcc --version | head -n1)"
            echo "CMake version: $(cmake --version | head -n1)"
            echo "Ninja version: $(ninja --version)"
            echo "Python version: $(python --version)"
            echo "LLVM version: $(llvm-config --version)"
            echo "DRJIT_LIBLLVM_PATH: $DRJIT_LIBLLVM_PATH"
            echo ""
            echo "To build Mitsuba 3.7:"
            echo "  cmake -B build -DCMAKE_BUILD_TYPE=Release"
            echo "  cmake --build build"
            echo ""
            echo "To test rendering:"
            echo "  mitsuba resources/data/scenes/hello.xml -m llvm_ad_rgb"
          '';
        };
      }
    );
}
