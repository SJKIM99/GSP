#include "pch.h"
#include "Collision.h"

constexpr int SCREEN_WDITH = 16;
constexpr int SCREEN_HEIGHT = 16;

bool isCollision(short x_pos, short y_pos)
{
	for (int i = 0; i < SCREEN_WDITH; ++i) {
		for (int j = 0; j < SCREEN_HEIGHT; ++j) {
			int tile_x = i + x_pos;
			int tile_y = j + y_pos;
			if ((tile_x < 0) || (tile_y < 0)) continue;
			if (0 == (tile_x / 3 + tile_y / 3) % 3) {
				return false;
			}
			else if (1 == (tile_x / 3 + tile_y / 3) % 3)
			{
				return false;
			}
			else {	//厘局拱 面倒 贸府 秦具窃
				if (0 == (tile_x / 2 + tile_y / 2) % 3) {
					return true;
				}
				else if (1 == (tile_x / 2 + tile_y / 2) % 3) {
					return false;
				}
				else {
					return false;
				}
			}
		}
	}
}