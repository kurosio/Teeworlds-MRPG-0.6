/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_GUILD_REQUEST_DATA_H
#define GAME_SERVER_COMPONENT_GUILD_REQUEST_DATA_H

// This class represents data for a guild request
class CGuildRequestData
{
	int m_FromUID;

public:
	CGuildRequestData() = delete;
	CGuildRequestData(int FromUID) noexcept : m_FromUID(FromUID) {}

	// Getter method for retrieving the FromUID member variable
	int GetFromUID() const noexcept { return m_FromUID; }
};


#endif
