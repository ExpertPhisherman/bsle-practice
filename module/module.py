import argparse
import sys

def main():
    parser = argparse.ArgumentParser(description="Module template")

    parser.add_argument("name", help="Module name")
    parser.add_argument("--brief", help="Module brief description")
    parser.add_argument("--par", help="Module paragraph description")

    args = parser.parse_args()

    # TODO: Create files
    pass

    return 0

if __name__ == "__main__":
    sys.exit(main())