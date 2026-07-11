#ifndef FRAMEWORK_INCLUDE_UBASE_DELEGATE_H
#define FRAMEWORK_INCLUDE_UBASE_DELEGATE_H

#include <new>
#include <cstring>
#include "Define.h"
#include <type_traits>

namespace Uface {
namespace UBase {
namespace detail {

template <class X, class Method>
struct MemberBinding {
    MemberBinding(X *object_, Method method_)
        : refs(1)
        , object(object_)
        , method(method_) {
    }

    int refs;
    X *object;
    Method method;
};

struct Identity {
    Identity()
        : refs(1) {
    }

    int refs;
};

template <class Functor>
struct OwnedFunctorBinding {
    OwnedFunctorBinding(Identity *identity_, const Functor &functor_)
        : refs(1)
        , identity(identity_)
        , functor(functor_) {
    }

    OwnedFunctorBinding(const OwnedFunctorBinding &other)
        : refs(1)
        , identity(other.identity)
        , functor(other.functor) {
        ++identity->refs;
    }

    ~OwnedFunctorBinding() {
        if (--identity->refs == 0) {
            delete identity;
        }
    }

    int refs;
    Identity *identity;
    Functor functor;
};

} // namespace detail

template <class R, class... Args>
class TDelegate {
private:
    typedef R (*Invoker)(void *, Args...);
    typedef bool (*Manager)(TDelegate &, const TDelegate *, int);
    typedef R (*FunctionPtr)(Args...);

    enum {
        ManageCopy,
        ManageDestroy,
        ManageRetain,
        ManageEqual
    };

    void *mObject;
    void *mIdentity;
    Invoker mInvoker;
    Manager mManager;

    template <class Functor>
    struct BareType {
        typedef typename std::remove_cv<
            typename std::remove_reference<Functor>::type>::type type;
    };

    template <class Functor>
    struct IsFunctorCandidate {
        typedef typename BareType<Functor>::type BareFunctor;
        enum {
            value = !std::is_same<BareFunctor, TDelegate>::value &&
                    !std::is_pointer<BareFunctor>::value &&
                    !std::is_function<BareFunctor>::value &&
                    !std::is_member_pointer<BareFunctor>::value &&
                    !std::is_same<BareFunctor, std::nullptr_t>::value &&
                    !std::is_integral<BareFunctor>::value
        };
    };

public:
    TDelegate()
        : mObject(0)
        , mIdentity(0)
        , mInvoker(0)
        , mManager(0) {
    }

    TDelegate(FunctionPtr function)
        : mObject(0)
        , mIdentity(0)
        , mInvoker(0)
        , mManager(0) {
        bind(function);
    }

    template <class Functor>
    TDelegate(const Functor &functor,
            typename std::enable_if<IsFunctorCandidate<Functor>::value, int>::type = 0)
        : mObject(0)
        , mIdentity(0)
        , mInvoker(0)
        , mManager(0) {
        bind(functor);
    }

    template <class X, class Y>
    TDelegate(R (X::*method)(Args...), Y *object)
        : mObject(0)
        , mIdentity(0)
        , mInvoker(0)
        , mManager(0) {
        bind(method, object);
    }

    template <class X, class Y>
    TDelegate(R (X::*method)(Args...) const, const Y *object)
        : mObject(0)
        , mIdentity(0)
        , mInvoker(0)
        , mManager(0) {
        bind(method, object);
    }

    TDelegate(const TDelegate &other)
        : mObject(0)
        , mIdentity(0)
        , mInvoker(0)
        , mManager(0) {
        copyFrom(other);
    }

    TDelegate &operator=(const TDelegate &other) {
        if (this != &other) {
            TDelegate temp(other);
            swap(temp);
        }
        return *this;
    }

    ~TDelegate() {
        clear();
    }

    void clear() {
        if (mManager) {
            (*mManager)(*this, 0, ManageDestroy);
        }
        mObject = 0;
        mIdentity = 0;
        mInvoker = 0;
        mManager = 0;
    }

    bool empty() const {
        return mInvoker == 0;
    }

