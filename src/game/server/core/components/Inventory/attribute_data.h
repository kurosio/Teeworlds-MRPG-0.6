/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_ATTRIBUTEDATA_H
#define GAME_SERVER_COMPONENT_ATTRIBUTEDATA_H

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