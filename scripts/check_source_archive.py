#!/usr/bin/env python3
"""Inspect a Git source archive and reuse the isolated installed-consumer gate."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import sys
import tarfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive', required=True, type=Path)
    parser.add_argument('--commit', required=True, help='exact 40-character source commit')
    parser.add_argument('--workspace', required=True, type=Path, help='new disposable directory')
    parser.add_argument('--cmake', default='cmake')
    parser.add_argument('--cxx', default='c++')
    args = parser.parse_args()
    if not re.fullmatch('[0-9a-f]{40}', args.commit):
        parser.error('--commit must be a full Git SHA-1')
    archive = args.archive.resolve(strict=True)
    workspace = args.workspace.resolve()
    if workspace.exists():
        parser.error('--workspace must not already exist; no existing tree is deleted')
    cmake = shutil.which(args.cmake)
    cxx = shutil.which(args.cxx)
    if not cmake or not cxx:
        parser.error('selected CMake and C++ compiler must be available')
    # Derive identity from the committed authority, never the current checkout.
    project = subprocess.check_output(['git', 'show', args.commit+':CMakeLists.txt'], text=True)
    version = re.search(r'project\(GENETIK VERSION ([0-9]+\.[0-9]+\.[0-9]+)', project).group(1)
    prefix = 'Genetik-'+version
    tracked = subprocess.check_output(['git', 'ls-tree', '-r', '-z', args.commit]).split(b'\0')
    blobs = {}
    for entry in tracked:
        if not entry:
            continue
        metadata, name = entry.split(b'\t', 1)
        mode, kind, identity = metadata.split()
        if kind != b'blob' or mode not in (b'100644', b'100755'):
            raise ValueError('unsupported tracked distribution entry')
        blobs[name.decode()] = (identity.decode(), int(mode, 8) & 0o777)
    with tarfile.open(archive, 'r:gz') as source:
        if source.pax_headers.get('comment') != args.commit:
            raise ValueError('archive commit identity differs from --commit')
        members = source.getmembers()
        paths = set()
        for member in members:
            path = PurePosixPath(member.name)
            if (path.is_absolute() or '..' in path.parts or not path.parts or
                    path.parts[0] != prefix or '.git' in path.parts or
                    not (member.isfile() or member.isdir())):
                raise ValueError(f'unsupported source archive member: {member.name}')
            if path in paths:
                raise ValueError(f'duplicate archive path: {member.name}')
            paths.add(path)
            # This package has no generated inputs, submodules or source symlinks.
            if len(path.parts) > 1 and (path.parts[1] in {'out', 'build'} or
                                       path.parts[1].startswith('build-')):
                raise ValueError(f'generated source input: {member.name}')
        required = ['LICENSE', 'CMakeLists.txt', 'cmake/GenetikConfig.cmake.in',
                    'cmake/GenetikFloatingPoint.cmake', 'genetik/CMakeLists.txt',
                    'genetik/Execution.hpp', 'genetik/Creature_i.hpp',
                    'genetik/EvaluatedCandidate.hpp', 'genetik/SymbolicRegression.hpp',
                    'genetik/detail/ScopedTiming.hpp', 'examples/quickstarts/CMakeLists.txt',
                    'examples/quickstarts/scalar.cpp', 'examples/quickstarts/symbolic.cpp',
                    'tests/oop_consumer/main.cpp', 'tests/package_integration.py']
        if any(PurePosixPath(prefix, name) not in paths for name in required):
            raise ValueError('source archive omits a required distribution input')
        files = {str(path.relative_to(prefix)) for path in paths
                 if source.getmember(str(path)).isfile()}
        if files != set(blobs):
            raise ValueError('archive file manifest differs from committed tree')
        workspace.mkdir(parents=True)
        manifest = []
        for member in members:
            destination = workspace / member.name
            if member.isdir():
                destination.mkdir(parents=True, exist_ok=True)
                continue
            destination.parent.mkdir(parents=True, exist_ok=True)
            data = source.extractfile(member).read()
            relative = str(PurePosixPath(member.name).relative_to(prefix))
            identity, mode = blobs[relative]
            blob = hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()
            if blob != identity or member.mode & 0o777 != mode:
                raise ValueError('archive differs from committed blob: '+relative)
            destination.write_bytes(data)
            destination.chmod(member.mode & 0o777)
            manifest.append({'path': member.name, 'bytes': len(data),
                             'sha256': hashlib.sha256(data).hexdigest()})
    (workspace/'source-manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
    extracted = workspace/prefix
    assert not (extracted/'.git').exists()
    command = [sys.executable, str(extracted/'tests/package_integration.py'),
               '--source', str(extracted), '--workspace', str(workspace/'genetik-delivery-fixture'),
               '--cmake', cmake, '--cxx', cxx]
    # All sources/consumers come from the archive. No active-checkout include paths.
    subprocess.run(command, check=True)
    result = {'source_commit': args.commit, 'archive': archive.name,
              'archive_sha256': hashlib.sha256(archive.read_bytes()).hexdigest(),
              'files': len(manifest), 'git_metadata_present': False,
              'cmake': cmake, 'cxx': cxx,
              'result': 'ordinary + relocated multi-TU, OOP, symbolic, quickstarts and source inclusion passed'}
    (workspace/'verification.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
