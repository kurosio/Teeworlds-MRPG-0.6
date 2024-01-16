/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_HOUSE_BANK_DATA_H
#define GAME_SERVER_COMPONENT_HOUSE_BANK_DATA_H

// This class represents the bank data for a house in a game
class CHouseBankData
{
	class CGS* m_pGS;
	int* m_pAccountID {};
	int m_Bank {};

	// Returns the player associated with the house
	class CPlayer* GetPlayer() const;

public:
	// Constructor that initializes the bank data with the game server, account ID, and initial bank value
	CHouseBankData(CGS* pGS, int* pAccountID, int Bank) : m_pGS(pGS), m_pAccountID(pAccountID), m_Bank(Bank) {}

	// Returns the current bank value
	int Get() const { return m_Bank; }

	// Adds the specified value to the bank
	void Add(int Value);

	// Takes the specified value from the bank
	void Take(int Value);

	// Resets the bank value to 0
	void Reset() { m_Bank = 0; }
};

#endif