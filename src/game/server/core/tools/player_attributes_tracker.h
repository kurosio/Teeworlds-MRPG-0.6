#ifndef GAME_SERVER_CORE_TOOLS_PLAYER_ATTRIBUTES_TRACKER_H
#define GAME_SERVER_CORE_TOOLS_PLAYER_ATTRIBUTES_TRACKER_H

class CGS;
class CPlayer;

class CPlayerAttributesTracker
{

public:
	struct Point
	{
		int AccountID {};
	};

	std::unordered_map<Point, int> m_TrackingData {};
};

#endif