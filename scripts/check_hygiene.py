#!/usr/bin/env python3
"""Audit output by location/role, without following symlinks or deleting anything."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT_FILES = {'AGENTS.md', 'CHANGELOG.md', 'CMakeLists.txt', 'LICENSE', 'README.md', '.gitignore', 'pyproject.toml'}
ROOT_DIRS = {'.agents', '.git', '.github', 'apps', 'benchmarks', 'cmake', 'docs', 'examples',
             'genetik', 'scripts', 'tests', 'out', 'build', 'python', 'integrations'}
WORKSPACE = {'Genetik', 'Genetik-data', '.tools', 'delivery-tools', 'archive', 'docs', 'examples', 'session.md'}
OUTPUT_DIRS = {'build', 'install', 'consumers', 'runs', 'data', 'logs', 'tmp', 'cleanup', 'worktrees'}
# Retained D7 delivery assets/evidence predate the current role layout. They are
# explicitly preserved, never used for new transient outputs, and not scanned.
PRESERVED_OUTPUT_DIRS = {'release', 'source-archive-check'}


def is_binary_executable(path):
    try:
        with path.open('rb') as stream:
            header = stream.read(4)
    except OSError:
        return False
    return header.startswith(b'\x7fELF') or header[:2] == b'MZ' or header in {
        b'\xfe\xed\xfa\xce', b'\xce\xfa\xed\xfe', b'\xfe\xed\xfa\xcf',
        b'\xcf\xfa\xed\xfe', b'\xca\xfe\xba\xbe',
    }


def audit(source, workspace=False):
    errors = []
    for p in sorted(source.iterdir()):
        if p.name not in ROOT_FILES | ROOT_DIRS:
            errors.append(f'unexpected repository-root entry: {p}')
        elif p.is_symlink():
            errors.append(f'root entry is a symlink (not traversed): {p}')
    output = source / 'out'
    if output.is_dir() and not output.is_symlink():
        for p in sorted(output.iterdir()):
            if p.name not in OUTPUT_DIRS | PRESERVED_OUTPUT_DIRS or not p.is_dir() or p.is_symlink():
                errors.append(f'unclassified output (use a role directory): {p}')
    # New CMake output is confined to the ignored root build/. Legacy out/ is
    # opaque during migration. Build content is allowed but must never be tracked.
    inside = subprocess.run(
        ['git', '-C', str(source), 'rev-parse', '--is-inside-work-tree'],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False,
    ).returncode == 0
    if inside:
        tracked = subprocess.run(
            ['git', '-C', str(source), 'ls-files', '--', 'build/'],
            capture_output=True, text=True, check=False,
        )
        if tracked.returncode != 0:
            errors.append(f'cannot inspect tracked build files: {source}')
        elif tracked.stdout.strip():
            errors.append(f'tracked file inside generated build/: {tracked.stdout.splitlines()[0]}')
    # Evidence/fixtures may use txt/csv/json; generated caches and binaries do not
    # belong in any source role. out/, build/ and .git/ are opaque, not scanned.
    for name in ROOT_DIRS - {'.git', 'out', 'build'}:
        top = source / name
        if not top.is_dir() or top.is_symlink():
            continue
        for root, dirs, files in os.walk(top, followlinks=False):
            dirs[:] = [d for d in dirs if not (Path(root) / d).is_symlink()]
            for d in dirs:
                if d in {'CMakeFiles', '__pycache__'}:
                    errors.append(f'generated source-tree directory: {Path(root) / d}')
            for f in files:
                p = Path(root) / f
                if p.is_symlink():
                    continue
                if f in {'CMakeCache.txt', 'cmake_install.cmake', 'compile_commands.json'} or p.suffix in {'.o', '.a', '.so', '.pyc'}:
                    errors.append(f'generated source-tree file: {p}')
                elif is_binary_executable(p) or p.suffix in {'.dll', '.dylib', '.exe'}:
                    errors.append(f'generated source-tree executable/library: {p}')
                if name == 'scripts' and p.suffix == '.cpp':
                    errors.append(f'compiled application belongs in apps/ or benchmarks/: {p}')
    if workspace:
        workspace_root = next((
            parent for parent in source.parents
            if {'.tools', 'delivery-tools', 'archive', 'Genetik'} <=
               {child.name for child in parent.iterdir()}
        ), source.parent)
        for p in sorted(workspace_root.iterdir()):
            if p.name not in WORKSPACE:
                errors.append(f'unexpected parent-workspace entry: {p}')
    return errors


def self_test():
    with tempfile.TemporaryDirectory(prefix='genetik-hygiene-') as tmp:
        source = Path(tmp) / 'Genetik'
        (source / 'docs').mkdir(parents=True)
        (source / 'tests').mkdir()
        (source / '.agents').mkdir()
        (source / 'python').mkdir()
        (source / 'integrations').mkdir()
        (source / 'pyproject.toml').write_text('[build-system]\n')
        (source / 'out').mkdir()
        (source / 'build' / 'release').mkdir(parents=True)
        (source / 'docs' / 'accepted.csv').write_text('preserved evidence\n')
        (source / 'out' / 'build').mkdir()
        (source / 'out' / 'build' / 'CMakeCache.txt').write_text('allowed output\n')
        for generated in ('CMakeCache.txt', 'release/genetik_demo', 'release/libgenetik.a',
                          'release/CMakeFiles/demo.dir/main.cpp.o', 'benchmarks/genetik_bench'):
            path = source / 'build' / generated
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text('generated output\n')
        (source / 'out' / 'worktrees').mkdir()
        (source / 'out' / 'release').mkdir()
        (source / 'out' / 'source-archive-check').mkdir()
        (Path(tmp) / '.tools').mkdir()
        (Path(tmp) / 'delivery-tools').mkdir()
        (Path(tmp) / 'archive').mkdir()
        (Path(tmp) / 'docs').mkdir()
        (Path(tmp) / 'examples').mkdir()
        (Path(tmp) / 'session.md').write_text('workspace note\n')
        assert not audit(source, True)
        (source / 'tests' / 'generated').mkdir()
        (source / 'tests' / 'generated' / 'CMakeCache.txt').touch()
        assert any('source-tree file' in e for e in audit(source))
        (source / 'tests' / 'generated' / 'CMakeCache.txt').unlink()
        (source / 'tests' / 'generated').rmdir()
        for role, relative in (
            ('benchmarks', 'generated.o'), ('apps', 'generated-bin'),
            ('examples', 'CMakeFiles/generated.dir'),
        ):
            artifact = source / role / relative
            artifact.parent.mkdir(parents=True, exist_ok=True)
            artifact.write_bytes(b'\x7fELF' if role == 'apps' else b'generated\n')
            if role == 'apps':
                artifact.chmod(0o755)
            assert audit(source), f'generated artifact accepted in {role}/'
            artifact.unlink()
            parent = artifact.parent
            while parent != source / role:
                parent.rmdir()
                parent = parent.parent
        # A tracked build artifact fails even though generated files are allowed there.
        subprocess.run(['git', '-C', str(source), 'init', '-q'], check=True)
        subprocess.run(['git', '-C', str(source), 'add', '-f', 'build/release/genetik_demo'], check=True)
        assert any('tracked file inside generated build/' in e for e in audit(source))
        subprocess.run(['git', '-C', str(source), 'rm', '--cached', '-q', 'build/release/genetik_demo'], check=True)
        (source / 'out' / 'probe.log').touch()
        assert any('unclassified output' in e for e in audit(source))
        (source / 'out' / 'probe.log').unlink()
        (source / 'loose.log').write_text('bad\n')
        assert any('root entry' in e for e in audit(source))
        (source / 'loose.log').unlink()
        (source / 'docs' / 'CMakeCache.txt').touch()
        assert any('source-tree file' in e for e in audit(source))
        (source / 'docs' / 'CMakeCache.txt').unlink()
        (Path(tmp) / 'build-old').mkdir()
        assert any('parent-workspace' in e for e in audit(source, True))
        # A link outside the source is never scanned.
        (source / 'docs' / 'external').symlink_to(Path(tmp), target_is_directory=True)
        assert not audit(source)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=Path, default=Path(__file__).resolve().parents[1])
    p.add_argument('--workspace', action='store_true', help='also audit this local parent workspace')
    p.add_argument('--self-test', action='store_true')
    a = p.parse_args()
    if a.self_test:
        self_test()
    errors = audit(a.source.resolve(), a.workspace)
    if errors:
        raise SystemExit('\n'.join(errors))
    print('Repository hygiene passed' + ('; parent workspace passed' if a.workspace else ''))


if __name__ == '__main__':
    main()
