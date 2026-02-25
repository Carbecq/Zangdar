#include "ThreadData.h"
#include "Search.h"

// https://www.chessprogramming.org/History_Heuristic

ThreadData::ThreadData()
{
}

ThreadData::ThreadData(ThreadData&& other) noexcept
    : thread(std::move(other.thread))
    , search(std::move(other.search))
    , nodes(other.nodes)
    , tbhits(other.tbhits)
    , history(std::move(other.history))
    , index(other.index)
    , depth(other.depth)
    , seldepth(other.seldepth)
    , score(other.score)
    , stopped(other.stopped.load(std::memory_order_relaxed))
    , best_move(other.best_move)
    , best_score(other.best_score)
    , best_depth(other.best_depth)
{
}
