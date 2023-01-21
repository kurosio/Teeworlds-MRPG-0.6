/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_TUTORIAL_EVENT_DATA_H
#define GAME_SERVER_COMPONENT_TUTORIAL_EVENT_DATA_H

enum TutorialTypes
{
	// vector
	TUTORIAL_MOVE_TO = 6,
	// integer
	TUTORIAL_EQUIP,
	TUTORIAL_PLAYER_FLAG,
	TUTORIAL_OPEN_VOTE_MENU,
	// string
	TUTORIAL_CHAT_MSG,

	TUTORIAL_VECTOR_ONE = 1 << TUTORIAL_MOVE_TO,
	TUTORIAL_INTEGER_ONE = 1 << TUTORIAL_EQUIP | 1 << TUTORIAL_PLAYER_FLAG | 1 << TUTORIAL_OPEN_VOTE_MENU,
	TUTORIAL_STRING_ONE = 1 << TUTORIAL_CHAT_MSG,
};

class TutorialBase : public MultiworldIdentifiableStaticData< std::deque< TutorialBase* > >
{
public:
	virtual ~TutorialBase() = default;

	template < typename CASTPOL>
	static void Init(int Type, const char* pText, CASTPOL Data)
	{
		Data.m_TutorialType = Type;
		str_copy(Data.m_aTextBuf, pText, sizeof(Data.m_aTextBuf));
		m_pData.push_back(new CASTPOL(Data));
	}

	int m_TutorialType{};
	char m_aTextBuf[1024]{};
};

class TutorialVectorOne final : public TutorialBase
{
public:
	vec2 m_Position;
};

class TutorialIntegerOne final : public TutorialBase
{
public:
	int m_Integer;
};

class TutorialStringOne final : public TutorialBase
{
public:
	char m_aStringBuf[256];
};

#endif