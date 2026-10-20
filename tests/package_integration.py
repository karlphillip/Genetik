#!/usr/bin/env python3
"""Disposable producer, ordinary install, relocation, removed source/build, fresh consumers.
Only test tooling uses Python; installed Genetik needs C++20 and CMake, no Python.
"""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
p=argparse.ArgumentParser()
p.add_argument('--source',type=Path,required=True)
p.add_argument('--workspace',type=Path,required=True)
p.add_argument('--cmake',required=True)
p.add_argument('--cxx',required=True)
p.add_argument('--cc',default=os.environ.get('CC','cc'))
a=p.parse_args()
src=a.source.resolve();root=a.workspace.resolve()
# Never remove the checkout. Own a recognizable isolated subtree, including spaces.
if root.name!='genetik-delivery-fixture' or root==src or root in src.parents:
    raise RuntimeError('unsafe disposable workspace')
if root.exists():shutil.rmtree(root)
root.mkdir(parents=True)
log=(root/'commands.log').open('w')
env=os.environ.copy()
for key in ['Genetik_DIR','Genetik_ROOT','CMAKE_PREFIX_PATH','CXXFLAGS','CFLAGS','LDFLAGS']:
    env.pop(key,None)
def run(args,fail=False):
    log.write(shlex.join(map(str,args))+'\n');log.flush()
    r=subprocess.run(list(map(str,args)),env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=120)
    log.write(r.stdout);log.flush()
    if (r.returncode==0)==fail:
        raise RuntimeError(f'Unexpected exit {r.returncode}: {args}\n{r.stdout}')
    return r.stdout
producer=root/'producer source';build=root/'producer build';prefix_a=root/'prefix A';prefix_b=root/'relocated prefix B'
producer.mkdir()
# Copy the current intended library/CMake code, not Git HEAD or installed artifacts.
for name in ['CMakeLists.txt','genetik','cmake','LICENSE']:
    source=src/name
    if source.is_dir():shutil.copytree(source,producer/name)
    else:shutil.copy2(source,producer/name)
base=[f'-DCMAKE_CXX_COMPILER={a.cxx}',f'-DCMAKE_C_COMPILER={a.cc}',
      '-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF','-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF',
      '-DCMAKE_FIND_PACKAGE_NO_PACKAGE_REGISTRY=ON']
run([a.cmake,'-S',producer,'-B',build,*base,f'-DCMAKE_INSTALL_PREFIX={prefix_a}',
     '-DCMAKE_BUILD_TYPE=Release','-DBUILD_TESTING=OFF','-DGENETIK_BUILD_TESTS=OFF',
     '-DGENETIK_BUILD_EXAMPLES=OFF','-DGENETIK_INSTALL=ON','-DGENETIK_SANITIZERS=ON','-DGENETIK_WARNINGS_AS_ERRORS=ON'])
run([a.cmake,'--build',build,'-j2']);run([a.cmake,'--install',build])
consumer=root/'external consumer';shutil.copytree(src/'tests/installed',consumer)
shutil.copytree(src/'quickstarts',consumer/'quickstarts')
def inspect(prefix,consumer_build):
    configs=list(prefix.rglob('GenetikConfig.cmake'))
    if len(configs)!=1:raise RuntimeError('missing/ambiguous installed config')
    resolved=(consumer_build/'resolved.txt').read_text().splitlines()[0]
    if Path(resolved).resolve()!=configs[0].parent.resolve():raise RuntimeError('foreign package resolution')
    for f in prefix.rglob('*.cmake'):
        text=f.read_text()
        for banned in [str(producer),str(build),str(prefix_a) if prefix==prefix_b else 'UNLIKELY_SENTINEL',
                       str(src/'genetik'),'-fsanitize','-Werror','--coverage','-fprofile','-pg']:
            if banned in text:raise RuntimeError(f'producer/development metadata leak: {banned} in {f}')
    commands=json.loads((consumer_build/'compile_commands.json').read_text())
    for c in commands:
        tokens=shlex.split(c['command'])
        if any(x in c['command'] for x in ['-fsanitize','-Werror','--coverage','-fprofile',' -pg ']):
            raise RuntimeError('development options leaked')
        is_c=Path(c['file']).suffix=='.c'
        for required in ['-fno-fast-math','-ffp-contract=off']:
            if (required in tokens)==is_c:raise RuntimeError('C++ strict FP requirements missing or leaked into C')
        if str(prefix/'include/Genetik') not in tokens:raise RuntimeError('installed include root missing')
        if str(producer) in c['command'] or str(build) in c['command'] or str(src/'genetik') in c['command']:
            raise RuntimeError('source/build include leak')
        # No implicit source-tree dependencies: all explicit include roots belong to prefix.
        for i,token in enumerate(tokens):
            path=None
            if token in ['-I','-isystem']:path=tokens[i+1]
            elif token.startswith('-I') and len(token)>2:path=token[2:]
            if path and not Path(path).resolve().is_relative_to(prefix.resolve()):raise RuntimeError('foreign include root')
    installed={p.relative_to(prefix).as_posix() for p in prefix.rglob('*') if p.is_file()}
    if not any(x.endswith('detail/ScopedTiming.hpp') for x in installed):raise RuntimeError('transitive header omitted')
    if not any(x.endswith('licenses/Genetik/LICENSE') for x in installed):raise RuntimeError('license omitted')
    if any(not (x.endswith('.hpp') or '/cmake/Genetik/' in x or x.endswith('/LICENSE')) for x in installed):
        raise RuntimeError('unexpected development artifacts installed')
    (root/f'{consumer_build.name}-files.txt').write_text('\n'.join(sorted(installed))+'\n')
