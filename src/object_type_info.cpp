// DynaMix
// Copyright (c) 2013-2017 Borislav Stanimirov, Zahary Karadjov
//
// Distributed under the MIT Software License
// See accompanying file LICENSE.txt or copy at
// https://opensource.org/licenses/MIT
//
#include "internal.hpp"
#include <dynamix/mixin_type_info.hpp>
#include <dynamix/object_type_info.hpp>
#include <dynamix/domain.hpp>
#include <dynamix/allocators.hpp>
#include <dynamix/exception.hpp>

namespace dynamix
{
namespace internal
{

object_type_info::object_type_info()
{
    zero_memory(_mixin_indices, sizeof(_mixin_indices));
    zero_memory(_call_table, sizeof(_call_table));

    // set the end of the array to point to the virtual default impl mixin
    // since every default impl message will have this as an id,
    // it will make the correct cross index when the message is called
    _mixin_indices[DYNAMIX_MAX_MIXINS] = DEFAULT_MSG_IMPL_INDEX;
}

object_type_info::~object_type_info()
{
}

static const object_type_info null_type_info;

const object_type_info& object_type_info::null()
{
    return null_type_info;
};

mixin_data_in_object* object_type_info::alloc_mixin_data() const
{
    size_t num_to_allocate = _compact_mixins.size() + MIXIN_INDEX_OFFSET;

    char* memory = domain::instance().allocator()->alloc_mixin_data(num_to_allocate);
    mixin_data_in_object* ret = new (memory) mixin_data_in_object[num_to_allocate];

    return ret;
}

void object_type_info::dealloc_mixin_data(mixin_data_in_object* data) const
{
    for (size_t i = MIXIN_INDEX_OFFSET; i < _compact_mixins.size() + MIXIN_INDEX_OFFSET; ++i)
    {
        data[i].~mixin_data_in_object();
    }

    domain::instance().allocator()->dealloc_mixin_data(reinterpret_cast<char*>(data));
}

void object_type_info::fill_call_table()
{
    // first pass
    // find top bid messages and prepare to calculate message buffer length length

    intptr_t message_data_buffer_size = 0;

    // in this pass we make use of the fact that _call_table begin and end start as nullptr
    // for a new type so will will use them as counters

    for (const mixin_type_info* info : _compact_mixins)
    {
        for (const message_for_mixin& msg : info->message_infos)
        {
            call_table_entry& table_entry = _call_table[msg.message->id];

            DYNAMIX_ASSERT(table_entry.top_bid_message != nullptr || (table_entry.begin == nullptr || table_entry.end == nullptr));

            if (msg.message->mechanism == message_t::unicast)
            {
                if (!table_entry.top_bid_message)
                {
                    // new message
                    table_entry.top_bid_message = &msg;
                }
                else if (table_entry.top_bid_message->priority < msg.priority)
                {
                    // we found bigger priority
                    // make it looks like a new message
                    table_entry.top_bid_message = &msg;

                    // also remove the top-priority size we've accumulated
                    message_data_buffer_size -= reinterpret_cast<intptr_t>(table_entry.begin) / sizeof(void*);
                    table_entry.begin = nullptr;
                }
                else if (table_entry.top_bid_message->priority == msg.priority)
                {
                    ++message_data_buffer_size;

                    // hacky usage of end to count top bidders
                    // we need this so we can update message_data_buffer_size if we encounter
                    // a message with a higher priority
                    ++table_entry.begin;

                    // we have multiple bidders for the same priority
                    if (table_entry.top_bid_message->bid < msg.bid)
                    {
                        table_entry.top_bid_message = &msg;

                        // count the first message too
                        ++message_data_buffer_size;
                        ++table_entry.begin;
                    }
                }
            }
            if(msg.message->mechanism == message_t::multicast)
            {
                // again we use begin to set the size of the buffer this particular message needs
                ++message_data_buffer_size;
                ++table_entry.begin;

                if (!table_entry.top_bid_message || table_entry.top_bid_message->bid != msg.bid)
                {
                    // additionally for each change of bid number in top_bid_message we will increment
                    // message_data_buffer_size
                    // we use this to make a guess for the buffer size so it
                    // can incorporate all messages and all nullptr separators
                    // it might be end up being a bigger number than needed (for example if bids go x,y,x)
                    // but in the most common case (no multicast bids) it will be +0
                    // furhter more this saves us from excessive allocations from using std::map or similar to
                    // count the exact number of unique bids

                    ++message_data_buffer_size;
                    ++table_entry.begin;
                }
                table_entry.top_bid_message = &msg;
            }
        }
    }

    const domain& dom = domain::instance();

    // pass through all messages to check for mutlticasts which we don't implement
    // but do have a default implementations
    // we need to set the buffer size to include those ones too
    for (size_t i = 0; i < dom._num_registered_messages; ++i)
    {
        if (!dom._messages[i] || !dom._messages[i]->default_impl_data)
        {
            // no such mesasge or
            // message doesn't have default implementation
            continue;
        }

        call_table_entry& table_entry = _call_table[i];

        // we make use of the fact that we se a value to top_bid_message for multicasts
        // (even though it's not used later) to check whether we implement the message
        if (dom._messages[i]->mechanism == message_t::multicast && !table_entry.top_bid_message)
        {
            message_data_buffer_size += 2; // 1 for the pointer to default implementation and 1 for nullptr
            table_entry.begin = nullptr;
        }
    }

    _message_data_buffer.reset(new pc_message_for_mixin[message_data_buffer_size]);
    auto message_data_buffer_ptr = _message_data_buffer.get();

    // second pass
    // update begin and end pointers of _call_table and add message datas to buffer
    for (const mixin_type_info* info : _compact_mixins)
    {
        for (const message_for_mixin& msg : info->message_infos)
        {
            call_table_entry& table_entry = _call_table[msg.message->id];

            if(table_entry.begin)
            {
                if (!table_entry.end)
                {
                    // begin is not null and end is null, so this message has not been updated
                    // but needs to be

                    auto begin = message_data_buffer_ptr;
                    message_data_buffer_ptr += reinterpret_cast<intptr_t>(table_entry.begin) / sizeof(void*);
                    DYNAMIX_ASSERT(message_data_buffer_ptr - _message_data_buffer.get() <= message_data_buffer_size);
                    table_entry.begin = begin;
                    table_entry.end = begin;
                }

                if (table_entry.top_bid_message->bid == msg.bid || msg.message->mechanism == message_t::multicast)
                {
                    // add all messages for multicasts
                    // add same-bid messages for unicasts
                    *table_entry.end++ = &msg;
                }
            }
        }
    }

    // third pass through all messages of the domain
    // if we implement it AND it has a buffer, sort it
    for(size_t i=0; i<dom._num_registered_messages; ++i)
    {
        if (!dom._messages[i])
        {
            // no such message
            continue;
        }

        call_table_entry& table_entry = _call_table[i];

        if (!table_entry.begin)
        {
            // either we don't implement this message
            // or it's a unicast with the same bid
            // we don't care about those
            continue;
        }

        // sort by bid
        std::sort(table_entry.begin, table_entry.end, [](const message_for_mixin* a, const message_for_mixin* b) -> bool
        {
            // descending
            return b->bid < a->bid;
        });

        if (dom._messages[i]->mechanism == message_t::multicast)
        {
            // for multicasts we have extra work to do
            // we need to sort messages with the same bid by priority
            // and then create gaps of nullptr between different bids
            auto begin = table_entry.begin;
            for (auto ptr = table_entry.begin; ptr < table_entry.end; ++ptr)
            {
                auto next = ptr + 1;

                if (next == table_entry.end || (*next)->bid != (*ptr)->bid)
                {
                    // gap
                    // sort by priority
                    std::sort(begin, next, [](const message_for_mixin* a, const message_for_mixin* b) -> bool
                    {
                        if (b->priority == a->priority)
                        {
                            const domain& dom = domain::instance();

                            // on the same priority sort by name of mixin
                            // this will guarantee that different compilations of the same mixins sets
                            // will always have the same order of multicast execution
                            const char* name_a = dom.mixin_info(a->_mixin_id).name;
                            const char* name_b = dom.mixin_info(b->_mixin_id).name;

                            return strcmp(name_a, name_b) < 0;
                        }
                        else
                        {
                            return b->priority < a->priority;
                        }
                    });

                    // add nulltpr "hole"
                    std::copy_backward(ptr, table_entry.end, next);
                    *next = nullptr;
                    ++ptr;
                    //++table_entry.end;
                    begin = ptr;
                }
            }
        }
    }

    // pass 1.5
    // check for unicast clashes
    // no messages with the same bid and priority may exist
    for (const mixin_type_info* info : _compact_mixins)
    {
        for (const message_for_mixin& msg : info->message_infos)
        {
            call_table_entry& table_entry = _call_table[msg.message->id];

            if (msg.message->mechanism == message_t::unicast)
            {
                DYNAMIX_THROW_UNLESS(
                    (table_entry.top_bid_message == &msg) || (table_entry.top_bid_message->bid > msg.bid || table_entry.top_bid_message->priority > msg.priority),
                    unicast_clash
                );
            }
        }
    }

    // final pass through all messages
    // if we don't implement a message and it has a default implementation, set it
    for (size_t i = 0; i<dom._num_registered_messages; ++i)
    {
        call_table_entry& table_entry = _call_table[i];

        if (table_entry.top_bid_message)
        {
            // we already implement this message
            continue;
        }

        const message_t* msg_data = dom._messages[i];

        if (!msg_data)
        {
            // no such message
            continue;
        }

        if (!msg_data->default_impl_data)
        {
            // message doesn't have a default implementation
            continue;
        }

        table_entry.top_bid_message = msg_data->default_impl_data;

        if (msg_data->mechanism == message_t::multicast)
        {
            table_entry.begin = message_data_buffer_ptr;
            *table_entry.begin = table_entry.top_bid_message;
            table_entry.end = table_entry.begin + 1;
            *table_entry.end = nullptr;
            ++message_data_buffer_ptr;
        }
    }
}

} // namespace internal
} // namespace dynamix
