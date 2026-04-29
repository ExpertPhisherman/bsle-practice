import argparse
import sys
from pathlib import Path

def main():
    parser = argparse.ArgumentParser(description="File template")

    parser.add_argument("name", help="File name")
    parser.add_argument("module", nargs="?", default=None, help="Module name (defaults to file name)")

    args = parser.parse_args()
    name = args.name.lower()
    module = args.module if args.module is not None else name

    source_contents = f"""/** @file {name}.c\n *\n * @brief {name.title()} source\n *\n * @par\n *\n */\n\n#include "{name}.h"\n\n\n\n/*** end of file ***/\n"""

    header_contents = f"""/** @file {name}.h\n *\n * @brief {name.title()} header\n *\n * @par\n *\n */\n\n#ifndef {name.upper()}_H\n#define {name.upper()}_H\n\n#include <stdio.h>\n#include <stdlib.h>\n#include <unistd.h>\n#include "common.h"\n\n\n\n#endif /* {name.upper()}_H */\n\n/*** end of file ***/\n"""

    base = Path("..") / module

    for subdir in ["src", "include"]:
        (base / subdir).mkdir(parents=True, exist_ok=True)

    with open(base / "src" / f"{name}.c", "w") as f:
        f.write(source_contents)

    with open(base / "include" / f"{name}.h", "w") as f:
        f.write(header_contents)

    for src, dst in [
        (Path("src") / "main.c",      base / "src"     / "main.c"),
        (Path("include") / "main.h",  base / "include" / "main.h"),
    ]:
        if src.exists() and not dst.exists():
            dst.write_bytes(src.read_bytes())

    return 0

if __name__ == "__main__":
    sys.exit(main())