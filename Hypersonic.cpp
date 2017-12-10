#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <stdlib.h>
#include <map>
#include <chrono>
#include <unordered_map>
#include <tuple>
#include <queue>

using namespace std;
enum ITEM { None, Range, BombCount };
enum ACTION { UP, DOWN, LEFT, RIGHT, BOMB, STAY };
class World;
typedef tuple<World, ACTION, int, int> Path;

//CONFIGS
const int DEPTH = 5;
const bool USE_BEAM = true;
const bool USE_DFS = false;
const bool USE_BFS = false;

const int BEAM_WIDHT = 100;
const int MAP_WIDTH = 13;
const int MAP_HEIGHT = 11;
const bool timer_on = false;

string toString(ACTION action)
{
	map<ACTION, string> dict = { { UP, "UP" },{ DOWN, "DOWN" },{ LEFT, "LEFT" },{ RIGHT, "RIGHT" }, { BOMB, "BOMB" }, {STAY, "STAY" } };
	return dict.at(action);
}

class Timer
{
public:
	Timer(string name) {
		start = std::chrono::high_resolution_clock::now();
		this->name = name;
	}

	~Timer()
	{
		if (timer_on) {
			Stop("DST");
		}
		
	}

	static void GlobalStart()
	{
		global_start = std::chrono::high_resolution_clock::now();
	}

	static long long GlobalElapsed()
	{
		auto elapsed = std::chrono::high_resolution_clock::now() - global_start;
		long long microseconds = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
		return microseconds / 1000;
	}

	static void GlobalPrint()
	{
		cerr << "Globaltime: " << GlobalElapsed() << endl;
	}

	void Stop(string reason = "")
	{
		auto elapsed = std::chrono::high_resolution_clock::now() - start;
		long long microseconds = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
		if (stats.find(name) == stats.end()) {
			stats[name] = make_tuple(0, 0);
		}
		stats[name] = make_tuple(get<0>(stats[name]) + microseconds, get<1>(stats[name]) + 1);
	}

	static void PrintStats()
	{
		if (timer_on) {
			cerr << "====ROUND TIME STATS==== T" << GlobalElapsed() << "ms" << endl;
			for (auto it = stats.begin(); it != stats.end(); it++)
			{
				int dur = get<0>(it->second);
				int calls = get<1>(it->second);
				cerr << it->first << " took:" << dur << "us, calls:" << calls << " avg:" << (float)dur / calls << "us" << endl;
			}
		}
	}

	static void ResetStats()
	{
		stats.clear();
		GlobalStart();
	}

private:
	string name;
	std::chrono::system_clock::time_point start;
	static std::chrono::system_clock::time_point global_start;
	static map<string, tuple<int, int>> stats;
};

class Point
{
public:
	Point()
		: m_x(0), m_y(0)
	{}

	Point(int x, int y)
		: m_x(x), m_y(y)
	{}

	void Set(int x, int y)
	{
		m_x = x;
		m_y = y;
	}

	int x() const
	{
		return m_x;
	}

	int y() const
	{
		return m_y;
	}

	const string toString()
	{
		return "X:" + to_string(m_x) + " Y:" + to_string(m_y);
	}

private:
	int m_x;
	int m_y;
};

//For point hashmap
struct PointHash {
	size_t operator()(const Point &data) const {
		return (size_t)(data.x() * 100 + data.y());
	}
};

bool operator ==(const Point &p1, const Point &p2) {
	return p1.x() == p2.x() && p1.y() == p2.y();
}

class Unit
{
public:
	Point GetLoc() const
	{
		return loc;
	}
	void SetLoc(int x, int y)
	{
		loc.Set(x, y);
	}
private:
	Point loc;
};

class Player : public Unit
{
public:
	void SetId(int id)
	{
		m_id = id;
	}

	int GetId()
	{
		return m_id;
	}

	void SetBombs(int bombs)
	{
		this->bombs = bombs;
	}

	int GetBombs()
	{
		return this->bombs;
	}

	void SetRange(int range)
	{
		this->range = range;
	}

	int GetRange()
	{
		return this->range;
	}

	void SetRemainingBombs(int num)
	{
		this->remaining_bombs = num;
	}

