#ifndef OSMIUM_UTIL_MISC_HPP
#define OSMIUM_UTIL_MISC_HPP

/*

This file is part of Osmium (http://osmcode.org/libosmium).

Copyright 2013-2018 Jochen Topf <jochen@topf.org> and others (see README).

Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

*/

#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <tuple>
#include <type_traits>

namespace osmium {

    /**
     * Like std::tie(), but takes its arguments as const references. Used
     * as a helper function when sorting.
     */
    template <typename... Ts>
    inline std::tuple<const Ts&...>
    const_tie(const Ts&... args) noexcept {
        return std::tuple<const Ts&...>(args...);
    }

    namespace detail {

        template <typename T>
        inline long long get_max_int() noexcept {
            return static_cast<long long>(std::numeric_limits<T>::max());
        }

        template <>
        inline long long get_max_int<uint64_t>() noexcept {
            return std::numeric_limits<long long>::max();
        }

        /**
         * Interpret the input string as number. Leading white space is
         * ignored. If there is any error, return 0.
         *
         * @tparam TReturn The return type.
         * @param str The input string.
         *
         * @pre @code str != nullptr @endcode
         *
         */
        template <typename TReturn>
        inline TReturn str_to_int(const char* str) {
            assert(str);
            errno = 0;
            char* end;
            const auto value = std::strtoll(str, &end, 10);
            if (errno != 0 || value < 0 || value >= get_max_int<TReturn>() || *end != '\0') {
                return 0;
            }

            return static_cast<TReturn>(value);
        }

    } // namespace detail

} // namespace osmium

#endif // OSMIUM_UTIL_MISC_HPP
