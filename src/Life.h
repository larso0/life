#ifndef LIFE_LIFE_H
#define LIFE_LIFE_H

#include <cstdint>
#include <vector>
#include <string>
#include <utility>

struct Cell
{
	Cell() : x{0}, y{0} {}
	Cell(int32_t x, int32_t y) : x{x}, y{y} {}
	int32_t x, y;
};

bool operator ==(const Cell& a, const Cell& b);
bool operator <(const Cell& a, const Cell& b);
std::istream& operator >>(std::istream& is, Cell& c);

using SparseGrid = std::vector<Cell>;
bool contains(const SparseGrid& grid, Cell c);
SparseGrid adjacent(const SparseGrid& grid, SparseGrid::const_iterator begin,
		    SparseGrid::const_iterator end);
uint32_t adjacent(const SparseGrid& grid, Cell c);
bool live(const SparseGrid& previous, Cell c);
SparseGrid advanceChunk(const SparseGrid& grid, SparseGrid::const_iterator begin,
		   SparseGrid::const_iterator end);
SparseGrid advance(const SparseGrid& prevoius);

void loadGrid(const std::string& fileName, SparseGrid& dst);

#endif