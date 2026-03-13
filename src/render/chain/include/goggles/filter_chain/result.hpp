#pragma once

#include "error.hpp"

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <nonstd/expected.hpp>
#include <utility>

namespace goggles {

template <typename T>
using Result = nonstd::expected<T, Error>;

template <typename T>
using ResultPtr = Result<std::unique_ptr<T>>;

template <typename T>
[[nodiscard]] inline auto make_error(ErrorCode code, std::string message,
                                     std::source_location loc = std::source_location::current())
    -> Result<T> {
    return nonstd::make_unexpected(Error{code, std::move(message), loc});
}

template <typename T>
[[nodiscard]] inline auto make_result_ptr(std::unique_ptr<T> ptr) -> ResultPtr<T> {
    return ResultPtr<T>{std::move(ptr)};
}

template <typename T>
[[nodiscard]] inline auto
make_result_ptr_error(ErrorCode code, std::string message,
                      std::source_location loc = std::source_location::current()) -> ResultPtr<T> {
    return nonstd::make_unexpected(Error{code, std::move(message), loc});
}

} // namespace goggles

// NOLINTBEGIN(cppcoreguidelines-macro-usage)

#define GOGGLES_TRY(expr)                                                                          \
    ({                                                                                             \
        auto _try_result = (expr);                                                                 \
        if (!_try_result)                                                                          \
            return nonstd::make_unexpected(_try_result.error());                                   \
        std::move(_try_result).value();                                                            \
    })

#define GOGGLES_MUST(expr)                                                                         \
    ({                                                                                             \
        auto _must_result = (expr);                                                                \
        if (!_must_result) {                                                                       \
            auto& _err = _must_result.error();                                                     \
            std::fprintf(stderr, "GOGGLES_MUST failed at %s:%u in %s\n  %s: %s\n",                 \
                         _err.location.file_name(), _err.location.line(),                          \
                         _err.location.function_name(), goggles::error_code_name(_err.code),       \
                         _err.message.c_str());                                                    \
            std::abort();                                                                          \
        }                                                                                          \
        std::move(_must_result).value();                                                           \
    })

#define GOGGLES_ASSERT(condition, ...)                                                             \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            std::fprintf(stderr, "GOGGLES_ASSERT failed: %s at %s:%u in %s\n", #condition,         \
                         __FILE__, __LINE__, __func__);                                            \
            __VA_OPT__(do {                                                                        \
                std::fprintf(stderr, "  ");                                                        \
                std::fprintf(stderr, __VA_ARGS__);                                                 \
                std::fprintf(stderr, "\n");                                                        \
            } while (false);)                                                                      \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)

// NOLINTEND(cppcoreguidelines-macro-usage)
