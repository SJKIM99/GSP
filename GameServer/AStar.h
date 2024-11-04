#pragma once

struct NODE
{
	int _x, _y;
	int _gCost, _hCost, _fCost;
	
	bool operator<(const NODE& rhs) const
	{
		return _fCost > rhs._fCost;
	}
};

class AStar
{
public:
	 vector<NODE> FindPath(int startX, int startY, int goalX, int gaolY);
};