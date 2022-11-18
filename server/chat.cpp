/*
 * Dwarfplex is based on Webfort, created by Vitaly Pronkin on 14/05/14.
 * Copyright (c) 2014 mifki, ISC license.
 * Copyright (c) 2020 white-rabbit, ISC license
 */

#include "chat.hpp"
#include "config.hpp"

// we need this for get_client()
#include "dfplex.hpp"

bool ChatMessage::is_expired(Client* client) const
{
    if (!m_time_remaining.get()) return true;

    auto iter = m_time_remaining->find(client->id);
    if (iter == m_time_remaining->end()) return true;

    return iter->second <= 0;
}

bool ChatMessage::is_flash(Client* client) const
{
    if (!m_time_remaining.get()) return false;

    auto iter = m_time_remaining->find(client->id);
    if (iter == m_time_remaining->end()) return false;

    return iter->second >= MESSAGE_TIME - MESSAGE_FLASH_TIME || iter->second < MESSAGE_FLASH_TIME;
}

void ChatMessage::expire(Client* client)
{
    if (!m_time_remaining) return;

    auto iter = m_time_remaining->find(client->id);
    if (iter != m_time_remaining->end())
    {
        m_time_remaining->erase(iter);
    }
}

void ChatLog::push_message(ChatMessage&& message)
{
    // set time remaining for all existing clients.
    message.m_time_remaining.reset(
        new std::map<std::shared_ptr<ClientIdentity>, int32_t>()
    );

    for (size_t i = 0; i < get_client_count(); ++i)
    {
        (*message.m_time_remaining)[get_client(i)->id] =
            MESSAGE_TIME;
    }

    // add message to log.
    m_messages.emplace_back(
        std::move(message)
    );

    // erase messages in big chunks when max size reached.
    while (m_messages.size() >= MAX_MESSAGE_COUNT)
    {
        size_t erase_count = m_messages.size() / 8;
        m_active_message_index -= erase_count;
        m_messages.erase(m_messages.begin(), m_messages.begin() + erase_count);
    }
}

void ChatLog::tick(Client* client)
{
    // tick each message's timer per-client.
    for (size_t i = m_active_message_index; i < m_messages.size(); ++i)
    {
        ChatMessage& message = m_messages.at(i);
        if (message.m_time_remaining)
        {
            auto iter = message.m_time_remaining->find(client->id);
            if (iter == message.m_time_remaining->end()) continue;
            if (iter->second-- <= 0)
            {
                message.m_time_remaining->erase(iter);
            }
        }
    }

    // update m_active_message_index, cleaning up maps.
    while (
        m_active_message_index < m_messages.size()
        && (!m_messages.at(m_active_message_index).m_time_remaining ||
        (m_messages.at(m_active_message_index).m_time_remaining
        && m_messages.at(m_active_message_index).m_time_remaining->empty()))
    )
    {
        m_messages.at(m_active_message_index).m_time_remaining.reset();
        ++m_active_message_index;
    }
}
