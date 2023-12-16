/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_GUILD_BANK_DATA_H
#define GAME_SERVER_COMPONENT_GUILD_BANK_DATA_H

class CGS;
class CPlayer;
class CGuildData;

// This class represents the bank data for a guild in a game
class CGuildBankController
{
	CGuildData* m_pGuild {};
	int m_Bank {};

	CGS* GS() const;

public:
	// Constructor that initializes the bank data with the game server, account ID, and initial bank value
	CGuildBankController(int Bank, CGuildData* pGuild) : m_pGuild(pGuild), m_Bank(Bank) {}

	// Returns the current bank value
	int Get() const { return m_Bank; }

	// Adds the specified value to the bank
	void Add(int Value, CPlayer* pPlayer);

	// Takes the specified value from the bank
	void Take(int Value, CPlayer* pPlayer);

	// Resets the bank value to 0
	void Reset() { m_Bank = 0; }
};

#endif