    bool operator!() const {
        return empty();
    }

    explicit operator bool() const {
        return !empty();
    }

    R operator()(Args... args) const {
        return (*mInvoker)(mObject, args...);
    }

    class RetainedCall {
    public:
        explicit RetainedCall(const TDelegate &delegate)
            : object(delegate.mObject)
            , identity(delegate.mIdentity)
            , invoker(delegate.mInvoker)
            , manager(delegate.mManager) {
            manageTarget(manager, object, identity, ManageRetain);
        }

        ~RetainedCall() {
            manageTarget(manager, object, identity, ManageDestroy);
        }

        R operator()(Args... args) const {
            return (*invoker)(object, args...);
        }

    private:
        RetainedCall(const RetainedCall &);
        RetainedCall &operator=(const RetainedCall &);

        void *object;
        void *identity;
        Invoker invoker;
        Manager manager;
    };

    R invokeRetained(Args... args) const {
        RetainedCall retained(*this);
        return retained(args...);
    }

    bool operator==(const TDelegate &other) const {
        if (mInvoker != other.mInvoker) {
            return false;
        }
        if (!mInvoker) {
            return true;
        }
        if (mManager) {
            return (*mManager)(const_cast<TDelegate &>(*this), &other, ManageEqual);
        }
        return mIdentity == other.mIdentity;
    }

    bool operator!=(const TDelegate &other) const {
        return !(*this == other);
    }

    bool operator==(FunctionPtr function) const {
        if (!function) {
            return empty();
        }
        TDelegate other(function);
        return *this == other;
    }

    bool operator!=(FunctionPtr function) const {
        return !(*this == function);
    }

    TDelegate &operator=(FunctionPtr function) {
        bind(function);
        return *this;
    }

    template <class Functor>
    typename std::enable_if<IsFunctorCandidate<Functor>::value, TDelegate &>::type
    operator=(const Functor &functor) {
        bind(functor);
        return *this;
    }

    void bind(FunctionPtr function) {
        clear();
        if (!function) {
            return;
        }
        mObject = functionToObject(function);
        mIdentity = mObject;
        mInvoker = &invokeRuntimeFunction;
        mManager = 0;
    }

    template <FunctionPtr Function>
    void bind() {
        clear();
        mObject = 0;
        mIdentity = 0;
        mInvoker = &invokeStatic<Function>;
        mManager = 0;
    }

    template <FunctionPtr Function>
    static TDelegate make() {
        TDelegate result;
        result.template bind<Function>();
        return result;
    }

    template <class X, class Y>
    void bind(R (X::*method)(Args...), Y *object) {
        clear();
        typedef detail::MemberBinding<X, R (X::*)(Args...)> Binding;
        Binding *binding = new Binding(static_cast<X *>(object), method);
        mObject = binding;
        mIdentity = 0;
        mInvoker = &invokeMember<X>;
        mManager = &manageMember<Binding>;
    }

    template <class X, class Y>
    void bind(Y *object, R (X::*method)(Args...)) {
        bind(method, object);
    }

    template <class X, class Y>
    void bind(R (X::*method)(Args...) const, const Y *object) {
        clear();
        typedef detail::MemberBinding<const X, R (X::*)(Args...) const> Binding;
        Binding *binding = new Binding(static_cast<const X *>(object), method);
        mObject = binding;
        mIdentity = 0;
        mInvoker = &invokeConstMember<X>;
        mManager = &manageMember<Binding>;
    }

    template <class X, class Y>
    void bind(const Y *object, R (X::*method)(Args...) const) {
        bind(method, object);
    }

    template <class X, R (X::*Method)(Args...)>
    void bind(X *object) {
        clear();
        mObject = object;
        mIdentity = object;
        mInvoker = &invokeStaticMember<X, Method>;
        mManager = 0;
    }

    template <class X, R (X::*Method)(Args...) const>
    void bind(const X *object) {
        clear();
        mObject = const_cast<X *>(object);
        mIdentity = const_cast<X *>(object);
        mInvoker = &invokeStaticConstMember<X, Method>;
        mManager = 0;
    }

