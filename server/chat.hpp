/*
 * Dwarfplex is based on Webfort, created by Vitaly Pronkin on 14/05/14.
 * Copyright (c) 2014 mifki, ISC license.
 * Copyright (c) 2020 white-rabbit, ISC license
 */

#pragma once

#include "Client.hpp"
#include <string>
#include <memory>
#include <map>

struct ChatMessage
{
    std::string m_contents;
    
    std::shared_ptr<ClientIdentity> m_sender;
    
    // time remaining on this message per-client;
    std::unique_ptr<std::map<std::shared_ptr<ClientIdentity>, int32_t>> m_time_remaining;
    
    void expire(Client*);
    bool is_expired(Client*) const;
    bool is_flash(Client*) const;
};

class ChatLog
{
public:
    std::vector<ChatMessage> m_messages;
    size_t m_active_message_index = 0;
    
    // (virtual methods are not linked -- so this can be called from other plugins.)
    virtual void push_message(ChatMessage&&);
    void tick(Client*);
};