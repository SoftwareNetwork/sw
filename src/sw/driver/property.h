// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2018 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <primitives/filesystem.h>
#include <variant>

namespace sw
{

namespace detail
{

template <typename T, typename Tuple>
struct has_type;

template <typename T, typename... Us>
struct has_type<T, std::tuple<Us...>> : std::disjunction<std::is_same<T, Us>...> {};

template <class ... Types>
struct PropertyVariant : std::variant<std::monostate, Types...>
{
    using base = std::variant<std::monostate, Types...>;

    //using base::variant;
    using base::operator=;

    PropertyVariant() = default;

    /*template <class T>
    operator T() const
    {
        static_assert(has_type<T, std::tuple<Types...>>::value, "no such type in variant");
        return std::get<T>(*this);
    }*/

    String toString() const
    {
        if (auto v = std::get_if<std::string>(this))
            return *v;
        else if (auto v = std::get_if<path>(this))
            return to_string(v->u8string());
        else if (auto v = std::get_if<bool>(this))
            return *v ? "true" : "false";
#define IF_OP(T) else if (auto v = std::get_if<T>(this)) return std::to_string(*v)
        // based on frequency
        // rewrite as visit?
        IF_OP(int32_t);
        IF_OP(double);
        IF_OP(int64_t);
        IF_OP(int8_t);
        IF_OP(float);
        IF_OP(uint8_t);
        IF_OP(uint32_t);
        IF_OP(int16_t);
        IF_OP(uint16_t);
        IF_OP(uint64_t);
#undef IF_OP
else if (auto v = std::get_if<std::monostate>(this))
return "";
        throw std::bad_variant_access();
    }

    operator std::string() const
    {
        return toString();
    }

    operator bool() const
    {
        if (auto v = std::get_if<bool>(this))
            return *v;
#define IF_OP(T) else if (auto v = std::get_if<T>(this)) return *v != 0
        // based on frequency
        // rewrite as visit?
        IF_OP(int32_t);
        IF_OP(double);
        IF_OP(int64_t);
        IF_OP(int8_t);
        IF_OP(float);
        IF_OP(uint8_t);
        IF_OP(uint32_t);
        IF_OP(int16_t);
        IF_OP(uint16_t);
        IF_OP(uint64_t);
#undef IF_OP
        return !empty();
    }

    bool empty() const
    {
        return base::index() == 0;
    }

    // same as get
    /*template <class T>
    T &value()
    {
        return get<T>(*this);
    }*/

    // same as get
    template <class T>
    const T &value() const
    {
        return get<T>(*this);
    }

    // same as value
    /*template <class T>
    T &get()
    {
        if (empty())
            *this = T();
        return std::get<T>(*this);
    }*/

    // same as value
    template <class T>
    const T &get() const
    {
        return std::get<T>(*this);
    }

    // manual types
    // add overloads? const char * const?
    explicit PropertyVariant(const char *s)
        : base(std::string(s))
    {
    }
    PropertyVariant &operator=(const char *s)
    {
        base::operator=(std::string(s));
        return *this;
    }

    //
    PropertyVariant &operator+=(const std::string &v)
    {
        *this = (std::string)*this + v;
        return *this;
    }

    // n/equ
    bool operator==(const PropertyVariant &rhs) const
    {
        // FIXME:
        return toString() == rhs.toString();
    }

    bool operator!=(const PropertyVariant &rhs) const
    {
        return !operator==(rhs);
    }

    template <class U>
    bool operator==(const U &rhs) const
    {
        auto v = std::get_if<U>(this);
        if (v)
            return *v == rhs;
        return U() == rhs;
    }

    template <class U>
    bool operator!=(const U &rhs) const
    {
        return !operator==(rhs);
    }
};

} // namespace detail

using PropertyValue = detail::PropertyVariant<
    bool,
    int8_t,
    int16_t,
    int32_t,
    int64_t,
    uint8_t,
    uint16_t,
    uint32_t,
    uint64_t,
    float,
    double,
    std::string,
    path
>;

// ops for const char *
/*inline bool operator==(const PropertyValue &v1, const char *v2)
{
    return v1.template get<std::string>() == v2;
}*/

// macros
#define VARIANT_GET(v, T) (v.template get<T>())
#define VARIANT_CAST(v, T) ((T)v)

#define OP_RAW(G, ret, op, T)                                    \
    inline ret operator op(const PropertyValue &v1, const T &v2) \
    {                                                            \
        return G(v1, T) op v2;                                   \
    }                                                            \
                                                                 \
    inline ret operator op(PropertyValue &v1, const T &v2)       \
    {                                                            \
        return G(v1, T) op v2;                                   \
    }                                                            \
                                                                 \
    inline ret operator op(const T &v1, const PropertyValue &v2) \
    {                                                            \
        return v1 op G(v2, T);                                   \
    }                                                            \
                                                                 \
    inline ret operator op(const T &v1, PropertyValue &v2)       \
    {                                                            \
        return v1 op G(v2, T);                                   \
    }

#define OP(ret, op, T) OP_RAW(VARIANT_GET, ret, op, T)

#define BOOL_OP(op, T) OP(bool, op, T)
#define BOOL_OP_BOOL(op, T) OP_RAW(VARIANT_CAST, bool, op, T)
#define T_OP(op, T) OP(T, op, T)

#define CMP_OPS_BUNCH_RAW(M, T) \
    M(==, T)                    \
    M(!=, T)                    \
    M(<=, T)                    \
    M(>=, T)                    \
    M(<, T)                     \
    M(>, T)

#define CMP_OPS_BUNCH(T) CMP_OPS_BUNCH_RAW(BOOL_OP, T)

#define ARITHM_BUNCH(T) \
    T_OP(+, T)          \
    T_OP(-, T)          \
    T_OP(*, T)          \
    T_OP(/, T)

#define OPS_BUNCH(T) \
    CMP_OPS_BUNCH(T) \
    ARITHM_BUNCH(T)

// ops
/*CMP_OPS_BUNCH_RAW(BOOL_OP_BOOL, bool)

// arithmetic
OPS_BUNCH(int8_t)
OPS_BUNCH(int16_t)
OPS_BUNCH(int32_t)
OPS_BUNCH(int64_t)
OPS_BUNCH(uint8_t)
OPS_BUNCH(uint16_t)
OPS_BUNCH(uint32_t)
OPS_BUNCH(uint64_t)
OPS_BUNCH(float)
OPS_BUNCH(double)

// string
CMP_OPS_BUNCH(std::string)*/

inline std::string operator+(const std::string &v1, const PropertyValue &v2)
{
    return v1 + (std::string)v2;
}

/*inline bool operator==(const PropertyValue &lhs, const PropertyValue &rhs)
{
    // FIXME:
    return lhs.operator==(rhs);
}

inline bool operator!=(const PropertyValue &lhs, const PropertyValue &rhs)
{
    return !operator==(lhs, rhs);
}*/

// undefs
#undef OPS_BUNCH
#undef T_OP
#undef CMP_OPS_BUNCH_RAW
#undef CMP_OPS_BUNCH
#undef ARITHM_BUNCH
#undef OPS_BUNCH
#undef BOOL_OP
#undef BOOL_OP_BOOL
#undef OP
#undef OP_RAW
#undef VARIANT_CAST
#undef VARIANT_GET

}
