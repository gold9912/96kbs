#!/usr/bin/env python3
import argparse
from pathlib import Path


def parse_input(value):
    if "=" not in value:
        raise argparse.ArgumentTypeError("inputs must use Name=path")
    name, path = value.split("=", 1)
    if not name.isidentifier():
        raise argparse.ArgumentTypeError(f"invalid C++ identifier: {name}")
    return name, Path(path)


def write_array(source, name, data):
    source.write(f"alignas(16) const unsigned char k{name}[] = {{\n")
    for offset in range(0, len(data), 12):
        chunk = data[offset:offset + 12]
        values = ", ".join(f"0x{byte:02x}" for byte in chunk)
        source.write(f"    {values},\n")
    source.write("};\n\n")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--header", required=True)
    parser.add_argument("--source", required=True)
    parser.add_argument("--namespace", required=True)
    parser.add_argument("inputs", nargs="+", type=parse_input)
    args = parser.parse_args()

    header_path = Path(args.header)
    source_path = Path(args.source)
    header_path.parent.mkdir(parents=True, exist_ok=True)
    source_path.parent.mkdir(parents=True, exist_ok=True)

    ns_parts = args.namespace.split("::")
    open_ns = "\n".join(f"namespace {part} {{" for part in ns_parts)
    close_ns = "\n".join("}" for _ in ns_parts)

    with header_path.open("w", encoding="utf-8", newline="\n") as header:
        header.write("#pragma once\n\n")
        header.write("#include <cstddef>\n\n")
        header.write(f"{open_ns}\n\n")
        header.write("struct Bytecode {\n")
        header.write("    const void* data;\n")
        header.write("    std::size_t size;\n")
        header.write("};\n\n")
        for name, _ in args.inputs:
            header.write(f"Bytecode {name}();\n")
        header.write(f"\n{close_ns}\n")

    with source_path.open("w", encoding="utf-8", newline="\n") as source:
        source.write('#include "embedded_shaders.h"\n\n')
        source.write(f"{open_ns}\n\n")
        for name, path in args.inputs:
            data = path.read_bytes()
            write_array(source, name, data)
            source.write(f"Bytecode {name}() {{\n")
            source.write(f"    return Bytecode{{k{name}, sizeof(k{name})}};\n")
            source.write("}\n\n")
        source.write(f"{close_ns}\n")


if __name__ == "__main__":
    main()

