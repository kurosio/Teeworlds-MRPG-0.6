/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "WorldSwapData.h"

std::map < int , CWorldSwapData > CWorldSwapData::ms_aWorldSwap;
std::list < CWorldSwapPosition > CWorldSwapPosition::ms_aWorldPositionLogic;