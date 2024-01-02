/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GuildBankManager.h"

#include <game/server/gamecontext.h>
#include "../GuildData.h"

CGS* CGuildBankController::GS() const { return m_pGuild->GS(); }

void CGuildBankController::Set(int Value)
{
	m_Bank = Value;
	Database->Execute<DB::UPDATE>(TW_GUILD_TABLE, "Bank = '%d' WHERE ID = '%d'", m_Bank, m_pGuild->GetID());
}

bool CGuildBankController::Spend(int Value)
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("Bank", TW_GUILD_TABLE, "WHERE ID = '%d'", m_pGuild->GetID());
	if(pRes->next())
	{
		int Bank = pRes->getInt("Bank");
		if(Bank >= Value)
		{
			m_Bank = Bank - Value;
			Database->Execute<DB::UPDATE>(TW_GUILD_TABLE, "Bank = '%d' WHERE ID = '%d'", m_Bank, m_pGuild->GetID());
			return true;
		}
	}

	return false;
}
