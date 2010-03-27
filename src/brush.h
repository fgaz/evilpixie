#ifndef BRUSH_H
#define BRUSH_H

#include "img.h"
#include "palette.h"


enum BrushStyle { MASK, FULLCOLOUR };

class Brush : public IndexedImg
{
public:
    Brush( BrushStyle style, int w, int h, uint8_t const* initial=0, int transparent=-1 );
    virtual ~Brush();

    Point const& Handle() const     { return m_Handle; }
    void SetHandle( Point const& p ) { m_Handle=p; }
    int TransparentColour() const   { return m_Transparent; }
    BrushStyle Style() const          { return m_Style; }

    // TODO: should base IndexedImg have palette?
    Palette const& GetPalette()
        { return m_Palette; }

    void SetPalette( RGBx const* pal )
        { m_Palette = Palette(pal); }

private:
    BrushStyle m_Style;
    Point m_Handle;
    int m_Transparent;  // -1 = none

    Palette m_Palette;
};


#endif //BRUSH_H

