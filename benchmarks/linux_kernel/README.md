# Linux kernel benchmark

This readme provides instructions on how to run Macroni's `kernelize` and
`kernelcehck` binaries on the Linux kernel's source code to find macro usage
warnings. These instructions were originally run on an Ubuntu 22.04.4 machine.

## Requirements

- LLVM/Clang 18.
- Python 3.
- Make.
- Macroni's `kernelize` and `kernelcheck` executables must be built and on your
  PATH. See this repo's top-level readme for instructions on how to build them.

## Instructions

1. Download the latest stable release of the Linux kernel (6.10 at the time of
   writing):

    ```sh
    git clone https://github.com/torvalds/linux.git \
        --branch v6.10 --depth=1 linux
    ```

2. Build the kernel and generate its compilation database:

    ```sh
    cd linux
    make defconfig LLVM=-18
    make prepare LLVM=-18
    make LLVM=-18 -j $(nproc)
    python3 scripts/clang-tools/gen_compile_commands.py
    ```

3. From within the directory of this readme, run the script
   `benchmarks/utils/kernelize_compile_commands.py` to lower each of the
   translation units in the Linux kernel to MLIR like so:

    ```sh
    python3 ../utils/kernelize_compile_commands.py \
        kernelize \
        linux/compile_commands.json \
        kernelize_linux_results/ \
        --num_processes=$(nproc) \
        --kernelize_option="--locations" \
        1> kernelize_linux_times.tsv \
        2> kernelize_linux.log
    ```

    The above command assumes that the `kernelize` binary is installed and on
    your PATH.

    This command will tell `kernelize` to try and translate each file in the
    Linux kernel. If `kernelize` succesffully translates a file, it will place
    the translated MLIR file in the newly-created `kernelize_linux_results`
    directory. If `kernelize` fails to translate a file, then it will dump the
    its error messages to a file named after the file it was attempting to
    translate, but with a `.log` extension, and place it in the
    `kernelize_linux_results` directory instead.

    The above command will also create a file named `kernelize_linux_times.tsv`
    that contains tab-separated values listing the time needed to translate each
    file, and another file named `kernelize_linux.log` containing various debug
    messages, such as the translator's current progress on translation.

    If you have questions about the `kernelize_compile_commands.py` script, pass
    it the `--help` flag for more information.

4. Run the `kernelcheck_directory.sh` script to check the translated files for
   warnings like so:

    ```sh
    ../utils/kernelcheck_directory.sh \
        kernelcheck \
        kernelize_linux_results/ \
        rcu_warnings.txt
    ```

    The above command assumes that the `kernelcheck` binary is installed and on
    your PATH.

    This will create a file named `rcu_warnings.txt` containing all the unique
    warnings found.
