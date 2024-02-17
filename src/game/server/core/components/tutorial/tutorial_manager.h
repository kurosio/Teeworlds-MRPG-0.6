/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_TUTORIAL_EVENT_CORE_H
#define GAME_SERVER_COMPONENT_TUTORIAL_EVENT_CORE_H
#include <game/server/core/mmo_component.h>

#include "tutorial_data.h"

class CTutorialManager : public MmoComponent
{
	~CTutorialManager() override
	{
		for(auto pItem : TutorialBase::Data())
			delete pItem;

		TutorialBase::Data().clear();
	}

	void OnInit() override;

	TutorialBase* GetTutorial(int ID) const { return TutorialBase::Data()[ID]; }

public:
	int GetSize() const { return (int)TutorialBase::Data().size(); }
	void HandleTutorial(CPlayer* pPlayer) const;
};

#endif
