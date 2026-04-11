#!/usr/bin/env python3

import sys
import subprocess


def main():
    output_file = sys.argv[1]
    common_args = sys.argv[2:]

    def run_command(args):
        result = subprocess.run(args, capture_output=True, text=True)
        if len(result.stdout) > 0:
            print(result.stdout)
        if len(result.stderr) > 0:
            print(result.stderr)
        if result.returncode != 0:
            exit(result.returncode)

    run_command(common_args + ["-MD", "-MF", f"{output_file}.d"])
    run_command(common_args + ["-Fo", output_file])


if __name__ == "__main__":
    main()