	int GetRemainingBombs()
	{
		return this->remaining_bombs;
	}

private:
	int m_id = 0;
	int range = 3;
	int bombs = 1;
	int remaining_bombs = 1;
};

class Bomb : public Unit
{
public:
	Bomb() {};
	Bomb(int expl_round, int owner, int radius, int x, int y)
	{
		SetRadius(radius);
		SetExplRound(expl_round);
		SetLoc(x, y);
		SetOwner(owner);
	}

	void SetExplRound(int round)
	{
		this->expl_round = round;
	}

	void SetRadius(int radius)
	{
		this->radius = radius;
	}

	int GetRadius() const
	{
		return this->radius;
	}

	void SetOwner(int owner)
	{
		//cerr << "Set Bomb owner" << owner << "at " << GetLoc().toString() << endl;
		this->owner = owner;
	}

	int GetOwner()
	{
		//cerr << "Get Bomb owner" << owner << "at " << GetLoc().toString() << endl;
		return this->owner;
	}

	int GetExplRound()
	{
		return expl_round;
	}

	bool GetCounted()
	{
		return counted;
	}

	void SetCounted()
	{
		counted = true;
	}

private:
	int expl_round;
	int radius;
	int owner = 999;
	bool counted = false; //Did i count this in score
};

class Tile : public Unit
{
public:
	int GetExplRound()
	{
		return expl_round;
	}

	void SetBomb(Bomb bomb)
	{
		this->bomb = bomb;
		//cerr << "Setting bomb at " << GetLoc().toString() << "owned by" << this->bomb.GetOwner() << endl;
		this->has_bomb = true;
	}

	Bomb* GetBomb()
	{
		//cerr << "Returning bomb at " << GetLoc().toString() << "owned by" << this->bomb.GetOwner() << endl;
		return &this->bomb;
	}

	void SetExplRound(int round)
	{
		//cerr << GetLoc().toString() << " will explode at " << round << endl;
		if (this->expl_round == 0 || this->expl_round > round)
		{
			this->expl_round = round;
		}
	}

	void SetBox(bool has_box)
	{
		//cerr << GetLoc().toString() << "Setting box" << has_box << endl;
		this->has_box = has_box;
	}

	void SetItem(ITEM id)
	{
		this->item = id;
	}

	ITEM GetItem()
	{
		return this->item;
	}

	void SetWall()
	{
		this->is_wall = true;
	}

	bool HasBomb()
	{
		return this->has_bomb;
	}

	bool HasBox()
	{
		//cerr << GetLoc().toString() << " has box:" << has_box << endl;
		return has_box;
	}

	bool IsTraversible()
	{
		return !has_box && !has_bomb && !is_wall;
	}

	bool IsWall() {
		return is_wall;
	}

	bool ExplosionCanPass()
	{
		return !IsWall() && GetItem() == ITEM::None && !HasBox() && !HasBomb();
	}

	void Explode()
	{
		this->has_bomb = false;
		if (!this->has_box) {
			this->item = ITEM::None;
		}
		this->has_box = false;
		this->explosion_by_me = false;
		this->expl_round = 0;
		
	}

	void SetExplosionByMe()
	{
		this->explosion_by_me = true;
	}

	bool ExplosionByMe()
	{
		return this->explosion_by_me;
	}

private:
	int expl_round = 0;
	bool explosion_by_me = false;
	Bomb bomb;
	bool has_bomb = false;
	bool has_box = false;
	ITEM item = ITEM::None;
	bool is_wall = false;
};

class World
{
public:
	World()
	{
		//nothing
	};

	void InitializeGraph()
	{
		for (int x = 0; x < MAP_WIDTH; x++) {
			for (int y = 0; y < MAP_HEIGHT; y++) {
				StoreNeighbourTiles(Point(x, y));
			}
		}
	}

