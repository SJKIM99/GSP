#include "pch.h"
#include "Sector.h"

//Sector* GSector = nullptr;

short Sector::GetMySector_X(short x)
{
    short sector_x = x / SECTOR_RANGE;
    return sector_x;
}

short Sector::GetMySector_Y(short y)
{
    short sector_y = y / SECTOR_RANGE;
    return sector_y;
}

void Sector::AddPlayerInSector(uint32 player_id, short sector_x, short sector_y)
{
    lock_guard<mutex> ll(sectorLocks[sector_y][sector_x]);
	sectors[sector_y][sector_x].insert(player_id);
}

bool Sector::UpdatePlayerInSector(uint32 player_id, short new_sector_x, short new_sector_y, short old_sector_x, short old_sector_y)
{
	if (new_sector_x != old_sector_x || new_sector_y != old_sector_y) {
		{
			lock_guard<mutex> ll(sectorLocks[old_sector_y][old_sector_x]);
			sectors[old_sector_y][old_sector_x].erase(player_id);
		}
		{
			lock_guard<mutex> ll(sectorLocks[new_sector_y][new_sector_x]);
			sectors[new_sector_y][new_sector_x].insert(player_id);
		}
		return true;
	}
    return false;
}

void Sector::RemovePlayerInSector(uint32 player_id, short sector_x, short sector_y)
{
	lock_guard<mutex> ll(sectorLocks[sector_y][sector_x]);
    sectors[sector_y][sector_x].erase(player_id);
}
