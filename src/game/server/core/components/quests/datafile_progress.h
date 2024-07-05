/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_COMPONENTS_QUESTS_DATAFILE_PROGRESS_H
#define GAME_SERVER_CORE_COMPONENTS_QUESTS_DATAFILE_PROGRESS_H

class CPlayerQuest;
class QuestDatafile
{
	CPlayerQuest* m_pQuest{};

public:
	void Init(CPlayerQuest* pQuest) { m_pQuest = pQuest; }
	void Create() const;
	void Load() const;
	bool Save() const;
	void Delete() const;
	std::string GetFilename() const;
};

#endif