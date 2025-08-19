def build_and_run [] {
  xmake b | ignore
  xmake run
}

def run_bench_llvm [str :string, ...flags] {
    $env.PATH = $env.Path
    # print $env.PATH
    rm -rf .xmake build
    print $str
    xmake f --toolchain=llvm --runtimes=c++_shared ...$flags | ignore
    build_and_run
}

def run_bench_msvc [str :string, flags, ...flags] {
    $env.PATH = $env.Path
    rm -rf .xmake build
    print $str
    xmake f ...$flags | ignore
    build_and_run
}

run_bench_llvm "LLVM debug (-O0)" "-m" debug
run_bench_llvm "LLVM release (-O1)" "-m" release "--level=fast"
run_bench_llvm "LLVM release (-O2)" "-m" release "--level=faster"
run_bench_llvm "LLVM release (-O3)" "-m" release "--level=fastest"
run_bench_llvm "LLVM release (-Ofast)" "-m" release "--level=aggressive"

run_bench_msvc "msvc debug (/Od)" "-m" debug
run_bench_msvc "msvc release (default)" "-m" release "--level=fast"
run_bench_msvc "msvc release (/O2)" "-m" release "--level=faster"
run_bench_msvc "msvc release (/Ox /fp:fast)" "-m" release "--level=fastest"
