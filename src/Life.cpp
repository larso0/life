#include "Life.h"
#include <algorithm>
#include <thread>
#include <future>
#include <fstream>
#include <stdexcept>

using namespace std;

bool operator ==(const Cell& a, const Cell& b)
{
	return a.x == b.x && a.y == b.y;
}

bool operator <(const Cell& a, const Cell& b)
{
	return (a.x < b.x) ? true : ((a.x == b.x) ? (a.y < b.y) : false);
}

std::istream& operator >>(std::istream& is, Cell& c)
{
	is >> c.x;
	is >> c.y;
	return is;
}

bool contains(const SparseGrid& grid, Cell cell)
{
	return binary_search(grid.begin(), grid.end(), cell);
}

SparseGrid adjacent(SparseGrid::const_iterator begin, SparseGrid::const_iterator end)
{
	SparseGrid adj;
	for (auto i = begin; i != end; i++)
	{
		adj.emplace_back(i->x - 1, i->y - 1);
		adj.emplace_back(i->x, i->y - 1);
		adj.emplace_back(i->x + 1, i->y - 1);
		adj.emplace_back(i->x - 1, i->y);
		adj.emplace_back(i->x + 1, i->y);
		adj.emplace_back(i->x - 1, i->y + 1);
		adj.emplace_back(i->x, i->y + 1);
		adj.emplace_back(i->x + 1, i->y + 1);
	}
	sort(adj.begin(), adj.end());
	auto last = unique(adj.begin(), adj.end());
	adj.erase(last, adj.end());
	return adj;
}

uint32_t adjacent(const SparseGrid& grid, Cell c)
{
	uint32_t count = 0;
	if (contains(grid, {c.x - 1, c.y - 1})) count++;
	if (contains(grid, {c.x, c.y - 1})) count++;
	if (contains(grid, {c.x + 1, c.y - 1})) count++;
	if (contains(grid, {c.x - 1, c.y})) count++;
	if (contains(grid, {c.x + 1, c.y})) count++;
	if (contains(grid, {c.x - 1, c.y + 1})) count++;
	if (contains(grid, {c.x, c.y + 1})) count++;
	if (contains(grid, {c.x + 1, c.y + 1})) count++;
	return count;
}

bool live(const SparseGrid& previous, Cell c)
{
	uint32_t adj = adjacent(previous, c);
	return adj == 3 || (adj == 2 && contains(previous, c));
}

SparseGrid advanceChunk(const SparseGrid& grid, SparseGrid::const_iterator begin,
		   SparseGrid::const_iterator end)
{
	SparseGrid adj = adjacent(begin, end);
	SparseGrid next;

	for (auto i = begin; i != end; i++)
		if (live(grid, *i)) next.push_back(*i);
	for (Cell c : adj)
		if (live(grid, c)) next.push_back(c);

	sort(next.begin(), next.end());
	auto last = unique(next.begin(), next.end());
	next.erase(last, next.end());
	return next;
}

SparseGrid advance(const SparseGrid& previous)
{
	unsigned n = thread::hardware_concurrency();
	unsigned chunk = previous.size() / n;
	if (chunk < 64) chunk = 64;
	n = previous.size() / chunk;
	
	vector<future<SparseGrid>> futures;
	futures.reserve(n);
	auto iter = previous.begin();
	for (unsigned i = 0; i < n; i++)
	{
		auto next = iter + chunk;
		futures.push_back(async(launch::async, advanceChunk, previous, iter, next));
		iter = next;
	}
	if (iter != previous.end())
		futures.push_back(async(launch::async, advanceChunk, previous, iter, previous.end()));

	SparseGrid next;
	for (auto& fut : futures)
	{
		fut.wait();
		SparseGrid sub = fut.get();
		copy(sub.begin(), sub.end(), back_inserter(next));
	}

	sort(next.begin(), next.end());
	auto last = unique(next.begin(), next.end());
	next.erase(last, next.end());
	return next;
}

void loadGrid(const std::string& fileName, SparseGrid& dst)
{

	std::ifstream fs(fileName);
	if(!fs.is_open())
		throw std::runtime_error("Can not open \"" + fileName + "\".");

	std::string line;
	std::getline(fs, line);
	if(line != "#Life 1.06")
		throw std::runtime_error("Incorrect file format, not life 1.06");

	while (fs.good())
	{
		int x, y;
		fs >> x >> y;
		y = -y;
		dst.emplace_back(static_cast<int32_t>(x), static_cast<int32_t>(y));
	}

	sort(dst.begin(), dst.end());
	auto last = unique(dst.begin(), dst.end());
	dst.erase(last, dst.end());

	fs.close();
}