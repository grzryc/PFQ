/***************************************************************
 *
 * (C) 2014 Nicola Bonelli <nicola@pfq.io>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 *
 ****************************************************************/

#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <cstring>
#include <vector>
#include <memory>
#include <array>
#include <type_traits>

#include <linux/pf_q.h>
#include <arpa/inet.h>

namespace pfq
{
namespace lang
{
    /* ipv4_t, network byte order type with converting constructor
     */

    struct ipv4_t
    {
        ipv4_t() = default;

        ipv4_t(const char *addr)
        {
            if (inet_pton(AF_INET, addr, &value) <= 0)
                throw std::runtime_error("pfq::lang::ipv4_t");
        }

        uint32_t value;
    };

    inline std::string
    show(ipv4_t value)
    {
        char buff[16];
        if (inet_ntop(AF_INET, &value, buff, sizeof(buff)) != NULL)
            throw std::runtime_error("pfq::lang::inet_ntop");

        return buff;
    }

    inline std::string
    pretty(ipv4_t value)
    {
        return show(value);
    }

    //////// pfq_functional_descr:

    using ::pfq_functional_descr;

    inline std::string
    show(pfq_functional_descr const &descr)
    {
        std::stringstream out;

        out << "functional_descr "
        << "symbol:"    << descr.symbol     << ' '
        << "arg:{ ";

        for(auto const &arg : descr.arg)
        {
            out << '(' << arg.addr << ',' << arg.size << ") ";
        }

        out << "} " << "next:"  << descr.next << ' ';

        return out.str();
    }

    //////// bool_type:

    template <bool Value>
    using bool_type = std::integral_constant<bool, Value>;

    //////// is_same_type_constructor:

    template <typename T, template <typename ...> class Tp>
    struct is_same_type_constructor : std::false_type
    { };

    template <template <typename ...> class Tp, typename ...Ti>
    struct is_same_type_constructor<Tp<Ti...>, Tp> : std::true_type
    { };

    //////// vector concat:

    template <typename Tp>
    inline std::vector<Tp>
    operator+(std::vector<Tp> v1, std::vector<Tp> &&v2)
    {
        v1.insert(v1.end(), std::make_move_iterator(v2.begin()),
                  std::make_move_iterator(v2.end()));
        return v1;
    }

    template <typename Tp>
    inline std::vector<Tp>
    operator+(std::vector<Tp> v1, std::vector<Tp> const &v2)
    {
        v1.insert(v1.end(), v2.begin(), v2.end());
        return v1;
    }

    //////// has_insertion_operator:

    template <typename C> char  has_insertion_test(typename std::remove_reference< decltype((std::cout << std::declval<C>())) >::type *);
    template <typename C> short has_insertion_test(...);

    template <typename T>
    struct has_insertion_operator : bool_type<sizeof(has_insertion_test<T>(0)) == sizeof(char)> {};


    //////// StorableShowBase polymorphic class:

    struct StorableShowBase
    {
        virtual std::string forall_show() const = 0;
        virtual void const *forall_addr() const = 0;
        virtual ~StorableShowBase() { }

        static const void *get_addr(std::string const &that)
        {
            return that.c_str();
        }

        template <typename T>
        static const void *get_addr(T const &that)
        {
            return &that;
        }

        template <typename T>
        static const void *get_addr(const std::vector<T> &that)
        {
            return that.data();
        }

        static std::string get_string(std::string const &that)
        {
            return '"' + that + '"';
        }

        template <typename T>
        static std::string get_string(std::vector<T> const &that)
        {
            std::string out("[");
            for(auto const &elem : that) {
                out += get_string(elem) + ' ';
            }
            return out + ']';
        }

        template <typename T, typename std::enable_if<has_insertion_operator<T>::value>::type * = nullptr >
        static std::string get_string(T const &that)
        {
            std::stringstream out;
            out << that;
            return out.str();
        }

        template <typename T, typename std::enable_if<!has_insertion_operator<T>::value>::type * = nullptr >
        static std::string get_string(T const &)
        {
            return "()";
        }
    };

    //////// StorableShow class:

    template <typename Tp>
    struct StorableShow final : StorableShowBase
    {
        StorableShow(Tp v)
        : value(std::move(v))
        {}

        Tp value;

        const void *forall_addr() const override
        {
            return get_addr(value);
        }

        std::string forall_show() const override
        {
            return get_string(value);
        }
    };

    //////// Function Argument class:

    struct Argument
    {
        Argument()
        : ptr()
        , size()
        , nelem()
        {}

        Argument(std::shared_ptr<StorableShowBase> p, size_t s, size_t n)
        : ptr(std::move(p))
        , size(s)
        , nelem(n)
        {}

        static Argument Null()
        {
            return Argument { std::shared_ptr<StorableShowBase>(), 0, 0 };
        }

