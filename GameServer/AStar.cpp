#include "pch.h"
#include "AStar.h"
#include "Collision.h"

vector<NODE> FindPath(int startX, int startY, int goalX, int goalY)
{
    if (goalX == -1 || goalY == -1) cout << "¿À·ù";
    priority_queue<NODE> openSet;
    map<pair<int, int>, pair<int, int>> cameFrom;
    map<pair<int, int>, int> gScores;

    auto Heuristic = [](int x1, int y1, int x2, int y2) {
        return abs(x1 - x2) + abs(y1 - y2);
    };

    openSet.push({ startX, startY, 0,0, Heuristic(startX, startY, goalX, goalY) });
    cameFrom[{startX, startY}] = { startX, startY };
    gScores[{startX, startY}] = 0;

    while (!openSet.empty()) {
        NODE current = openSet.top();
        openSet.pop();
        
        vector<NODE> path;
        if (current._x == goalX && current._y == goalY) {
            while (!(current._x == startX && current._y == startY)) {
                path.push_back({ current._x, current._y });
                auto prev = cameFrom[{current._x, current._y}];
                current._x = prev.first;
                current._y = prev.second;
            }
            return path;
        }

        int dx[] = { 0,0,-1,1 };
        int dy[] = { -1,1,0,0 };

        for (int i = 0; i < 4; ++i) {
            int nextX = current._x + dx[i];
            int nextY = current._y + dy[i];
            if (nextX < 0 || nextX >= W_WIDTH || nextY < 0 || nextY >= W_HEIGHT) continue;
            if (isCollision(nextX, nextY)) continue;

            int newCost = gScores[{current._x, current._y}] + 1;
            if (!gScores.count({ nextX, nextY }) || newCost < gScores[{nextX, nextY}]) {
                gScores[{nextX, nextY}] = newCost;
                int priority = newCost + Heuristic(nextX, nextY, goalX, goalY);
                openSet.push({ nextX, nextY, priority, newCost, Heuristic(nextX, nextY, goalX, goalY) });
                cameFrom[{nextX, nextY}] = { current._x, current._y };
            }
        }
    }
    return {};
}