	void DoNextTurn()
	{
		ResolveBombs();
		SetSimStartRound(GetCurrentRound());

		if (USE_DFS) {
			cerr << "Starting DFS at" << GetCurrentLoc().toString() << endl;
			DFS(DEPTH);
		} else 
		if (USE_BEAM) {
			cerr << "Starting Beam at" << GetCurrentLoc().toString() << endl;
			Beam(DEPTH);
		} else
		if (USE_BFS) {
			cerr << "Starting BFS at" << GetCurrentLoc().toString() << endl;
			BFS(DEPTH);
		}

		this->currentActions = this->bestActions;
		cerr << "Round " << GetCurrentRound() << " Besactions " << GetActionList() << " score " << this->bestScore << " bombsleft" << players[GetPlayerId()].GetRemainingBombs() << endl;

		bool do_bomb = false;
		int action_id = 0;
		if (this->bestActions[action_id] == ACTION::BOMB) {
			DoAction(this->bestActions[action_id]);
			do_bomb = true;
			action_id++;
		}


		ACTION direction = this->bestActions[action_id];
		Point new_loc = GetCurrentLoc();
		if (direction == UP) {
			new_loc.Set(new_loc.x(), new_loc.y() - 1);
		}
		else
			if (direction == DOWN) {
				new_loc.Set(new_loc.x(), new_loc.y() + 1);
			}
			else
				if (direction == LEFT) {
					new_loc.Set(new_loc.x() - 1, new_loc.y());
				}
				else
					if (direction == RIGHT) {
						new_loc.Set(new_loc.x() + 1, new_loc.y());
					}
		if (direction == STAY) {
			//dont change it
		}

		DoAction(this->bestActions[action_id]);

		if (do_bomb) {
			DoBomb(new_loc.x(), new_loc.y());
		}
		else {
			DoMove(new_loc.x(), new_loc.y());
		}

		this->currentActions.clear();
		this->bestActions.clear();
		this->bestScore = 0;
		this->currentScore = 0;
	}

	void SetPlayerId(int id)
	{
		this->player_id = id;
	}

	Tile* GetTile(int x, int y)
	{
		if (x < MAP_WIDTH && x >= 0 && y < MAP_HEIGHT && y >= 0) {
			return &grid[x][y];
		}
		else {
			return nullptr;
		}
	}

	void SetPlayerLoc(int player_id, int x, int y)
	{
		players[player_id].SetLoc(x, y);
		ResolvePickups(player_id);
	}

	void SetPlayerRange(int player_id, int range)
	{
		players[player_id].SetRange(range);
	}

	void SetPlayerBombs(int player_id, int bombs)
	{
		players[player_id].SetBombs(bombs);
	}

	bool HasBombAt(int x, int y)
	{
		Tile* tile = GetTile(x, y);
		return tile->HasBomb();
	}

	void SetBomb(int x, int y, int owner, int expl_round, int range)
	{
		Tile* tile = GetTile(x, y);
		Bomb bomb(expl_round, owner, range, x, y);

		tile->SetBomb(bomb);
		UpdateExplosion(tile->GetLoc());
		active_bombs.push_back(bomb.GetLoc());
	}

private:

	int GetSimStartRound()
	{
		return this->sim_start_round;
	}

	void SetSimStartRound(int round)
	{
		this->sim_start_round = round;
	}

	int GetCurrentRound()
	{
		return this->current_round;
	}

	void SetCurrentRound(int round)
	{
		this->current_round = round;
	}

	int GetPlayerId()
	{
		return this->player_id;
	}

	void SetMyLoc(int x, int y)
	{
		SetPlayerLoc(GetPlayerId(), x, y);
	}
	
	Point GetCurrentLoc()
	{
		return players[GetPlayerId()].GetLoc();
	}

	Point GetPlayerLoc(int player_id)
	{
		players[player_id].GetLoc();
	}

	string GetActionList()
	{
		string action_list = "";
		for (ACTION act : this->currentActions) {
			action_list += ">" + toString(act);
		}
		return action_list;

	}

