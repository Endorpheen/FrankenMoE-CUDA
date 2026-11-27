#!/usr/bin/env python3
"""Compatibility entry point for the GooseInfer console."""
from pathlib import Path
from runpy import run_path

if __name__ == "__main__":
    run_path(str(Path(__file__).with_name("gooseinfer-console.py")), run_name="__main__")
