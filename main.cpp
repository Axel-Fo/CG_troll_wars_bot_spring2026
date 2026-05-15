#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <optional>

using namespace std;

const int DIST_MAX = 1e9;

struct Point {
    int x, y;
};

struct Tree {
    string type;
    Point pos;
    int size;
    int health;
    int fruits;
    int cooldown;
};

class Action {
public:
    enum Type { MOVE, HARVEST, DROP, WAIT, MSG, PLANT, CHOP, MINE, PICK, TRAIN };

    // MODIFIE : plus d'id dans les factory methods, meilleurs noms de params
    static Action move(int targetX, int targetY) {
        Action a; a.type = MOVE; a.moveX = targetX; a.moveY = targetY; return a;
    }
    static Action harvest() {
        Action a; a.type = HARVEST; return a;
    }
    static Action drop() {
        Action a; a.type = DROP; return a;
    }
    static Action wait() {
        Action a; a.type = WAIT; return a;
    }
    static Action msg(const string& text) {
        Action a; a.type = MSG; a.message = text; return a;
    }
    static Action plant(const string& treeType) {
        Action a; a.type = PLANT; a.message = treeType; return a;
    }
    static Action chop() {
        Action a; a.type = CHOP; return a;
    }
    static Action mine() {
        Action a; a.type = MINE; return a;
    }
    static Action pick(const string& resource) {
        Action a; a.type = PICK; a.message = resource; return a;
    }
    // MODIFIE : TRAIN a ses 4 vrais attributs nommés correctement
    static Action train(int movementSpeed, int carryCapacity, int harvestPower, int chopPower) {
        Action a; a.type = TRAIN;
        a.trainMovementSpeed = movementSpeed;
        a.trainCarryCapacity = carryCapacity;
        a.trainHarvestPower  = harvestPower;
        a.trainChopPower     = chopPower;
        return a;
    }

    // MODIFIE : toString prend l'id du troll en paramètre
    string toString(int trollId) const {
        switch (type) {
            case MOVE:    return "MOVE " + to_string(trollId) + " " + to_string(moveX) + " " + to_string(moveY);
            case HARVEST: return "HARVEST " + to_string(trollId);
            case DROP:    return "DROP " + to_string(trollId);
            case WAIT:    return "WAIT";
            case MSG:     return "MSG " + message;
            case PLANT:   return "PLANT " + to_string(trollId) + " " + message;
            case CHOP:    return "CHOP " + to_string(trollId);
            case MINE:    return "MINE " + to_string(trollId);
            case PICK:    return "PICK " + to_string(trollId) + " " + message;
            case TRAIN:   return "TRAIN " + to_string(trainMovementSpeed) + " "
                               + to_string(trainCarryCapacity) + " "
                               + to_string(trainHarvestPower)  + " "
                               + to_string(trainChopPower);
        }
        return "WAIT";
    }

private:
    Type type;
    // MODIFIE : noms explicites selon l'usage
    int moveX = 0, moveY = 0;
    int trainMovementSpeed = 0, trainCarryCapacity = 0,
        trainHarvestPower  = 0, trainChopPower     = 0;
    string message;
};

// =====================================================================
// MODIFIE : dist utilise maintenant la grille pour détecter les obstacles
// =====================================================================
vector<string> grid;
int gridWidth, gridHeight;

bool isWalkable(Point p) {
    if (p.x < 0 || p.y < 0 || p.x >= gridWidth || p.y >= gridHeight) return false;
    char c = grid[p.y][p.x];
    return c == '.' || c == '0' || c == '1'; // GRASS ou shack
}

optional<int> dist(Point a, Point b) {
    if (!isWalkable(a) || !isWalkable(b)) return nullopt;
    // BFS pour distance réelle sur la grille
    vector<vector<int>> visited(gridHeight, vector<int>(gridWidth, -1));
    vector<Point> queue;
    queue.push_back(a);
    visited[a.y][a.x] = 0;
    for (int i = 0; i < (int)queue.size(); i++) {
        Point cur = queue[i];
        if (cur.x == b.x && cur.y == b.y) return visited[cur.y][cur.x];
        for (auto& next : vector<Point>{{cur.x+1,cur.y},{cur.x-1,cur.y},{cur.x,cur.y+1},{cur.x,cur.y-1}}) {
            if (isWalkable(next) && visited[next.y][next.x] == -1) {
                visited[next.y][next.x] = visited[cur.y][cur.x] + 1;
                queue.push_back(next);
            }
        }
    }
    return nullopt; // pas de chemin
}
// =====================================================================

class Troll {
public:
    int id;
    int player;
    Point pos;
    int movementSpeed;
    int carryCapacity;
    int harvestPower;
    int chopPower;
    int carryPlum;
    int carryLemon;
    int carryApple;
    int carryBanana;
    int carryIron;
    int carryWood;

    Action action = Action::wait();

    int totalCarried() const {
        return carryPlum + carryLemon + carryApple + carryBanana + carryIron + carryWood;
    }

