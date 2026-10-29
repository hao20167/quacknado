"""Canonical project-source inventory; excludes builds, assets and dependencies."""
import hashlib
from pathlib import Path

def source_files(root=Path('.')):
    root=Path(root)
    files=[]
    for folder in ('src','shaders','benchmarks','tests','scripts','cmake'):
        base=root/folder
        if base.exists():
            files.extend(p for p in base.rglob('*') if p.is_file() and not p.is_symlink() and '__pycache__' not in p.parts)
    files.extend(root/name for name in ('CMakeLists.txt','CMakePresets.json','.clang-format') if (root/name).is_file())
    return sorted(files,key=lambda p:p.relative_to(root).as_posix())

def source_hashes(root=Path('.')):
    root=Path(root)
    return {p.relative_to(root).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in source_files(root)}
