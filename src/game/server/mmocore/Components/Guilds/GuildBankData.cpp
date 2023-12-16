/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GuildBankData.h"

#include "game/server/gamecontext.h"
#include "GuildData.h"

CPlayer* CGuildBankData::GetPlayer() const { return m_pGS->GetPlayerByUserID(*m_pAccountID); }

void CGuildBankData::Add(int Value)
{
	CPlayer* pPlayer = GetPlayer();
	if(!pPlayer)
		return;

	ResultPtr pRes = Database->Execute<DB::SELECT>("ID, HouseBank", TW_GUILD_TABLE, "WHERE UserID = '%d'", *m_pAccountID);

	if(pRes->next())
	{
		int HouseID = pRes->getInt("ID");

		if(pPlayer->Account()->SpendCurrency(Value))
		{
			m_Bank = pRes->getInt("HouseBank") + Value;
			Database->Execute<DB::UPDATE>(TW_GUILD_TABLE, "HouseBank = '%d' WHERE ID = '%d'", m_Bank, HouseID);

			int ClientID = pPlayer->GetCID();
			m_pGS->Chat(ClientID, "You put {VAL} gold in the safe, now {VAL}!", Value, m_Bank);
		}
	}
}

void CGuildBankData::Take(int Value)
{
	CPlayer* pPlayer = GetPlayer();
	if(!pPlayer)
		return;

	ResultPtr pRes = Database->Execute<DB::SELECT>("ID, HouseBank", TW_GUILD_TABLE, "WHERE UserID = '%d'", *m_pAccountID);
	if(pRes->next())
	{
		int ClientID = pPlayer->GetCID();

		int HouseID = pRes->getInt("ID");
		int Bank = pRes->getInt("HouseBank");

		Value = minimum(Value, Bank);

		if(Value > 0)
		{
			pPlayer->Account()->AddGold(Value);

			m_Bank = Bank - Value;

			Database->Execute<DB::UPDATE>(TW_GUILD_TABLE, "HouseBank = '%d' WHERE ID = '%d'", m_Bank, HouseID);

			m_pGS->Chat(ClientID, "You take {VAL} gold in the safe {VAL}!", Value, m_Bank);
		}
	}
}