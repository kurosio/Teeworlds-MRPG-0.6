#include "motd_menu.h"

#include <game/server/gamecontext.h>

void MotdMenu::AddImpl(int extra, int extra2, std::string_view command, const std::string& description)
{
	Point p;
	str_copy(p.m_aDesc, description.c_str(), sizeof(p.m_aDesc));
	str_utf8_fix_truncation(p.m_aDesc);
	p.m_Command = command;
	p.m_Extra = extra;
	p.m_Extra2 = extra2;
	m_Points.push_back(p);
	m_ScrollManager.SetMaxScrollPos(static_cast<int>(m_Points.size()));
}

void MotdMenu::Tick()
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
	constexpr int lineSizeY = 20;
	constexpr int startLineY = -284;
	const int targetX = pChar->m_Core.m_Input.m_TargetX;
	const int targetY = pChar->m_Core.m_Input.m_TargetY;

	// prepare the buffer for the MOTD
	int linePos = 2;
	std::string buffer = Instance::Localize(m_ClientID, "* Use 'Self kill' to close the motd!\n\n");
	auto addScrollBar = [&](int index)
	{
		// initialize variables
		const auto totalItems = m_Points.size();
		const auto currentScrollPos = m_ScrollManager.GetScrollPos();
		const auto visibleItems = m_ScrollManager.GetMaxVisibleItems();

		const auto visibleProportion = static_cast<float>(visibleItems) / static_cast<float>(totalItems);
		auto scrollBarHeight = round_to_int(visibleProportion * visibleItems);
		if(scrollBarHeight < 1)
			scrollBarHeight = 1;

		// calculate position
		const auto progress = static_cast<float>(currentScrollPos) / static_cast<float>(totalItems - visibleItems);
		const auto scrollBarPosition = static_cast<int>(progress * (visibleItems - scrollBarHeight));

		// get current index
		const auto currentBar = index - currentScrollPos;
		const char* pSymbol = currentBar >= scrollBarPosition && currentBar < scrollBarPosition + scrollBarHeight ? "\u258D" : "\u258F";
		buffer.append(pSymbol);
	};

	auto addLineToBuffer = [&](const std::string& line, bool isSelected)
	{
		buffer.append(isSelected ? "\u279C " : "\u257E ");
		buffer.append(line);
		buffer.append("\n");
	};

	// key events and freeze input is hovered worked area
	const int startWorkedAreaY = startLineY;
	const int endWorkedAreaY = startLineY + 24 * lineSizeY;
	if((targetX > -196 && targetX < 196 && targetY >= startWorkedAreaY && targetY < endWorkedAreaY))
	{
		// handle scrolling with key events
		if(pServer->Input()->IsKeyClicked(m_ClientID, KEY_EVENT_NEXT_WEAPON))
		{
			m_ScrollManager.ScrollUp();
			m_ResendMotdTick = pServer->Tick() + 5;
		}
		else if(pServer->Input()->IsKeyClicked(m_ClientID, KEY_EVENT_PREV_WEAPON))
		{
			m_ScrollManager.ScrollDown();
			m_ResendMotdTick = pServer->Tick() + 5;
		}

		pServer->Input()->BlockInputGroup(m_ClientID, BLOCK_INPUT_FIRE | BLOCK_INPUT_FREEZE_HAMMER);
	}

	// add menu items to buffer
	for(int i = m_ScrollManager.GetScrollPos(); i < m_ScrollManager.GetEndScrollPos() && i < static_cast<int>(m_Points.size()); ++i, ++linePos)
	{
		// add scrollbar symbol
		addScrollBar(i);

		// empty command only text
		const auto& command = m_Points[i].m_Command;
		if(command == "NULL")
		{
			buffer.append(m_Points[i].m_aDesc).append("\n");
			continue;
		}

		// initialize variables
		const int checkYStart = startLineY + linePos * lineSizeY;
		const int checkYEnd = startLineY + (linePos + 1) * lineSizeY;
		const bool isSelected = (targetX > -196 && targetX < 196 && targetY >= checkYStart && targetY < checkYEnd);

		// is hovered and clicked
		if(isSelected && pServer->Input()->IsKeyClicked(m_ClientID, KEY_EVENT_FIRE))
		{
			bool updatedMotd = false;
			const auto& extra = m_Points[i].m_Extra;

			if(command == "CLOSE")
			{
				ClearMotd(pServer, pChar->GetPlayer());
				return;
			}

			if(command == "MENU")
			{
				updatedMotd = true;
				m_Menulist = extra;
				m_MenuExtra = m_Points[i].m_Extra2 <= NOPE ? std::nullopt : std::make_optional<int>(m_Points[i].m_Extra2);
			}

			if(command == "BACKPAGE")
			{
				updatedMotd = true;
				m_Menulist = m_LastMenulist;
			}

			if(pGS->OnClientMotdCommand(m_ClientID, command.c_str(), extra))
			{
				if(m_Flags & MTFLAG_CLOSE_ON_SELECT)
				{
					ClearMotd(pServer, pChar->GetPlayer());
					return;
				}

				updatedMotd = true;
			}

			if(updatedMotd)
			{
				UpdateMotd(pServer, pGS, pChar->GetPlayer());
				return;
			}
		}

		addLineToBuffer(m_Points[i].m_aDesc, isSelected);
	}
	buffer += "\n" + m_Description;

	// update buffer if it has changed
	if(m_LastBuffer != buffer)
	{
		m_LastBuffer = buffer;
		m_ResendMotdTick = pServer->Tick() + pServer->TickSpeed();
		pServer->SendMotd(m_ClientID, buffer.c_str());
	}
	else if(pServer->Tick() >= m_ResendMotdTick)
	{
		m_ResendMotdTick = pServer->Tick() + pServer->TickSpeed();
		UpdateMotd(pServer, pGS, pChar->GetPlayer());
	}
}

