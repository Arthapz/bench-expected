#include <cassert>
#include <coroutine>
#include <expected>
#include <nanobench.h>
#include <ranges>

#if (defined(__clang__) or defined(__GNUC__))
    #define Try_EXT(m)                                  \
        ({                                              \
            auto res = (m);                             \
            if (!res.has_value()) [[unlikely]]          \
                return std::unexpected { res.error() }; \
            std::move(res).value();                     \
        })
    #define Ok_EXT(x) return x
#endif

namespace stdr = std::ranges;
namespace nb   = ankerl::nanobench;

template<typename VAL, typename ERR>
class Expected: public std::expected<VAL, ERR> {
  public:
    using std::expected<VAL, ERR>::expected;

    struct promise_type {
        constexpr auto initial_suspend() const noexcept -> std::suspend_never { return {}; }

        constexpr auto final_suspend() const noexcept -> std::suspend_never { return {}; }

        constexpr auto get_return_object() noexcept -> Expected { return Expected { this }; }

        template<typename... Args>
        constexpr auto return_value(Args&&... args) noexcept {
            assert(expected_ptr != nullptr);
            *expected_ptr = Expected { std::in_place, std::forward<Args>(args)... };
        }

        constexpr auto return_value(ERR error) noexcept {
            assert(expected_ptr != nullptr);
            *expected_ptr = Expected {
                std::unexpected<ERR> { std::in_place, std::move(error) }
            };
        }

        [[noreturn]]
        auto unhandled_exception() const noexcept {
            std::abort();
        }

        Expected* expected_ptr = nullptr;
    };

    constexpr Expected(promise_type* promise) noexcept : Expected {} {
        promise->expected_ptr = this;
    }

    constexpr auto await_ready() const noexcept -> bool { return this->has_value(); }

    constexpr auto await_resume() noexcept -> VAL { return std::move(*this).value(); }

    template<typename Promise>
    constexpr auto await_suspend(std::coroutine_handle<Promise> handle) noexcept -> bool {
        handle.promise().return_value(this->error());
        handle.destroy();
        return true;
    }
};

#define Try(x) co_await x
#define Ok(x)  co_return x

enum class Error {
    A,
    B,
    C,
};

[[gnu::noinline, msvc::noinline]]
auto foo(int val) -> Expected<int, Error> {
    if (val % 2 == 0) return val;
    return std::unexpected { Error::A };
}

[[gnu::noinline, msvc::noinline]]
auto bar(int val) -> Expected<int, Error> {
    if (val % 5) return val + 1;
    return std::unexpected { Error::B };
}

[[gnu::noinline, msvc::noinline]]
auto foobar(int val) -> Expected<int, Error> {
    if (val % 10 == 0) return val + 2;
    return std::unexpected { Error::C };
}

#if (defined(__clang__) or defined(__GNUC__))
[[gnu::noinline, msvc::noinline]]
auto use_try_ext(int val) -> Expected<void, Error> {
    auto f  = Try_EXT(foo(val));
    auto b  = Try_EXT(bar(f));
    auto fb = Try_EXT(foobar(b));

    nb::doNotOptimizeAway(fb);

    Ok_EXT({});
}
#endif

[[gnu::noinline, msvc::noinline]]
auto use_try(int val) -> Expected<void, Error> {
    auto f  = Try(foo(val));
    auto b  = Try(bar(f));
    auto fb = Try(foobar(b));

    nb::doNotOptimizeAway(fb);

    Ok({});
}

[[gnu::noinline, msvc::noinline]]
auto foo2(int val) -> std::expected<int, Error> {
    if (val % 2 == 0) return val;
    return std::unexpected { Error::A };
}

[[gnu::noinline, msvc::noinline]]
auto bar2(int val) -> std::expected<int, Error> {
    if (val % 5) return val + 1;
    return std::unexpected { Error::B };
}

[[gnu::noinline, msvc::noinline]]
auto foobar2(int val) -> std::expected<int, Error> {
    if (val % 10 == 0) return val + 2;
    return std::unexpected { Error::C };
}

[[gnu::noinline, msvc::noinline]]
auto use_monadic(int val) -> std::expected<void, Error> {
    return foo2(val).and_then(bar2).and_then(foobar2).transform([](auto val) noexcept -> void {
        nb::doNotOptimizeAway(val);
    });
}

[[gnu::noinline, msvc::noinline]]
auto use_ifs(int val) -> Expected<void, Error> {
    auto foo_ret = foo(val);
    if (not foo_ret) return std::unexpected { std::move(foo_ret).error() };

    auto bar_ret = bar(*foo_ret);
    if (not bar_ret) return std::unexpected { std::move(bar_ret).error() };

    auto foobar_ret = foobar(*bar_ret);
    if (not foobar_ret) return std::unexpected { std::move(foobar_ret).error() };

    nb::doNotOptimizeAway(*foobar_ret);

    return {};
}

auto run_benchs(std::string_view title, int value) {
    constexpr auto warmup   = 100;
    constexpr auto relative = true;
    constexpr auto counters = true;
    constexpr auto epoch    = 10'000'000;

    auto bench = nb::Bench {};
    bench.title(stdr::data(title))
      .warmup(warmup)
      .relative(relative)
      .minEpochIterations(epoch)
      .performanceCounters(counters);

    bench.run("ifs", [value] noexcept {
        auto ret = use_ifs(value);
        nb::doNotOptimizeAway(ret);
    });
    bench.run("monadic", [value] noexcept {
        auto ret = use_monadic(value);
        nb::doNotOptimizeAway(ret);
    });
#if (defined(__clang__) or defined(__GNUC__))
    bench.run("try_ext", [value] noexcept {
        auto ret = use_try_ext(value);
        nb::doNotOptimizeAway(ret);
    });
#endif
    bench.run("try (coroutines)", [value] noexcept {
        auto ret = use_try(value);
        nb::doNotOptimizeAway(ret);
    });
}

auto main() -> int {
    run_benchs("Happy path", 10);
    run_benchs("Error path", 5);

    return 0;
}