        template <typename Tp>
        static Argument Data(Tp const &pod)
        {
            static_assert( std::is_pod<Tp>::value, "Data argument must be a pod type");
            auto ptr = std::make_shared<StorableShow<Tp>>(pod);
            return Argument{ std::dynamic_pointer_cast<StorableShowBase>(ptr), sizeof(pod), static_cast<size_t>(-1) };
        }

        template <typename Tp>
        static Argument Vector(std::vector<Tp> const &vec)
        {
            static_assert( std::is_pod<Tp>::value, "Vector type argument must be a pod type");
            auto ptr = std::make_shared<StorableShow<std::vector<Tp>>>(vec);
            return Argument{ std::dynamic_pointer_cast<StorableShowBase>(ptr), sizeof(Tp), vec.size() };
        }

        static Argument String(std::string str)
        {
            auto ptr = std::make_shared<StorableShow<std::string>>(std::move(str));
            return Argument{ std::dynamic_pointer_cast<StorableShowBase>(ptr), 0, static_cast<size_t>(-1) };
        }

        static Argument SVector(std::vector<std::string> const &svec)
        {
            std::string str;
            int n = 0;
            for(auto const & s : svec)
            {
                if (n++)
                    str += '\x1e';
                str += s;
            }

            auto ptr = std::make_shared<StorableShow<std::string>>(std::move(str));
            return Argument{ std::dynamic_pointer_cast<StorableShowBase>(ptr), 0, svec.size() };
        }

        static Argument FunPtr(std::size_t n)
        {
            return Argument{ std::shared_ptr<StorableShowBase>(), n, static_cast<size_t>(-1) };
        }

        std::shared_ptr<StorableShowBase> ptr;
        size_t size;
        size_t nelem;
    };


    inline Argument make_argument()
    {
        return Argument::Null();
    }

    inline Argument make_argument(std::string s)
    {
        return Argument::String(std::move(s));
    }

    inline Argument make_argument(const char *arr)
    {
        return Argument::String(arr);
    }

    template <typename Tp>
    inline Argument make_argument(Tp const &pod)
    {
        return Argument::Data(pod);
    }

    template <typename Tp>
    inline Argument make_argument(std::vector<Tp> const &pod)
    {
        return Argument::Vector(pod);
    }

    inline Argument make_argument(std::vector<std::string> const &vec)
    {
        return Argument::SVector(vec);
    }

    inline std::string
    show(const Argument &arg)
    {
        std::stringstream out;

        if (!arg.ptr && arg.size == 0 && arg.nelem == 0)  {
            out << "ArgNull";
        }
        else if (arg.ptr && arg.size != 0 && arg.nelem == static_cast<size_t>(-1))  {
            out << "ArgData " << arg.ptr->forall_show();
        }
        else if (arg.ptr && arg.size != 0 && arg.nelem != static_cast<size_t>(-1)) {
            out << "ArgVector " << arg.ptr->forall_show();
        }
        else if (arg.ptr && arg.size == 0)  {
            out << "ArgString " << arg.ptr->forall_show();
        }
        else {
            out << "ArgFun " << arg.size;
        }

        return out.str();
    }

    inline std::string
    pretty(const Argument &arg)
    {
        if (arg.ptr)
            return arg.ptr->forall_show();
        else if (arg.size)
            return "arg[" + std::to_string(arg.size) + "]";
        return "";
    }

    //////// Function descriptor class:

    struct FunctionDescr
    {
        std::string                 symbol;
        std::array<Argument, 8>     arg;
        std::size_t                 index;
        std::size_t                 link;
    };


