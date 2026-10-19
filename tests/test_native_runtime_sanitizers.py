#!/usr/bin/env python3
"""Exercise the embedded native value runtime with ASan/UBSan on GCC/Clang."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
header = (ROOT / 'include/nextviper/native_runtime.hpp').read_text()
runtime = header.split('R"NVCPP(', 1)[1].split(')NVCPP"', 1)[0]
checks = r'''
#include <cassert>
int main() {
    NV object=nv_builtin("object_new",{});
    nv_builtin("set_prop",{object,nv_s("count"),nv_i(100000)});
    nv_builtin("set_prop",{object,nv_s("negative"),nv_i(-200000)});
    assert(nv_text(object)=="{\"count\": 100000, \"negative\": -200000}");
    NV array=nv_builtin("array_new",{});
    nv_builtin("push",{array,nv_s("hello")});
    nv_builtin("push",{array,nv_f(1.75)});
    nv_builtin("push",{array,array});
    assert(nv_text(array)=="[\"hello\", 1.75, <cycle>]");
    NV twice=nv_function([](const NVArgs& args){return nv_binary("mul",args.at(0),nv_i(2));},1,"twice");
    assert(nv_num(nv_call(twice,{nv_f(1.75)}))==3.5);
    bool rejected=false;
    try { nv_builtin("get",{array,nv_i(99)}); } catch(const std::runtime_error&) {rejected=true;}
    assert(rejected);
    rejected=false;
    try { nv_binary("add",nv_i(INT64_MAX),nv_i(1)); } catch(const std::runtime_error&) {rejected=true;}
    assert(rejected);
    // Break the deliberately created cycle to avoid leaving a reference cycle in this isolated check.
    array.a->pop_back();
    std::cout << "native runtime sanitizer checks passed\n";
}
'''
with tempfile.TemporaryDirectory(prefix='nv-sanitize-') as directory:
    source = Path(directory) / 'runtime.cpp'
    binary = Path(directory) / 'runtime'
    source.write_text(runtime + checks)
    subprocess.run([os.environ.get('CXX', 'g++'), '-std=c++20', '-O1', '-g',
                    '-fsanitize=' + os.environ.get('NV_SANITIZERS', 'address,undefined'), '-fno-omit-frame-pointer', str(source),
                    '-o', str(binary), '-pthread'], check=True, timeout=120)
    environment = dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1')
    # LeakSanitizer may be unavailable under tracing/container environments. The
    # language's broader reference-cycle limitations are documented separately.
    subprocess.run([str(binary)], check=True, timeout=30, env=environment)
