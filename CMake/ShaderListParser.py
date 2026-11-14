#!/usr/bin/env python3
import stat
import sys
import json
from pathlib import Path, PurePath
import os
from ninja import ninja_syntax
import platform


def main():
    target_name = sys.argv[1]
    shader_output_dir = Path(sys.argv[2])
    shader_format = sys.argv[3]
    output_file = Path(sys.argv[4])
    slangc_path = Path(sys.argv[5])
    dxc_library_dir = Path(sys.argv[6])
    shaders_dir = Path(sys.argv[7])
    include_list = sys.argv[8]
    embed_enabled = sys.argv[9] == "1"
    embed_script = Path(sys.argv[10])
    embed_cpp_output = Path(sys.argv[11])
    embed_h_output = Path(sys.argv[12])
    optimize_spirv = sys.argv[13] == "1"
    shader_list_files = sys.argv[14:]

    working_dir = output_file.parent

    if platform.system() in ("Linux", "Darwin"):
        current_mode = os.stat(slangc_path).st_mode
        os.chmod(slangc_path, current_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)

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
            for directory in include_list.split('%%'):
                include_variable += f'-I "{os.path.relpath(directory, working_dir)}" '
        writer.variable("includes", include_variable)

        slangc_path_name = "slangc_path"
        writer.variable(slangc_path_name, os.path.relpath(slangc_path, working_dir))

        shader_input_dir_name = "input_dir"
        writer.variable(shader_input_dir_name, os.path.relpath(shaders_dir, working_dir))

        shader_output_dir_name = "output_dir"
        writer.variable(shader_output_dir_name, os.path.relpath(shader_output_dir, working_dir))

        writer.newline()

        format_is_metal = shader_format == "metallib"

        format_extension = ".cso"
        slangc_target = "dxil"
        language_define = "-DKE_TARGET_DXIL"
        if shader_format == "spirv":
            format_extension = ".spv"
            slangc_target = "spirv"
            language_define = "-DKE_TARGET_SPIRV"
        elif format_is_metal:
            # Emit Metal source and build the metallib through xcrun, so the intermediate .metal file
            # stays on disk for debugging (and gets recorded in the metallib with -frecord-sources).
            format_extension = ".metal"
            slangc_target = "metal"
            language_define = "-DKE_TARGET_METALLIB"

        slangc_downstream_arguments = ""
        if shader_format == "cso":
            # Slang emits DXIL through the dxcompiler library, which it does not bundle
            slangc_downstream_arguments = f' -dxc-path "{os.path.relpath(dxc_library_dir, working_dir)}"'

        slangc_optimization = ""
        if shader_format == "spirv":
            slangc_optimization = "-O2" if optimize_spirv else "-O0"
        slangc_optimization_arguments = f"{slangc_optimization}" if slangc_optimization else ""
        if shader_format == "spirv":
            # Slang names every SPIR-V entry point 'main' by default, but pipelines look them up by source name
            slangc_optimization_arguments += " -fvk-use-entrypoint-name"

        writer.comment("--------------------------------------------------------------------")
        writer.comment(f" Build rules")
        writer.comment("--------------------------------------------------------------------")
        writer.newline()

        if format_is_metal:
            writer.rule("metal_to_air",
                        "xcrun -sdk macosx metal -c $in -o $out -frecord-sources -gline-tables-only")

            writer.newline()
            writer.rule("air_to_metallib",
                        "xcrun -sdk macosx metallib $in -o $out")
            writer.newline()

        writer.rule("slangc_compile",
                    f"${slangc_path_name} $in -o $out -depfile $out.d $profile_args -entry $entry_point -target {slangc_target} {slangc_optimization_arguments} {language_define}=1 -g{slangc_downstream_arguments} $includes",
                    depfile="$out.d",
                    # slangc leaves the output file untouched when its content did not change, so
                    # without restat the output stays older than the edited input forever and the
                    # shader is recompiled on every build.
                    restat=True)

        # Shader model profile prefix -> Slang stage name
        shader_stages = {
            "vs": "vertex",
            "ps": "fragment",
            "cs": "compute",
            "gs": "geometry",
            "hs": "hull",
            "ds": "domain",
            "ms": "mesh",
            "as": "amplification",
        }

        def get_slangc_profile_arguments(_shader_type: str) -> str:
            if shader_format != "spirv":
                return f"-profile {_shader_type}"
            # A shader model profile (vs_6_0, ...) makes Slang emit SPIR-V 1.3, which needs Vulkan 1.1.
            # Request SPIR-V 1.0 explicitly so the shaders also load on Vulkan 1.0 devices.
            stage = shader_stages.get(_shader_type.split("_")[0])
            if stage is None:
                raise ValueError(f"Unknown shader stage for shader type '{_shader_type}'")
            return f"-profile spirv_1_0 -stage {stage}"

        writer.newline()
        writer.comment("--------------------------------------------------------------------")
        writer.comment(f" Build commands")
        writer.comment("--------------------------------------------------------------------")

        embed_entries = []

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

                    def get_output_shader_relative_path(extension: str):
                        shader = shader_file.relative_to(shaders_dir)
                        return shader.with_name(f"{shader.stem}_{entry_point}{extension}")

                    def get_output_shader_path(extension: str):
                        return PurePath(f"${shader_output_dir_name}") / get_output_shader_relative_path(extension)

                    # Output location
                    output_shader = get_output_shader_path(format_extension)

                    input_path = PurePath(f'${shader_input_dir_name}') / shader_file.relative_to(shaders_dir)
                    print(input_path)

                    writer.newline()
                    writer.build(
                        [str(output_shader)],
                        "slangc_compile",
                        [str(input_path)],
                        implicit=[
                            f"${slangc_path_name}"
                        ],
                        variables={
                            "profile_args": get_slangc_profile_arguments(shader_type),
                            "entry_point": entry_point
                        },
                    )

                    final_shader = output_shader
                    final_extension = format_extension
                    if format_is_metal:
                        air_shader = get_output_shader_path(".air")
                        writer.build(
                            [str(air_shader)],
                            "metal_to_air",
                            str(output_shader))
                        metallib_shader = get_output_shader_path(".metallib")
                        writer.build(
                            [str(metallib_shader)],
                            "air_to_metallib",
                            str(air_shader))
                        final_shader = metallib_shader
                        final_extension = ".metallib"

                    if embed_enabled:
                        shader_rel = shader_file.relative_to(shaders_dir)
                        shader_rel_stem = shader_rel.with_suffix("").as_posix().replace("/", "_")
                        identifier = f"{shader_rel_stem}_{entry_point}"
                        real_path = shader_output_dir / get_output_shader_relative_path(final_extension)
                        embed_entries.append({
                            "identifier": identifier,
                            "ninja_target": str(final_shader),
                            "real_path": str(real_path),
                        })

                    print(f" - Shader type [{shader_type}], Entry Point [{entry_point}]")

        if embed_enabled:
            writer.newline()
            writer.comment("--------------------------------------------------------------------")
            writer.comment(f" Embedded shader generation")
            writer.comment("--------------------------------------------------------------------")
            writer.newline()

            manifest_path = working_dir / "embed_manifest.json"
            with open(manifest_path, 'w') as manifest_file:
                json.dump(
                    [{"identifier": e["identifier"], "path": e["real_path"]} for e in embed_entries],
                    manifest_file,
                    indent=2,
                )

            embed_script_name = "embed_script"
            writer.variable(embed_script_name, os.path.relpath(embed_script, working_dir))

            writer.rule(
                "embed_shaders",
                f"${python_name} ${embed_script_name} $in $embed_cpp $embed_h $target_name",
            )

            writer.newline()
            writer.build(
                [str(embed_cpp_output), str(embed_h_output)],
                "embed_shaders",
                [os.path.relpath(manifest_path, working_dir)],
                implicit=[e["ninja_target"] for e in embed_entries] + [f"${embed_script_name}"],
                variables={
                    "embed_cpp": str(embed_cpp_output),
                    "embed_h": str(embed_h_output),
                    "target_name": target_name,
                },
            )

        writer.close()


if __name__ == "__main__":
    main()

