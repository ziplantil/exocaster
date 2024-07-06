/***
exocaster -- audio streaming helper
config.hh -- configuration object

MIT License 

Copyright (c) 2024 ziplantil

Permission is hereby granted, free of charge, to any person obtaining a 
copy of this software and associated documentation files (the "Software"), 
to deal in the Software without restriction, including without limitation 
the rights to use, copy, modify, merge, publish, distribute, sublicense, 
and/or sell copies of the Software, and to permit persons to whom the 
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in 
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS 
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
DEALINGS IN THE SOFTWARE.

***/

#ifndef CONFIG_HH
#define CONFIG_HH

#include <cstdint>
#include <concepts>
#include <limits>
#include <stdexcept>
#include <type_traits>

#include <nlohmann/json.hpp>

namespace exo {

using ConfigObject = nlohmann::json;

class InvalidConfigError : public std::logic_error {
    using std::logic_error::logic_error;
};

namespace cfg {

/** Returns an empty ConfigObject. */
inline exo::ConfigObject empty() noexcept {
    return exo::ConfigObject(nullptr);
}

/** Parses a ConfigObject from a C++ file or stream. */
template <typename T>
inline exo::ConfigObject parseFromFile(T& stream) {
    return nlohmann::json::parse(stream);
}

/** Parses a ConfigObject from a pair of iterators. */
template <typename Iterator>
inline exo::ConfigObject parseFromMemory(Iterator begin, Iterator end) {
    return nlohmann::json::parse(begin, end);
}

template <std::signed_integral T>
T rangeCheckInt_(std::intmax_t value) {
    if (value < static_cast<std::intmax_t>(std::numeric_limits<T>::min()))
        throw std::range_error("value");
    if (value > static_cast<std::intmax_t>(std::numeric_limits<T>::max()))
        throw std::range_error("value");
    return static_cast<T>(value);
}

template <std::unsigned_integral T>
T rangeCheckUInt_(std::uintmax_t value) {
    if (value > static_cast<std::uintmax_t>(std::numeric_limits<T>::max()))
        throw std::range_error("value");
    return static_cast<T>(value);
}

/** Indexes a ConfigObject, which must be an array, with a numeric index. */
inline const exo::ConfigObject& index(const exo::ConfigObject& c,
                                      std::size_t index) {
    return c[index];
}

/** Indexes a ConfigObject, which must be an object, with a string. */
inline const exo::ConfigObject& key(const exo::ConfigObject& c,
                                    const std::string& key) {
    return c[key];
}

/** Checks whether the ConfigObject is null. */
inline bool isNull(const exo::ConfigObject& c) noexcept {
    return c.is_null();
}

/** Checks whether the ConfigObject is a boolean. */
inline bool isBoolean(const exo::ConfigObject& c) noexcept {
    return c.is_boolean();
}

/** Checks whether the ConfigObject is an integer. */
inline bool isInt(const exo::ConfigObject& c) noexcept {
    return c.is_number_integer();
}

/** Checks whether the ConfigObject is an unsigned integer. */
inline bool isUInt(const exo::ConfigObject& c) noexcept {
    return c.is_number_unsigned();
}

/** Checks whether the ConfigObject is a floating-point number,
    or a type assignable to a floating-point number. */
inline bool isFloat(const exo::ConfigObject& c) noexcept {
    return c.is_number();
}

/** Checks whether the ConfigObject is a string. */
inline bool isString(const exo::ConfigObject& c) noexcept {
    return c.is_string();
}

/** Checks whether the ConfigObject is an array. */
inline bool isArray(const exo::ConfigObject& c) noexcept {
    return c.is_array();
}

/** Checks whether the ConfigObject is an object. */
inline bool isObject(const exo::ConfigObject& c) noexcept {
    return c.is_object();
}

/** Checks whether the ConfigObject has a specified key. */
inline bool hasKey(const exo::ConfigObject& c,
                   const std::string& name) noexcept {
    return cfg::isObject(c) && c.contains(name);
}

/** Checks whether the specified key in the ConfigObject is a null.
    Returns false if the key is not present at all. */
inline bool hasNull(const exo::ConfigObject& c,
                    const std::string& name) noexcept {
    return cfg::hasKey(c, name) && cfg::isNull(cfg::key(c, name));
}

/** Checks whether the specified key in the ConfigObject is a boolean.
    Returns false if the key is not present at all. */
inline bool hasBoolean(const exo::ConfigObject& c,
                       const std::string& name) noexcept {
    return cfg::hasKey(c, name) && cfg::isBoolean(cfg::key(c, name));
}

/** Checks whether the specified key in the ConfigObject is an integer.
    Returns false if the key is not present at all. */
inline bool hasInt(const exo::ConfigObject& c,
                   const std::string& name) noexcept {
    return cfg::hasKey(c, name) && cfg::isInt(cfg::key(c, name));
}

/** Checks whether the specified key in the ConfigObject is
    an unsigned integer.
    Returns false if the key is not present at all. */
inline bool hasUInt(const exo::ConfigObject& c,
                    const std::string& name) noexcept {
    return cfg::hasKey(c, name) && cfg::isUInt(cfg::key(c, name));
}

/** Checks whether the specified key in the ConfigObject is
    a floating-point number, or a number assignable to one.
    Returns false if the key is not present at all. */
inline bool hasFloat(const exo::ConfigObject& c,
                     const std::string& name) noexcept {
    return cfg::hasKey(c, name) && cfg::isFloat(cfg::key(c, name));
}

/** Checks whether the specified key in the ConfigObject is a string.
    Returns false if the key is not present at all. */
inline bool hasString(const exo::ConfigObject& c,
                      const std::string& name) noexcept {
    return cfg::hasKey(c, name) && cfg::isString(cfg::key(c, name));
}

/** Checks whether the specified key in the ConfigObject is an array.
    Returns false if the key is not present at all. */
inline bool hasArray(const exo::ConfigObject& c,
                     const std::string& name) noexcept {
    return cfg::hasKey(c, name) && cfg::isArray(cfg::key(c, name));
}

/** Checks whether the specified key in the ConfigObject is an object.
    Returns false if the key is not present at all. */
inline bool hasObject(const exo::ConfigObject& c,
                      const std::string& name) noexcept {
    return cfg::hasKey(c, name) && cfg::isObject(cfg::key(c, name));
}

/** Gets the boolean value of a ConfigObject. Throws if not a boolean. */
inline bool getBoolean(const exo::ConfigObject& c) {
    if (cfg::isBoolean(c))
        return c.template get<bool>();
    else
        throw std::range_error("not boolean");
}

/** Gets the integer value of a ConfigObject. Throws if not an integer. */
inline std::intmax_t getIntMax(const exo::ConfigObject& c) {
    if (cfg::isInt(c))
        return c.template get<std::intmax_t>();
    else
        throw std::range_error("not int");
}

/** Gets the unsigned integer value of a ConfigObject.
    Throws if not an unsigned integer. */
inline std::uintmax_t getUIntMax(const exo::ConfigObject& c) {
    if (cfg::isUInt(c))
        return c.template get<std::uintmax_t>();
    else
        throw std::range_error("not uint");
}

/** Gets the floating-point value of a ConfigObject.
    Throws if not a number. */
inline double getFloat(const exo::ConfigObject& c) {
    if (cfg::isFloat(c))
        return c.template get<double>();
    else
        throw std::range_error("not float");
}

/** Gets the string value of a ConfigObject. Throws if not a string. */
inline std::string getString(const exo::ConfigObject& c) {
    if (cfg::isString(c))
        return c.template get<std::string>();
    else
        throw std::range_error("not string");
}

/** Gets the boolean value of a ConfigObject.
    If not a boolean or missing, returns the fallback. */
inline bool getBoolean(const exo::ConfigObject& c, bool fallback) {
    if (cfg::isBoolean(c))
        return c.template get<bool>();
    else
        return fallback;
}

/** Gets the integer value of a ConfigObject.
    If not an integer or missing, returns the fallback. */
inline std::intmax_t getIntMax(const exo::ConfigObject& c,
                               std::intmax_t fallback) {
    if (cfg::isInt(c))
        return c.template get<std::intmax_t>();
    else
        return fallback;
}

/** Gets the unsigned integer value of a ConfigObject.
    If not an unsigned integer or missing, returns the fallback. */
inline std::uintmax_t getUIntMax(const exo::ConfigObject& c,
                                 std::uintmax_t fallback) {
    if (cfg::isUInt(c))
        return c.template get<std::uintmax_t>();
    else
        return fallback;
}

/** Gets the floating-point value of a ConfigObject.
    If not assignable to floating-point number or missing,
    returns the fallback. */
inline double getFloat(const exo::ConfigObject& c, double fallback) {
    if (cfg::isFloat(c))
        return c.template get<double>();
    else
        return fallback;
}

/** Gets the string value of a ConfigObject.
    If not a string or missing, returns the fallback. */
inline std::string getString(const exo::ConfigObject& c,
                             const std::string& fallback) {
    if (cfg::isString(c))
        return c.template get<std::string>();
    else
        return fallback;
}

/** Gets the boolean value of a ConfigObject by key.
    Throws if not a boolean. */
inline bool namedBoolean(const exo::ConfigObject& c,
                         const std::string& name) {
    return cfg::getBoolean(cfg::key(c, name));
}

/** Gets the integer value of a ConfigObject by key.
    If not an integer or missing, returns the fallback. */
inline std::intmax_t namedIntMax(const exo::ConfigObject& c,
                                 const std::string& name) {
    return cfg::getIntMax(cfg::key(c, name));
}

/** Gets the unsigned integer value of a ConfigObject by key.
    If not an unsigned integer or missing, returns the fallback. */
inline std::uintmax_t namedUIntMax(const exo::ConfigObject& c,
                                   const std::string& name) {
    return cfg::getUIntMax(cfg::key(c, name));
}

/** Gets the floating-point value of a ConfigObject by key.
    If not assignable to floating-point number or missing,
    returns the fallback. */
inline double namedFloat(const exo::ConfigObject& c,
                         const std::string& name) {
    return cfg::getFloat(cfg::key(c, name));
}

/** Gets the string value of a ConfigObject by key.
    If not a string or missing, returns the fallback. */
inline std::string namedString(const exo::ConfigObject& c,
                               const std::string& name) {
    return cfg::getString(cfg::key(c, name));
}

/** Gets the boolean value of a ConfigObject by key.
    If not a boolean or missing, returns the fallback. */
inline bool namedBoolean(const exo::ConfigObject& c,
                         const std::string& name, bool fallback) {
    if (!cfg::hasKey(c, name)) return fallback;
    return cfg::getBoolean(cfg::key(c, name), fallback);
}

/** Gets the integer value of a ConfigObject by key.
    If not an integer or missing, returns the fallback. */
inline std::intmax_t namedIntMax(const exo::ConfigObject& c,
                                 const std::string& name,
                                 std::intmax_t fallback) {
    if (!cfg::hasKey(c, name)) return fallback;
    return cfg::getIntMax(cfg::key(c, name), fallback);
}

/** Gets the unsigned integer value of a ConfigObject by key.
    If not an unsigned integer or missing, returns the fallback. */
inline std::uintmax_t namedUIntMax(const exo::ConfigObject& c,
                                   const std::string& name,
                                   std::uintmax_t fallback) {
    if (!cfg::hasKey(c, name)) return fallback;
    return cfg::getUIntMax(cfg::key(c, name), fallback);
}

/** Gets the floating-point value of a ConfigObject by key.
    If not assignable to floating-point number or missing,
    returns the fallback. */
inline double namedFloat(const exo::ConfigObject& c,
                         const std::string& name, double fallback) {
    if (!cfg::hasKey(c, name)) return fallback;
    return cfg::getFloat(cfg::key(c, name), fallback);
}

/** Gets the string value of a ConfigObject by key.
    If not a string or missing, returns the fallback. */
inline std::string namedString(const exo::ConfigObject& c,
                               const std::string& name,
                               const std::string& fallback) {
    if (!cfg::hasKey(c, name)) return fallback;
    return cfg::getString(cfg::key(c, name), fallback);
}

/** Gets the integer value of a ConfigObject.
    Throws if the value is out of range, is missing or is not an integer. */
template <std::signed_integral T>
T getInt(const exo::ConfigObject& c) {
    return cfg::rangeCheckInt_<T>(cfg::getIntMax(c));
}

/** Gets the unsigned integer value of a ConfigObject.
    Throws if the value is out of range, is missing or
    is not an unsigned integer. */
template <std::unsigned_integral T>
T getUInt(const exo::ConfigObject& c) {
    return cfg::rangeCheckUInt_<T>(cfg::getUIntMax(c));
}

/** Gets the integer value of a ConfigObject.
    If not an integer or missing, returns the fallback.
    Throws if the value is out of range. */
template <std::signed_integral T>
T getInt(const exo::ConfigObject& c, T fallback) {
    return cfg::rangeCheckInt_<T>(cfg::getIntMax(c, fallback));
}

/** Gets the unsigned integer value of a ConfigObject.
    If not an unsigned integer or missing, returns the fallback.
    Throws if the value is out of range. */
template <std::unsigned_integral T>
T getUInt(const exo::ConfigObject& c, T fallback) {
    return cfg::rangeCheckUInt_<T>(cfg::getUIntMax(c, fallback));
}

/** Gets the integer value of a ConfigObject by key.
    Throws if the value is out of range, is missing or is not an integer. */
template <std::signed_integral T>
T namedInt(const exo::ConfigObject& c, const std::string& name) {
    return cfg::rangeCheckInt_<T>(cfg::namedIntMax(c, name));
}

/** Gets the unsigned integer value of a ConfigObject by key.
    Throws if the value is out of range, is missing or
    is not an unsigned integer. */
template <std::unsigned_integral T>
T namedUInt(const exo::ConfigObject& c, const std::string& name) {
    return cfg::rangeCheckUInt_<T>(cfg::namedUIntMax(c, name));
}

/** Gets the integer value of a ConfigObject by key.
    If not an integer or missing, returns the fallback.
    Throws if the value is out of range. */
template <std::signed_integral T>
T namedInt(const exo::ConfigObject& c, const std::string& name, T fallback) {
    return cfg::rangeCheckInt_<T>(cfg::namedIntMax(c, name, fallback));
}

/** Gets the unsigned integer value of a ConfigObject by key.
    If not an unsigned integer or missing, returns the fallback.
    Throws if the value is out of range. */
template <std::unsigned_integral T>
T namedUInt(const exo::ConfigObject& c, const std::string& name, T fallback) {
    return cfg::rangeCheckUInt_<T>(cfg::namedUIntMax(c, name, fallback));
}

/** Returns something that can be iterated to iterate over the items
    in a ConfigObject that is an array. */
inline const auto& iterateArray(const exo::ConfigObject& c) {
    return c;
}

/** Returns something that can be iterated to iterate over the key-value pairs
    in a ConfigObject that is an object. */
inline auto iterateObject(const exo::ConfigObject& c) {
    return c.items();
}

template <typename T>
struct false_ : std::false_type { };

template <typename T>
bool hasOfType_(const exo::ConfigObject& c, const std::string& name) {
    if constexpr (std::is_same_v<T, bool>)
        return cfg::hasBoolean(c, name);
    else if constexpr (std::is_floating_point_v<T>)
        return cfg::hasFloat(c, name);
    else if constexpr (std::is_unsigned_v<T>)
        return cfg::hasUInt(c, name);
    else if constexpr (std::is_signed_v<T>)
        return cfg::hasInt(c, name);
    else if constexpr (std::is_same_v<T, std::string>)
        return cfg::hasString(c, name);
    else
        static_assert(cfg::false_<T>::value,
                      "unsupported type for mustRead/mayRead");
}

template <typename T>
T readOfType_(const exo::ConfigObject& c, const std::string& name) {
    if constexpr (std::is_same_v<T, bool>)
        return cfg::namedBoolean(c, name);
    else if constexpr (std::is_floating_point_v<T>)
        return cfg::namedFloat(c, name);
    else if constexpr (std::is_unsigned_v<T>)
        return cfg::namedUInt<T>(c, name);
    else if constexpr (std::is_signed_v<T>)
        return cfg::namedInt<T>(c, name);
    else if constexpr (std::is_same_v<T, std::string>)
        return cfg::namedString(c, name);
    else
        static_assert(cfg::false_<T>::value,
                      "unsupported type for mustRead/mayRead");
}


template <typename T>
std::string formatTypeName_() {
    if constexpr (std::is_same_v<T, bool>)
        return "a boolean";
    else if constexpr (std::is_unsigned_v<T>)
        return "an unsigned integer";
    else if constexpr (std::is_signed_v<T>)
        return "a signed integer";
    else if constexpr (std::is_floating_point_v<T>)
        return "a float";
    else if constexpr (std::is_same_v<T, std::string>)
        return "a string";
    else
        static_assert(cfg::false_<T>::value,
                      "unsupported type for mustRead/mayRead");
}

/** Reads a value of the specified type from the ConfigObject by key.
    Throws if the value is missing or of the wrong type. */
template <typename T>
T mustRead(const exo::ConfigObject& c, const std::string& name) {
    if (!cfg::hasKey(c, name))
        throw exo::InvalidConfigError(
            "missing required field '" + name + "' in configuration");
    if (!cfg::hasOfType_<T>(c, name))
        throw exo::InvalidConfigError(
            "field '" + name + "' in configuration is not "
                + formatTypeName_<T>());
    return cfg::readOfType_<T>(c, name);
}

/** Reads a value of the specified type from the ConfigObject by key.
    If the value is missing, returns the fallback value.
    Throws if the value is of the wrong type. */
template <typename T>
T mayRead(const exo::ConfigObject& c, const std::string& name, T fallback) {
    if (!cfg::hasKey(c, name))
        return fallback;
    return cfg::mustRead<T>(c, name);
}

/** Reads a value of the specified type from the ConfigObject by key.
    Throws if the value is missing or of the wrong type.
    The section string can be used to make error messages clearer. */
template <typename T>
T mustRead(const exo::ConfigObject& c, const std::string& section,
           const std::string& name) {
    if (!cfg::hasKey(c, name))
        throw exo::InvalidConfigError(
            "missing required field '" + name + "' in "
            "configuration section '" + section + "'");
    if (!cfg::hasOfType_<T>(c, name))
        throw exo::InvalidConfigError(
            "field '" + name + "' in configuration section '" + section + "' "
            "is not " + formatTypeName_<T>());
    return readOfType_<T>(c, name);
}

/** Reads a value of the specified type from the ConfigObject by key.
    If the value is missing, returns the fallback value.
    Throws if the value is of the wrong type.
    The section string can be used to make error messages clearer. */
template <typename T>
T mayRead(const exo::ConfigObject& c, const std::string& section,
          const std::string& name, T fallback) {
    if (!cfg::hasKey(c, name))
        return fallback;
    return mustRead<T>(c, section, name);
}

}; // namespace exo::cfg

} // namespace exo

#endif /* CONFIG_HH */
