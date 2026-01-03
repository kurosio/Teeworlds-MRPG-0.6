#ifndef GAME_SERVER_CORE_BALANCE_BALANCE_H
#define GAME_SERVER_CORE_BALANCE_BALANCE_H

#include <array>
#include <string>

#include <game/server/core/mmo_context.h>

class Balance
{
public:
	struct DungeonFactor
	{
		float BaseFactor {};
		int MinValue {};
	};

	static Balance& Get();
	static void Init();

	float GetAttributeCap(AttributeIdentifier ID) const;
	int GetAttributeBase(AttributeIdentifier ID) const;
	DungeonFactor GetDungeonFactor(AttributeIdentifier ID, bool IsBoss) const;
	float GetBotGroupPercent(AttributeGroup Group) const;
	float GetBotBossDownscaleDivider() const;

private:
	static constexpr size_t kAttributeCount = static_cast<size_t>(AttributeIdentifier::ATTRIBUTES_NUM) + 1;

	struct DungeonFactors
	{
		float DamagePercent {};
		float CritPercent {};
		float OtherPercent {};
		float BossHpPercent {};
		int MinValue {};
		int BossMinValue {};
	};

	struct BotGroupScaling
	{
		float DamagePercent {};
		float DpsPercent {};
		float HealerPercent {};
		float BossDownscaleDivider {};
	};

	std::array<float, kAttributeCount> m_AttributeCaps {};
	std::array<int, kAttributeCount> m_AttributeBase {};
	DungeonFactors m_DungeonFactors;
	BotGroupScaling m_BotGroupScaling;

	static size_t ToIndex(AttributeIdentifier ID);
	void Initialize();
};

#endif
