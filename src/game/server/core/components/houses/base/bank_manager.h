#ifndef GAME_SERVER_COMPONENT_HOUSE_BANK_MANAGER_H
#define GAME_SERVER_COMPONENT_HOUSE_BANK_MANAGER_H

#include "interface_house.h"

class CGS;
class CPlayer;
class CBankManager
{
	CGS* GS() const;
	CPlayer* GetPlayer() const;
	IHouse* m_pHouse {};
	BigInt m_Bank {};

public:
	CBankManager(IHouse* pHouse, const BigInt& Bank)
		: m_pHouse(pHouse), m_Bank(Bank) { }

	BigInt Get() const { return m_Bank; }
	void Add(int Value);
	void Take(int Value);
	bool Spend(int Value);
	void Reset() { m_Bank = 0; }
};


#endif