#ifndef AUTO_SCOPE_GUARD_HEADER
#define AUTO_SCOPE_GUARD_HEADER 1

// From: https://quuxplusone.github.io/blog/2018/08/11/the-auto-macro/

namespace scope_guard_with_auto_macro_impl_detail
{
    namespace
    {
        template<class L>
        class AtScopeExit {
            L& m_lambda;
        public:
            AtScopeExit(L& action) : m_lambda(action) {}
            ~AtScopeExit() { m_lambda(); }
        };
    }
}

#define TOKEN_PASTEx(x, y) x ## y
#define TOKEN_PASTE(x, y) TOKEN_PASTEx(x, y)

#define Auto_INTERNAL1(lname, aname, ...) \
    auto lname = [&]() { __VA_ARGS__; }; \
    ::scope_guard_with_auto_macro_impl_detail::AtScopeExit<decltype(lname)> aname(lname);

#define Auto_INTERNAL2(ctr, ...) \
    Auto_INTERNAL1(TOKEN_PASTE(Auto_func_, ctr), \
        TOKEN_PASTE(Auto_instance_, ctr), __VA_ARGS__)

#define Auto(...) \
    Auto_INTERNAL2(__COUNTER__, __VA_ARGS__)

#endif // !AUTO_SCOPE_GUARD_HEADER