	//Tile helpers
	Tile* GetTileAt(Point point)
	{
		return GetTile(point.x(), point.y());
	}
	Tile* GetNonWallTile(int x, int y)
	{
		Tile* tile = GetTile(x, y);
		if (tile != nullptr && !tile->IsWall()) {
			return tile;
		}
		return nullptr;
	}
	Tile* LeftTile(Tile* current_tile)
	{
		return GetDirTile(current_tile, "left");
	}
	Tile* RightTile(Tile* current_tile)
	{
		return GetDirTile(current_tile, "right");
	}
	Tile* UpTile(Tile* current_tile)
	{
		return GetDirTile(current_tile, "up");
	}
	Tile* DownTile(Tile* current_tile)
	{
		return GetDirTile(current_tile, "down");
	}
	Tile* GetDirTile(Tile* current_tile, const string& dir)
	{
		Timer tim("World::GetDirTile");
		if (graph[current_tile->GetLoc()].find(dir) != graph[current_tile->GetLoc()].end()) {
			return GetTileAt(graph[current_tile->GetLoc()][dir]);
		} else {
			return nullptr;
		}
	}
	vector<Tile*> GetNeighbours(const Point& loc)
	{
		Tile* tile = GetTileAt(loc);
		vector<Tile*> neighbours;

		neighbours.push_back(UpTile(tile));
		neighbours.push_back(DownTile(tile));
		neighbours.push_back(RightTile(tile));
		neighbours.push_back(LeftTile(tile));
		return neighbours;
	}

	void StoreNeighbourTiles(Point loc)
	{
		Tile* up = GetTile(loc.x(), loc.y() - 1);
		Tile* down = GetTile(loc.x(), loc.y() + 1);
		Tile* left = GetTile(loc.x() - 1, loc.y());
		Tile* right = GetTile(loc.x() + 1, loc.y());

		map<string, Point> neighbours;

		if (up != nullptr) {
			neighbours["up"] = up->GetLoc();
		}
		if (down != nullptr) {
			neighbours["down"] = down->GetLoc();
		}
		if (left != nullptr) {
			neighbours["left"] = left->GetLoc();
		}
		if (right != nullptr) {
			neighbours["right"] = right->GetLoc();
		}
		graph[loc] = neighbours;
	}

	//Bombing related
	void SetMyBomb(int x, int y) {
		SetBomb(x, y, GetPlayerId(), GetCurrentRound() + 9, players[GetPlayerId()].GetRange());
		players[GetPlayerId()].SetRemainingBombs(players[GetPlayerId()].GetRemainingBombs() - 1);
	}

	vector<Tile*> GetAffectedTiles(const Bomb& bomb)
	{
		vector<Tile*> result;
		Tile* curr_tile = GetTileAt(bomb.GetLoc());

		Tile* left = curr_tile;
		Tile* right = curr_tile;
		Tile* up = curr_tile;
		Tile* down = curr_tile;
		result.push_back(curr_tile);
		//cerr << "UpdateExplosion " << loc.toString() << endl;

		for (int i = 0; i < bomb.GetRadius() - 1; i++) {
			left = (left != nullptr ? LeftTile(left) : nullptr);
			right = (right != nullptr ? RightTile(right) : nullptr);
			up = (up != nullptr ? UpTile(up) : nullptr);
			down = (down != nullptr ? DownTile(down) : nullptr);

			if (left != nullptr) {
				result.push_back(left);
				left = (left->ExplosionCanPass() ? left : nullptr);
			}
			if (right != nullptr) {
				result.push_back(right);
				right = (right->ExplosionCanPass() ? right : nullptr);
			}
			if (up != nullptr) {
				result.push_back(up);
				up = (up->ExplosionCanPass() ? up : nullptr);
			}
			if (down != nullptr) {
				result.push_back(down);
				down = (down->ExplosionCanPass() ? down : nullptr);
			}
		}
		return result;
	}

	void UpdateBombExplosions()
	{
		for (const Point& loc : this->active_bombs) {
			UpdateExplosion(loc);
		}
	}

