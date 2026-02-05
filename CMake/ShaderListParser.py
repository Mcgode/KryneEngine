#!/usr/bin/env python3

import sys
import json
from pathlib import Path, PurePath
import os
from ninja import ninja_syntax


def main():
    target_name = sys.argv[1]
    shader_output_dir = Path(sys.argv[2])
    shader_format = sys.argv[3]
    output_file = Path(sys.argv[4])
    shader_tools = {}
    for pair in sys.argv[5].split("%%"):
        shader_tools[pair.split("=")[0]] = Path(pair.split("=")[1])
    shaders_dir = Path(sys.argv[6])
    python_script = Path(sys.argv[7])
    include_list = sys.argv[8]
    shader_list_files = sys.argv[9:]

    working_dir = output_file.parent

    with open(output_file, 'w') as f:
        writer = ninja_syntax.Writer(f, 150)

        writer.comment("####################################################################")
        writer.comment(f" {target_name} shaders compilation ninja file")
        writer.comment("####################################################################")
        writer.newline()

        writer.comment("--------------------------------------------------------------------")
        writer.comment(f" Variables")
        writer.comment("--------------------------------------------------------------------")
        writer.newline()

        python_name = "python"
        writer.variable(python_name, f"\"{sys.executable}\"")

        include_variable = ''
        if include_list != "None":
            for directory in include_list.split(';'):
                include_variable += f'-I "{os.path.relpath(directory, working_dir)}" '
        writer.variable("includes", include_variable)

        build_shader_script_name = "build_shader_script"
        writer.variable(build_shader_script_name, os.path.relpath(python_script, working_dir))

        dxc_path_name = "dxc_path"
        writer.variable(dxc_path_name, os.path.relpath(shader_tools["dxc"], working_dir))

        spirv_cross_path_name = "spirv_cross_path"
        if "spirv-cross" in shader_tools:
            writer.variable(spirv_cross_path_name, os.path.relpath(shader_tools["spirv-cross"], working_dir))

        shader_input_dir_name = "input_dir"
        writer.variable(shader_input_dir_name, os.path.relpath(shaders_dir, working_dir))

        shader_output_dir_name = "output_dir"
        writer.variable(shader_output_dir_name, os.path.relpath(shader_output_dir, working_dir))

        writer.newline()

        compile_dxc_to_spirv = shader_format == "spirv" or shader_format == "metallib"
        format_is_metal = shader_format == "metallib"

        base_command = f"${dxc_path_name} "
        if compile_dxc_to_spirv:
            base_command += "-spirv "
        base_command += "$in "
        base_command += "-T $shader_type "
        base_command += "-E $entry_point "
        base_command += "$includes "
        # base_command += "-Zi "
        # base_command += "-Qembed_debug "

        command = f"${python_name} ${build_shader_script_name} $out {base_command}"

        format_extension = ".cso"
        if compile_dxc_to_spirv:
            format_extension = ".spv"

        writer.comment("--------------------------------------------------------------------")
        writer.comment(f" Build rules")
        writer.comment("--------------------------------------------------------------------")
        writer.newline()

        writer.rule("dxc_compile",
                    command,
                    depfile="$out.d",
                    deps="gcc")

        if format_is_metal:
            writer.newline()
            writer.rule("spirv_to_metal",
                        f"${spirv_cross_path_name} $in --msl --msl-version 30100 --msl-argument-buffers --output $out")

            writer.newline()
            writer.rule("metal_to_air",
                        "xcrun -sdk macosx metal -c $in -o $out -frecord-sources -gline-tables-only")

            writer.newline()
            writer.rule("air_to_metallib",
                        "xcrun -sdk macosx metallib $in -o $out")

        writer.newline()
        writer.comment("--------------------------------------------------------------------")
        writer.comment(f" Build commands")
        writer.comment("--------------------------------------------------------------------")

        for input_file_path in shader_list_files:
            with open(input_file_path, 'r') as input_file:
                current_dir = Path(input_file_path).parent
                input_json = json.load(input_file)

                if "Path" not in input_json or "Configurations" not in input_json:
                    continue

                shader_file = current_dir / input_json["Path"]
                configuration_count = len(input_json["Configurations"])
                print(f"Creating commands for {shader_file.relative_to(shaders_dir)}, "
                      f"with {configuration_count} different configurations")

                for configuration in input_json["Configurations"]:
                    entry_point = configuration["EntryPoint"]
                    shader_type = configuration["ShaderType"]

                    def get_output_shader_path(extension: str):
                        shader = shader_file.relative_to(shaders_dir)
                        shader = shader.with_name(f"{shader.stem}_{entry_point}{extension}")
                        shader = PurePath(f"${shader_output_dir_name}") / shader
                        return shader

                    # Output location
                    output_shader = get_output_shader_path(format_extension)

                    input_path = PurePath(f'${shader_input_dir_name}') / shader_file.relative_to(shaders_dir)
                    print(input_path)

                    writer.newline()
                    writer.build(
                        [str(output_shader)],
                        "dxc_compile",
                        [str(input_path)],
                        implicit=[
                            f"${build_shader_script_name}"
                        ],
                        variables={
                            "shader_type": shader_type,
                            "entry_point": entry_point
                        },
                    )

                    if format_is_metal:
                        metal_shader = get_output_shader_path(".metal")
                        writer.build(
                            [str(metal_shader)],
                            "spirv_to_metal",
                            str(output_shader))
                        air_shader = get_output_shader_path(".air")
                        writer.build(
                            [str(air_shader)],
                            "metal_to_air",
                            str(metal_shader))
                        metallib_shader = get_output_shader_path(".metallib")
                        writer.build(
                            [str(metallib_shader)],
                            "air_to_metallib",
                            str(air_shader))


                    print(f" - Shader type [{shader_type}], Entry Point [{entry_point}]")
        writer.close()


if __name__ == "__main__":
    main()

