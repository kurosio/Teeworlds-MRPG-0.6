#include "motd_menu.h"

#include <game/server/event_key_manager.h>
#include <game/server/gamecontext.h>

void MotdMenu::Handle()
{
	if(m_Points.empty())
		return;

	std::string buffer;
	const auto pServer = (IServer*)Instance::Server();
	const auto pGS = (CGS*)Instance::GameServerPlayer(m_ClientID);
	if(const auto pChar = pGS->GetPlayerChar(m_ClientID))
	{
		constexpr int linesizeY = 20;
		constexpr int startLineY = -280;
		int TargetX = pChar->m_Core.m_Input.m_TargetX;
		int TargetY = pChar->m_Core.m_Input.m_TargetY;

		for(size_t i = 0; i < m_Points.size(); ++i)
		{
			const int checksY = startLineY + (i * linesizeY);
			const int checkeY = startLineY + ((i + 1) * linesizeY);
			const bool isSelected = (TargetX > -196 && TargetX < 196 && TargetY >= checksY && TargetY < checkeY);

			if(isSelected)
			{
				if(CEventKeyManager::IsKeyClicked(m_ClientID, KEY_EVENT_FIRE))
				{
					const auto& command = m_Points[i].command;
					if(m_CommandHandlers[command] != nullptr)
					{
						m_CommandHandlers[command](m_ClientID);
					}
				}

				buffer += '#' + std::to_string(i) + ". " + m_Points[i].description + "\n";
			}
			else
			{
				buffer += std::to_string(i) + ". " + m_Points[i].description + "\n";
			}
		}

		buffer += "\n" + m_Description;
	}

	if(m_LastBuffer != buffer || pServer->Tick() % pServer->TickSpeed() == 0)
	{
		if(m_LastBuffer != buffer)
			m_LastBuffer = buffer;
		pGS->SendMotd(m_ClientID, buffer.c_str());
	}
}