    template <class X, R (X::*Method)(Args...)>
    static TDelegate make(X *object) {
        TDelegate result;
        result.template bind<X, Method>(object);
        return result;
    }

    template <class X, R (X::*Method)(Args...) const>
    static TDelegate make(const X *object) {
        TDelegate result;
        result.template bind<X, Method>(object);
        return result;
    }

    template <class Functor>
    typename std::enable_if<IsFunctorCandidate<Functor>::value, void>::type
    bind(const Functor &functor) {
        typedef typename BareType<Functor>::type FunctorType;
        clear();
        detail::Identity *identity = new detail::Identity;
        typedef detail::OwnedFunctorBinding<FunctorType> Binding;
        Binding *binding = 0;
        try {
            binding = new Binding(identity, functor);
        } catch (...) {
            delete identity;
            throw;
        }
        mObject = binding;
        mIdentity = identity;
        mInvoker = &invokeOwnedFunctor<FunctorType>;
        mManager = &manageOwned<Binding>;
    }

    template <class Functor>
    void bindRef(Functor &functor) {
        typedef typename BareType<Functor>::type FunctorType;
        clear();
        detail::Identity *identity = new detail::Identity;
        mObject = &functor;
        mIdentity = identity;
        mInvoker = &invokeRefFunctor<FunctorType>;
        mManager = &manageRefIdentity;
    }

    template <class Functor>
    void bindRef(const Functor &functor) {
        typedef typename BareType<Functor>::type FunctorType;
        clear();
        detail::Identity *identity = new detail::Identity;
        mObject = const_cast<FunctorType *>(&functor);
        mIdentity = identity;
        mInvoker = &invokeConstRefFunctor<FunctorType>;
        mManager = &manageRefIdentity;
    }

    template <class Functor>
    void bindFunctorRef(Functor &functor) {
        bindRef(functor);
    }

    template <class Functor>
    void bindFunctorRef(const Functor &functor) {
        bindRef(functor);
    }

private:
    void swap(TDelegate &other) {
        void *object = mObject;
        void *identity = mIdentity;
        Invoker invoker = mInvoker;
        Manager manager = mManager;

        mObject = other.mObject;
        mIdentity = other.mIdentity;
        mInvoker = other.mInvoker;
        mManager = other.mManager;

        other.mObject = object;
        other.mIdentity = identity;
        other.mInvoker = invoker;
        other.mManager = manager;
    }

    void copyFrom(const TDelegate &other) {
        mObject = other.mObject;
        mIdentity = other.mIdentity;
        mInvoker = other.mInvoker;
        mManager = other.mManager;
        if (mManager) {
            (*mManager)(*this, &other, ManageCopy);
        }
    }

    template <class T>
    static bool manageMember(TDelegate &self, const TDelegate *source, int operation) {
        T *binding = static_cast<T *>(self.mObject);
        if (operation == ManageCopy || operation == ManageRetain) {
            ++binding->refs;
            return false;
        }
        if (operation == ManageDestroy) {
            if (--binding->refs == 0) {
                delete binding;
            }
            return false;
        }
        if (operation == ManageEqual) {
            T *other = static_cast<T *>(source->mObject);
            return binding->object == other->object && binding->method == other->method;
        }
        return false;
    }

    template <class Binding>
    static bool manageOwned(TDelegate &self, const TDelegate *source, int operation) {
        Binding *binding = static_cast<Binding *>(self.mObject);
        if (operation == ManageCopy) {
            self.mObject = new Binding(*static_cast<const Binding *>(source->mObject));
            self.mIdentity = static_cast<Binding *>(self.mObject)->identity;
            return false;
        }
        if (operation == ManageRetain) {
            ++binding->refs;
            return false;
        }
        if (operation == ManageEqual) {
            return self.mIdentity == source->mIdentity;
        }
        if (operation == ManageDestroy && --binding->refs == 0) {
            delete binding;
        }
        return false;
    }

