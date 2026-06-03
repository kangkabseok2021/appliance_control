"""3 pytest tests for the PID header code generator."""
import sys, os, re, subprocess, tempfile
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'python'))

import pytest
from codegen import generate


@pytest.fixture
def generated_header(tmp_path):
    out = str(tmp_path / "pid_gains.h")
    generate(out)
    return out


def test_generated_file_is_valid_c(generated_header):
    """gcc -fsyntax-only must accept the generated header."""
    wrapper = generated_header.replace(".h", "_test.c")
    with open(wrapper, "w") as f:
        f.write(f'#include "{generated_header}"\nint main(void){{return 0;}}\n')
    result = subprocess.run(
        ["gcc", "-fsyntax-only", wrapper],
        capture_output=True, text=True
    )
    assert result.returncode == 0, f"GCC rejected generated header:\n{result.stderr}"


def test_all_defines_present(generated_header):
    """All six #define symbols must appear in the generated file."""
    required = ["PID_KP", "PID_KI", "PID_KD", "PID_DT", "PID_CLAMP_MIN", "PID_CLAMP_MAX"]
    content = open(generated_header).read()
    for sym in required:
        assert sym in content, f"Missing #define {sym}"


def test_float_suffix_on_all_constants(generated_header):
    """Every numeric literal in #define lines must have the 'f' suffix."""
    content = open(generated_header).read()
    for line in content.splitlines():
        if line.startswith("#define PID_"):
            # Extract the value part (after the symbol)
            parts = line.split()
            if len(parts) >= 3:
                val = parts[2]
                # Must end with 'f' (float literal)
                assert val.endswith("f"), \
                    f"Missing 'f' suffix on line: {line!r}"
