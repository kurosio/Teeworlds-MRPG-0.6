/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_COMPONENTS_INVENTORY_ATTRIBUTE_DATA_H
#define GAME_SERVER_CORE_COMPONENTS_INVENTORY_ATTRIBUTE_DATA_H

using AttributeDescriptionPtr = std::shared_ptr< class CAttributeDescription >;

class CAttributeDescription : public MultiworldIdentifiableData< std::map < AttributeIdentifier, AttributeDescriptionPtr > >
{
	std::string m_Name;
	AttributeIdentifier m_ID{};
	AttributeGroup m_Group{};
	int m_UpgradePrice{};

public:
	static AttributeDescriptionPtr CreateElement(AttributeIdentifier ID)
	{
		auto element = std::make_shared<CAttributeDescription>();
		element->m_ID = ID;
		m_pData[ID] = element;
		return element;
	}

	void Init(const std::string& Name, int UpgradePrice, AttributeGroup Group)
	{
		m_Name = Name;
		m_UpgradePrice = UpgradePrice;
		m_Group = Group;
	}

	const char* GetName() const noexcept
	{
		return m_Name.c_str();
	}

	bool IsGroup(AttributeGroup Type) const noexcept
	{
		return m_Group == Type;
	}

	int GetUpgradePrice() const noexcept
	{
		return m_UpgradePrice;
	}

	AttributeGroup GetGroup() const noexcept
	{
		return m_Group;
	}

	inline static std::optional<float> CalculateChance(AttributeIdentifier ID, int Value)
	{
		float multiplier = 0.0f;
		switch(ID)
		{
			case AttributeIdentifier::AttackSPD: multiplier = (MAX_PERCENT_ATTRIBUTE_ATTACK_SPEED / g_Config.m_SvReachValueMaxAttackSpeed); break;
			case AttributeIdentifier::AmmoRegen: multiplier = (MAX_PERCENT_ATTRIBUTE_AMMO_REGEN / g_Config.m_SvReachValueMaxAmmoRegen); break;
			case AttributeIdentifier::Vampirism: multiplier = (MAX_PERCENT_ATTRIBUTE_VAMPIRISM / g_Config.m_SvReachValueMaxVampirism); break;
			case AttributeIdentifier::Crit: multiplier = (MAX_PERCENT_ATTRIBUTE_CRIT_CHANCE / g_Config.m_SvReachValueMaxCritChance); break;
			case AttributeIdentifier::Lucky: multiplier = (MAX_PERCENT_ATTRIBUTE_LUCKY / g_Config.m_SvReachValueMaxLucky); break;
			case AttributeIdentifier::LuckyDropItem: multiplier = (MAX_PERCENT_ATTRIBUTE_LUCKY_DROP / g_Config.m_SvReachValueMaxLuckyDrop); break;
			default: multiplier = 0.f;
		}

		return multiplier <= 0.f ? std::nullopt : std::make_optional<float>(static_cast<float>(Value) * multiplier);
	}
};

class CAttribute
{
	AttributeIdentifier m_ID{};
	int m_Value{};

public:
	CAttribute() = default;
	explicit CAttribute(AttributeIdentifier ID, int Value)
		: m_ID(ID), m_Value(Value)
	{}

	void SetID(AttributeIdentifier ID)
	{
		m_ID = ID;
	}

	void SetValue(int Value)
	{
		m_Value = Value;
	}

	AttributeIdentifier GetID() const
	{
		return m_ID;
	}

	int GetValue() const
	{
		return m_Value;
	}

	bool HasValue() const
	{
		return m_Value > 0;
	}

	CAttributeDescription* Info() const
	{
		auto it = CAttributeDescription::Data().find(m_ID);
		dbg_assert(it != CAttributeDescription::Data().end(), "Attribute ID not found in data");
		return it->second.get();
	}
};

#endif