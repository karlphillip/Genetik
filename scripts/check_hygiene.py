#!/usr/bin/env python3
"""Audit output by location/role, without following symlinks or deleting anything."""
import argparse
import os
from pathlib import Path
import tempfile

ROOT_FILES = {'AGENTS.md', 'CHANGELOG.md', 'CMakeLists.txt', 'LICENSE', 'README.md', '.gitignore', 'pyproject.toml'}
ROOT_DIRS = {'.git', '.github', 'apps', 'benchmarks', 'cmake', 'docs', 'examples',
             'genetik', 'scripts', 'tests', 'out', 'python', 'integrations'}
WORKSPACE = {'Genetik', '.tools', 'delivery-tools', 'archive', 'docs', 'examples', 'session.md'}
OUTPUT_DIRS = {'build', 'install', 'consumers', 'runs', 'data', 'logs', 'tmp', 'cleanup'}
# Retained D7 delivery assets/evidence predate the current role layout. They are
# explicitly preserved, never used for new transient outputs, and not scanned.
PRESERVED_OUTPUT_DIRS = {'release', 'source-archive-check'}


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
    # Evidence/fixtures may use txt/csv/json; generated caches and binaries do not
    # belong in any source role. out/ and .git/ are opaque, not recursively scanned.
    for name in ROOT_DIRS - {'.git', 'out'}:
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
        (source / 'python').mkdir()
        (source / 'integrations').mkdir()
        (source / 'pyproject.toml').write_text('[build-system]\n')
        (source / 'out').mkdir()
        (source / 'docs' / 'accepted.csv').write_text('preserved evidence\n')
        (source / 'out' / 'build').mkdir()
        (source / 'out' / 'build' / 'CMakeCache.txt').write_text('allowed output\n')
        (source / 'out' / 'release').mkdir()
        (source / 'out' / 'source-archive-check').mkdir()
        (Path(tmp) / '.tools').mkdir()
        (Path(tmp) / 'delivery-tools').mkdir()
        (Path(tmp) / 'archive').mkdir()
        (Path(tmp) / 'docs').mkdir()
        (Path(tmp) / 'examples').mkdir()
        (Path(tmp) / 'session.md').write_text('workspace note\n')
        assert not audit(source, True)
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
