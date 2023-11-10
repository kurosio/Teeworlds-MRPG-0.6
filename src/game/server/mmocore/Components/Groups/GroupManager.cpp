/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GroupManager.h"

#include "GroupData.h"

#include <game/server/gamecontext.h>

void CGroupManager::OnInit()
{
	// Initialize a GroupData object with the GroupIdentifier and AccountIDs
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_groups");
	while(pRes->next())
	{
		GroupIdentifier ID = pRes->getInt("ID");
		std::string AccountIDs = pRes->getString("AccountIDs").c_str();
		GroupData(ID).Init(AccountIDs);
	}
}
