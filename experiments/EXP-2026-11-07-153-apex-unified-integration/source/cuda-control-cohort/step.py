#!/usr/bin/env python3
"""Run one local-check step with cuda-final as the working directory."""
import os, subprocess, sys
here = os.path.dirname(os.path.abspath(__file__))
sys.exit(subprocess.run(sys.argv[1:], cwd=here).returncode)
