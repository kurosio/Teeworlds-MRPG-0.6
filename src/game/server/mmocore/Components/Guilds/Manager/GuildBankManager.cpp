/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GuildBankManager.h"

#include <game/server/gamecontext.h>
#include "../GuildData.h"

CGS* CGuildBankController::GS() const { return m_pGuild->GS(); }

void CGuildBankController::Add(int Value, CPlayer* pByPlayer)
{
	if(pByPlayer)
	{
		ResultPtr pRes = Database->Execute<DB::SELECT>("Bank", TW_GUILD_TABLE, "WHERE ID = '%d'", m_pGuild->GetID());
		if(pRes->next())
		{
			if(pByPlayer->Account()->SpendCurrency(Value))
			{
				m_Bank = pRes->getInt("Bank") + Value;
				Database->Execute<DB::UPDATE>(TW_GUILD_TABLE, "Bank = '%d' WHERE ID = '%d'", m_Bank, m_pGuild->GetID());

				int ClientID = pByPlayer->GetCID();
				GS()->Chat(ClientID, "You put {VAL} gold in the safe, now {VAL}!", Value, m_Bank);
			}
		}
	}
}

void CGuildBankController::Take(int Value, CPlayer* pByPlayer)
{
	if(pByPlayer)
	{
		ResultPtr pRes = Database->Execute<DB::SELECT>("Bank", TW_GUILD_TABLE, "WHERE ID = '%d'", m_pGuild->GetID());
		if(pRes->next())
		{
			int ClientID = pByPlayer->GetCID();

			int Bank = pRes->getInt("Bank");
			Value = minimum(Value, Bank);
			if(Value > 0)
			{
				pByPlayer->Account()->AddGold(Value);

				m_Bank = Bank - Value;

				Database->Execute<DB::UPDATE>(TW_GUILD_TABLE, "Bank = '%d' WHERE ID = '%d'", m_Bank, m_pGuild->GetID());
				GS()->Chat(ClientID, "You take {VAL} gold in the safe {VAL}!", Value, m_Bank);
			}
		}
	}
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