    static bool manageRefIdentity(TDelegate &self, const TDelegate *source, int operation) {
        detail::Identity *identity = static_cast<detail::Identity *>(self.mIdentity);
        if (operation == ManageCopy || operation == ManageRetain) {
            ++identity->refs;
            return false;
        }
        if (operation == ManageEqual) {
            return self.mIdentity == source->mIdentity;
        }
        if (operation == ManageDestroy && --identity->refs == 0) {
            delete identity;
        }
        return false;
    }

    template <FunctionPtr Function>
    static R invokeStatic(void *, Args... args) {
        return (*Function)(args...);
    }

    static R invokeRuntimeFunction(void *object, Args... args) {
        FunctionPtr function = objectToFunction(object);
        return (*function)(args...);
    }

    template <class X>
    static R invokeMember(void *object, Args... args) {
        typedef detail::MemberBinding<X, R (X::*)(Args...)> Binding;
        Binding *binding = static_cast<Binding *>(object);
        return (binding->object->*binding->method)(args...);
    }

    template <class X>
    static R invokeConstMember(void *object, Args... args) {
        typedef detail::MemberBinding<const X, R (X::*)(Args...) const> Binding;
        Binding *binding = static_cast<Binding *>(object);
        return (binding->object->*binding->method)(args...);
    }

    template <class X, R (X::*Method)(Args...)>
    static R invokeStaticMember(void *object, Args... args) {
        return (static_cast<X *>(object)->*Method)(args...);
    }

    template <class X, R (X::*Method)(Args...) const>
    static R invokeStaticConstMember(void *object, Args... args) {
        return (static_cast<const X *>(object)->*Method)(args...);
    }

    template <class Functor>
    static R invokeFunctor(void *object, Args... args) {
        return (*static_cast<Functor *>(object))(args...);
    }

    template <class Functor>
    static R invokeOwnedFunctor(void *object, Args... args) {
        typedef detail::OwnedFunctorBinding<Functor> Binding;
        return static_cast<Binding *>(object)->functor(args...);
    }

    template <class Functor>
    static R invokeRefFunctor(void *object, Args... args) {
        return (*static_cast<Functor *>(object))(args...);
    }

    template <class Functor>
    static R invokeConstRefFunctor(void *object, Args... args) {
        return (*static_cast<const Functor *>(object))(args...);
    }

    static void manageTarget(Manager manager, void *object, void *identity, int operation) {
        if (!manager) {
            return;
        }
        TDelegate temp;
        temp.mObject = object;
        temp.mIdentity = identity;
        (*manager)(temp, 0, operation);
        temp.mObject = 0;
        temp.mIdentity = 0;
        temp.mManager = 0;
    }

    static void *functionToObject(FunctionPtr function) {
        static_assert(sizeof(FunctionPtr) <= sizeof(void *),
            "Fast::TDelegate cannot store this function pointer without a holder");
        union Caster {
            FunctionPtr function;
            void *object;
        } caster;
        caster.object = 0;
        caster.function = function;
        return caster.object;
    }

    static FunctionPtr objectToFunction(void *object) {
        union Caster {
            FunctionPtr function;
            void *object;
        } caster;
        caster.object = object;
        return caster.function;
    }
};

template <class R>
using TDelegate0 = TDelegate<R>;

template <class R, class P1>
using TDelegate1 = TDelegate<R, P1>;

template <class R, class P1, class P2>
using TDelegate2 = TDelegate<R, P1, P2>;

template <class R, class P1, class P2, class P3>
using TDelegate3 = TDelegate<R, P1, P2, P3>;

template <class R, class P1, class P2, class P3, class P4>
using TDelegate4 = TDelegate<R, P1, P2, P3, P4>;

template <class R, class P1, class P2, class P3, class P4, class P5>
using TDelegate5 = TDelegate<R, P1, P2, P3, P4, P5>;

template <class R, class P1, class P2, class P3, class P4, class P5, class P6>
using TDelegate6 = TDelegate<R, P1, P2, P3, P4, P5, P6>;

} // namespace UBase
} // namespace Uface

#endif // FRAMEWORK_INCLUDE_UBASE_DELEGATE_H
