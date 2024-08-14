# Macroni

Macroni is an MLIR dialect that adds macro expansions to VAST's tower of IRS.

## Requirements

The following instructions assume you are using an Ubuntu 22.04.4 LTS operating
system and are at the root of a local clone of the macroni project tree:

- LLVM, MLIR, and a PASTA-specific version of Clang. If you are only interested
  in using Macroni's `Safety` and `Kernel` dialects, then you may use Clang 18
  instead. Otherwise you can install PASTA's patched version of Clang as
  follows:

  ```bash
  wget -O external/llvm-pasta-beeda8d.tar.xz https://github.com/trail-of-forks/llvm-project/releases/download/beeda8d/llvm-pasta-beeda8d.tar.xz
  mkdir -p external/llvm-pasta-beeda8d
  tar -xvf external/llvm-pasta-beeda8d.tar.xz --directory external/llvm-pasta-beeda8d/
  ```

- [`PASTA`](https://github.com/trailofbits/pasta/). If you are only interested
  in using Macroni's `Safety` and `Kernel` dialects, then you do not need PASTA.
  Otherwise follow these steps to download it as a submodule:

  ```bash
  cd external/pasta
  git submodule update --init
  ```

- [`VAST`](https://github.com/trailofbits/vast)
  
  ```bash
  cd external/vast
  git submodule update --init --recursive
  ```

- Ccache (not required but strongly recommended)

  ```bash
  sudo apt install ccache
  ```

- Mold (not required but strongly recommended)

  ```bash
  sudo apt install mold
  ```

- Ninja (not required but strongly recommended)

  ```bash
  sudo apt install ninja-build
  ```

## Building Macroni

Configure Macroni, e.g., assuming all requirements and recommendations have been
installed:

```bash
cmake -S . -B build/ -G "Ninja Multi-Config" \
  -D Clang_DIR:PATH="`realpath -s external/llvm-pasta-beeda8d/lib/cmake/clang`" \
  -D CMAKE_BUILD_TYPE:STRING="Debug" \
  -D CMAKE_C_COMPILER:PATH="`realpath -s external/llvm-pasta-beeda8d/bin/clang`" \
  -D CMAKE_CXX_COMPILER:PATH="`realpath -s external/llvm-pasta-beeda8d/bin/clang++`" \
  -D CMAKE_EXPORT_COMPILE_COMMANDS:BOOL=true \
  -D CMAKE_INSTALL_PREFIX:PATH=build/ \
  -D CMAKE_LINKER_TYPE:STRING=MOLD \
  -D LLVM_DIR:PATH="`realpath -s external/llvm-pasta-beeda8d/lib/cmake/llvm`" \
  -D LLVM_ENABLE_RTTI:BOOL=true \
  -D LLVM_USE_LINKER:STRING="mold" \
  -D MLIR_DIR:PATH="`realpath -s external/llvm-pasta-beeda8d/lib/cmake/mlir`"
```

Build macroni:

```bash
cmake --build build/
```

## Running Macroni

Once the `macronify` binary has been built, you can run it on a C source file.
Assuming `macronify` has been installed and its location added to your path:

```bash
macronify -xc some_file.c
```

## Testing

To run Macroni's tests first install LIT and FileCheck:

- LIT
  
  ```bash
  python3 -m pip install lit
  ```

- FileCheck

  ```bash
  wget https://apt.llvm.org/llvm.sh
  chmod +x llvm.sh
  sudo ./llvm.sh 17
  sudo apt install llvm-17-tools
  ```

Then add the following definitions when configuring building Macroni:

```bash
cmake -S . -B build/ -G "Ninja Multi-Config" \
  ...# Previous definitions
  -D MACRONI_ENABLE_TESTING:BOOL=ON \
  -D LLVM_EXTERNAL_LIT:STRING="`which -s lit`"
```

Finally, run Macroni's tests by building the target `check-macroni`:

```bash
cmake --build build/ --target check-macroni
```

## License

Macroni is licensed according to the Apache 2.0 license. Macroni links against
and uses Clang and LLVM APIs. Clang is also licensed under Apache 2.0, with
[LLVM](https://github.com/llvm/llvm-project/blob/main/clang/LICENSE.TXT)
exceptions.
