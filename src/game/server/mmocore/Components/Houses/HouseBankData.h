/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_HOUSE_BANK_DATA_H
#define GAME_SERVER_COMPONENT_HOUSE_BANK_DATA_H

class CHouseBankData
{
	class CGS* m_pGS;
	int* m_pAccountID{};
	int m_Bank{};

	class CPlayer* GetPlayer() const;

public:
	CHouseBankData(CGS* pGS, int* pAccountID, int Bank) : m_pGS(pGS), m_pAccountID(pAccountID), m_Bank(Bank) {};

	int Get() const { return m_Bank; }

	void Add(int Value);
	void Take(int Value);
	void Reset() { m_Bank = 0; }
};

#endif