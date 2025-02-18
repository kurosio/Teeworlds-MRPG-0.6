#include "bank_manager.h"
#include <game/server/gamecontext.h>

CGS* CBankManager::GS() const
{
	return m_pHouse->GS();
}


CPlayer* CBankManager::GetPlayer() const
{
	return m_pHouse->GetPlayer();
}


void CBankManager::Add(int Value)
{
	// check valid
	auto* pPlayer = GetPlayer();
	if(!pPlayer)
		return;


	// try spend value
	if(!pPlayer->Account()->SpendCurrency(Value))
		return;

	// update database
	m_Bank += Value;
	Database->Execute<DB::UPDATE>(m_pHouse->GetTableName(),
		"Bank = '{}' WHERE ID = '{}'",
		m_Bank, m_pHouse->GetID());
	GS()->Chat(pPlayer->GetCID(), "You put '{} gold' in the safe, now '{}'!", Value, m_Bank);
}

void CBankManager::Take(int Value)
{
	// check valid
	auto* pPlayer = GetPlayer();
	if(!pPlayer || m_Bank <= 0)
		return;


	// initialize variables
	auto ClientID = pPlayer->GetCID();
	Value = minimum(Value, m_Bank.to_clamp<int>());

	// try update
	if(Value > 0)
	{
		m_Bank -= Value;
		pPlayer->Account()->AddGold(Value);
		Database->Execute<DB::UPDATE>(m_pHouse->GetTableName(),
			"Bank = '{}' WHERE ID = '{}'",
			m_Bank, m_pHouse->GetID());
		GS()->Chat(ClientID, "You take '{} gold' in the safe '{}'!", Value, m_Bank);
	}
}

bool CBankManager::Spend(int Value)
{
	// check valid
	if(m_Bank <= 0 || m_Bank < Value)
		return false;


	// update
	m_Bank -= Value;
	Database->Execute<DB::UPDATE>(m_pHouse->GetTableName(),
		"Bank = '{}' WHERE ID = '{}'",
		m_Bank, m_pHouse->GetID());
	return true;
}
