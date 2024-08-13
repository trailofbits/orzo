import argparse
import dataclasses
import multiprocessing
import pathlib
from datetime import datetime, timedelta
import sys
import json
import subprocess
import os
from typing import Optional


class Arguments(argparse.Namespace):
    """
    This script's arguments.
    """

    kernelize_path: pathlib.Path
    compile_commands_file: pathlib.Path
    output_directory: pathlib.Path
    print_commands: bool
    num_processes: int
    kernelize_option: list[str]


def parse_arguments() -> Arguments:
    description = """
Run kernelize on all the files in the given compilation database. When kernelize
successfully lowers a compilation unit to MLIR, the result is placed in the
given output directory. When kernelize fails to lower a compilation unit to
MLIR, a log file containing the stderr output is placed in the output directory
instead. The time needed to lower each compilation unit to MLIR is printed to
stdout in TSV format. The benchmark's progress is printed to stderr.""".lstrip()

    # Positional arguments.

    parser = argparse.ArgumentParser(description=description)
    parser.add_argument(
        "kernelize_path", type=pathlib.Path, help="Path to kernelize executable."
    )
    parser.add_argument(
        "compile_commands_file",
        type=pathlib.Path,
        help="Path to the Clang compilation database for the benchmark.",
    )
    parser.add_argument(
        "output_directory", type=pathlib.Path, help="Directory to place results."
    )

    # Optional arguments.

    print_commands_help = """Enable this option to print the kernelize
commands that are run on each compilation unit. Turned off by
default.""".replace(
        "\n", " "
    )
    parser.add_argument(
        "--print_commands",
        action=argparse.BooleanOptionalAction,
        help=print_commands_help,
    )
    num_processes_help = """The number of processes to use to run the benchmark. Defaults to
the number of CPUS on this machine.""".replace(
        "\n", " "
    )
    parser.add_argument(
        "--num_processes",
        type=int,
        default=os.cpu_count(),
        help=num_processes_help,
    )
    kernelize_option_help = """Additional kernelize options. Can be specified
multiple times to specify multiple
options.""".replace(
        "\n", " "
    )
    parser.add_argument(
        "--kernelize_option", action="append", help=kernelize_option_help
    )

    arguments = Arguments()
    parser.parse_args(namespace=arguments)

    return arguments


def load_compile_command_files(filepath: pathlib.Path) -> list[pathlib.Path]:
    """
    Loads the files in the given compilation database at the given path.
    """

    with open(filepath) as fp:
        return [command["file"] for command in json.load(fp)]


@dataclasses.dataclass
class KernelizeInput:
    """
    Contains all the necessary information to run kernelize on a file.
    """

    kernelize_path: pathlib.Path
    filepath: pathlib.Path
    compile_commands_dir: pathlib.Path
    kernelize_option: list[str]
    output_directory: pathlib.Path
    print_commands: bool


def kernelize_file(kernelize_input: KernelizeInput) -> Optional[timedelta]:
    """
    Reuns kernelize on the given file with the given arguments. If successful,
    returns the time elapsed while lowering the compilation unit to MLIR;
    otherwise returns None.
    """

    # We have to pack the arguments into a dataclass like this since Pool.imap()
    # requires multiprocessed functions to accept a single argument.
    (
        kernelize_path,
        filepath,
        compile_commands_dir,
        kernelize_option,
        output_directory,
        print_commands,
    ) = dataclasses.astuple(kernelize_input)

    input_filepath = pathlib.PurePath(os.path.abspath(filepath))
    input_mlir_name = input_filepath.with_suffix(".mlir").name
    input_log_name = input_filepath.with_suffix(".log").name
    output_filepath = output_directory / input_mlir_name
    log_filepath = output_directory / input_log_name
    while output_filepath.is_file():
        output_name = output_filepath.name
        output_filepath = output_filepath.with_name(output_name + "_")
    while log_filepath.is_file():
        log_name = log_filepath.name
        log_filepath = log_filepath.with_name(log_name + "_")

    extra_args = ["-w", "-Wno-error", "-Wno-everything"]

    command = f"cd {compile_commands_dir} && " + " ".join(
        [str(kernelize_path)]
        + [f"-p {compile_commands_dir}"]
        + kernelize_option
        + [str(input_filepath)]
        + [f'--extra-arg="{arg}"' for arg in extra_args]
    )

    if print_commands:
        print(command, file=sys.stderr)

    begin = datetime.now()
    cp = subprocess.run(command, shell=True, capture_output=True)
    elapsed = datetime.now() - begin
    failed = 0 != cp.returncode

    if failed:
        with open(log_filepath, "wb") as logfile:
            logfile.write(cp.stderr)
    else:
        with open(output_filepath, "wb") as outfile:
            outfile.write(cp.stdout)

    return None if failed else elapsed


def print_tsv_row(row: list[str]):
    print("\t".join(row))


def kernelize_compilation_database(
    kernelize_path: pathlib.Path,
    kernelize_option: list[str],
    compile_commands_file: pathlib.Path,
    output_directory: pathlib.Path,
    num_processes: int,
    print_commands=False,
) -> int:
    """
    Returns the number of compilation units kernelize successfully lowers to
    MLIR.
    """
    tsv_header = ["Compilation unit", "Runtime or failure"]
    num_passing = 0

    filepaths = load_compile_command_files(compile_commands_file)
    compile_commands_dir = compile_commands_file.parent

    output_directory.mkdir(parents=True, exist_ok=True)
    print_tsv_row(tsv_header)
    kernelize_inputs = [
        KernelizeInput(
            kernelize_path=kernelize_path,
            filepath=filepath,
            compile_commands_dir=compile_commands_dir,
            kernelize_option=kernelize_option,
            output_directory=output_directory,
            print_commands=print_commands,
        )
        for filepath in filepaths
    ]

    with multiprocessing.Pool(num_processes) as pool:
        for (i, elapsed), filepath in zip(
            enumerate(pool.imap(kernelize_file, kernelize_inputs), 1),
            filepaths,
        ):
            num_passing += int(elapsed is not None)
            failed = elapsed is None

            row = [filepath, str(elapsed) if not failed else "FAIL"]
            print_tsv_row(row)

            prefix = "error" if failed else "finished"
            msg = f"{prefix} processing {i}/{len(filepaths)} files"
            print(msg, file=sys.stderr)

    return num_passing


def main() -> int:
    arguments = parse_arguments()
    output_directory = arguments.output_directory.absolute()

    num_passing = kernelize_compilation_database(
        kernelize_path=pathlib.Path(arguments.kernelize_path),
        kernelize_option=arguments.kernelize_option,
        compile_commands_file=arguments.compile_commands_file,
        output_directory=output_directory,
        num_processes=arguments.num_processes,
        print_commands=arguments.print_commands,
    )

    print(f"Total successful: {num_passing}", file=sys.stderr)

    return 0


if "__main__" == __name__:
    exit(main())
