#ifndef OSMIUM_OSM_LOCATION_HPP
#define OSMIUM_OSM_LOCATION_HPP

/*

This file is part of Osmium (http://osmcode.org/libosmium).

Copyright 2013-2016 Jochen Topf <jochen@topf.org> and others (see README).

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

#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iosfwd>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>

namespace osmium {

    /**
     * Exception signaling an invalid location, ie a location
     * outside the -180 to 180 and -90 to 90 degree range.
     */
    struct invalid_location : public std::range_error {

        explicit invalid_location(const std::string& what) :
            std::range_error(what) {
        }

        explicit invalid_location(const char* what) :
            std::range_error(what) {
        }

    }; // struct invalid_location

    namespace detail {

        constexpr const int coordinate_precision = 10000000;

        // Convert string with a floating point number into integer suitable
        // for use as coordinate in a Location.
        inline int32_t string_to_location_coordinate(const char** data) {
            const char* str = *data;
            const char* full = str;

            int64_t result = 0;
            int sign = 1;

            // one more than significant digits to allow rounding
            int64_t scale = 8;

            // paranoia check for maximum number of digits
            int max_digits = 10;

            // optional minus sign
            if (*str == '-') {
                sign = -1;
                ++str;
            }

            if (*str != '.') {
                // there has to be at least one digit
                if (*str >= '0' && *str <= '9') {
                    result = *str - '0';
                    ++str;
                } else {
                    goto error;
                }

                // optional additional digits before decimal point
                while (*str >= '0' && *str <= '9' && max_digits > 0) {
                    result = result * 10 + (*str - '0');
                    ++str;
                    --max_digits;
                }

                if (max_digits == 0) {
                    goto error;
                }
            } else {
                // need at least one digit after decimal dot if there was no
                // digit before decimal dot
                if (*(str + 1) < '0' || *(str + 1) > '9') {
                    goto error;
                }
            }

            // optional decimal point
            if (*str == '.') {
                ++str;

                // read significant digits
                for (; scale > 0 && *str >= '0' && *str <= '9'; --scale, ++str) {
                    result = result * 10 + (*str - '0');
                }

                // ignore non-significant digits
                max_digits = 20;
                while (*str >= '0' && *str <= '9' && max_digits > 0) {
                    ++str;
                    --max_digits;
                }

                if (max_digits == 0) {
                    goto error;
                }
            }

            // optional exponent in scientific notation
            if (*str == 'e' || *str == 'E') {
                ++str;

                int esign = 1;
                // optional minus sign
                if (*str == '-') {
                    esign = -1;
                    ++str;
                }

                int64_t eresult = 0;

                // there has to be at least one digit in exponent
                if (*str >= '0' && *str <= '9') {
                    eresult = *str - '0';
                    ++str;
                } else {
                    goto error;
                }

                // optional additional digits in exponent
                max_digits = 5;
                while (*str >= '0' && *str <= '9' && max_digits > 0) {
                    eresult = eresult * 10 + (*str - '0');
                    ++str;
                    --max_digits;
                }

                if (max_digits == 0) {
                    goto error;
                }

                scale += eresult * esign;
            }

            if (scale < 0) {
                for (; scale < 0 && result > 0; ++scale) {
                    result /= 10;
                }
            } else {
                for (; scale > 0; --scale) {
                    result *= 10;
                }
            }

            result = (result + 5) / 10 * sign;

            if (result > std::numeric_limits<int32_t>::max() ||
                result < std::numeric_limits<int32_t>::min()) {
                goto error;
            }

            *data = str;
            return static_cast<int32_t>(result);

        error:

            throw invalid_location{std::string{"wrong format for coordinate: '"} + full + "'"};
        }

        // Convert integer as used by location for coordinates into a string.
        template <typename T>
        inline T append_location_coordinate_to_string(T iterator, int32_t value) {
            // handle negative values
            if (value < 0) {
                *iterator++ = '-';
                value = -value;
            }

            // write digits into temporary buffer
            int32_t v = value;
            char temp[10];
            char* t = temp;
            do {
                *t++ = char(v % 10) + '0';
                v /= 10;
            } while (v != 0);

            while (t-temp < 7) {
                *t++ = '0';
            }

            // write out digits before decimal point
            if (value >= coordinate_precision) {
                if (value >= 10 * coordinate_precision) {
                    if (value >= 100 * coordinate_precision) {
                        *iterator++ = *--t;
                    }
                    *iterator++ = *--t;
                }
                *iterator++ = *--t;
            } else {
                *iterator++ = '0';
            }

            // remove trailing zeros
            const char* tn = temp;
            while (tn < t && *tn == '0') {
                ++tn;
            }

            // decimal point
            if (t != tn) {
                *iterator++ = '.';
                while (t != tn) {
                    *iterator++ = *--t;
                }
            }

            return iterator;
        }

    } // namespace detail

    /**
     * Locations define a place on earth.
     *
     * Locations are stored in 32 bit integers for the x and y
     * coordinates, respectively. This gives you an accuracy of a few
     * centimeters, good enough for OSM use. (The main OSM database
     * uses the same scheme.)
     *
     * An undefined Location can be created by calling the constructor
     * without parameters.
     *
     * Coordinates are never checked on whether they are inside bounds.
     * Call valid() to check this.
     */
    class Location {

        int32_t m_x;
        int32_t m_y;

    public:

        // this value is used for a coordinate to mark it as undefined
        // MSVC doesn't declare std::numeric_limits<int32_t>::max() as
        // constexpr, so we hard code this for the time being.
        // static constexpr int32_t undefined_coordinate = std::numeric_limits<int32_t>::max();
        static constexpr int32_t undefined_coordinate = 2147483647;

        static int32_t double_to_fix(const double c) noexcept {
            return static_cast<int32_t>(std::round(c * detail::coordinate_precision));
        }

        static constexpr double fix_to_double(const int32_t c) noexcept {
            return static_cast<double>(c) / detail::coordinate_precision;
        }

        /**
         * Create undefined Location.
         */
        explicit constexpr Location() noexcept :
            m_x(undefined_coordinate),
            m_y(undefined_coordinate) {
        }

        /**
         * Create Location with given x and y coordinates.
         * Note that these coordinates are coordinate_precision
         * times larger than the real coordinates.
         */
        constexpr Location(const int32_t x, const int32_t y) noexcept :
            m_x(x),
            m_y(y) {
        }

        /**
         * Create Location with given x and y coordinates.
         * Note that these coordinates are coordinate_precision
         * times larger than the real coordinates.
         */
        constexpr Location(const int64_t x, const int64_t y) noexcept :
            m_x(static_cast<int32_t>(x)),
            m_y(static_cast<int32_t>(y)) {
        }

        /**
         * Create Location with given longitude and latitude.
         */
        Location(const double lon, const double lat) :
            m_x(double_to_fix(lon)),
            m_y(double_to_fix(lat)) {
        }

        Location(const Location&) = default;
        Location(Location&&) = default;
        Location& operator=(const Location&) = default;
        Location& operator=(Location&&) = default;
        ~Location() = default;

        /**
         * Check whether the coordinates of this location
         * are defined.
         */
        explicit constexpr operator bool() const noexcept {
            return m_x != undefined_coordinate && m_y != undefined_coordinate;
        }

        /**
         * Check whether the coordinates are inside the
         * usual bounds (-180<=lon<=180, -90<=lat<=90).
         */
        constexpr bool valid() const noexcept {
            return m_x >= -180 * detail::coordinate_precision
                && m_x <=  180 * detail::coordinate_precision
                && m_y >=  -90 * detail::coordinate_precision
                && m_y <=   90 * detail::coordinate_precision;
        }

        constexpr int32_t x() const noexcept {
            return m_x;
        }

        constexpr int32_t y() const noexcept {
            return m_y;
        }

        Location& set_x(const int32_t x) noexcept {
            m_x = x;
            return *this;
        }

        Location& set_y(const int32_t y) noexcept {
            m_y = y;
            return *this;
        }

        /**
         * Get longitude.
         *
         * @throws invalid_location if the location is invalid
         */
        double lon() const {
            if (!valid()) {
                throw osmium::invalid_location("invalid location");
            }
            return fix_to_double(m_x);
        }

        /**
         * Get longitude without checking the validity.
         */
        double lon_without_check() const {
            return fix_to_double(m_x);
        }

        /**
         * Get latitude.
         *
         * @throws invalid_location if the location is invalid
         */
        double lat() const {
            if (!valid()) {
                throw osmium::invalid_location("invalid location");
            }
            return fix_to_double(m_y);
        }

        /**
         * Get latitude without checking the validity.
         */
        double lat_without_check() const {
            return fix_to_double(m_y);
        }

        Location& set_lon(double lon) noexcept {
            m_x = double_to_fix(lon);
            return *this;
        }

        Location& set_lat(double lat) noexcept {
            m_y = double_to_fix(lat);
            return *this;
        }

        Location& set_lon(const char* str) {
            const char** data = &str;
            m_x = detail::string_to_location_coordinate(data);
            if (**data != '\0') {
                throw invalid_location{std::string{"characters after coordinate: '"} + *data + "'"};
            }
            return *this;
        }

        Location& set_lat(const char* str) {
            const char** data = &str;
            m_y = detail::string_to_location_coordinate(data);
            if (**data != '\0') {
                throw invalid_location{std::string{"characters after coordinate: '"} + *data + "'"};
            }
            return *this;
        }

        Location& set_lon_partial(const char** str) {
            m_x = detail::string_to_location_coordinate(str);
            return *this;
        }

        Location& set_lat_partial(const char** str) {
            m_y = detail::string_to_location_coordinate(str);
            return *this;
        }

        template <typename T>
        T as_string_without_check(T iterator, const char separator = ',') const {
            iterator = detail::append_location_coordinate_to_string(iterator, x());
            *iterator++ = separator;
            return detail::append_location_coordinate_to_string(iterator, y());
        }

        template <typename T>
        T as_string(T iterator, const char separator = ',') const {
            if (!valid()) {
                throw osmium::invalid_location("invalid location");
            }
            return as_string_without_check(iterator, separator);
        }

    }; // class Location

    /**
     * Locations are equal if both coordinates are equal.
     */
    inline constexpr bool operator==(const Location& lhs, const Location& rhs) noexcept {
        return lhs.x() == rhs.x() && lhs.y() == rhs.y();
    }

    inline constexpr bool operator!=(const Location& lhs, const Location& rhs) noexcept {
        return ! (lhs == rhs);
    }

    /**
     * Compare two locations by comparing first the x and then
     * the y coordinate. If either of the locations is
     * undefined the result is undefined.
     */
    inline constexpr bool operator<(const Location& lhs, const Location& rhs) noexcept {
        return (lhs.x() == rhs.x() && lhs.y() < rhs.y()) || lhs.x() < rhs.x();
    }

    inline constexpr bool operator>(const Location& lhs, const Location& rhs) noexcept {
        return rhs < lhs;
    }

    inline constexpr bool operator<=(const Location& lhs, const Location& rhs) noexcept {
        return ! (rhs < lhs);
    }

    inline constexpr bool operator>=(const Location& lhs, const Location& rhs) noexcept {
        return ! (lhs < rhs);
    }

    /**
     * Output a location to a stream.
     */
    template <typename TChar, typename TTraits>
    inline std::basic_ostream<TChar, TTraits>& operator<<(std::basic_ostream<TChar, TTraits>& out, const osmium::Location& location) {
        if (location) {
            out << '(';
            location.as_string(std::ostream_iterator<char>(out), ',');
            out << ')';
        } else {
            out << "(undefined,undefined)";
        }
        return out;
    }

    namespace detail {

        template <int N>
        inline size_t hash(const osmium::Location& location) noexcept {
            return location.x() ^ location.y();
        }

        template <>
        inline size_t hash<8>(const osmium::Location& location) noexcept {
            size_t h = location.x();
            h <<= 32;
            return h ^ location.y();
        }

    } // namespace detail

} // namespace osmium

namespace std {

// This pragma is a workaround for a bug in an old libc implementation
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmismatched-tags"
#endif
    template <>
    struct hash<osmium::Location> {
        using argument_type = osmium::Location;
        using result_type = size_t;
        size_t operator()(const osmium::Location& location) const noexcept {
            return osmium::detail::hash<sizeof(size_t)>(location);
        }
    };
#ifdef __clang__
#pragma clang diagnostic pop
#endif

} // namespace std

#endif // OSMIUM_OSM_LOCATION_HPP
