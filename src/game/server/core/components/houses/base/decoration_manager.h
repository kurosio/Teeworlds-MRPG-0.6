#ifndef GAME_SERVER_COMPONENT_HOUSES_BASE_DECORATION_MANAGER_H
#define GAME_SERVER_COMPONENT_HOUSES_BASE_DECORATION_MANAGER_H

#include "interface_house.h"

class CGS;
class CHouse;
class CPlayer;
class EntityPoint;
class CEntityDrawboard;
class CEntityHouseDecoration;

class CDecorationManager
{
	std::string m_DecorationTableName {};
	CEntityDrawboard* m_pDrawBoard {};
	IHouse* m_pHouse {};

public:
	CGS* GS() const;
	CDecorationManager(IHouse* pHouse, const std::string& DecorationTableName)
	{
		m_DecorationTableName = DecorationTableName;
		m_pHouse = pHouse;
		Init();
	}
	~CDecorationManager();

	bool StartDrawing(CPlayer* pPlayer) const;
	bool EndDrawing(CPlayer* pPlayer);
	bool Add(const EntityPoint* pPoint) const;
	bool Remove(const EntityPoint* pPoint) const;
	bool HasFreeSlots() const;
	IHouse* GetHouse() const { return m_pHouse; }

private:
	void Init();
};

#endif