	void ResolveBombs()
	{
		Timer tim("Resolving bomb");
		bool bombs_left = active_bombs.size() > 0;

		//COUNT SCORES FOR NEW BO)MBS
		for (const Point& loc : this->active_bombs) {
			Bomb* current_bomb = GetTileAt(loc)->GetBomb();
			if (current_bomb->GetOwner() == GetPlayerId()) {
				if (!current_bomb->GetCounted()) {
					vector<Tile*> aff_tiles = GetAffectedTiles(*current_bomb);
					for (Tile* tile : aff_tiles) {
						if (tile->HasBox() && tile->GetExplRound() >= current_bomb->GetExplRound()) {
							this->currentScore += (8 - (GetCurrentRound() - GetSimStartRound())) * 100;
						}
					}
					current_bomb->SetCounted();
				}
			}
		}

		players[GetPlayerId()].SetRemainingBombs(players[GetPlayerId()].GetRemainingBombs() + increment_bombs);
		increment_bombs = 0;

		if (IDieNextRound()) {
			this->currentScore -= 99999999;
		}

		bool smth_exploded = false;
		//===RESOLVE EXPLOSIONS===
		for (auto loc_it = active_bombs.begin(); loc_it != active_bombs.end(); loc_it++) {
			Bomb* current_bomb = GetTileAt(*loc_it)->GetBomb();
			if (current_bomb->GetExplRound() == GetCurrentRound()) {
				bool my_bomb = current_bomb->GetOwner() == GetPlayerId();
				if (my_bomb) {
					increment_bombs++;
				}
				loc_it = active_bombs.erase(loc_it);
				loc_it--;
				smth_exploded = true;
			}
		}

		if (smth_exploded) {
			for (int x = 0; x < MAP_WIDTH; x++) {
				for (int y = 0; y < MAP_HEIGHT; y++) {
					Tile* tile = GetTile(x, y);
					if (tile->GetExplRound() == GetCurrentRound()) {
						if (tile->GetLoc() == GetPlayerLoc(GetPlayerId())) {
							this->currentScore -= 99999999;
						}
						//cerr << tile->GetLoc().toString() << " exploding at " << GetCurrentRound() << endl;
						tile->Explode();
					}
				}
			}
		}

		if (smth_exploded) {
			UpdateBombExplosions();
		}
	}

	void UpdateExplosion(const Point& loc)
	{
		Timer tim("World::UpdateExplosion");
		Tile* curr_tile = GetTileAt(loc);
		Bomb* bomb = curr_tile->GetBomb();
		int tile_expl_round = curr_tile->GetExplRound();
		int bomb_expl_round = bomb->GetExplRound();
		if (tile_expl_round != 0 && tile_expl_round < bomb_expl_round) {
			bomb->SetExplRound(tile_expl_round);
			bomb_expl_round = tile_expl_round;
		}

		bool my_bomb = (bomb->GetOwner() == GetPlayerId());

		vector<Tile*> aff_tiles = GetAffectedTiles(*bomb);
		for (Tile* tile : aff_tiles) {
			if (tile->GetExplRound() > bomb_expl_round || tile->GetExplRound() == 0) {
				tile->SetExplRound(bomb_expl_round);
				if (my_bomb) {
					tile->SetExplosionByMe();
				}

				if (tile->HasBomb() && tile->GetBomb()->GetExplRound() > bomb_expl_round) {
					UpdateExplosion(tile->GetLoc());
				}
			}
		}
	}

	void ResolvePickups(int player_id)
	{
		bool is_me = player_id == GetPlayerId();
		bool smth_changed = false;
		Tile* tile = GetTileAt(GetPlayerLoc(player_id));
		if (tile->GetItem() == ITEM::BombCount) {
			players[player_id].SetBombs(players[player_id].GetBombs() + 1);
			players[player_id].SetRemainingBombs(players[player_id].GetRemainingBombs() + 1);
			this->currentScore += (is_me ? 300 : 0);
			smth_changed = true;
		}
		else
			if (tile->GetItem() == ITEM::Range) {
				players[player_id].SetRange(players[player_id].GetRange() + 1);
				this->currentScore += (is_me ? 300 : 0);
				smth_changed = true;
			}

		tile->SetItem(ITEM::None);
		if (smth_changed) {
			if (tile->GetExplRound() != 0) {
				UpdateBombExplosions();
			}
		}
	}

