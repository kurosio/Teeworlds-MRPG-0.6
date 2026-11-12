#ifndef GAME_SERVER_COMPONENT_HOUSE_ENTITIES_DOOR_H
#define GAME_SERVER_COMPONENT_HOUSE_ENTITIES_DOOR_H

#include <game/server/entity.h>

#include <components/houses/base/interface_house.h>

class CHouse;
class CGuildHouse;

class CEntityHouseDoor : public CEntity
{
	enum class State
	{
		Closed,
		Opened
	};

	IHouse* m_pHouse {};
	std::string m_Name {};
	vec2 m_PosControll {};
	State m_State {};

public:
	CEntityHouseDoor(CGameWorld* pGameWorld, IHouse* pHouse, const std::string& Name, vec2 Pos);

	void Tick() override;
	void Snap(int SnappingClient) override;

	void Open();
	void Close();
	void Reverse();

	constexpr bool IsClosed() const { return m_State == State::Closed; }
	const std::string& GetName() const { return m_Name; }

private:
	bool PlayerHouseTick(CHouse* pHouse);
	bool GuildHouseTick(CGuildHouse* pHouse);
};

#endif
