#pragma once

// scope guard
#define CONCATENATE_IMPL(s1, s2) s1##s2
#define CONCATENATE(s1, s2) CONCATENATE_IMPL(s1, s2)

#ifdef __COUNTER__
#define ANONYMOUS_VARIABLE(str) CONCATENATE(str, __COUNTER__)
#else
#define ANONYMOUS_VARIABLE(str) CONCATENATE(str, __LINE__)
#endif

#define SCOPE_EXIT \
    auto ANONYMOUS_VARIABLE(SCOPE_EXIT_STATE) = ::detail::ScopeGuardOnExit() + [&]()

template <class F>
class ScopeGuard
{
    F f;
    bool active{ true };

public:
    ScopeGuard(F f)
        : f(std::move(f))
    {}
    ~ScopeGuard()
    {
        if (active)
            f();
    }
    void dismiss()
    {
        active = false;
    }

    ScopeGuard() = delete;
    ScopeGuard(const ScopeGuard &) = delete;
    ScopeGuard &operator=(const ScopeGuard &) = delete;

    ScopeGuard(ScopeGuard &&rhs)
        : f(std::move(rhs.f)), active(rhs.active)
    {
        rhs.dismiss();
    }
};

namespace detail
{
    enum class ScopeGuardOnExit {};

    template <typename F>
    ScopeGuard<F> operator+(ScopeGuardOnExit, F &&fn)
    {
        return ScopeGuard<F>(std::forward<F>(fn));
    }
}

// lambda overloads
template <class... Fs> struct overload_set;

template <class F1, class... Fs>
struct overload_set<F1, Fs...> : F1, overload_set<Fs...>::type
{
    typedef overload_set type;

    overload_set(F1 head, Fs... tail)
        : F1(head), overload_set<Fs...>::type(tail...)
    {}

    using F1::operator();
    using overload_set<Fs...>::type::operator();
};

template <class F>
struct overload_set<F> : F
{
    typedef F type;
    using F::operator();
};

template <class... Fs>
typename overload_set<Fs...>::type overload(Fs... x)
{
    return overload_set<Fs...>(x...);
}