def consume(prefix,name,extra=(),build_it=True,fail=False):
    directory=root/name
    output=run([a.cmake,'-S',consumer,'-B',directory,*base,f'-DEXPECTED_PREFIX={prefix}',
                '-DCMAKE_BUILD_TYPE=Release',*extra],fail)
    if fail:return output
    if build_it:
        run([a.cmake,'--build',directory,'-j2'])
        for exe in ['installed_consumer','quickstarts/scalar_quickstart','quickstarts/symbolic_quickstart']:
            run([directory/exe])
            dependencies=run(['ldd',directory/exe])
            if any(x in dependencies for x in ['libasan','libubsan','libclang_rt']):
                raise RuntimeError('development runtime dependency leaked')
        inspect(prefix,directory)
    return directory
consume(prefix_a,'ordinary consumer build')
shutil.move(prefix_a,prefix_b)
shutil.rmtree(producer);shutil.rmtree(build)
assert not producer.exists() and not build.exists() and src.is_dir()
consume(prefix_b,'relocated consumer build')
no_version=consume(prefix_b,'no version',('-DREQUEST_NO_VERSION=ON',),False)
if (no_version/'resolved.txt').read_text().splitlines()[1]!='0.1.0':
    raise RuntimeError('package version metadata missing without version request')
consume(prefix_b,'exact version',('-DREQUEST_VERSION=0.1.0','-DREQUEST_EXACT=ON'),False)
for version in ['0.0.9','0.2.0','0.1.1','1.0']:
    consume(prefix_b,'rejected '+version,(f'-DREQUEST_VERSION={version}',),False,True)
message=consume(prefix_b,'required component',('-DREQUEST_COMPONENT=imaginary',),False,True)
if 'no package components' not in message:raise RuntimeError('unclear component diagnostic')
# CMake 3.16 predates cxx_std_23; newer consuming CMake may select newer standards.
version=run([a.cmake,'--version']).splitlines()[0].split()[-1]
if tuple(map(int,version.split('.')[:2]))>=(3,20):
    consume(prefix_b,'CXX23 consumer',('-DCMAKE_CXX_STANDARD=23',))
embed=root/'parent build'
run([a.cmake,'-S',src/'tests/embedding','-B',embed,*base,f'-DGENETIK_SOURCE={src}',
     '-DGENETIK_SANITIZERS=ON','-DGENETIK_WARNINGS_AS_ERRORS=ON'])
run([a.cmake,'--build',embed,'-j2']);run([embed/'parent_app']);run([embed/'unrelated'])
commands=json.loads((root/'relocated consumer build/compile_commands.json').read_text())
# Source inclusion must not create development options or modify parent BUILD_TESTING.
cache=(embed/'CMakeCache.txt').read_text()
if 'GENETIK_BUILD_EXAMPLES:BOOL=OFF' not in cache or 'GENETIK_BUILD_TESTS:BOOL=OFF' not in cache:
    raise RuntimeError('embedding defaults enabled development extras')
log.close()
print('ordinary + relocated multi-TU consumers, installed quickstarts, metadata/flags, versions/components, C++23 (when available), and source inclusion passed')
print(f'evidence={root}')
