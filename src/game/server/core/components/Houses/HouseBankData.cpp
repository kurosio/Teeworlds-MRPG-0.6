/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "HouseBankData.h"

#include "game/server/gamecontext.h"
#include "HouseData.h"

// Returns the player associated with the house
CPlayer* CHouseBankData::GetPlayer() const { return m_pGS->GetPlayerByUserID(*m_pAccountID); }

// Adds the specified value to the bank
void CHouseBankData::Add(int Value)
{
	// Get the pointer to the player object using the GetPlayer() function
	CPlayer* pPlayer = GetPlayer();
	if(!pPlayer)
		return;

	// Execute a SELECT query on the database to retrieve the ID and HouseBank columns from the TW_HOUSES_TABLE
	// where the UserID is equal to the value of m_pAccountID
	ResultPtr pRes = Database->Execute<DB::SELECT>("ID, HouseBank", TW_HOUSES_TABLE, "WHERE UserID = '%d'", *m_pAccountID);

	// Check if there is a result from the query
	if(pRes->next())
	{
		// Retrieve the value of the ID column from the result
		int HouseID = pRes->getInt("ID");

		// Check if the player has enough currency to spend the specified value
		if(pPlayer->Account()->SpendCurrency(Value))
		{
			// Update the value of the HouseBank column in the TW_HOUSES_TABLE by adding the specified value to the current value
			m_Bank = pRes->getInt("HouseBank") + Value;
			Database->Execute<DB::UPDATE>(TW_HOUSES_TABLE, "HouseBank = '%d' WHERE ID = '%d'", m_Bank, HouseID);

			// Send a chat message to the player indicating the amount of gold they have put in the safe
			int ClientID = pPlayer->GetCID();
			m_pGS->Chat(ClientID, "You put {c} gold in the safe, now {c}!", Value, m_Bank);
		}
	}
}

// Takes the specified value from the bank
void CHouseBankData::Take(int Value)
{
	// Get the pointer to the player object using the GetPlayer() function
	CPlayer* pPlayer = GetPlayer();
	if(!pPlayer)
		return;

	// Execute a SELECT query on the database to retrieve the ID and HouseBank columns from the TW_HOUSES_TABLE table where the UserID matches the value of m_pAccountID
	ResultPtr pRes = Database->Execute<DB::SELECT>("ID, HouseBank", TW_HOUSES_TABLE, "WHERE UserID = '%d'", *m_pAccountID);
	if(pRes->next())
	{
		// Get the ClientID of the player
		int ClientID = pPlayer->GetCID();

		// Get the HouseID and Bank values from the result
		int HouseID = pRes->getInt("ID");
		int Bank = pRes->getInt("HouseBank");

		// Update the Value to be the minimum of Value and Bank
		Value = minimum(Value, Bank);

		// If Value is greater than 0
		if(Value > 0)
		{
			// Add Value to the player's money
			pPlayer->Account()->AddGold(Value);

			// Update the m_Bank variable to be Bank minus Value
			m_Bank = Bank - Value;

			// Execute an UPDATE query on the database to update the HouseBank column of the TW_HOUSES_TABLE table where the ID matches HouseID
			Database->Execute<DB::UPDATE>(TW_HOUSES_TABLE, "HouseBank = '%d' WHERE ID = '%d'", m_Bank, HouseID);

			// Send a message to the client with the updated information
			m_pGS->Chat(ClientID, "You take {c} gold in the safe {c}!", Value, m_Bank);
		}
	}
}