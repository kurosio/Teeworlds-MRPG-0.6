/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_GROUP_DATA_H
#define GAME_SERVER_COMPONENT_GROUP_DATA_H

using GroupIdentifier = int;
class GroupData : public MultiworldIdentifiableStaticData< std::map< int, GroupData > >
{
	GroupIdentifier m_ID{};
	int m_OwnerUID{};
	int m_TeamColor {};
	std::unordered_map<int, bool> m_AccountIds{};

public:
	GroupData() = default;
	GroupData(GroupIdentifier ID) : m_ID(ID) { }

	void Init(int OwnerUID, int Color, class DBSet&& SetAccountIDs);
	bool Add(class CGS* pGS, int AccountID);
	bool Remove(class CGS* pGS, int AccountID);
	bool Disband();
	bool HasAccountID(int AccountID) const { return m_AccountIds.find(AccountID) != m_AccountIds.end(); };
	void ChangeOwner(class CGS* pGS, int AccountID);
	void ChangeColor(int NewColor);
	int GetTeamColor() const { return m_TeamColor; }

	GroupIdentifier GetID() const { return m_ID; }
	int OwnerUID() const { return m_OwnerUID; }
	std::unordered_map<int, bool>& GetAccounts() { return m_AccountIds; }

private:
	void Save() const;
};

#endif