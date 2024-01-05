/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GuildBankManager.h"

#include <game/server/gamecontext.h>
#include "../GuildData.h"

CGS* CGuildBankManager::GS() const { return m_pGuild->GS(); }

void CGuildBankManager::Set(int Value)
{
	m_Bank = Value;
	Database->Execute<DB::UPDATE>(TW_GUILDS_TABLE, "Bank = '%d' WHERE ID = '%d'", m_Bank, m_pGuild->GetID());
}

bool CGuildBankManager::Spend(int Value)
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("Bank", TW_GUILDS_TABLE, "WHERE ID = '%d'", m_pGuild->GetID());
	if(pRes->next())
	{
		int Bank = pRes->getInt("Bank");
		if(Bank >= Value)
		{
			m_Bank = Bank - Value;
			Database->Execute<DB::UPDATE>(TW_GUILDS_TABLE, "Bank = '%d' WHERE ID = '%d'", m_Bank, m_pGuild->GetID());
			return true;
		}
	}

	return false;
}