void MotdMenu::Send(int Menulist)
{
	// add close button if necessary
	if(m_Flags & MTFLAG_CLOSE_BUTTON)
	{
		AddImpl(NOPE, NOPE, "CLOSE", "Close");
	}

	// reinitialize player motd menu
	const auto* pGS = (CGS*)Instance::GameServerPlayer(m_ClientID);
	if(auto* pPlayer = pGS->GetPlayer(m_ClientID))
	{
		m_Menulist = Menulist;
		if(pPlayer->m_pMotdMenu)
		{
			m_LastMenulist = (pPlayer->m_pMotdMenu->m_Menulist != Menulist) ? pPlayer->m_pMotdMenu->m_Menulist : pPlayer->m_pMotdMenu->m_LastMenulist;
		}
		pPlayer->m_pMotdMenu = std::make_unique<MotdMenu>(*this);
	}
}

void MotdMenu::ClearMotd(IServer* pServer, CPlayer* pPlayer)
{
	m_Points.clear();
	pServer->SendMotd(m_ClientID, "");
	if(pPlayer && pPlayer->m_pMotdMenu)
	{
		pPlayer->m_pMotdMenu.reset();
	}
}

void MotdMenu::UpdateMotd(IServer* pServer, CGS* pGS, CPlayer* pPlayer)
{
	ScrollManager oldScrollData = m_ScrollManager;
	const auto oldMenuExtra = m_MenuExtra;

	m_ResendMotdTick = pServer->Tick() + pServer->TickSpeed();
	pGS->SendMenuMotd(pPlayer, m_Menulist);
	oldScrollData.SetMaxScrollPos((int)pPlayer->m_pMotdMenu->m_Points.size());

	pPlayer->m_pMotdMenu->m_ScrollManager = oldScrollData;
	pPlayer->m_pMotdMenu->m_MenuExtra = oldMenuExtra;
}