	//Scoring
	bool AmIStuck()
	{
		Tile* curr_tile = GetTileAt(GetPlayerLoc(GetPlayerId()));
		vector<Tile*> neighbours = GetNeighbours(curr_tile->GetLoc());
		vector<Tile*> traversed;
		vector<Tile*> to_process;
		to_process.push_back(curr_tile);
		to_process.insert(to_process.end(), neighbours.begin(), neighbours.end());

		//when standing on bomb im immediately stuck
		for (int i = 0; i < 3; i++) {
			vector<Tile*> current_proc;
			current_proc.insert(current_proc.end(), to_process.begin(), to_process.end());
			to_process.clear();
			for (Tile* tile : current_proc) {
				if (tile != nullptr
					&& find(traversed.begin(), traversed.end(), tile) == traversed.end()
					&& tile->IsTraversible())
				{
					if (tile->GetExplRound() == 0) {
						return false;
					}
					vector<Tile*> new_tiles = GetNeighbours(tile->GetLoc());
					to_process.insert(to_process.end(), new_tiles.begin(), new_tiles.end());
				}
				traversed.push_back(tile);
			}
		}
		return true;
	}

	bool IDieNextRound()
	{
		Tile* player_loc = GetTileAt(GetPlayerLoc(GetPlayerId()));
		if ((player_loc->GetExplRound() - GetCurrentRound()) == 1) { 
			return true;
		}
		return false;
	}

	int GetScore()
	{
		int score = this->currentScore;
		if (AmIStuck()) {
			score -= 99999999;
		}
		return score;
	}

	//Walking
	bool TimeIsUp()
	{
		int timeout_at = (GetSimStartRound() == 0 ? 970 : 95);
		if (Timer::GlobalElapsed() >= timeout_at && !this->bestActions.empty()) {
			timeout_detected = true;
			return true;
		}
		return false;
	}

	vector<ACTION> GetPossibleActions()
	{
		Timer tim("World::GetPossibleActions");
		vector<ACTION> actions;

		Tile* cur_tile = GetTileAt(GetCurrentLoc());
		if (cur_tile == nullptr) {
			cerr << "No possible actions from this, im at insane location:" << GetCurrentLoc().toString() << endl;
		}

		Tile* up = UpTile(cur_tile);
		Tile* down = DownTile(cur_tile);
		Tile* right = RightTile(cur_tile);
		Tile* left = LeftTile(cur_tile);

		if (!cur_tile->HasBomb() && players[GetPlayerId()].GetRemainingBombs() > 0) {
			actions.push_back(ACTION::BOMB);
		}

		if (up != nullptr && up->IsTraversible()) {
			actions.push_back(ACTION::UP);
		}
		if (right != nullptr && right->IsTraversible()) {
			actions.push_back(ACTION::RIGHT);
		}
		if (down != nullptr && down->IsTraversible()) {
			actions.push_back(ACTION::DOWN);
		}
		if (left != nullptr && left->IsTraversible()) {
			actions.push_back(ACTION::LEFT);
		}
		actions.push_back(ACTION::STAY);

		//Simple heuristic to keep towards middle, as my depth currently cant cover
		//entire map (so i dont want to get stuck in a corner with no boxes in sight)
		map<ACTION, double> heur_map;
		heur_map[UP] = (double)GetCurrentLoc().y() / MAP_HEIGHT;
		heur_map[DOWN] = (double)1 - heur_map.at(UP);
		heur_map[LEFT] = (double)GetCurrentLoc().x() / MAP_WIDTH;
		heur_map[RIGHT] = (double)1 - heur_map.at(LEFT);
		heur_map[STAY] = -1;
		heur_map[BOMB] = 2;

		sort(actions.begin(), actions.end(), [&](const auto& a, const auto& b)
		{
			return heur_map.at(a) > heur_map.at(b);
		});

		return actions;
	}

	//Beam Search
	void Walk(int path_idx)
	{
		World world = get<0>(paths[path_idx]);
		ACTION action = get<1>(paths[path_idx]);
		int depth = get<2>(paths[path_idx]);

		world.ResolveBombs();
		world.DoAction(action);
		world.GetPaths(depth);
		paths.erase(paths.begin() + path_idx);
	}

	void GetPaths(int depth)
	{
		Timer tim("World::GetPaths");
		int score = GetScore();
		
		if (depth == 0) {
			if (score > this->bestScore || this->bestActions.empty()) {
				//cerr << ">>Optional best Round" << GetCurrentRound() << " Besactions " << GetActionList() << " score " << score << endl;
				this->bestScore = score;
				this->bestActions = this->currentActions;
			}
			return;
		}

		if (TimeIsUp()) {
			return;
		}

		for (ACTION action : GetPossibleActions()) {
			Path path = make_tuple(*this, action, depth - 1, score);
			paths.push_back(path);
		}
	}

