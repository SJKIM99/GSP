#pragma once

class AStar
{
public:
	struct Node
	{
		int16 _xPos, _yPos;
		float _g, _h;
		Node* _parent;

		Node(int x, int y, float g = 0, float h = 0, Node* parent = nullptr)
			: _xPos(x), _yPos(y), _g(g), _h(h), _parent(parent) {}

		float f() const { return _g + _h; }
	};

	AStar(int16 x, int16 y) : maxX(x), maxY(y) {}

	vector<pair<int, int>> findPath(int16 startX, int startY, int targetX, int targetY);

private:
	struct CompareNode
	{
		bool operator()(Node* n1, Node* n2)
		{
			return n1->f() > n2->f();
		}
	};

	int16 maxX, maxY;

	float heuristic(int x1, int y1, int x2, int y2) const
	{
		return sqrt((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2));
	}

	vector<Node*> getNeighbors(Node* node);
	vector<pair<int, int>> reconstructPath(Node* node) const;
};


