#ifndef SNITCH_FUNCTION_HPP
#define SNITCH_FUNCTION_HPP

#include "snitch/snitch_concepts.hpp"
#include "snitch/snitch_config.hpp"

#include <utility>
#include <variant>

namespace snitch {
template<typename... Args>
struct overload : Args... {
    using Args::operator()...;
};

template<typename... Args>
overload(Args...) -> overload<Args...>;

template<auto T>
struct constant {
    static constexpr auto value = T;
};
} // namespace snitch

namespace snitch::impl {
template<typename T>
struct function_traits {
    static_assert(!std::is_same_v<T, T>, "incorrect template parameter for small_function");
};

template<typename T, bool Noexcept>
struct function_traits_base {
    static_assert(!std::is_same_v<T, T>, "incorrect template parameter for small_function");
};

template<typename Ret, typename... Args>
struct function_traits<Ret(Args...) noexcept> {
    using return_type             = Ret;
    using function_ptr            = Ret (*)(Args...) noexcept;
    using function_data_ptr       = Ret (*)(void*, Args...) noexcept;
    using function_const_data_ptr = Ret (*)(const void*, Args...) noexcept;

    static constexpr bool is_noexcept = true;

    template<typename ObjectType, auto MemberFunction>
    static constexpr function_data_ptr to_free_function() noexcept {
        return [](void* ptr, Args... args) noexcept {
            if constexpr (std::is_same_v<return_type, void>) {
                (static_cast<ObjectType*>(ptr)->*constant<MemberFunction>::value)(
                    std::forward<Args>(args)...);
            } else {
                return (static_cast<ObjectType*>(ptr)->*constant<MemberFunction>::value)(
                    std::forward<Args>(args)...);
            }
        };
    }

    template<typename ObjectType, auto MemberFunction>
    static constexpr function_const_data_ptr to_const_free_function() noexcept {
        return [](const void* ptr, Args... args) noexcept {
            if constexpr (std::is_same_v<return_type, void>) {
                (static_cast<const ObjectType*>(ptr)->*constant<MemberFunction>::value)(
                    std::forward<Args>(args)...);
            } else {
                return (static_cast<const ObjectType*>(ptr)->*constant<MemberFunction>::value)(
                    std::forward<Args>(args)...);
            }
        };
    }
};

template<typename Ret, typename... Args>
struct function_traits<Ret(Args...)> {
    using return_type             = Ret;
    using function_ptr            = Ret (*)(Args...);
    using function_data_ptr       = Ret (*)(void*, Args...);
    using function_const_data_ptr = Ret (*)(const void*, Args...);

    static constexpr bool is_noexcept = false;

    template<typename ObjectType, auto MemberFunction>
    static constexpr function_data_ptr to_free_function() noexcept {
        return [](void* ptr, Args... args) {
            if constexpr (std::is_same_v<return_type, void>) {
                (static_cast<ObjectType*>(ptr)->*constant<MemberFunction>::value)(
                    std::forward<Args>(args)...);
            } else {
                return (static_cast<ObjectType*>(ptr)->*constant<MemberFunction>::value)(
                    std::forward<Args>(args)...);
            }
        };
    }

    template<typename ObjectType, auto MemberFunction>
    static constexpr function_const_data_ptr to_const_free_function() noexcept {
        return [](const void* ptr, Args... args) {
            if constexpr (std::is_same_v<return_type, void>) {
                (static_cast<const ObjectType*>(ptr)->*constant<MemberFunction>::value)(
                    std::forward<Args>(args)...);
            } else {
                return (static_cast<const ObjectType*>(ptr)->*constant<MemberFunction>::value)(
                    std::forward<Args>(args)...);
            }
        };
    }
};
} // namespace snitch::impl

