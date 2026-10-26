"""Isolated optional numerical consumer; no runtime network or producer include roots."""
import argparse,os,shutil,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);p.add_argument('--workspace',type=Path,required=True);p.add_argument('--cmake',required=True)
a=p.parse_args();root=a.workspace.resolve();src=a.source.resolve()
if root.name!='d6-optional-fixture' or root==src or root in src.parents:raise RuntimeError('unsafe fixture root')
if root.exists():shutil.rmtree(root)
root.mkdir(parents=True);log=(root/'commands.log').open('w');env=os.environ.copy();env['OPENBLAS_NUM_THREADS']='1';env['OMP_NUM_THREADS']='1'
def run(args):
    log.write(' '.join(map(str,args))+'\n');log.flush()
    result=subprocess.run(list(map(str,args)),env=env,stdout=log,stderr=subprocess.STDOUT,timeout=300)
    log.flush()
    if result.returncode:raise RuntimeError(f'optional package command failed: {args}; see {log.name}')
producer=root/'source';producer.mkdir()
for name in ['CMakeLists.txt','genetik','cmake','LICENSE']:
    if (src/name).is_dir():shutil.copytree(src/name,producer/name)
    else:shutil.copyfile(src/name,producer/name)
prefix=root/'prefix';build=root/'producer'
run([a.cmake,'-S',producer,'-B',build,'-DGENETIK_BUILD_TESTS=OFF','-DGENETIK_BUILD_EXAMPLES=OFF','-DBUILD_TESTING=OFF','-DGENETIK_INSTALL=ON',f'-DCMAKE_INSTALL_PREFIX={prefix}'])
run([a.cmake,'--install',build]);relocated=root/'relocated';shutil.move(prefix,relocated);shutil.rmtree(producer);shutil.rmtree(build)
consumer=root/'consumer';consumer.mkdir()
(consumer/'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.16)
project(D6Consumer LANGUAGES CXX)
find_package(Genetik 0.5 CONFIG REQUIRED COMPONENTS ScientificNumerics)
add_executable(consumer main.cpp other.cpp)
target_link_libraries(consumer PRIVATE Genetik::ScientificNumerics)
get_target_property(base Genetik::Genetik INTERFACE_LINK_LIBRARIES)
if(NOT base STREQUAL "Threads::Threads")
message(FATAL_ERROR "optional dependency leaked into base target")
endif()
''')
(consumer/'main.cpp').write_text('''#include <AdvancedLinearAlgebra.hpp>
extern int other();
int main(){auto a=TypedValue::matrix(2,2,std::array{2.,1.,1.,2.});auto r=symmetricEigen(a);if(!r.success()||std::abs(r.value->eigenvalues.at(1)-3)>1e-12)return 1;return other();}
''')
(consumer/'other.cpp').write_text('''#include <ScientificRegression.hpp>
#include <ExpressionSecondDerivative.hpp>
int other(){auto a=TypedValue::vector(std::array{3.,4.});return a.at(1)==4?0:1;}
'''.replace('#include <ScientificRegression.hpp>','#include <ScientificRegression.hpp>\n#include <TypedValue.hpp>'))
run([a.cmake,'-S',consumer,'-B',root/'consumer-build',f'-DCMAKE_PREFIX_PATH={relocated}','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON','-DCMAKE_BUILD_TYPE=Release'])
run([a.cmake,'--build',root/'consumer-build','-j2']);run([root/'consumer-build/consumer'])
print('D6 relocated optional LAPACK and two-TU installed consumer passed')
