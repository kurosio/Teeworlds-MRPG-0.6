/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_ATTRIBUTEDATA_H
#define GAME_SERVER_COMPONENT_ATTRIBUTEDATA_H

using AttributeDescriptionPtr = std::shared_ptr< class CAttributeDescription >;

class CAttributeDescription : public MultiworldIdentifiableData< std::map < AttributeIdentifier, AttributeDescriptionPtr > >
{
	char m_aName[32]{};
	AttributeIdentifier m_ID{};
	AttributeGroup m_Group{};
	int m_UpgradePrice{};

public:
	static AttributeDescriptionPtr CreateElement(AttributeIdentifier ID)
	{
		m_pData[ID] = std::make_shared<CAttributeDescription>();
		m_pData[ID]->m_ID = ID;
		return m_pData[ID];
	}

	void Init(const std::string& Name, int UpgradePrice, AttributeGroup Group)
	{
		str_copy(m_aName, Name.c_str(), sizeof(m_aName));
		m_UpgradePrice = UpgradePrice;
		m_Group = Group;
	}

	const char* GetName() const
	{
		return m_aName;
	}

	bool IsGroup(AttributeGroup Type) const
	{
		return m_Group == Type;
	}

	int GetUpgradePrice() const
	{
		return m_UpgradePrice;
	}

	AttributeGroup GetGroup() const
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
	CAttribute(AttributeIdentifier ID, int Value)
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
		return CAttributeDescription::Data()[m_ID].get();
	}
};

#endif