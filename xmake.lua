add_requires("nanobench")

option("level", { default = "fast" })

if is_mode("release") then
    set_optimize(get_config("level"))
else
    set_optimize("none")
    set_symbols("debug")
end

target("benchmark")
set_languages("c++latest")
add_files("src/*.cpp")

add_packages("nanobench")
