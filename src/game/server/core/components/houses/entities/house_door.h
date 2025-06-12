#ifndef GAME_SERVER_COMPONENT_HOUSE_ENTITIES_DOOR_H
#define GAME_SERVER_COMPONENT_HOUSE_ENTITIES_DOOR_H

#include <game/server/entity.h>

#include "../base/interface_house.h"

class CHouse;
class CGuildHouse;

class CDoorDurability
{
	int m_Health {};
	int m_LastDamageTick {};
	IHouse* m_pHouse {};

public:
	CDoorDurability() = default;

	void Init(IHouse* pHouse);
	bool IncreaseHealth(int Health);
	bool TakeDamage(int Damage);
	void Tick();

	int GetHealth() const { return m_Health; }
	int GetMaxHealth() const;
	int GetTickShift() const
	{
		const int MaxHealth = GetMaxHealth();
		if(MaxHealth == 0)
			return 0;
		return ((MaxHealth - m_Health) * 6) / MaxHealth;
	}
	constexpr bool IsDestroyed() const { return m_Health <= 0; }
};

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

	CDoorDurability m_DurabilityManager {};

public:
	CEntityHouseDoor(CGameWorld* pGameWorld, IHouse* pHouse, const std::string& Name, vec2 Pos);

	void Tick() override;
	void Snap(int SnappingClient) override;

	void Open() { m_State = State::Opened; }
	void Close() { m_State = State::Closed; }
	void Reverse();

	constexpr bool IsClosed() const { return m_State == State::Closed; }
	const std::string& GetName() const { return m_Name; }

private:
	bool PlayerHouseTick(CHouse* pHouse);
	bool GuildHouseTick(CGuildHouse* pHouse);
};

#endif