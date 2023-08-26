/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_TUTORIAL_EVENT_DATA_H
#define GAME_SERVER_COMPONENT_TUTORIAL_EVENT_DATA_H

enum class TutorialType : int
{
	// vector
	TUTORIAL_MOVE_TO = 6,
	// integer
	TUTORIAL_EQUIP,
	TUTORIAL_PLAYER_FLAG,
	TUTORIAL_OPEN_VOTE_MENU,
	TUTORIAL_ACCEPT_QUEST,
	TUTORIAL_FINISHED_QUEST,
	// string
	TUTORIAL_CHAT_MSG,

	TUTORIAL_VECTOR_ONE = 1 << TUTORIAL_MOVE_TO,
	TUTORIAL_INTEGER_ONE = 1 << TUTORIAL_EQUIP | 1 << TUTORIAL_PLAYER_FLAG | 1 << TUTORIAL_OPEN_VOTE_MENU | 1 << TUTORIAL_ACCEPT_QUEST | 1 << TUTORIAL_FINISHED_QUEST,
	TUTORIAL_STRING_ONE = 1 << TUTORIAL_CHAT_MSG,
};

class TutorialBase : public MultiworldIdentifiableStaticData< std::deque< TutorialBase* > >
{
	TutorialType m_TutorialType {};
	char m_aTextBuf[1024] {};

public:
	virtual ~TutorialBase() = default;

	template < typename CASTPOL >
	static void Init(int Type, const char* pText, CASTPOL Data)
	{
		Data.m_TutorialType = (TutorialType)Type;
		str_copy(Data.m_aTextBuf, pText, sizeof(Data.m_aTextBuf));
		m_pData.push_back(new CASTPOL(Data));
	}

	TutorialType GetTutorialType() const { return m_TutorialType; }
	const char* GetText() const { return m_aTextBuf; }
};

template<typename... Args>
class TutorialData final : public TutorialBase
{
public:
	std::tuple< Args...> m_Data;
};

#endif