    inline std::string
    show(const FunctionDescr &descr)
    {
        std::string out;

        out = "FunctionDescr " + descr.symbol + " [";

        for(auto const &a : descr.arg)
        {
            out += show(a) + " ";
        }
        out +=  "] " + std::to_string(descr.index) + " " + std::to_string(descr.link) + ')';

        return out;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////

    struct SkBuff { };

    template <typename a>
    struct Action { };

    template <typename Sig>
    struct Function
    {
        using type = Function<Sig>;
    };

    template <typename Tp>
    struct is_Function : std::false_type
    { };

    template <typename S>
    struct is_Function<Function<S>> : std::true_type
    { };

    using NetFunction  = Function< Action<SkBuff>(SkBuff) >;
    using NetPredicate = Function< bool(SkBuff) >;
    using NetProperty  = Function< uint64_t(SkBuff) >;

    struct MFunction;
    struct MFunction1;
    struct MFunction2;
    struct MFunction3;

    template <typename P> struct MFunctionP;
    template <typename P> struct MFunction1P;
    template <typename P, typename F> struct MFunctionPF;
    template <typename P, typename F1, typename F2> struct MFunctionPFF;
    template <typename F> struct MFunctionF;
    template <typename F, typename G> struct MFunctionFF;

    struct Predicate;
    struct Predicate1;
    struct Predicate2;
    struct Predicate3;

    template <typename Prop> struct PredicateR;
    template <typename Prop> struct PredicateR1;

    struct Property;

    template <typename Pred> struct Combinator1;
    template <typename Pred1, typename Pred2> struct Combinator2;
    template <typename F, typename G> struct Composition;

    template <typename Tp>
    struct is_property :
        bool_type<std::is_same<Tp, Property>::value>
    { };


    template <typename Tp>
    struct is_predicate :
        bool_type<std::is_same<Tp, Predicate>::value               ||
                  std::is_same<Tp, Predicate1>::value              ||
                  std::is_same<Tp, Predicate2>::value              ||
                  std::is_same<Tp, Predicate3>::value              ||
                  is_same_type_constructor<Tp, PredicateR>::value  ||
                  is_same_type_constructor<Tp, PredicateR1>::value ||
                  is_same_type_constructor<Tp, Combinator1>::value ||
                  is_same_type_constructor<Tp, Combinator2>::value >
    { };


    template <typename Tp>
    struct is_mfunction :
        bool_type<std::is_same<Tp, MFunction>::value                ||
                  std::is_same<Tp, MFunction1>::value               ||
                  std::is_same<Tp, MFunction2>::value               ||
                  std::is_same<Tp, MFunction3>::value               ||
                  is_same_type_constructor<Tp, MFunction1P>::value  ||
                  is_same_type_constructor<Tp, MFunctionP>::value   ||
                  is_same_type_constructor<Tp, MFunctionPF>::value  ||
                  is_same_type_constructor<Tp, MFunctionPFF>::value ||
                  is_same_type_constructor<Tp, MFunctionF>::value   ||
                  is_same_type_constructor<Tp, MFunctionFF>::value  ||
                  is_same_type_constructor<Tp, Composition>::value>
    { };


    //////// Combinator:


    template <typename Pred>
    struct Combinator1 : NetPredicate
    {
        static_assert(is_predicate<Pred>::value, "combinator: argument must be a predicate");

        Combinator1(std::string symbol, Pred p)
        : symbol_(std::move(symbol))
        , pred_(p)
        {}

        std::string symbol_;
        Pred pred_;
    };

    template <typename Pred1, typename Pred2>
    struct Combinator2 : NetPredicate
    {
        static_assert(is_predicate<Pred1>::value, "combinator: argument 1 must be a predicate");
        static_assert(is_predicate<Pred2>::value, "combinator: argument 2 must be a predicate");

        Combinator2(std::string symbol, Pred1 p1, Pred2 p2)
        : symbol_(std::move(symbol))
        , pred1_(p1)
        , pred2_(p2)
        {}

        std::string symbol_;
        Pred1       pred1_;
        Pred2       pred2_;
    };


    template <typename P>
    inline std::string
    pretty(Combinator1<P> const &comb)
    {
        if (comb.symbol_ == "not")
            return "!" + pretty(comb.pred_);

        throw std::logic_error("combinator: internal error");
    }


    template <typename P1, typename P2>
    inline std::string
    pretty(Combinator2<P1,P2> const &comb)
    {
        if (comb.symbol_ == "or")
            return '(' + pretty(comb.pred1_) + " | " + pretty(comb.pred2_) + ')';
        if (comb.symbol_ == "and")
            return '(' + pretty(comb.pred1_) + " & " + pretty(comb.pred2_) + ')';
        if (comb.symbol_ == "xor")
            return '(' + pretty(comb.pred1_) + " ^ " + pretty(comb.pred2_) + ')';

        throw std::logic_error("combinator: internal error");
    }


    template <typename Pred>
    inline std::pair<std::vector<FunctionDescr>, std::size_t>
    serialize(Combinator1<Pred> const &f, std::size_t n)
    {
       std::vector<FunctionDescr> pred, comb =
       {
           { f.symbol_, { { Argument::FunPtr(n+1) } }, n, -1UL }
       };

       std::size_t n1;

       std::tie(pred, n1) = serialize(f.pred_, n+1);

       return { std::move(comb) + std::move(pred), n1 };
    }


    template <typename Pred1, typename Pred2>
    inline std::pair<std::vector<FunctionDescr>, std::size_t>
    serialize(Combinator2<Pred1, Pred2> const &f, std::size_t n)
    {
       std::vector<FunctionDescr> pred1, pred2, comb;

       std::size_t n1, n2;

       std::tie(pred1, n1) = serialize(f.pred1_, n+1);
       std::tie(pred2, n2) = serialize(f.pred2_, n1);

       comb = { { f.symbol_, { { Argument::FunPtr(n+1), Argument::FunPtr(n1) } }, n, -1UL } };

       return { std::move(comb) + std::move(pred1) + std::move(pred2), n2 };
    }


    //////// Property:


    struct Property
    {
        template <typename ...Ts>
        Property(std::string symbol, Ts &&...args)
        : symbol_(std::move(symbol))
        , args_({make_argument(std::forward<Ts>(args))...})
        {
        }

        std::string             symbol_;
        std::vector<Argument>   args_;
    };

    ///////// pretty property:

    inline std::string
    pretty(Property const &descr)
    {
        if (descr.args_.empty())
            return descr.symbol_;

        std::string ret = '(' + descr.symbol_ + ' ';
        for(auto &a : descr.args_) {
            auto tmp = pretty(a);
            if (!tmp.empty())
                ret += std::move(tmp);
        }

        return ret + ')';
    }


    //////// serialize property:

    template <typename T, size_t N, typename C>
    std::array<T, N> make_array(C const &cont)
    {
        std::array<T, N> ret;
        size_t n = 0;

        for(auto &e : cont)
        {
            ret[n] = e;
            if (n == N)
                break;
        }

        return ret;
    }

    template <typename C>
    void fix_computation(C &cont)
    {
        if (!cont.empty())
        {
            cont.back().link = static_cast<std::size_t>(-1);
        }
    }

    inline std::pair<std::vector<FunctionDescr>, std::size_t>
    serialize(Property const &p, std::size_t n)
    {
        return { { FunctionDescr { p.symbol_, make_array<Argument, 8>(p.args_), n, -1UL } }, n+1 };
    }


    //////// Predicates:


    struct Predicate : NetPredicate
    {
        Predicate(std::string symb)
        : symbol_ {std::move(symb)}
        {}

        std::string symbol_;
    };

    struct Predicate1 : NetPredicate
    {
        template <typename Tp>
        Predicate1(std::string symb, Tp const &arg)
        : symbol_{std::move(symb)}
        , arg_ (make_argument(arg))
        {}

        std::string symbol_;
        Argument    arg_;
    };

    struct Predicate2 : NetPredicate
    {
        template <typename Tp1, typename Tp2>
        Predicate2(std::string symb, Tp1 const &arg1, Tp2 const &arg2)
        : symbol_{std::move(symb)}
        , arg1_ (make_argument(arg1))
        , arg2_ (make_argument(arg2))
        {}

        std::string symbol_;
        Argument    arg1_;
        Argument    arg2_;
    };

    struct Predicate3 : NetPredicate
    {
        template <typename Tp1, typename Tp2, typename Tp3>
        Predicate3(std::string symb, Tp1 const &arg1, Tp2 const &arg2, Tp3 const &arg3)
        : symbol_{std::move(symb)}
        , arg1_ (make_argument(arg1))
        , arg2_ (make_argument(arg2))
        , arg3_ (make_argument(arg3))
        {}

        std::string symbol_;
        Argument    arg1_;
        Argument    arg2_;
        Argument    arg3_;
    };

    template <typename Prop>
    struct PredicateR : NetPredicate
    {
        static_assert(is_property<Prop>::value, "predicate: argument must be a property");

        PredicateR(std::string symb, Prop const &p)
        : symbol_{std::move(symb)}
        , prop_(p)
        { }

        std::string symbol_;
        Prop        prop_;
    };

    template <typename Prop>
    struct PredicateR1 : NetPredicate
    {
        static_assert(is_property<Prop>::value, "predicate: argument must be a property");

        template <typename Tp>
        PredicateR1(std::string symb, Prop const &p, Tp const &arg)
        : symbol_{std::move(symb)}
        , prop_ (p)
        , arg_ (make_argument(arg))
        { }

        std::string symbol_;
        Prop        prop_;
        Argument    arg_;
    };

    ///////// pretty predicates:

    inline std::string
    pretty(Predicate const &descr)
    {
        return descr.symbol_;
    }

    inline std::string
    pretty(Predicate1 const &descr)
    {
        return '(' + descr.symbol_ + ' ' + pretty(descr.arg_) + ')';
    }

    inline std::string
    pretty(Predicate2 const &descr)
    {
        return '(' + descr.symbol_ + ' ' + pretty(descr.arg1_) + ' ' + pretty(descr.arg2_) + ')';
    }

    inline std::string
    pretty(Predicate3 const &descr)
    {
        return '(' + descr.symbol_ + ' ' + pretty(descr.arg1_) + ' ' + pretty(descr.arg2_) + ' ' + pretty (descr.arg3_) + ')';
    }

    template <typename Prop>
    inline std::string
    pretty(PredicateR<Prop> const &descr)
    {
        return '(' + descr.symbol_ + ' ' + pretty(descr.prop_) +  ')';
    }

    template <typename Prop>
    inline std::string
    pretty(PredicateR1<Prop> const &descr)
    {
        return '(' + descr.symbol_ + ' ' + pretty(descr.prop_) + ' ' + pretty(descr.arg_) + ')';
    }

    //////// serialize predicates:

    inline std::pair<std::vector<FunctionDescr>, std::size_t>
    serialize(Predicate const &p, std::size_t n)
    {
        return { { FunctionDescr { p.symbol_,  {{}}, n, -1UL } }, n+1 };
    }

    inline std::pair<std::vector<FunctionDescr>, std::size_t>
    serialize(Predicate1 const &p, std::size_t n)
    {
        return { { FunctionDescr { p.symbol_,  {{ p.arg_}}, n, -1UL } }, n+1 };
    }

    inline std::pair<std::vector<FunctionDescr>, std::size_t>
    serialize(Predicate2 const &p, std::size_t n)
    {
        return { { FunctionDescr { p.symbol_,  {{ p.arg1_, p.arg2_}}, n, -1UL } }, n+1 };
    }

    inline std::pair<std::vector<FunctionDescr>, std::size_t>
    serialize(Predicate3 const &p, std::size_t n)
    {
        return { { FunctionDescr { p.symbol_,  {{ p.arg1_, p.arg2_, p.arg3_ }}, n, -1UL } }, n+1 };
    }

    template <typename Prop>
    inline std::pair<std::vector<FunctionDescr>, std::size_t>
    serialize(PredicateR<Prop> const &p, std::size_t n)
    {
       std::vector<FunctionDescr> prop, pred =
       {
           { p.symbol_, { {Argument::FunPtr(n+1) } }, n, -1UL }
       };

       std::size_t n1;

       std::tie(prop, n1) = serialize(p.prop_, n+1);

       return { std::move(pred) + std::move(prop), n1 };
    }

    template <typename Prop>
    inline std::pair<std::vector<FunctionDescr>, std::size_t>
    serialize(PredicateR1<Prop> const &p, std::size_t n)
    {
       std::vector<FunctionDescr> prop, pred =
       {
           { p.symbol_, { {Argument::FunPtr(n+1), p.arg_ } }, n, -1UL }
       };

       std::size_t n1;

       std::tie(prop, n1) = serialize(p.prop_, n+1);

       return { std::move(pred) + std::move(prop), n1 };
    }


    ///////// NetFunction class:


    struct MFunction : NetFunction
    {
        MFunction(std::string symbol)
        : symbol_(std::move(symbol))
        { }

        std::string  symbol_;
    };


    struct MFunction1 : NetFunction
    {
        template <typename T>
        MFunction1(std::string symbol, T const &arg)
        : symbol_(std::move(symbol))
        , arg_(make_argument(arg))
        { }

        std::string     symbol_;
        Argument        arg_;
    };

    struct MFunction2 : NetFunction
    {
        template <typename T1, typename T2>
        MFunction2(std::string symbol, T1 const &arg1, T2 const &arg2)
        : symbol_(std::move(symbol))
        , arg1_(make_argument(arg1))
        , arg2_(make_argument(arg2))
        { }

        std::string     symbol_;
        Argument        arg1_;
        Argument        arg2_;
    };

    struct MFunction3 : NetFunction
    {
        template <typename T1, typename T2, typename T3>
        MFunction3(std::string symbol, T1 const &arg1, T2 const &arg2, T3 const &arg3)
        : symbol_(std::move(symbol))
        , arg1_(make_argument(arg1))
        , arg2_(make_argument(arg2))
        , arg3_(make_argument(arg3))
        { }

        std::string     symbol_;
        Argument        arg1_;
        Argument        arg2_;
        Argument        arg3_;
    };


    template <typename P>
    struct MFunction1P : NetFunction
    {
        static_assert(is_predicate<P>::value, "function: argument must be a predicate");

        template <typename T>
        MFunction1P(std::string symbol, T const &arg, const P &pred)
        : symbol_(std::move(symbol))
        , arg_(make_argument(arg))
        , pred_(pred)
        { }

        std::string     symbol_;
        Argument        arg_;
        P               pred_;
    };


    template <typename P>
    struct MFunctionP : NetFunction
    {
        static_assert(is_predicate<P>::value, "function: argument must be a predicate");

        MFunctionP(std::string symbol, P const &pred)
        : symbol_(std::move(symbol))
        , pred_(pred)
        { }

        std::string     symbol_;
        P               pred_;
    };


    template <typename P, typename F>
    struct MFunctionPF : NetFunction
    {
        static_assert(is_predicate<P>::value, "function: argument 1 must be a predicate");
        static_assert(is_mfunction<F>::value, "function: argument 2 must be a monadic function");

        MFunctionPF(std::string symbol, P const &pred, F const &fun)
        : symbol_(std::move(symbol))
        , pred_(pred)
        , fun_(fun)
        { }

        std::string  symbol_;
        P            pred_;
        F            fun_;
    };


    template <typename P, typename F, typename G>
    struct MFunctionPFF : NetFunction
    {
        static_assert(is_predicate<P>::value, "function: argument 1 must be a predicate");
        static_assert(is_mfunction<F>::value, "function: argument 2 must be a monadic function");
        static_assert(is_mfunction<G>::value, "function: argument 3 must be a monadic function");

        MFunctionPFF(std::string symbol, P const &pred, F const &fun1, G const &fun2)
        : symbol_(std::move(symbol))
        , pred_(pred)
        , fun1_(fun1)
        , fun2_(fun2)
        { }

        std::string  symbol_;
        P            pred_;
        F            fun1_;
        G            fun2_;
    };

    template <typename F>
    struct MFunctionF : NetFunction
    {
        static_assert(is_mfunction<F>::value, "function: argument must be a monadic function");

        MFunctionF(std::string symbol, F const &fun)
        : symbol_(std::move(symbol))
        , fun_(fun)
        { }

        std::string     symbol_;
        F               fun_;
    };

    template <typename F, typename G>
    struct MFunctionFF : NetFunction
    {
        static_assert(is_mfunction<F>::value, "function: argument 1 must be a monadic function");
        static_assert(is_mfunction<G>::value, "function: argument 2 must be a monadic function");

        MFunctionFF(std::string symbol, F const &f, G const &g)
        : symbol_(std::move(symbol))
        , f_(f)
        , g_(g)
        { }

        std::string     symbol_;
        F               f_;
        G               g_;
    };

    //
    // Composition
    //

    template <typename F, typename G> struct kleisly;

    template <typename F, typename G>
    struct Composition
    {
        static_assert(is_mfunction<F>::value, "composition: argument 1 must be a monadic function");
        static_assert(is_mfunction<G>::value, "composition: argument 2 must be a monadic function");

        using type = typename kleisly<typename F::type, typename G::type>::type;

        F f_;
        G g_;
    };

    template <template <typename > class M, typename A, typename B, typename C>
    struct kleisly< Function< M<B>(A) >, Function < M<C>(B)> >
    {
        using type = Function < M<C>(A) >;
    };

    template <template <typename > class M, typename A, typename B, typename F, typename G>
    struct kleisly< Function< M<B>(A) >, Composition <F, G> >
    {
        using type = typename kleisly< Function<M<B>(A)>, typename kleisly<F,G>::type>::type;
    };


    ///// pretty NetFunction:

    inline std::string
    pretty(MFunction const &descr)
    {
        return descr.symbol_;
    }

    inline std::string
    pretty(MFunction1 const &descr)
    {
        return '(' + descr.symbol_ + ' ' + pretty (descr.arg_) + ')';
    }

    inline std::string
    pretty(MFunction2 const &descr)
    {
        return '(' + descr.symbol_ + ' ' + pretty (descr.arg1_) + ' ' + pretty(descr.arg2_) + ')';
    }

    inline std::string
    pretty(MFunction3 const &descr)
    {
        return '(' + descr.symbol_ + ' ' + pretty (descr.arg1_) + ' ' + pretty(descr.arg2_) + ' ' + pretty(descr.arg3_) + ')';
    }

    template <typename P>
    inline std::string
    pretty(MFunction1P<P> const &descr)
    {
        return '(' + descr.symbol_ + ' ' + pretty (descr.arg_) + ' ' + pretty (descr.pred_) + ')';
    }

    template <typename P>
    inline std::string
    pretty(MFunctionP<P> const &descr)
    {
        return '(' + descr.symbol_ + ' ' + pretty(descr.pred_) + ')';
    }

    template <typename P, typename C>
    inline std::string
    pretty(MFunctionPF<P,C> const &descr)
    {
        return '(' + descr.symbol_ + ' ' + pretty(descr.pred_) + ' ' + pretty(descr.fun_) + ')';
    }

    template <typename P, typename C1, typename C2>
    inline std::string
    pretty(MFunctionPFF<P,C1,C2> const &descr)
    {
        return '(' + descr.symbol_ + ' ' + pretty(descr.pred_) + ' ' + pretty(descr.fun1_) + ' ' + pretty(descr.fun2_) + ')';
    }

    template <typename F>
    inline std::string
    pretty(MFunctionF<F> const &descr)
    {
        return '(' + descr.symbol_ + ' ' + pretty(descr.fun_) + ')';
    }

    template <typename F, typename G>
    inline std::string
    pretty(MFunctionFF<F, G> const &descr)
    {
        return '(' + descr.symbol_ + ' ' + pretty(descr.f_) + ' ' + pretty(descr.g_) + ')';
    }

    template <typename C1, typename C2>
    inline std::string
    pretty(Composition<C1,C2> const &descr)
    {
        return pretty(descr.f_) + " >-> " + pretty(descr.g_);
    }

    ///// serialize NetFunction:

    inline std::pair<std::vector<FunctionDescr>, std::size_t>
    serialize(MFunction const &f, std::size_t n)
    {
        return { { FunctionDescr { f.symbol_, {{}}, n, n+1 } }, n+1 };
    }

    inline std::pair<std::vector<FunctionDescr>, std::size_t>
    serialize(MFunction1 const &f, std::size_t n)
    {
        return { { FunctionDescr { f.symbol_, {{ f.arg_ }}, n, n+1 } }, n+1 };
    }

    inline std::pair<std::vector<FunctionDescr>, std::size_t>
    serialize(MFunction2 const &f, std::size_t n)
    {
        return { { FunctionDescr { f.symbol_, {{ f.arg1_, f.arg2_ }}, n, n+1 } }, n+1 };
    }

    inline std::pair<std::vector<FunctionDescr>, std::size_t>
    serialize(MFunction3 const &f, std::size_t n)
    {
        return { { FunctionDescr { f.symbol_, {{ f.arg1_, f.arg2_, f.arg3_ }}, n, n+1 } }, n+1 };
    }

    template <typename P>
    inline std::pair<std::vector<FunctionDescr>, std::size_t>
    serialize(MFunction1P<P> const &f, std::size_t n)
    {
        std::vector<FunctionDescr> p1, v1;
        std::size_t n1;

        std::tie(p1, n1) = serialize(f.pred_, n+1);

        v1 = { { f.symbol_,  { { f.arg_, Argument::FunPtr(n+1) } }, n, n1 } };

        return { std::move(v1) + std::move(p1), n1 };
    }

    template <typename P>
    inline std::pair<std::vector<FunctionDescr>, std::size_t>
    serialize(MFunctionP<P> const &f, std::size_t n)
    {
        std::vector<FunctionDescr> p1, v1;
        std::size_t n1;

        std::tie(p1, n1) = serialize(f.pred_, n+1);

        v1 = { { f.symbol_,  { { Argument::FunPtr(n+1) } }, n, n1 } };

        return { std::move(v1) + std::move(p1), n1 };
    }


    template <typename P, typename C>
    inline std::pair<std::vector<FunctionDescr>, std::size_t>
    serialize(MFunctionPF<P, C> const &f, std::size_t n)
    {
        std::vector<FunctionDescr> p1, v1, c1;
        std::size_t n1, n2;

        std::tie(p1, n1) = serialize(f.pred_, n+1);
        std::tie(c1, n2) = serialize(f.fun_, n1);

        fix_computation(c1);

        v1 = { { f.symbol_, { { Argument::FunPtr(n+1), Argument::FunPtr(n1) } }, n, n2 } };

        return { { std::move(v1) + std::move(p1) + std::move(c1) }, n2 };
    }


    template <typename P, typename C1, typename C2>
    inline std::pair<std::vector<FunctionDescr>, std::size_t>
    serialize(MFunctionPFF<P, C1, C2> const &f, std::size_t n)
    {
        std::vector<FunctionDescr> p1, v1, c1, c2;
        std::size_t n1, n2, n3;

        std::tie(p1, n1) = serialize(f.pred_ , n+1 );
        std::tie(c1, n2) = serialize(f.fun1_ , n1  );
        std::tie(c2, n3) = serialize(f.fun2_ , n2  );

        fix_computation(c1);
        fix_computation(c2);

        v1 = { { f.symbol_, { { Argument::FunPtr(n+1), Argument::FunPtr(n1), Argument::FunPtr(n2) } }, n, n3 } };

        return { std::move(v1) + std::move(p1) + std::move(c1) + std::move(c2), n3 };
    }

    template <typename F>
    inline std::pair<std::vector<FunctionDescr>, std::size_t>
    serialize(MFunctionF<F> const &f, std::size_t n)
    {
        std::vector<FunctionDescr> f1, v1;
        std::size_t n1;

        std::tie(f1, n1) = serialize(f.fun_, n+1);

        fix_computation(f1);

        v1 = { { f.symbol_,  { { Argument::FunPtr(n+1) } }, n, n1 } };

        return { std::move(v1) + std::move(f1), n1 };
    }

    template <typename F, typename G>
    inline std::pair<std::vector<FunctionDescr>, std::size_t>
    serialize(MFunctionFF<F,G> const &fun, std::size_t n)
    {
        std::vector<FunctionDescr> f1, f2, v1;
        std::size_t n1, n2;

        std::tie(f1, n1) = serialize(fun.f_, n+1);
        std::tie(f2, n2) = serialize(fun.g_, n1);

        fix_computation(f1);
        fix_computation(f2);

        v1 = { { fun.symbol_,  { { Argument::FunPtr(n+1), Argument::FunPtr(n1) } }, n, n2 } };

        return { std::move(v1) + std::move(f1) + std::move(f2), n2 };
    }


    template <typename C1, typename C2>
    inline std::pair<std::vector<FunctionDescr>, std::size_t>
    serialize(Composition<C1, C2> const &f, std::size_t n)
    {
        std::vector<FunctionDescr> v1, v2;
        std::size_t n1, n2;

        std::tie(v1, n1) = serialize(f.f_, n);
        std::tie(v2, n2) = serialize(f.g_, n1);

        return { std::move(v1) + std::move(v2), n2 };
    }

    //
    // serialize a vector of simple MFunction (utility)
    //

    inline std::pair<std::vector<FunctionDescr>, std::size_t>
    serialize (std::vector<MFunction> const &cont, std::size_t n)
    {
        std::vector<FunctionDescr> ret, v;
        std::size_t n1 = n;

        for(auto & f : cont)
        {
            std::tie(v, n1) = serialize(f, n1);
            ret = std::move(ret) + v;
        }

        return { ret, n1 };
    }

    ////// public constructors:

    template <typename P>
    inline Combinator1<P>
    combinator1(std::string symbol, P const &pred)
    {
        return Combinator1<P>{ std::move(symbol), pred };
    }

    template <typename P1, typename P2>
    inline Combinator2<P1,P2>
    combinator2(std::string symbol, P1 const &pred1, P2 const &pred2)
    {
        return Combinator2<P1,P2>{ std::move(symbol), pred1, pred2 };
    }

    inline Predicate
    predicate(std::string symbol)
    {
        return Predicate{ std::move(symbol) };
    }

    template <typename Tp>
    inline Predicate1
    predicate1(std::string symbol, Tp const &arg)
    {
        return Predicate1{ std::move(symbol), arg };
    }

    template <typename Tp1, typename Tp2>
    inline Predicate2
    predicate2(std::string symbol, Tp1 const &arg1, Tp2 const &arg2)
    {
        return Predicate2{ std::move(symbol), arg1, arg2 };
    }

    template <typename Tp1, typename Tp2, typename Tp3>
    inline Predicate3
    predicate3(std::string symbol, Tp1 const &arg1, Tp2 const &arg2, Tp3 const &arg3)
    {
        return Predicate3{ std::move(symbol), arg1, arg2, arg3 };
    }

    template <typename Prop>
    inline PredicateR<Prop>
    predicateR(std::string symbol, Prop const &p)
    {
        return PredicateR<Prop>{ std::move(symbol), p };
    }

    template <typename Prop, typename Tp>
    inline PredicateR1<Prop>
    predicateR1(std::string symbol, Prop const &p, Tp const &arg)
    {
        return PredicateR1<Prop>{ std::move(symbol), p, arg };
    }


    template <typename ...Ts>
    Property
    property(std::string symbol, Ts &&...args)
    {
        return Property{ std::move(symbol), std::forward<Ts>(args)... };
    }


    inline MFunction
    mfunction(std::string symbol)
    {
        return MFunction{ std::move(symbol) };
    }

    template <typename T>
    inline MFunction1
    mfunction1(std::string symbol, const T &arg)
    {
        return MFunction1{ std::move(symbol), arg };
    }

    template <typename T1, typename T2>
    inline MFunction2
    mfunction2(std::string symbol, const T1 &arg1, const T2 &arg2)
    {
        return MFunction2{ std::move(symbol), arg1, arg2 };
    }

    template <typename T1, typename T2, typename T3>
    inline MFunction3
    mfunction3(std::string symbol, const T1 &arg1, const T2 &arg2, const T3 &arg3)
    {
        return MFunction3{ std::move(symbol), arg1, arg2, arg3 };
    }

    template <typename T, typename P>
    inline MFunction1P<P>
    mfunction1P(std::string symbol, T const &arg, P const &p)
    {
        return MFunction1P<P>{ std::move(symbol), arg, p };
    }

    template <typename P>
    inline MFunctionP<P>
    mfunctionP(std::string symbol, P const &p)
    {
        return MFunctionP<P>{ std::move(symbol), p };
    }

    template <typename P, typename C>
    inline MFunctionPF<P,C>
    mfunctionPF(std::string symbol, P const &p, C const &c)
    {
        return MFunctionPF<P,C>{ std::move(symbol), p, c };
    }

    template <typename P, typename C1, typename C2>
    inline MFunctionPFF<P,C1, C2>
    mfunctionPFF(std::string symbol, P const &p, C1 const &c1, C2 const &c2)
    {
        return MFunctionPFF<P,C1,C2>{ std::move(symbol), p, c1, c2 };
    }

    template <typename F>
    inline MFunctionF<F>
    mfunctionF(std::string symbol, F const &fun)
    {
        return MFunctionF<F>{ std::move(symbol), fun };
    }

    template <typename F, typename G>
    inline MFunctionFF<F,G>
    mfunctionFF(std::string symbol, F const &f, G const &g)
    {
        return MFunctionFF<F, G>{ std::move(symbol), f, g};
    }

    //
    // Kleisli composition: >->
    //

    template <typename C1,
              typename C2,
              typename = typename kleisly<typename C1::type, typename C2::type>::type>
    inline Composition<C1, C2>
    operator>>(C1 c1, C2 c2)
    {
        return { std::move(c1), std::move(c2) };
    }


} // namespace lang
} // namespace pfq

