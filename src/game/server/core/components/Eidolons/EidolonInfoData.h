/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_EIDOLONINFODATA_H
#define GAME_SERVER_COMPONENT_EIDOLONINFODATA_H

class CEidolonInfoData
{
public:
	using EidolonDescriptionList = std::vector< CEidolonInfoData >;
	CEidolonInfoData(int ItemID, int DataBotID, std::initializer_list<std::string> Lines) : m_ItemID(ItemID), m_DataBotID(DataBotID), m_LinesDescription(Lines) {}

private:
	int m_ItemID{};
	int m_DataBotID{};
	std::list <std::string> m_LinesDescription{};

	static EidolonDescriptionList m_EidolonsInfoData;

public:
	int GetItemID() const { return m_ItemID; }
	int GetDataBotID() const { return m_DataBotID; }

	class DataBotInfo* GetDataBot() const;
	class CItemDescription* GetItem() const;

	static EidolonDescriptionList& Data() { return m_EidolonsInfoData; }
	std::list<std::string> GetLinesDescription() const { return m_LinesDescription; }
};

#endif