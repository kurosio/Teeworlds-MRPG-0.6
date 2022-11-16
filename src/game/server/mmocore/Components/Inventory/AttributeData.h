/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_ATTRIBUTEDATA_H
#define GAME_SERVER_COMPONENT_ATTRIBUTEDATA_H

using AttributeDescriptionPtr = std::shared_ptr< class CAttributeDescription >;

class CAttributeDescription : public MultiworldIdentifiableStaticData< std::map < AttributeIdentifier, AttributeDescriptionPtr > >
{
	char m_aName[32]{};
	char m_aFieldName[32]{};
	AttributeIdentifier m_ID{};
	AttributeType m_Type{};
	int m_UpgradePrice{};
	int m_Dividing{};

public:
	CAttributeDescription() = default;

	static AttributeDescriptionPtr CreateDataItem(AttributeIdentifier ID)
	{
		m_pData[ID] = std::make_shared<CAttributeDescription>();
		m_pData[ID]->m_ID = ID;
		return m_pData[ID];
	}

	void Init(const std::string& Name, const std::string& FieldName, int UpgradePrice, AttributeType Type, int Dividing)
	{
		str_copy(m_aName, Name.c_str(), sizeof(m_aName));
		str_copy(m_aFieldName, FieldName.c_str(), sizeof(m_aFieldName));
		m_UpgradePrice = UpgradePrice;
		m_Type = Type;
		m_Dividing = Dividing;
	}

	static bool IsValidItem(AttributeIdentifier ID) { return m_pData.find(ID) != m_pData.end(); }

	const char* GetName() const { return m_aName; }
	const char* GetFieldName() const { return m_aFieldName; }
	bool HasField() const { return m_aFieldName[0] != '\0' && m_UpgradePrice > 0; }
	int GetUpgradePrice() const { return m_UpgradePrice; }
	bool IsType(AttributeType Type) const { return m_Type == Type; }
	AttributeType GetType() const { return m_Type; }
	int GetDividing() const { return m_Dividing; }
};

class CAttribute
{
	AttributeIdentifier m_ID{};
	int m_Value{};

public:
	CAttribute() = default;
	CAttribute(AttributeIdentifier ID, int Value) : m_ID(ID), m_Value(Value) {}

	void SetID(AttributeIdentifier ID) { m_ID = ID; }
	void SetValue(int Value) { m_Value = Value; }

	AttributeIdentifier GetID() const { return m_ID; }
	int GetValue() const { return m_Value; }
	bool HasValue() const { return m_Value > 0; }
	CAttributeDescription* Info() const { return CAttributeDescription::Data()[m_ID].get(); }
};

#endif