    bool isFull() const {
        return totalCarried() >= carryCapacity;
    }

private:
    static vector<Point> adjacentToShack(Point shackPos) {
        return {
            {shackPos.x + 1, shackPos.y},
            {shackPos.x - 1, shackPos.y},
            {shackPos.x, shackPos.y + 1},
            {shackPos.x, shackPos.y - 1}
        };
    }

    static bool isAdjacentToShack(Point pos, Point shackPos) {
        for (auto& adj : adjacentToShack(shackPos))
            if (adj.x == pos.x && adj.y == pos.y) return true;
        return false;
    }

    Tree* findTree(const vector<Tree>& trees) {
        Tree* best = nullptr;
        int bestDist = DIST_MAX;
        for (auto& tree : trees) {
            if (tree.fruits <= 0) continue;
            auto d = dist(pos, tree.pos);
            if (d.has_value() && d.value() < bestDist) {
                bestDist = d.value();
                best = const_cast<Tree*>(&tree);
            }
        }
        return best;
    }

public:
    void defineAction(Point shackPos, const vector<Tree>& trees, bool setUp) {
        if (isAdjacentToShack(pos, shackPos) && totalCarried() > 0) {
            action = Action::drop();
            return;
        }
        if (isFull()) {
            auto adj = adjacentToShack(shackPos);
            Point best = adj[0];
            int bestDist = DIST_MAX;
            for (auto& p : adj) {
                auto d = dist(pos, p);               // MODIFIE : utilise dist()
                if (d.has_value() && d.value() < bestDist) {
                    bestDist = d.value();
                    best = p;
                }
            }
            action = Action::move(best.x, best.y);
            return;
        }
        for (auto& tree : trees) {
            if (tree.pos.x == pos.x && tree.pos.y == pos.y && tree.fruits > 0) {
                action = Action::harvest();
                return;
            }
        }
        Tree* target = findTree(trees);
        if (target != nullptr) {
            action = Action::move(target->pos.x, target->pos.y);
            return;
        }
        action = Action::wait();
    }
};

Point shackPos;
Point enemyShackPos;
vector<Point> ironMines;  
int targetNbrTroll = 5;
bool setUp = true;

vector<Point> adjacentToShack(Point shackP) {
    return {
        {shackP.x + 1, shackP.y},
        {shackP.x - 1, shackP.y},
        {shackP.x, shackP.y + 1},
        {shackP.x, shackP.y - 1}
    };
}

vector<Tree>  trees;
vector<Troll> trolls;


void play() {
    bool first = true;
    for (auto& troll : trolls) {
        if (troll.player != 0) continue;
        if (!first) cout << ";";
        cout << troll.action.toString(troll.id);
        first = false;
    }
    cout << endl;
}

int main()
{
    cin >> gridWidth >> gridHeight; cin.ignore(); 

    grid.resize(gridHeight);                       
    for (int i = 0; i < gridHeight; i++) {
        getline(cin, grid[i]);
        for (int j = 0; j < gridWidth; j++) {
            if (grid[i][j] == '0') shackPos = {j, i};
            if (grid[i][j] == '1') enemyShackPos = {j, i};
            if (grid[i][j] == 'I') ironMines.push_back({j, i}); 
        }
    }

    int myPlum, myLemon, myApple, myBanana, myIron, myWood;
    int enemyPlum, enemyLemon, enemyApple, enemyBanana, enemyIron, enemyWood;

    while (1) {
        for (int i = 0; i < 2; i++) {
            int plum, lemon, apple, banana, iron, wood;
            cin >> plum >> lemon >> apple >> banana >> iron >> wood; cin.ignore();
            if (i == 0) { myPlum = plum; myLemon = lemon; myApple = apple; myBanana = banana; myIron = iron; myWood = wood;}
            else        { enemyPlum = plum; enemyLemon = lemon; enemyApple = apple; enemyBanana = banana; enemyIron = iron; enemyWood = wood;}
        }

        trees.clear();
        int trees_count;
        cin >> trees_count; cin.ignore();
        for (int i = 0; i < trees_count; i++) {
            Tree t;
            cin >> t.type >> t.pos.x >> t.pos.y >> t.size >> t.health >> t.fruits >> t.cooldown; cin.ignore();
            trees.push_back(t);
        }

        trolls.clear();
        int trolls_count;
        cin >> trolls_count; cin.ignore();
        for (int i = 0; i < trolls_count; i++) {
            Troll t;
            cin >> t.id >> t.player >> t.pos.x >> t.pos.y
                >> t.movementSpeed >> t.carryCapacity >> t.harvestPower >> t.chopPower
                >> t.carryPlum >> t.carryLemon >> t.carryApple >> t.carryBanana
                >> t.carryIron >> t.carryWood; cin.ignore();
            trolls.push_back(t);
        }

        int nbrTroll = 0 ;
        for (auto& troll : trolls) {
            if (troll.player == 0) nbrTroll += 1 ;
        }

        if(nbrTroll <= targetNbrTroll) setUp = false;

        for (auto& troll : trolls) {
            if (troll.player != 0) continue;
            troll.defineAction(shackPos, trees, setUp);
        }

        play();
    }
}