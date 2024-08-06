#include "motd_menu.h"

#include <game/server/event_key_manager.h>
#include <game/server/gamecontext.h>

void MotdMenu::AddImpl(int extra, const std::string& command, const std::string& description)
{
	Point p;
	str_copy(p.m_aDesc, description.c_str(), sizeof(p.m_aDesc));
	str_utf8_fix_truncation(p.m_aDesc);
	p.m_Command = command;
    p.m_Extra = extra;
	m_Points.push_back(p);
}

void MotdMenu::Handle()
{
    // check is has points
    if(m_Points.empty())
        return;

    const auto pServer = Instance::Server();
    const auto pGS = (CGS*)Instance::GameServerPlayer(m_ClientID);
    const auto pChar = pGS->GetPlayerChar(m_ClientID);

    // check valid character
    if(!pChar)
        return;

    // initialize variables
    constexpr int visibleLines = 10;
    constexpr int linesizeY = 20;
    constexpr int startLineY = -280;
    const int targetX = pChar->m_Core.m_Input.m_TargetX;
    const int targetY = pChar->m_Core.m_Input.m_TargetY;
    const int maxScrollPos = maximum(0, static_cast<int>(m_Points.size()) - visibleLines);

    // is key event next weapon (scroll)
    if(CEventKeyManager::IsKeyClicked(m_ClientID, KEY_EVENT_NEXT_WEAPON))
    {
        m_ScrollPos = minimum(++m_ScrollPos, maxScrollPos);
        m_ResendMotdTick = pServer->Tick() + 5;
    }
    // is key event prev weapon (scroll)
    if(CEventKeyManager::IsKeyClicked(m_ClientID, KEY_EVENT_PREV_WEAPON))
    {
        m_ScrollPos = maximum(--m_ScrollPos, 0);
        m_ResendMotdTick = pServer->Tick() + 5;
    }

    std::string buffer;
    int linePos = 0;
    if(m_ScrollPos > 0)
    {
        buffer += "△ Scroll up △\n";
        linePos = 1;
    }

    // lambda prepares
    auto addLineToBuffer = [&](int index, const std::string& line, bool isSelected)
    {
        buffer += (isSelected ? "✘ " : "➜ ") + std::to_string(index + 1) + ". " + line + "\n";
    };

    // prepare buffer with points
    for(int i = m_ScrollPos; i < m_ScrollPos + visibleLines && i < static_cast<int>(m_Points.size()); ++i, ++linePos)
    {
        int checksY = startLineY + linePos * linesizeY;
        int checkeY = startLineY + (linePos + 1) * linesizeY;
        bool isSelected = (targetX > -196 && targetX < 196 && targetY >= checksY && targetY < checkeY);

        if(isSelected && CEventKeyManager::IsKeyClicked(m_ClientID, KEY_EVENT_FIRE))
        {
            const auto& command = m_Points[i].m_Command;
            const auto& extra = m_Points[i].m_Extra;

            const size_t oldSize = pChar->GetPlayer()->m_vpInstanceMotdMenu.size();
            if(pGS->OnClientMotdCommand(m_ClientID, command.c_str(), extra))
            {
	            const size_t nowSize = pChar->GetPlayer()->m_vpInstanceMotdMenu.size();
                if(oldSize == nowSize && m_Flags & MTFLAG_CLOSE_LAST_MENU_ON_SELECT)
                {
                    if(pChar->GetPlayer()->m_vpInstanceMotdMenu.size() > 1)
                        pChar->GetPlayer()->m_vpInstanceMotdMenu.pop_back();
                    else
                        ClearMotd(pGS, pChar->GetPlayer());
                }
                if(oldSize == nowSize && m_Flags & MTFLAG_CLOSE_ON_SELECT)
                    ClearMotd(pGS, pChar->GetPlayer());
                return;
            }
        }

        addLineToBuffer(i, m_Points[i].m_aDesc, isSelected);
    }

    // close button
    if(m_Flags & MTFLAG_CLOSE_BUTTON)
    {
        int closeIndex = static_cast<int>(m_Points.size());
        int checksY = startLineY + closeIndex * linesizeY;
        int checkeY = startLineY + (closeIndex + 1) * linesizeY;
        bool isCloseSelected = (targetX > -196 && targetX < 196 && targetY >= checksY && targetY < checkeY);

        if(isCloseSelected && CEventKeyManager::IsKeyClicked(m_ClientID, KEY_EVENT_FIRE))
        {
            ClearMotd(pGS, pChar->GetPlayer());
            return;
        }

        addLineToBuffer(closeIndex, "Close", isCloseSelected);
    }

    if(m_ScrollPos < maxScrollPos)
    {
        buffer += "▽ Scroll down ▽\n";
    }

    buffer += "\n" + m_Description;

    // updater buffer
    if(m_LastBuffer != buffer || pServer->Tick() >= m_ResendMotdTick)
    {
        m_LastBuffer = buffer;
        pGS->SendMotd(m_ClientID, buffer.c_str());
        m_ResendMotdTick = pServer->Tick() + pServer->TickSpeed();
    }
}

void MotdMenu::Send()
{
    const auto pGS = (CGS*)Instance::GameServerPlayer(m_ClientID);
    if(const auto pPlayer = pGS->GetPlayer(m_ClientID))
        pPlayer->m_vpInstanceMotdMenu.push_back(std::make_unique<MotdMenu>(*this));
}

void MotdMenu::ClearMotd(CGS* pGS, CPlayer* pPlayer)
{
    m_Points.clear();
    m_ScrollPos = 0;
    pGS->SendMotd(m_ClientID, "");
    pPlayer->m_vpInstanceMotdMenu.clear();
}
