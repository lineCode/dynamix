// DynaMix
// Copyright (c) 2013-2017 Borislav Stanimirov, Zahary Karadjov
//
// Distributed under the MIT Software License
// See accompanying file LICENSE.txt or copy at
// https://opensource.org/licenses/MIT
//
#pragma once

/**
* \file
* Functions and macros associated with calling and querying the next bidder of
* a message,
*/

#include "global.hpp"
#include "message_macros.hpp"
#include "exception.hpp"
#include "object.hpp"

namespace dynamix
{
template <typename Mixin, typename Message, typename ...Args>
auto call_next_bidder(Mixin* mixin, Message* message, Args&&... args)
-> decltype(std::declval<typename Message::caller_func>()(nullptr, std::forward<Args>(args)...))
{
    auto& dom = internal::domain::instance();

    internal::mixin_type_info& mixin_info = _dynamix_get_mixin_type_info(mixin);
    auto& msg = static_cast<const internal::message_t&>(_dynamix_get_mixin_feature_fast(message));

    const object* obj = object_of(mixin);
    const internal::object_type_info::call_table_entry& entry = obj->_type_info->_call_table[msg.id];

    if (msg.mechanism == internal::message_t::unicast)
    {
        auto ptr = entry.begin;
        // since this is called from a mixin this loop must end eventually
        while ((*ptr++)->_mixin_id != mixin_info.id);
        DYNAMIX_THROW_UNLESS(ptr < entry.end, bad_next_bidder_call);
        const internal::message_for_mixin* msg_data = *ptr;
        auto data = _DYNAMIX_GET_MIXIN_DATA((*obj), msg_data->_mixin_id);

        auto func = reinterpret_cast<typename Message::caller_func>(msg_data->caller);
        return func(data, args...);
    }
    else
    {

    }
}

} // namespace dynamix

/**
* \brief Macro that calls the next bidder from a message with a higher bid
*
* Use it the method which implements a given message. The return type is the same
* as the message return type.*
*
* \param message_tag is the message tag for the required message (ie `foo_msg`)
* \param args are the arguments for this message. Typically the same as the
*  arguments of the current method.
*/
#define DYNAMIX_CALL_NEXT_BIDDER(message_tag, ...) \
    ::dynamix::call_next_bidder(this, message_tag, __VA_ARGS__)
