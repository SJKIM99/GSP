#include "pch.h"
#include "AStar.h"

vector<pair<int, int>> AStar::findPath(int16 startX, int startY, int targetX, int targetY)
{
	priority_queue<Node*, vector<Node*>, CompareNode> openSet;
	unordered_map<int, Node*> openMap;
	unordered_map<int, bool> closedSet;

	Node* startNode = new Node(startX, startY, 0, heuristic(startX, startY, targetX, targetY));
	openSet.push(startNode);
	openMap[startX * maxY + startY] = startNode;

	while (!openSet.empty()) {
		Node* current = openSet.top();
		openSet.pop();
		openMap.erase(current->_xPos * maxY + current->_yPos);

		if (current->_xPos == targetX && current->_yPos == targetY) {
			return reconstructPath(current);
		}

		closedSet[current->_xPos * maxY + current->_yPos] = true;
		for (Node* neighbor : getNeighbors(current)) {

			int hash = neighbor->_xPos * maxY + neighbor->_yPos;
			if (closedSet[hash]) {
				delete neighbor;
				continue;
			}

			float tentativeG = current->_g + 1;
			if (openMap.find(hash) == openMap.end() || tentativeG < openMap[hash]->_g) {

				neighbor->_g = tentativeG;
				neighbor->_h = heuristic(neighbor->_xPos, neighbor->_yPos, targetX, targetY);
				neighbor->_parent = current;
				openSet.push(neighbor);
				openMap[hash] = neighbor;
			}
		}
	}
	return {};
}

vector<AStar::Node*> AStar::getNeighbors(Node* node)
{
	vector<Node*> neighbors;
	int dx[] = { -1, 1, 0, 0 };
	int dy[] = { 0, 0, -1, 1 };

	for (int i = 0; i < 4; ++i) {

		int nx = node->_xPos + dx[i];
		int ny = node->_yPos + dy[i];
		if (nx >= 0 && nx < maxX && ny >= 0 && ny < maxY) {
			neighbors.push_back(new Node(nx, ny));
		}
	}
	return neighbors;
}

vector<pair<int, int>> AStar::reconstructPath(Node* node) const
{
	vector<pair<int, int>> path;
	while (node) {

		path.push_back({ node->_xPos, node->_yPos });
		node = node->_parent;
	}
	reverse(path.begin(), path.end());
	return path;
}
