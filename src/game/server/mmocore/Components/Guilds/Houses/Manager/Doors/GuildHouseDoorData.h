/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_GUILD_HOUSE_DOOR_DATA_H
#define GAME_SERVER_COMPONENT_GUILD_HOUSE_DOOR_DATA_H

class CHouseData;
class CGameWorld;
class CEntityHouseDoor;
class CHouseDoorsController;

class CGuildHouseDoor
{
	friend CHouseDoorsController;
	std::string m_Name {};
	CEntityHouseDoor* m_pDoor {};
	vec2 m_Pos {};

public:
	CGuildHouseDoor(CGameWorld* pWorld, CHouseData* pHouse, std::string&& Name, vec2 Pos);
	~CGuildHouseDoor();

	const char* GetName() const { return m_Name.c_str(); }
	vec2 GetPos() const { return m_Pos; }
	bool IsClosed() const;

	void Open() const;
	void Close() const;
};

#endif