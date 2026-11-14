import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SHADER_LIST_PARSER = PROJECT_ROOT / "CMake" / "ShaderListParser.py"
SHADER_EMBED_GENERATOR = PROJECT_ROOT / "CMake" / "ShaderEmbedGenerator.py"


class ShaderListParserTests(unittest.TestCase):
    @staticmethod
    def generate_ninja_file(shader_format, optimize_spirv):
        with tempfile.TemporaryDirectory(dir=PROJECT_ROOT) as temporary_directory:
            working_dir = Path(temporary_directory)
            shaders_dir = working_dir / "shaders"
            shaders_dir.mkdir()
            shader_list_file = shaders_dir / "test.shader"
            shader_list_file.write_text(json.dumps({
                "Path": "test.slang",
                "Configurations": [{
                    "EntryPoint": "Main",
                    "ShaderType": "vs_6_0",
                }],
            }))

            slangc_path = working_dir / "slangc"
            slangc_path.touch()

            output_file = working_dir / "build.ninja"
            subprocess.run([
                sys.executable,
                str(SHADER_LIST_PARSER),
                "TestShaders",
                str(working_dir / "output"),
                shader_format,
                str(output_file),
                str(slangc_path),
                str(working_dir / "dxc"),
                str(shaders_dir),
                "None",
                "0",
                str(SHADER_EMBED_GENERATOR),
                str(working_dir / "unused.cpp"),
                str(working_dir / "unused.h"),
                "1" if optimize_spirv else "0",
                str(shader_list_file),
            ], check=True, capture_output=True, text=True)
            return output_file.read_text()

    def test_spirv_defaults_to_no_optimizer(self):
        ninja_file = self.generate_ninja_file("spirv", False)

        self.assertIn("-target spirv -O0 ", ninja_file)
        self.assertNotIn("-O2", ninja_file)

    def test_spirv_optimization_can_be_enabled(self):
        ninja_file = self.generate_ninja_file("spirv", True)

        self.assertIn("-target spirv -O2 ", ninja_file)
        self.assertNotIn("-O0", ninja_file)

    def test_spirv_keeps_entry_point_names_and_targets_spirv_1_0(self):
        ninja_file = self.generate_ninja_file("spirv", False)

        self.assertIn("-fvk-use-entrypoint-name", ninja_file)
        self.assertIn("profile_args = -profile spirv_1_0 -stage vertex", ninja_file)
        self.assertNotIn("-dxc-path", ninja_file)

    def test_non_spirv_target_does_not_get_spirv_flags(self):
        ninja_file = self.generate_ninja_file("cso", False)

        self.assertIn("-target dxil ", ninja_file)
        self.assertIn('-dxc-path "dxc"', ninja_file)
        self.assertIn("profile_args = -profile vs_6_0", ninja_file)
        self.assertNotIn("-O0", ninja_file)
        self.assertNotIn("-O2", ninja_file)
        self.assertNotIn("-fvk-use-entrypoint-name", ninja_file)

    def test_metallib_keeps_intermediate_metal_source(self):
        ninja_file = self.generate_ninja_file("metallib", False)

        self.assertIn("-target metal ", ninja_file)
        self.assertIn("build $output_dir/test_Main.metal: slangc_compile", ninja_file)
        self.assertIn("build $output_dir/test_Main.air: metal_to_air $output_dir/test_Main.metal", ninja_file)
        self.assertIn("build $output_dir/test_Main.metallib: air_to_metallib $output_dir/test_Main.air", ninja_file)


if __name__ == "__main__":
    unittest.main()