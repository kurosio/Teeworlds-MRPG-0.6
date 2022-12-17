/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "HouseBankData.h"

#include "game/server/gamecontext.h"

// house bank data
CPlayer* CHouseBankData::GetPlayer() const { return m_pGS->GetPlayerFromUserID(*m_pAccountID); }

void CHouseBankData::Add(int Value)
{
	CPlayer* pPlayer = GetPlayer();
	if(!pPlayer)
		return;

	ResultPtr pRes = Database->Execute<DB::SELECT>("ID, HouseBank", "tw_houses", "WHERE UserID = '%d'", *m_pAccountID);
	if(pRes->next())
	{
		int HouseID = pRes->getInt("ID");

		if(pPlayer->SpendCurrency(Value))
		{
			m_Bank = pRes->getInt("HouseBank") + Value;
			Database->Execute<DB::UPDATE>("tw_houses", "HouseBank = '%d' WHERE ID = '%d'", m_Bank, HouseID);

			int ClientID = pPlayer->GetCID();
			m_pGS->Chat(ClientID, "You put {VAL} gold in the safe {VAL}!", Value, m_Bank);
		}
	}
}

void CHouseBankData::Take(int Value)
{
	CPlayer* pPlayer = GetPlayer();
	if(!pPlayer)
		return;

	ResultPtr pRes = Database->Execute<DB::SELECT>("ID, HouseBank", "tw_houses", "WHERE UserID = '%d'", *m_pAccountID);
	if(pRes->next())
	{
		int ClientID = pPlayer->GetCID();
		int HouseID = pRes->getInt("ID");
		int Bank = pRes->getInt("HouseBank");

		// update data
		Value = min(Value, Bank);
		if(Value > 0)
		{
			pPlayer->AddMoney(Value);
			m_Bank = Bank - Value;
			Database->Execute<DB::UPDATE>("tw_houses", "HouseBank = '%d' WHERE ID = '%d'", m_Bank, HouseID);

			// send information
			m_pGS->Chat(ClientID, "You take {VAL} gold in the safe {VAL}!", Value, m_Bank);
		}
	}
}