	void Beam(int depth)
	{
		paths.clear();
		this->GetPaths(depth);
		while (!paths.empty()) {
			if (TimeIsUp()) {
				return;
			}

			Walk(0);
			if (paths.size() > BEAM_WIDHT * 4) {
				std::sort(paths.begin(), paths.end(), [](const Path& a, const Path& b)
					{
						return get<3>(a) > get<3>(b);
					}
				);
				cerr << "Clear up paths: " << paths.size() << "First score:" << get<0>(paths[0]).GetScore() << " Last score:" << get<0>(paths[paths.size() - 1]).GetScore() << endl;
				paths.erase(paths.begin() + BEAM_WIDHT, paths.end());
			}
		}
	}

	//Best first search

	struct ComparePaths
	{
	public:
		bool operator() (const Path& a, const Path& b)
		{
			return get<3>(a) < get<3>(b);
		}
	};

	void GetBFSPaths(int depth)
	{
		Timer tim("World::GetBFSPaths");
		if (TimeIsUp()) {
			return;
		}

		int score = GetScore();
		if (depth == 0) {
			if (score > this->bestScore || this->bestActions.empty()) {
				//cerr << ">>Optional best Round" << GetCurrentRound() << " Besactions " << GetActionList() << " score " << score << endl;
				this->bestScore = score;
				this->bestActions = this->currentActions;
			}
			return;
		}

		for (ACTION action : GetPossibleActions()) {
			Path path = make_tuple(*this, action, depth - 1, score);
			bfs_pq.push(path);
		}
	}

	void WalkBFS(const Path& path)
	{
		World world = get<0>(path);
		ACTION action = get<1>(path);
		int depth = get<2>(path);

		world.ResolveBombs();
		world.DoAction(action);
		world.GetBFSPaths(depth);
	}

	void BFS(int depth)
	{
		bfs_pq = priority_queue<Path, vector<Path>, ComparePaths>();
		this->GetBFSPaths(depth);
		while (!bfs_pq.empty()) {
			if (TimeIsUp()) {
				return;
			}
			Path current = bfs_pq.top();
			bfs_pq.pop();
			WalkBFS(current);
		}
	}


	//Depth first search
	void DFS(int depth)
	{
		if (TimeIsUp()) {
			return;
		}
		ResolveBombs();
		if (depth == 0 || this->currentScore < -10000) {
			int score = GetScore();
			if (score > this->bestScore || this->bestActions.empty()) {
				this->bestScore = score;
				this->bestActions = this->currentActions;
				//cerr << "bestscore now:" << this->bestScore << GetActionList() << endl;
			}
			//cerr << "Returning with score:" << score << GetActionList() << endl;
			return;
		}

		for (ACTION action : GetPossibleActions()) {
			World new_world = *this;
			new_world.DoAction(action);
			new_world.DFS(depth - 1);
		}
	}

	//Do stuff
	void DoAction(ACTION action)
	{
		Timer tim("World::DoAction");
		this->currentActions.push_back(action);
		switch (action) {
		case ACTION::UP:
			//cerr << "UP" << endl;
			SetMyLoc(GetCurrentLoc().x(), GetCurrentLoc().y() - 1);
			break;
		case ACTION::RIGHT:
			//cerr << "RIGHT" << endl;
			SetMyLoc(GetCurrentLoc().x() + 1, GetCurrentLoc().y());
			break;
		case ACTION::DOWN:
			//cerr << "DOWN" << endl;
			SetMyLoc(GetCurrentLoc().x(), GetCurrentLoc().y() + 1);
			break;
		case ACTION::LEFT:
			//cerr << "LEFT" << endl;
			SetMyLoc(GetCurrentLoc().x() - 1, GetCurrentLoc().y());
			break;
		case ACTION::BOMB:
			//cerr << "BOMB" << endl;
			SetMyBomb(GetCurrentLoc().x(), GetCurrentLoc().y());
			break;
		case ACTION::STAY:
			//cerr << "BOMB" << endl;
			SetMyLoc(GetCurrentLoc().x(), GetCurrentLoc().y());
			this->currentScore -= 3000; //Penalize for doing nothing
			break;
		default:
			cerr << "unknown action, someone fucked up" << action << endl;
			break;
		}
		if (action != BOMB) {
			SetCurrentRound(GetCurrentRound() + 1);
		}
	}

