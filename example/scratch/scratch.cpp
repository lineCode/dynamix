// DynaMix
// Copyright (c) 2013-2017 Borislav Stanimirov, Zahary Karadjov
//
// Distributed under the MIT Software License
// See accompanying file LICENSE.txt or copy at
// https://opensource.org/licenses/MIT
//
#include <iostream>
#include <string>
#include <sstream>
#include <dynamix/dynamix.hpp>
#include <dynamix/gen/no_arity_message_macros.hpp>

using namespace std;
using namespace dynamix;

DYNAMIX_DECLARE_MIXIN(a);
DYNAMIX_DECLARE_MIXIN(b);
DYNAMIX_DECLARE_MIXIN(c);
DYNAMIX_DECLARE_MIXIN(d);

DYNAMIX_MESSAGE_1(void, trace, std::ostream&, out);
DYNAMIX_MESSAGE_1(void, priority_trace, std::ostream&, out);

int main()
{
    object o;

    mutate(o)
        .add<a>()
        .add<b>()
        .add<c>()
        .add<d>();

    trace(o, cout);
    cout << endl;

    return 0;
}


class a
{
public:
    void trace(std::ostream& out)
    {
        out << "a";
    }

    void priority_trace(std::ostream& out)
    {
        out << "-1";
    }
};

class b
{
public:
    void trace(std::ostream& out)
    {
        out << "b";
    }

    void priority_trace(std::ostream& out)
    {
        out << "2";
    }
};

class c
{
public:
    void trace(std::ostream& out)
    {
        out << "c";
    }

    void priority_trace(std::ostream& out)
    {
        out << "1";
    }
};

class d
{
public:
    void trace(std::ostream& out)
    {
        out << "d";
    }

    void priority_trace(std::ostream& out)
    {
        out << "0";
    }
};

// this order should be important if the messages aren't sorted by mixin name
DYNAMIX_DEFINE_MIXIN(b, bid(2, trace_msg) & priority(2, priority_trace_msg));
DYNAMIX_DEFINE_MIXIN(a, bid(-1, trace_msg) & priority(-1, priority_trace_msg));
DYNAMIX_DEFINE_MIXIN(c, bid(1, trace_msg) & priority(1, priority_trace_msg));
DYNAMIX_DEFINE_MIXIN(d, trace_msg & priority_trace_msg);

DYNAMIX_DEFINE_MESSAGE(trace);
DYNAMIX_DEFINE_MESSAGE(priority_trace);