namespace snitch {
template<typename T>
class small_function;

namespace impl {
template<typename T>
struct is_small_function : std::false_type {};

template<typename T>
struct is_small_function<small_function<T>> : std::true_type {};

template<typename T>
concept not_small_function = !is_small_function<T>::value;

template<typename T, typename FunPtr>
concept function_ptr_or_stateless_lambda = not_small_function<T> && convertible_to<T, FunPtr>;

template<typename T, typename FunPtr>
concept functor = not_small_function<T> && !function_ptr_or_stateless_lambda<T, FunPtr> &&
                  requires { &T::operator(); };
} // namespace impl

template<typename T>
class small_function {
    using traits = impl::function_traits<T>;

public:
    using return_type             = typename traits::return_type;
    using function_ptr            = typename traits::function_ptr;
    using function_data_ptr       = typename traits::function_data_ptr;
    using function_const_data_ptr = typename traits::function_const_data_ptr;

private:
    struct function_and_data_ptr {
        void*             data = nullptr;
        function_data_ptr ptr;
    };

    struct function_and_const_data_ptr {
        const void*             data = nullptr;
        function_const_data_ptr ptr;
    };

    using data_type =
        std::variant<function_ptr, function_and_data_ptr, function_and_const_data_ptr>;

    data_type data;

public:
    constexpr small_function(const small_function&) noexcept            = default;
    constexpr small_function& operator=(const small_function&) noexcept = default;

    template<impl::function_ptr_or_stateless_lambda<function_ptr> FunctionType>
    constexpr small_function(const FunctionType& obj) noexcept :
        data{static_cast<function_ptr>(obj)} {}

    template<impl::functor<function_ptr> FunctorType>
    constexpr small_function(FunctorType& obj) noexcept :
        small_function(obj, constant<&FunctorType::operator()>{}) {}

    template<impl::functor<function_ptr> FunctorType>
    constexpr small_function(const FunctorType& obj) noexcept :
        small_function(obj, constant<&FunctorType::operator()>{}) {}

    // Prevent inadvertently using temporary stateful lambda; not supported at the moment.
    template<impl::functor<function_ptr> FunctorType>
    constexpr small_function(FunctorType&& obj) noexcept = delete;

    template<typename ObjectType, auto MemberFunction>
    constexpr small_function(ObjectType& obj, constant<MemberFunction>) noexcept :
        data{function_and_data_ptr{
            &obj, traits::template to_free_function<ObjectType, MemberFunction>()}} {}

    template<typename ObjectType, auto MemberFunction>
    constexpr small_function(const ObjectType& obj, constant<MemberFunction>) noexcept :
        data{function_and_const_data_ptr{
            &obj, traits::template to_const_free_function<ObjectType, MemberFunction>()}} {}

    // Prevent inadvertently using temporary object; not supported at the moment.
    template<typename ObjectType, auto M>
    constexpr small_function(ObjectType&& obj, constant<M>) noexcept = delete;

    template<typename... CArgs>
    constexpr return_type operator()(CArgs&&... args) const noexcept(traits::is_noexcept) {
        if constexpr (std::is_same_v<return_type, void>) {
            std::visit(
                overload{
                    [&](function_ptr f) { (*f)(std::forward<CArgs>(args)...); },
                    [&](const function_and_data_ptr& f) {
                        (*f.ptr)(f.data, std::forward<CArgs>(args)...);
                    },
                    [&](const function_and_const_data_ptr& f) {
                        (*f.ptr)(f.data, std::forward<CArgs>(args)...);
                    }},
                data);
        } else {
            return std::visit(
                overload{
                    [&](function_ptr f) { return (*f)(std::forward<CArgs>(args)...); },
                    [&](const function_and_data_ptr& f) {
                        return (*f.ptr)(f.data, std::forward<CArgs>(args)...);
                    },
                    [&](const function_and_const_data_ptr& f) {
                        return (*f.ptr)(f.data, std::forward<CArgs>(args)...);
                    }},
                data);
        }
    }
};
} // namespace snitch

#endif
