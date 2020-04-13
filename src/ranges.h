#pragma once

#include "colours.h"
#include "point.h"
#include <vector>

struct Range {
    std::vector<PenColour> pens;
    // Cosmetic stuff. For editing ranges with pretty layout.
    Point pos;
    bool horizontal;

};



class RangeGrid {
public:
    RangeGrid(int w, int h);

    bool Get(Point const& pos, PenColour& out) const;
    void Set(Point const& pos, PenColour const& pen);
    void Clear(Point const& pos);
    Box const& Bound() const { return m_Bound; }
private:
    Box m_Bound;
    std::vector<bool> m_Valid;
    std::vector<PenColour> m_Pens;
};

inline RangeGrid::RangeGrid(int w, int h) :
    m_Bound(0, 0, w, h),
    m_Valid(w * h, false),
    m_Pens(w * h)
{
}

inline bool RangeGrid::Get(Point const& pos, PenColour& out) const
{
    if (!m_Bound.Contains(pos)) {
        return false;
    }
    size_t idx = ((pos.y-m_Bound.y) * m_Bound.w) + (pos.x - m_Bound.x);
    if (!m_Valid[idx]) {
        return false;
    }
    out = m_Pens[idx];
    return true;
}

inline void RangeGrid::Set(Point const& pos, PenColour const& pen)
{
    assert(m_Bound.Contains(pos));
    size_t idx = ((pos.y-m_Bound.y) * m_Bound.w) + (pos.x - m_Bound.x);
    m_Valid[idx] = true;
    m_Pens[idx] = pen;
}

inline void RangeGrid::Clear(Point const& pos)
{
    assert(m_Bound.Contains(pos));
    size_t idx = ((pos.y-m_Bound.y) * m_Bound.w) + (pos.x - m_Bound.x);
    m_Valid[idx] = false;
}

