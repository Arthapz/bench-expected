#include <cassert>
#include <coroutine>
#include <expected>
#include <nanobench.h>

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

template<typename VAL, typename ERR>
class Expected: public std::expected<VAL, ERR> {
  public:
    using std::expected<VAL, ERR>::expected;

    struct promise_type {
        constexpr auto initial_suspend() const noexcept -> std::suspend_never { return {}; }

        constexpr auto final_suspend() const noexcept -> std::suspend_never { return {}; }

        constexpr auto get_return_object() noexcept -> Expected { return Expected { this }; }

        template<typename... Args>
        constexpr void return_value(Args&&... args) noexcept {
            assert(expected_ptr != nullptr);
            *expected_ptr = Expected { std::in_place, std::forward<Args>(args)... };
        }

        constexpr void return_value(ERR error) noexcept {
            assert(expected_ptr != nullptr);
            *expected_ptr = Expected {
                std::unexpected<ERR> { std::in_place, std::move(error) }
            };
        }

        [[noreturn]]
        void unhandled_exception() const noexcept {
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

    // std::println("{}", fb);

    Ok_EXT({});
}
#endif

[[gnu::noinline, msvc::noinline]]
auto use_try(int val) -> Expected<void, Error> {
    auto f  = Try(foo(val));
    auto b  = Try(bar(f));
    auto fb = Try(foobar(b));

    // std::println("{}", fb);

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
    return foo2(val).and_then(bar2).and_then(foobar2).transform([](auto val) noexcept -> void {});
}

[[gnu::noinline, msvc::noinline]]
auto use_ifs(int val) -> Expected<void, Error> {
    auto foo_ret = foo(val);
    if (not foo_ret) return std::unexpected { std::move(foo_ret).error() };

    auto bar_ret = bar(*foo_ret);
    if (not bar_ret) return std::unexpected { std::move(bar_ret).error() };

    auto foobar_ret = foobar(*bar_ret);
    if (not foobar_ret) return std::unexpected { std::move(foobar_ret).error() };

    return {};
}

auto main() -> int {
    namespace nb = ankerl::nanobench;

    auto bench = nb::Bench {};
    bench.title("Expected error handling methods").warmup(100).relative(true);

    bench.minEpochIterations(3'000'000);

    bench.performanceCounters(true);

    constexpr auto happy_value = 10;
    constexpr auto error_value = 5;

    bench.run("ifs happy path", []() noexcept {
        auto ret = use_ifs(happy_value);
        nb::doNotOptimizeAway(ret);
    });
    bench.run("ifs error path", []() noexcept {
        auto ret = use_ifs(error_value);
        nb::doNotOptimizeAway(ret);
    });

    bench.run("monadic happy path", []() noexcept {
        auto ret = use_monadic(happy_value);
        nb::doNotOptimizeAway(ret);
    });
    bench.run("monadic error path", []() noexcept {
        auto ret = use_monadic(error_value);
        nb::doNotOptimizeAway(ret);
    });

#if (defined(__clang__) or defined(__GNUC__))
    bench.run("try_ext happy path", []() noexcept {
        auto ret = use_try_ext(happy_value);
        nb::doNotOptimizeAway(ret);
    });
    bench.run("try_ext error path", []() noexcept {
        auto ret = use_try_ext(error_value);
        nb::doNotOptimizeAway(ret);
    });
#endif

    bench.run("try happy path", []() noexcept {
        auto ret = use_try(happy_value);
        nb::doNotOptimizeAway(ret);
    });
    bench.run("try error path", []() noexcept {
        auto ret = use_try(error_value);
        nb::doNotOptimizeAway(ret);
    });
    return 0;
}
