#pragma once

#include <mutex>
#include <utility>

// scope guard
#define CONCATENATE_IMPL(s1, s2) s1##s2
#define CONCATENATE(s1, s2) CONCATENATE_IMPL(s1, s2)

#define ANONYMOUS_VARIABLE_COUNTER(s) CONCATENATE(s, __COUNTER__)
#define ANONYMOUS_VARIABLE_LINE(s) CONCATENATE(s, __LINE__)

#ifdef __COUNTER__
#define ANONYMOUS_VARIABLE(s) ANONYMOUS_VARIABLE_COUNTER(s)
#else
#define ANONYMOUS_VARIABLE(s) ANONYMOUS_VARIABLE_LINE(s)
#endif

#define SCOPE_EXIT \
    auto ANONYMOUS_VARIABLE(SCOPE_EXIT_STATE) = ::detail::ScopeGuardOnExit() + [&]()

#define SCOPE_EXIT_NO_EXCEPTIONS \
    auto ANONYMOUS_VARIABLE(SCOPE_EXIT_STATE) = ::detail::ScopeGuardOnExitWithoutExceptions() + [&]()

#define RUN_ONCE_IMPL(kv) \
    kv std::once_flag ANONYMOUS_VARIABLE_LINE(RUN_ONCE_FLAG); ScopeGuard(&ANONYMOUS_VARIABLE_LINE(RUN_ONCE_FLAG)) + [&]()

#define RUN_ONCE              RUN_ONCE_IMPL(static)
#define RUN_ONCE_THREAD_LOCAL RUN_ONCE_IMPL(thread_local)

class ScopeGuard
{
    using Function = std::function<void()>;

public:
    ScopeGuard(Function f, bool during_exception = true)
        : f(std::move(f)), during_exception(during_exception)
    {}
    ScopeGuard(std::once_flag *flag)
        : flag(flag)
    {}
    ~ScopeGuard()
    {
        if (!active)
            return;
        if (during_exception)
            run();
        else if (!std::current_exception())
            run();
    }

    void dismiss()
    {
        active = false;
    }

    template <typename F>
    ScopeGuard &operator+(F &&fn)
    {
        f = std::move(fn);
        return *this;
    }

public:
    ScopeGuard() = delete;
    ScopeGuard(const ScopeGuard &) = delete;
    ScopeGuard &operator=(const ScopeGuard &) = delete;

    ScopeGuard(ScopeGuard &&rhs)
        : f(std::move(rhs.f)), active(rhs.active)
    {
        rhs.dismiss();
    }

private:
    Function f;
    bool active{ true };
    bool during_exception{ true };
    std::once_flag *flag{ nullptr };

    void run()
    {
        if (!f)
            return;

        if (flag)
            std::call_once(*flag, f);
        else
            f();
    }
};

namespace detail
{
    enum class ScopeGuardOnExit {};
    enum class ScopeGuardOnExitWithoutExceptions {};

    template <typename F>
    ScopeGuard operator+(ScopeGuardOnExit, F &&fn)
    {
        return ScopeGuard(std::forward<F>(fn));
    }

    template <typename F>
    ScopeGuard operator+(ScopeGuardOnExitWithoutExceptions, F &&fn)
    {
        return ScopeGuard(std::forward<F>(fn), false);
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
