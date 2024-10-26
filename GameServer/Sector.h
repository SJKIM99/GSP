#pragma once
//#include "Protocol.h"

class Sector
{
public:
	std::array<std::array<std::unordered_set<uint32>, W_WIDTH / SECTOR_RANGE>, W_HEIGHT / SECTOR_RANGE> sectors;
	std::array<std::array<mutex, W_WIDTH / SECTOR_RANGE>, W_HEIGHT / SECTOR_RANGE> sectorLocks;

public:
	std::mutex sector_lock;

public:
	Sector() {};
	~Sector() {};

	short	GetMySector_X(short x);
	short	GetMySector_Y(short y);

	void	AddPlayerInSector(uint32 player_id, short sector_x, short sector_y);
	void	RemovePlayerInSector(uint32 player_id, short sector_x, short sector_y);
	bool	UpdatePlayerInSector(uint32 player_id, short new_sector_x, short new_sector_y, short old_sector_x, short old_sector_y);
	unordered_set<uint32>	GetObjectSector(short sector_x, short sector_y);

private:
	USE_LOCK;
};

//extern Sector* GSector;

