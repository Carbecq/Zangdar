#include "ThreadData.h"

// https://www.chessprogramming.org/History_Heuristic

ThreadData::ThreadData()
{

}

//*********************************************************************
void ThreadData::reset()
{
    std::memset(_info,                 0, sizeof(SearchInfo)*STACK_SIZE);
    history.reset();
}

//=========================================================
//! \brief  Mise à jour de la Principal variation
//!
//! \param[in]  name   coup en notation UCI
//---------------------------------------------------------
void ThreadData::update_pv(SearchInfo* si, const MOVE move) const
{
    si->pv.length = 1 + (si+1)->pv.length;
    si->pv.line[0] = move;
    memcpy(si->pv.line+1, (si+1)->pv.line, sizeof(MOVE) * (si+1)->pv.length);
}

