#pragma once

class GameSession;

struct NODE {
    short _x, _y;
    int _gCost, _hCost, _fCost;
    bool operator<(const NODE& rhs) const {
        return _fCost > rhs._fCost;
    }
};

// 해시 함수 정의
struct hash_pair {
    template <class T1, class T2>
    size_t operator()(const pair<T1, T2>& pair) const {
        auto hash1 = hash<T1>()(pair.first);
        auto hash2 = hash<T2>()(pair.second);
        return hash1 ^ (hash2 << 1);
    }
};

vector<NODE> FindPath(short startX, short startY, short goalX, short goalY);
