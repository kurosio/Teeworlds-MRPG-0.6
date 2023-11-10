/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_GROUP_DATA_H
#define GAME_SERVER_COMPONENT_GROUP_DATA_H

using GroupIdentifier = int;
class GroupData : public MultiworldIdentifiableStaticData< std::unordered_map< int, GroupData > >
{
	GroupIdentifier m_ID{};
	std::list<int> m_AccountIds{};

public:
	GroupData() = default;
	GroupData(GroupIdentifier ID) : m_ID(ID) { }

	void Init(std::string AccessIdsList);
	bool Add(int AccountID);
	bool Remove(int AccountID);
	bool CheckGroup(int AccountID);

	std::list<int>& GetAccounts() { return m_AccountIds; }

private:
	void Save() const;
};

#endif