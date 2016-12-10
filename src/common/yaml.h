/*
 * Copyright (c) 2016, Egor Pugin
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     1. Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *     2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *     3. Neither the name of the copyright holder nor the names of
 *        its contributors may be used to endorse or promote products
 *        derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "filesystem.h"

#include <yaml-cpp/yaml.h>

#include <unordered_set>

#define EXTRACT_VAR(r, val, var, type)   \
    do                                   \
    {                                    \
        auto v = r[var];                \
        if (v.IsDefined())               \
            val = v.template as<type>(); \
    } while (0)
#define EXTRACT(val, type) EXTRACT_VAR(root, val, #val, type)
#define EXTRACT_AUTO(val) EXTRACT(val, decltype(val))

using yaml = YAML::Node;

template <class T>
auto get_scalar(const yaml &node, const String &key, const T &default_ = T())
{
    const auto &n = node[key];
    if (n.IsDefined())
    {
        if (!n.IsScalar())
            throw std::runtime_error("'" + key + "' should be a scalar");
        return n.as<T>();
    }
    return default_;
};

template <class F>
void get_scalar_f(const yaml &node, const String &key, F &&f)
{
    const auto &n = node[key];
    if (n.IsDefined())
    {
        if (!n.IsScalar())
            throw std::runtime_error("'" + key + "' should be a scalar");
        f(n);
    }
};

template <class T>
auto get_sequence(const yaml &node)
{
    std::vector<T> result;
    const auto &n = node;
    if (!n.IsDefined())
        return result;
    if (n.IsScalar())
        result.push_back(n.as<String>());
    else
    {
        if (!n.IsSequence())
            return result;
        for (const auto &v : n)
            result.push_back(v.as<String>());
    }
    return result;
};

template <class T>
auto get_sequence(const yaml &node, const String &key, const T &default_ = T())
{
    const auto &n = node[key];
    if (n.IsDefined() && !(n.IsScalar() || n.IsSequence()))
        throw std::runtime_error("'" + key + "' should be a sequence");
    auto result = get_sequence<T>(n);
    if (!default_.empty())
        result.push_back(default_);
    return result;
};

template <class T>
auto get_sequence_set(const yaml &node)
{
    auto vs = get_sequence<T>(node);
    return std::set<T>(vs.begin(), vs.end());
}

template <class T1, class T2 = T1>
auto get_sequence_set(const yaml &node, const String &key)
{
    auto vs = get_sequence<T2>(node, key);
    return std::set<T1>(vs.begin(), vs.end());
}

template <class T1, class T2 = T1>
auto get_sequence_unordered_set(const yaml &node, const String &key)
{
    auto vs = get_sequence<T2>(node, key);
    return std::unordered_set<T1>(vs.begin(), vs.end());
}

template <class F>
void get_sequence_and_iterate(const yaml &node, const String &key, F &&f)
{
    const auto &n = node[key];
    if (n.IsDefined())
    {
        if (!n.IsSequence())
            throw std::runtime_error("'" + key + "' should be a sequence");
        for (const auto &v : n)
            f(v);
    }
};

template <class F>
void get_map(const yaml &node, const String &key, F &&f)
{
    const auto &n = node[key];
    if (n.IsDefined())
    {
        if (!n.IsMap())
            throw std::runtime_error("'" + key + "' should be a map");
        f(n);
    }
};

template <class F>
void get_map_and_iterate(const yaml &node, const String &key, F &&f)
{
    const auto &n = node[key];
    if (n.IsDefined())
    {
        if (!n.IsMap())
            throw std::runtime_error("'" + key + "' should be a map");
        for (const auto &v : n)
            f(v);
    }
};

template <class T>
void get_string_map(const yaml &node, const String &key, T &data)
{
    const auto &n = node[key];
    if (n.IsDefined())
    {
        if (!n.IsMap())
            throw std::runtime_error("'" + key + "' should be a map");
        for (const auto &v : n)
            data.emplace(v.first.as<String>(), v.second.as<String>());
    }
};

template <class F1, class F2, class F3>
void get_variety(const yaml &node, const String &key, F1 &&f_scalar, F2 &&f_seq, F3 &&f_map)
{
    const auto &n = node[key];
    if (!n.IsDefined())
        return;
    switch (n.Type())
    {
    case YAML::NodeType::Scalar:
        f_scalar(n);
        break;
    case YAML::NodeType::Sequence:
        f_seq(n);
        break;
    case YAML::NodeType::Map:
        f_map(n);
        break;
    }
}

template <class F1, class F3>
void get_variety_and_iterate(const yaml &node, F1 &&f_scalar, F3 &&f_map)
{
    const auto &n = node;
    if (!n.IsDefined())
        return;
    switch (n.Type())
    {
    case YAML::NodeType::Scalar:
        f_scalar(n);
        break;
    case YAML::NodeType::Sequence:
        for (const auto &v : n)
            f_scalar(v);
        break;
    case YAML::NodeType::Map:
        for (const auto &v : n)
            f_map(v);
        break;
    }
}

template <class F1, class F3>
void get_variety_and_iterate(const yaml &node, const String &key, F1 &&f_scalar, F3 &&f_map)
{
    const auto &n = node[key];
    get_variety_and_iterate(n, std::forward<F1>(f_scalar), std::forward<F1>(f_map));
}

void merge(yaml &to, const yaml &from);

yaml load_yaml_config(const path &p);
yaml load_yaml_config(const String &s);
