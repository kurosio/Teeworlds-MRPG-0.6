/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_TUTORIAL_EVENT_CORE_H
#define GAME_SERVER_COMPONENT_TUTORIAL_EVENT_CORE_H
#include <game/server/mmocore/MmoComponent.h>

#include "TutorialData.h"

class CTutorialEventCore : public MmoComponent
{
	~CTutorialEventCore() override
	{
		for(auto pItem : TutorialBase::Data())
			delete pItem;

		TutorialBase::Data().clear();
	}

	void OnInit() override;
	void OnInitAccount(CPlayer* pPlayer) override;

	void Save(CPlayer* pPlayer);
	void Load(CPlayer* pPlayer);

	TutorialBase* GetTutorial(int ID) const { return TutorialBase::Data()[ID]; }

public:
	void TryCheckNextTutorialStep(CPlayer* pPlayer) const;
};

#endif
