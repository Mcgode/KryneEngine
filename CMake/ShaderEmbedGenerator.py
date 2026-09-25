#!/usr/bin/env python3
import sys
import json
import re
from pathlib import Path

BYTES_PER_LINE = 16


def sanitize_identifier(name: str) -> str:
    identifier = re.sub(r'[^0-9A-Za-z_]', '_', name)
    if identifier[:1].isdigit():
        identifier = f"_{identifier}"
    return identifier


def format_byte_array(data: bytes) -> str:
    lines = []
    for i in range(0, len(data), BYTES_PER_LINE):
        chunk = data[i:i + BYTES_PER_LINE]
        lines.append("        " + ", ".join(f"0x{b:02x}" for b in chunk) + ",")
    return "\n".join(lines)


def main():
    manifest_path = Path(sys.argv[1])
    out_cpp = Path(sys.argv[2])
    out_h = Path(sys.argv[3])
    target_name = sys.argv[4]

    with open(manifest_path, 'r') as f:
        manifest = json.load(f)

    namespace = sanitize_identifier(target_name)

    declarations = []
    definitions = []
    for entry in manifest:
        identifier = sanitize_identifier(entry["identifier"])
        data = Path(entry["path"]).read_bytes()

        declarations.append(f"    eastl::span<const u8> {identifier}();")
        definitions.append(
            f"    namespace\n    {{\n"
            f"        constexpr u8 k{identifier}_Data[] = {{\n"
            f"{format_byte_array(data)}\n"
            f"        }};\n"
            f"    }}\n\n"
            f"    eastl::span<const u8> {identifier}()\n"
            f"    {{\n"
            f"        return k{identifier}_Data;\n"
            f"    }}"
        )

    header_name = out_h.name

    header_content = (
        "#pragma once\n\n"
        "#include <EASTL/span.h>\n"
        "#include <KryneEngine/Core/Common/Types.hpp>\n\n"
        f"namespace KryneEngine::EmbeddedShaders::{namespace}\n"
        "{\n"
        + "\n".join(declarations)
        + "\n}\n"
    )

    cpp_content = (
        f"#include \"{header_name}\"\n\n"
        f"namespace KryneEngine::EmbeddedShaders::{namespace}\n"
        "{\n"
        + "\n\n".join(definitions)
        + "\n}\n"
    )

    out_h.parent.mkdir(parents=True, exist_ok=True)
    out_cpp.parent.mkdir(parents=True, exist_ok=True)
    out_h.write_text(header_content)
    out_cpp.write_text(cpp_content)


if __name__ == "__main__":
    main()