	void DoMove(int x, int y)
	{
		cout << "MOVE " << x << " " << y << (timeout_detected ? " !@$%&" : "") << endl;
		timeout_detected = false;
	}

	void DoBomb(int x, int y)
	{
		cout << "BOMB " << x << " " << y << (timeout_detected ? " !@$%&" : "") << endl;
		timeout_detected = false;
	}

private:
	bool timeout_detected = false;

	int current_round = 0;
	int sim_start_round = 0; //To know how deep in simulation I am
	int increment_bombs = 0; //Calculation on what explodes is done at end of previous round. Dont forget

	int currentScore = 0;
	static int bestScore;
	vector<ACTION> currentActions;
	static vector<ACTION> bestActions;

	static vector<Path> paths;
	static priority_queue<Path, vector<Path>, ComparePaths> bfs_pq;
	
	int player_id = 0; //my id
	Player players[4]; //In case I decide to track them

	Tile grid[MAP_WIDTH][MAP_HEIGHT];
	static unordered_map <Point, map<string, Point>, PointHash> graph;
	vector<Point> active_bombs;
};




std::chrono::system_clock::time_point Timer::global_start;
map<string, tuple<int, int>> Timer::stats;

int World::bestScore = 0;
vector<ACTION> World::bestActions;
unordered_map <Point, map<string, Point>, PointHash> World::graph;
vector<Path> World::paths;
priority_queue<Path, vector<Path>, World::ComparePaths> World::bfs_pq;






int main()
{
	int width;
	int height;
	int myId;
	cin >> width >> height >> myId; cin.ignore();

	World ai;
	ai.SetPlayerId(myId);

	// game loop
	int round_num = 0;
	while (1) {
		Timer::GlobalStart();
		for (int y = 0; y < height; y++) {
			string row;
			cin >> row; cin.ignore();

			//Read Map
			if (round_num == 0) {
				for (size_t x = 0; x < row.size(); x++) {

					Tile* current_tile = ai.GetTile(x, y);
					current_tile->SetLoc(x, y);
					if (row[x] == 'X') {
						current_tile->SetWall();
					}
					else
						if (row[x] == '0' || row[x] == '1' || row[x] == '2') {
							current_tile->SetBox(true);
							if (row[x] == '1') {
								current_tile->SetItem(ITEM::Range);
							} else
							if (row[x] == '2') {
								current_tile->SetItem(ITEM::BombCount);
							}
						}
				}

			}
		}
		if (round_num == 0) {
			ai.InitializeGraph();
		}


		int entities;
		cin >> entities; cin.ignore();
		for (int i = 0; i < entities; i++) {
			int entityType; //0 player, 1 bomb
			int owner; //id of player
					   //coords
			int x;
			int y;
			//For players: number of bombs the player can still place.
			//For bombs: number of rounds left until the bomb explodes.
			int param1;
			int param2;
			//The param2 is not useful for the current league, and will always be:
			//For players: explosion range of the player's bombs (= 3).
			//For bombs: explosion range of the bomb (= 3).
			cin >> entityType >> owner >> x >> y >> param1 >> param2; cin.ignore();
			if (entityType == 0) {
				ai.SetPlayerLoc(owner, x, y);
				ai.SetPlayerBombs(owner, param1);
				ai.SetPlayerRange(owner, param2);
			}
			else
			if (entityType == 1 && !ai.HasBombAt(x, y) && param1 == 8) {
				//cerr << "found bomb at x" << x << "y" << y << "p1:" << param1 << endl;
				ai.SetBomb(x, y, owner, round_num + param1, param2);
			}
		}
		ai.DoNextTurn();

		Timer::PrintStats();
		Timer::ResetStats();

		round_num++;
	}

	return 0;
}
