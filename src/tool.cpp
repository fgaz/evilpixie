#include "tool.h"
#include "app.h"
#include "draw.h"
#include "project.h"
#include "editor.h"
#include "editview.h"
#include "cmd.h"
#include "global.h"
#include "brush.h"

#include <algorithm>    // for min,max
#include <vector>

#include <cstdio>

// ---------------------
// helper class to collect multiple drawing ops into a single Cmd_Draw.
// Upon creation, DrawTransaction takes a backup of the image being drawn to.
// As the image is draw upon, AddDamage() should be called to keep track of the
// area which has been modified.
// When drawing is complete, Commit() will return a Cmd_Draw object in the DONE
// state (ie the drawing has already been performed).
// The returned cmd is ready to place upon the undo stack. It's set up to
// restore the original image if Undo() is called.
class DrawTransaction
{
public:
    DrawTransaction( Project& proj, int frame);
    // AddDamage expands the damaged area to include the given box.
    // It'll also call Project::Damage(), so all projectlisteners
    // will be informed that drawing has occured.
    void AddDamage(int frame, Box const& affected);
    // Commit() returns a Cmd (in the DONE state) which encapsulates the
    // changes to the image in an undoable form.
    Cmd* Commit();

    //
    void Rollback();

private:
    Project& m_Proj;
    int m_Frame;
    Img m_Backup;
    Box m_Affected;
};

DrawTransaction::DrawTransaction( Project& proj, int frame) :
    m_Proj(proj),
    m_Frame(frame),
    m_Backup( proj.ImgConst(frame)),        // copy pristine frame
    m_Affected( 0,0,0,0 )           // start with nothing affected
{
}

void DrawTransaction::AddDamage(int frame, Box const& b)
{
    assert(frame==m_Frame); // don't yet support drawing across multiple frames in one go...
    assert( m_Backup.Bounds().Contains(b) );
    m_Proj.Damage(frame,b);
    m_Affected.Merge(b);
}


Cmd* DrawTransaction::Commit()
{
    Cmd* c = new Cmd_Draw(m_Proj,m_Frame, m_Affected, m_Backup);
    return c;
}


void DrawTransaction::Rollback()
{
    Box dirty(m_Affected);
    Blit(m_Backup,m_Affected, m_Proj.GetAnim().GetFrame(m_Frame), dirty);
    m_Proj.Damage(m_Frame, dirty);
}


//-------


// helper for drawing cursor (using FG pen)
static void PlonkBrushToViewFG( EditView& view, Point const& pos, Box& viewdmg )
{
    Brush& b = view.Ed().CurrentBrush();
    Box pb( pos-b.Handle(), b.W(), b.H() );

    viewdmg = view.ProjToView( pb );
    DrawMode dm = view.Ed().Mode();

    if (dm.mode == DrawMode::DM_REPLACE)
    {
        if( b.Style() == MASK )
        {
            RectFill( view.Canvas(), viewdmg, view.Ed().FGPen());
        }
        else
        {
            BlitZoom( b, b.Bounds(),
                view.Canvas(), viewdmg,
                b.GetPalette(),
                view.XZoom(),
                view.YZoom());
        }
    }
    else if( b.Style() == MASK || dm.mode == DrawMode::DM_COLOUR )
    {
        BlitZoomMatte( b, b.Bounds(),
            view.Canvas(), viewdmg,
            view.XZoom(),
            view.YZoom(),
            b.TransparentColour(),
            view.Ed().FGPen() );
    }
    else
    {
        BlitZoomTransparent( b, b.Bounds(),
            view.Canvas(), viewdmg,
            view.Proj().PaletteConst(),
            view.XZoom(),
            view.YZoom(),
            b.TransparentColour());
    }
}

// helper for drawing cursor (using BG colour, _overriding_ any colour
// in the brush)
static void PlonkBrushToViewBG( EditView& view, Point const& pos, Box& viewdmg )
{
    Brush& b = view.Ed().CurrentBrush();
    Box pb( pos-b.Handle(), b.W(), b.H() );

    viewdmg = view.ProjToView( pb );

    BlitZoomMatte( b, b.Bounds(),
        view.Canvas(), viewdmg,
        view.XZoom(),
        view.YZoom(),
        b.TransparentColour(),
        view.Ed().BGPen() );
}


// helper to draw the current brush on a view, using the current editor settings
static void PlonkBrushToView( EditView& view, Point const& pos, Box& viewdmg, Button button )
{
    if( button==DRAW )
        PlonkBrushToViewFG( view,pos,viewdmg );
    else if( button==ERASE )
        PlonkBrushToViewBG( view,pos,viewdmg );
}



// helper to draw the current brush on the project, using the current editor settings
static void PlonkBrushToProj( Editor& ed, int frame, Point const& pos, Box& projdmg, Button button )
{
    Project& proj = ed.Proj();
    Brush const& brush = ed.CurrentBrush();
    Box dmg = brush.Bounds();
    dmg.Translate(pos);
    dmg.Translate(-brush.Handle());

    Img& target = proj.GetAnim().GetFrame(frame); 
    DrawMode dm = ed.Mode();
    if( button == DRAW )
    {
    
        if (dm.mode == DrawMode::DM_REPLACE)
        {
            if( brush.Style() == MASK )
                RectFill( target, dmg, ed.FGPen());
            else
                Blit( brush, brush.Bounds(), target, dmg);
        }
        else if( brush.Style() == MASK || dm.mode == DrawMode::DM_COLOUR )
        {
            BlitMatte( brush, brush.Bounds(),
                target, dmg,
                brush.TransparentColour(), ed.FGPen() );
        }
        else
        {
            BlitTransparent( brush,
                brush.Bounds(),
                brush.GetPalette(),
                target, dmg,
                brush.TransparentColour() );
        }
    }
    else if( button == ERASE )
    {
        if (dm.mode == DrawMode::DM_REPLACE)
        {
            RectFill( target, dmg, ed.BGPen());
        }
        else
        {
            BlitMatte( brush, brush.Bounds(),
                target, dmg,
                brush.TransparentColour(), ed.BGPen() );
        }
    }

    projdmg=dmg;
}


// helper - draw a crosshair cursor, centred on the given point (project coords)
void DrawCrossHairCursor( EditView& view, Point const& centre, Colour const& c )
{
    Box const& bnd = view.Canvas().Bounds();
    int yzoom = view.XZoom();
    int xzoom = view.YZoom();
    Point p = view.ProjToView(centre);

    Box hbox( bnd.XMin(), p.y, bnd.W(), 1*xzoom );
    view.Canvas().FillBox( c,hbox );
    view.AddCursorDamage( hbox );

    Box vbox( p.x, bnd.YMin(), 1*yzoom, bnd.H() );
    view.Canvas().FillBox( c,vbox );
    view.AddCursorDamage( vbox );
}








//------------------------------

PencilTool::PencilTool( Editor& owner ) :
    Tool( owner ),
    m_Pos(0,0),
    m_DownButton(NONE),
    m_View(0),
    m_Tx(0)
{
}

PencilTool::~PencilTool()
{
    if(m_Tx)
        delete m_Tx;
}

void PencilTool::OnDown( EditView& view, Point const& p, Button b )
{
//    printf("tool::down(%d)\n",b );
    m_Pos = p;
    if( m_DownButton!=NONE )
        return;

    m_DownButton = b;
    m_View = &view;
    assert(m_Tx==0);
    m_Tx = new DrawTransaction(view.Proj(),view.Frame());

    // draw 1st pixel
    Plot_cb( p.x, p.y, (void*)this );
}

void PencilTool::OnMove( EditView&, Point const& p)
{
    if( m_DownButton == NONE )
    {
        m_Pos = p;  // for cursor
        return;
    }

    // feels 'wrong' to do continuous lines if grid is on...
    if( Owner().GridActive() )
        Plot_cb( p.x, p.y, (void*)this );
    else
    {
        // TODO: should not draw first point (m_Pos) - it's already been drawn.
        WalkLine( m_Pos.x, m_Pos.y, p.x, p.y, Plot_cb, this );
    }
    m_Pos = p;
}

void PencilTool::OnUp( EditView& , Point const& p, Button b )
{
    if( m_DownButton != b )
        return;

    m_Pos = p;
    m_DownButton = NONE;
    m_View = 0;
    assert( m_Tx != 0 );
    Owner().AddCmd(m_Tx->Commit());
    delete m_Tx;
    m_Tx = 0;
}

void PencilTool::DrawCursor( EditView& view )
{
    if( m_DownButton != NONE )
        return;
    Box viewdmg;
    PlonkBrushToViewFG( view, m_Pos, viewdmg );
    view.AddCursorDamage( viewdmg );
}

//static
void PencilTool::Plot_cb( int x, int y, void* user )
{

    PencilTool* that = (PencilTool*)user;
    Editor& ed = that->Owner();
    EditView& view = *that->m_View;

    Box dmg;
    PlonkBrushToProj(ed,view.Frame(),Point(x,y), dmg, that->m_DownButton );

    assert(that->m_Tx != 0);
    that->m_Tx->AddDamage( view.Frame(),dmg );
}


//------------------------------


LineTool::LineTool( Editor& owner ) :
    Tool( owner ),
    m_From(0,0),
    m_To(0,0),
    m_DownButton(NONE),
    m_View(0),
    m_Tx(0)
{
}

LineTool::~LineTool()
{
    assert( m_Tx==0 );
}


void LineTool::OnDown( EditView& view, Point const& p, Button b )
{
//    printf("tool::down(%d)\n",b );
    if( m_DownButton!=NONE )
        return;
    m_View = &view;

    m_DownButton = b;
    m_From = p;
    m_To = p;
}

void LineTool::OnMove( EditView&, Point const& p)
{
    if( m_DownButton == NONE )
        m_From = p;     // needed to draw cursor
    else
        m_To = p;       // dragging for end of line
}

void LineTool::OnUp( EditView& view, Point const& p, Button b )
{
    if( m_DownButton == b )
    {
        DrawTransaction tx(view.Proj(),view.Frame());
        m_Tx = &tx; // so Plot_cb can get at it
        m_To = p;
        WalkLine( m_From.x, m_From.y, m_To.x, m_To.y, Plot_cb, this );
        Cmd* c = tx.Commit();
        m_Tx = 0;
        Owner().AddCmd(c);
        m_DownButton = NONE;
        m_View = 0;
        m_From = m_To;
    }
}

//static
void LineTool::Plot_cb( int x, int y, void* user )
{
    LineTool* that = (LineTool*)user;
    Editor& ed = that->Owner();
    EditView& view = *that->m_View;

    Box dmg;
    PlonkBrushToProj(ed,view.Frame(),Point(x,y), dmg, that->m_DownButton );

    assert(that->m_Tx != 0);
    that->m_Tx->AddDamage(that->m_View->Frame(),dmg);
}



void LineTool::DrawCursor( EditView& view )
{
    m_CursorDamage.SetEmpty();
    if( m_DownButton == NONE )
    {
        PlonkBrushToViewFG( view, m_From, m_CursorDamage );
    }
    else
    {
        WalkLine( m_From.x, m_From.y, m_To.x, m_To.y, PlotCursor_cb, this );
    }
    view.AddCursorDamage( m_CursorDamage );
}


//static
void LineTool::PlotCursor_cb( int x, int y, void* user )
{
    Point pos(x,y);

    LineTool* that = (LineTool*)user;
    EditView& view = *that->m_View;
    Editor& ed = that->Owner();
    Brush const& b = ed.CurrentBrush();
//    Project& proj = view.Proj();

    Box pb( Point(x,y)-b.Handle(), b.W(), b.H() );

    Box dmg;
    PlonkBrushToView( view, pos, dmg, that->m_DownButton );
    that->m_CursorDamage.Merge( dmg );
}


//------------------------------


BrushPickupTool::BrushPickupTool( Editor& owner ) :
    Tool( owner ),
    m_Anchor(0,0),
    m_DragPoint(0,0),
    m_DownButton(NONE),
    m_View(0)
{
}



void BrushPickupTool::OnDown( EditView&, Point const& p, Button b )
{
    if( m_DownButton!=NONE )
        return;
    m_DownButton = b;
    m_Anchor = p;
    m_DragPoint = p;
}

void BrushPickupTool::OnMove( EditView&, Point const& p)
{
    m_DragPoint = p;
}

void BrushPickupTool::OnUp( EditView& view, Point const& p, Button )
{
    Project& proj = view.Proj();

    if( m_DownButton == NONE )
        return;
    m_DragPoint = p;


    bool erase = (m_DownButton == ERASE);
    m_DownButton = NONE;

    Box pickup( m_Anchor, m_DragPoint );

    // if grid on, exclude right and bottom edge of selection
    if( Owner().GridActive() )
    {
        pickup.w -=1;
        pickup.h -=1;
    }

    pickup.ClipAgainst( proj.GetAnim().GetFrame(view.Frame()).Bounds() );

    if( pickup.Empty() )
        return;

    Brush* brush = new Brush( FULLCOLOUR, proj.GetAnim().GetFrame(view.Frame()), pickup, Owner().BGPen() );

    // copy in palette
    brush->SetPalette( proj.PaletteConst() );

    if( Owner().GridActive() )
        brush->SetHandle( Point(0,0) );

    g_App->SetCustomBrush( brush );
    Owner().SetBrush( -1 );

    // if picking up with right button, erase area after pickup
    if( erase )
    {
        // don't really need a draw transaction here (could just directly craft a Cmd_Draw())
        // but hey :-)
        DrawTransaction tx(view.Proj(), view.Frame());
        proj.GetAnim().GetFrame(view.Frame()).FillBox( Owner().BGPen(), pickup );
        tx.AddDamage(view.Frame(), pickup );
        Cmd* c = tx.Commit();
        Owner().AddCmd(c);
    }

    // NOTE: this is effectively "delete this!"
    Owner().UseTool( TOOL_PENCIL );
    // this ptr no longer valid...
}

void BrushPickupTool::DrawCursor( EditView& view )
{
    Colour white(255,255,255);
    Colour black(0,0,0);

    if( m_DownButton == NONE )
    {
        DrawCrossHairCursor( view, m_DragPoint, white );
        return;
    }

    Box pickup( m_Anchor, m_DragPoint );
    pickup.ClipAgainst( view.Proj().GetAnim().GetFrame(view.Frame()).Bounds() );

    Box vb = view.ProjToView( pickup );
    view.Canvas().OutlineBox( white,vb );
    vb.Expand(-1);
    view.Canvas().OutlineBox( black,vb );
    vb.Expand(1);
    view.AddCursorDamage( vb );
}




//------------------------------


FloodFillTool::FloodFillTool( Editor& owner ) :
    Tool(owner),
    m_Pos(0,0)
{
}



void FloodFillTool::OnDown( EditView& view, Point const& p, Button b )
{
    Project& proj = view.Proj();

    if( b != DRAW && b != ERASE )
        return;
    if( !proj.GetAnim().GetFrame(view.Frame()).Bounds().Contains(p ) )
        return;

    PenColour fillcolour = Owner().FGPen();
    if( b == ERASE )
        fillcolour = Owner().BGPen();

    {
        DrawTransaction tx(proj, view.Frame());
        Box dmg;
        FloodFill( proj.GetAnim().GetFrame(view.Frame()), p, fillcolour, dmg );
        if( !dmg.Empty() )
        {
            tx.AddDamage(view.Frame(), dmg );
            Cmd* c = tx.Commit();
            Owner().AddCmd(c);
        }
    }
}

void FloodFillTool::OnMove( EditView& , Point const& p)
{
    m_Pos = p;
}

void FloodFillTool::OnUp( EditView& , Point const& , Button )
{
}

void FloodFillTool::DrawCursor( EditView& view )
{
    Colour white(255,255,255);
    Colour black(0,0,0);
    Box c( m_Pos, 1,1 );
    Box vb = view.ProjToView( c );
    view.Canvas().OutlineBox( white,vb );
    vb.Expand(-1);
    view.Canvas().OutlineBox( black,vb );
    vb.Expand(1);
    view.AddCursorDamage( vb );
}


//------------------------------


RectTool::RectTool( Editor& owner ) :
    Tool( owner ),
    m_From(0,0),
    m_To(0,0),
    m_DownButton(NONE)
{
}


void RectTool::OnDown( EditView& , Point const& p, Button b )
{
//    printf("tool::down(%d)\n",b );
    if( m_DownButton!=NONE )
        return;
    m_DownButton = b;
    m_From = p;
    m_To = p;
}

void RectTool::OnMove( EditView& , Point const& p)
{
    if( m_DownButton == NONE )
        m_From = p;     // needed to draw cursor
    else
        m_To = p;       // dragging for end of line
}

void RectTool::OnUp( EditView& view, Point const& p, Button b )
{
    if( m_DownButton != b )
        return;

    m_To = p;
    Box dmg;
    Box rect( m_From, m_To );

    int x,y;

    DrawTransaction tx(view.Proj(),view.Frame());

    // top (including left and rightmost pixels)
    dmg.SetEmpty();
    y = rect.YMin();
    for( x=rect.XMin(); x<=rect.XMax(); ++x )
    {
        Box tmp;
        PlonkBrushToProj( view.Ed(), view.Frame(), Point(x,y), tmp, m_DownButton );
        dmg.Merge( tmp );
    }
    tx.AddDamage( view.Frame(), dmg );

    // right edge (exclude top and bottom rows)
    dmg.SetEmpty();
    x = rect.XMax();
    for( y=rect.YMin()+1; y<=rect.YMax()-1; ++y )
    {
        Box tmp;
        PlonkBrushToProj( view.Ed(), view.Frame(), Point(x,y), tmp, m_DownButton );
        dmg.Merge( tmp );
    }
    tx.AddDamage( view.Frame(), dmg );

    // bottom edge (including right and leftmost pixels)
    dmg.SetEmpty();
    y = rect.YMax();
    for( x=rect.XMax(); x>=rect.XMin(); --x )
    {
        Box tmp;
        PlonkBrushToProj( view.Ed(), view.Frame(), Point(x,y), tmp, m_DownButton );
        dmg.Merge( tmp );
    }
    tx.AddDamage( view.Frame(), dmg );

    // left edge (exclude bottom and top rows)
    dmg.SetEmpty();
    x = rect.XMin();
    for( y=rect.YMax()-1; y>=rect.YMin()+1; --y )
    {
        Box tmp;
        PlonkBrushToProj( view.Ed(), view.Frame(), Point(x,y), tmp, m_DownButton );
        dmg.Merge( tmp );
    }
    tx.AddDamage( view.Frame(), dmg );

    Cmd* c = tx.Commit();
    Owner().AddCmd(c);
    m_DownButton = NONE;
    m_From = m_To;
}



void RectTool::DrawCursor( EditView& view )
{
    Box dmg;
    if( m_DownButton == NONE )
    {
        // not rubberbanding
        Colour white(255,255,255);
        DrawCrossHairCursor( view, m_From, white );
        PlonkBrushToView( view, m_From, dmg, DRAW );
        view.AddCursorDamage( dmg );
        return;
    }

    // rubberbanding

    Box rect( m_From, m_To );
    int x,y;

    // top (include left and rightmost pixels)
    dmg.SetEmpty();
    y = rect.YMin();
    for( x=rect.XMin(); x<=rect.XMax(); ++x )
    {
        Box tmp;
        PlonkBrushToView( view, Point(x,y), tmp, m_DownButton );
        dmg.Merge( tmp );
    }
    view.AddCursorDamage( dmg );

    // right edge (exclude top and bottom rows)
    dmg.SetEmpty();
    x = rect.XMax();
    for( y=rect.YMin()+1; y<=rect.YMax()-1; ++y )
    {
        Box tmp;
        PlonkBrushToView( view, Point(x,y), tmp, m_DownButton );
        dmg.Merge( tmp );
    }
    view.AddCursorDamage( dmg );

    // bottom edge (include right and leftmost)
    dmg.SetEmpty();
    y = rect.YMax();
    for( x=rect.XMax(); x>=rect.XMin(); --x )
    {
        Box tmp;
        PlonkBrushToView( view, Point(x,y), tmp, m_DownButton );
        dmg.Merge( tmp );
    }
    view.AddCursorDamage( dmg );

    // left edge (exclude bottom and top)
    dmg.SetEmpty();
    x = rect.XMin();
    for( y=rect.YMax()-1; y>=rect.YMin()+1; --y )
    {
        Box tmp;
        PlonkBrushToView( view, Point(x,y), tmp, m_DownButton );
        dmg.Merge( tmp );
    }
    view.AddCursorDamage( dmg );

}



//------------------------------


FilledRectTool::FilledRectTool( Editor& owner ) :
    Tool( owner ),
    m_From(0,0),
    m_To(0,0),
    m_DownButton(NONE)
{
}


void FilledRectTool::OnDown( EditView& , Point const& p, Button b )
{
//    printf("tool::down(%d)\n",b );
    if( m_DownButton!=NONE )
        return;
    m_DownButton = b;
    m_From = p;
    m_To = p;
}

void FilledRectTool::OnMove( EditView& , Point const& p)
{
    if( m_DownButton == NONE )
        m_From = p;     // needed to draw cursor
    else
        m_To = p;       // dragging for end of line
}

void FilledRectTool::OnUp( EditView& view, Point const& p, Button b )
{
    if( m_DownButton != b )
        return;

    Project& proj = view.Proj();

    m_To = p;
    Img& img = proj.GetAnim().GetFrame(view.Frame());
    Box r( m_From, m_To );

    DrawTransaction tx(view.Proj(), view.Frame());
    if( m_DownButton == DRAW )
        img.FillBox( Owner().FGPen(),r );
    else    //if( m_DownButton == ERASE )
        img.FillBox( Owner().BGPen(),r );

    tx.AddDamage(view.Frame(),r);    
    Cmd* c = tx.Commit();
    Owner().AddCmd(c);
    m_DownButton = NONE;
    m_From = m_To;
}    




void FilledRectTool::DrawCursor( EditView& view )
{
    if( m_DownButton == NONE )
    {
        Colour white(255,255,255);
        // not rubberbanding
        DrawCrossHairCursor( view, m_From, white );
        return;
    }

    // rubberbanding
    Box r = view.ProjToView( Box( m_From, m_To ) );


    PenColour c;
    if( m_DownButton == DRAW )
        c = Owner().FGPen();
    else
        c = Owner().BGPen();

    view.Canvas().FillBox(c,r );
    view.AddCursorDamage( r );
}


//------------------------------


CircleTool::CircleTool( Editor& owner ) :
    Tool( owner ),
    m_From(0,0),
    m_To(0,0),
    m_DownButton(NONE),
    m_View(0),
    m_Tx(0)
{
}

CircleTool::~CircleTool()
{
    assert(m_Tx==0);
}

void CircleTool::OnDown( EditView& view, Point const& p, Button b )
{
//    printf("tool::down(%d)\n",b );
    if( m_DownButton!=NONE )
        return;
    m_View = &view;

    m_DownButton = b;
    m_From = p;
    m_To = p;
}

void CircleTool::OnMove( EditView& , Point const& p)
{
    if( m_DownButton == NONE )
        m_From = p;     // needed to draw cursor
    else
        m_To = p;       // dragging for end of line
}

void CircleTool::OnUp( EditView& view, Point const& p, Button b )
{
    if( m_DownButton != b )
        return;

    m_To = p;

    DrawTransaction tx(view.Proj(),view.Frame());
    m_Tx = &tx;
    int rx = std::abs( m_To.x - m_From.x );
    int ry = std::abs( m_To.y - m_From.y );
    WalkEllipse( m_From.x, m_From.y, rx, ry, Plot_cb, this );
    Cmd* c= tx.Commit();
    m_Tx = 0;
    Owner().AddCmd(c);
    m_DownButton = NONE;
    m_View = 0;
    m_From = m_To;
}

//static
void CircleTool::Plot_cb( int x, int y, void* user )
{
    CircleTool* that = (CircleTool*)user;
    Editor& ed = that->Owner();
    EditView& view = *that->m_View;

    Box dmg;
    PlonkBrushToProj(ed,view.Frame(),Point(x,y), dmg, that->m_DownButton );

    assert(that->m_Tx != 0 );
    that->m_Tx->AddDamage(view.Frame(),dmg);
}



void CircleTool::DrawCursor( EditView& view )
{
    m_CursorDamage.SetEmpty();
    if( m_DownButton == NONE )
    {
        Colour white(255,255,255);
        DrawCrossHairCursor( view, m_From, white );
        PlonkBrushToViewFG( view, m_From, m_CursorDamage );
    }
    else
    {
        int rx = std::abs( m_To.x - m_From.x );
        int ry = std::abs( m_To.y - m_From.y );
        WalkEllipse( m_From.x, m_From.y, rx, ry, PlotCursor_cb, this );
    }
    view.AddCursorDamage( m_CursorDamage );
}


//static
void CircleTool::PlotCursor_cb( int x, int y, void* user )
{
    Point pos(x,y);

    CircleTool* that = (CircleTool*)user;
    EditView& view = *that->m_View;
    Editor& ed = that->Owner();
    Brush const& b = ed.CurrentBrush();
//    Project& proj = view.Proj();

    Box pb( Point(x,y)-b.Handle(), b.W(), b.H() );

    Box dmg;
    PlonkBrushToView( view, pos, dmg, that->m_DownButton );
    that->m_CursorDamage.Merge( dmg );
}

//------------------------------


FilledCircleTool::FilledCircleTool( Editor& owner ) :
    Tool( owner ),
    m_From(0,0),
    m_To(0,0),
    m_DownButton(NONE),
    m_View(0),
    m_Tx(0)
{
}

FilledCircleTool::~FilledCircleTool()
{
    assert(m_Tx==0);
}


void FilledCircleTool::OnDown( EditView& view, Point const& p, Button b )
{
//    printf("tool::down(%d)\n",b );
    if( m_DownButton!=NONE )
        return;
    m_View = &view;

    m_DownButton = b;
    m_From = p;
    m_To = p;
}

void FilledCircleTool::OnMove( EditView& , Point const& p)
{
    if( m_DownButton == NONE )
        m_From = p;     // needed to draw cursor
    else
        m_To = p;       // dragging for end of line
}

void FilledCircleTool::OnUp( EditView& view, Point const& p, Button b )
{
    if( m_DownButton != b )
        return;

    m_To = p;

    assert(m_Tx==0);
    DrawTransaction tx(view.Proj(), view.Frame());
    m_Tx = &tx;
    int rx = std::abs( m_To.x - m_From.x );
    int ry = std::abs( m_To.y - m_From.y );
    WalkFilledEllipse( m_From.x, m_From.y, rx, ry, Draw_hline_cb, this );
    Cmd* c = tx.Commit();
    m_Tx = 0;
    Owner().AddCmd(c);
    m_DownButton = NONE;
    m_View = 0;
    m_From = m_To;
}

//static
void FilledCircleTool::Draw_hline_cb( int x0, int x1, int y, void* user )
{
    FilledCircleTool* that = (FilledCircleTool*)user;
    EditView& view = *that->m_View;
    Project& proj = view.Proj();

    Box b( x0,y,x1-x0,1 );
    if( that->m_DownButton == DRAW )
        proj.GetAnim().GetFrame(view.Frame()).FillBox(that->Owner().FGPen(),b);
    else if( that->m_DownButton == ERASE )
        proj.GetAnim().GetFrame(view.Frame()).FillBox(that->Owner().BGPen(),b);
    assert(that->m_Tx!=0);
    that->m_Tx->AddDamage(view.Frame(), b );
}



void FilledCircleTool::DrawCursor( EditView& view )
{
    m_CursorDamage.SetEmpty();
    if( m_DownButton == NONE )
    {
        Colour white(255,255,255);
        DrawCrossHairCursor( view, m_From, white );
        PlonkBrushToViewFG( view, m_From, m_CursorDamage );
    }
    else
    {
        int rx = std::abs( m_To.x - m_From.x );
        int ry = std::abs( m_To.y - m_From.y );
        WalkFilledEllipse( m_From.x, m_From.y, rx, ry, Cursor_hline_cb, this );
    }
    view.AddCursorDamage( m_CursorDamage );
}


//static
void FilledCircleTool::Cursor_hline_cb( int x0, int x1, int y, void* user )
{
    Point pos(x0,y);

    FilledCircleTool* that = (FilledCircleTool*)user;
    EditView& view = *that->m_View;

    PenColour c;
    if( that->m_DownButton == DRAW )
        c = that->Owner().FGPen();
    else
        c = that->Owner().BGPen();

    Box projb( x0,y,x1-x0,1 );
    Box viewb = view.ProjToView( projb );
    view.Canvas().FillBox( c, viewb );

    that->m_CursorDamage.Merge( viewb );
}

//------------------------------


EyeDropperTool::EyeDropperTool( Editor& owner, ToolType previoustooltype ) :
    Tool(owner),
    m_PreviousToolType(previoustooltype)
{
    owner.SetMouseStyle( MOUSESTYLE_EYEDROPPER );
}


EyeDropperTool::~EyeDropperTool()
{
    Owner().SetMouseStyle( MOUSESTYLE_DEFAULT );
}


void EyeDropperTool::OnDown( EditView& view, Point const& p, Button b )
{
    if( b != DRAW && b != ERASE )
        return;
    Project& proj = view.Proj();
    if( !proj.GetAnim().GetFrame(view.Frame()).Bounds().Contains(p ) )
        return;

    PenColour c = proj.PickUpPen(p, view.Frame());

    if( b == DRAW )
        Owner().SetFGPen(c);
    else if( b == ERASE )
        Owner().SetBGPen(c);

    Owner().UseTool( m_PreviousToolType );
}

void EyeDropperTool::OnMove( EditView& , Point const&)
{
}

void EyeDropperTool::OnUp( EditView& , Point const& , Button )
{
}

void EyeDropperTool::DrawCursor( EditView& )
{
/*
    Box c( m_Pos, 1,1 );
    Box vb = view.ProjToView( c );
    Colour white( Colour(255,255,255) );
    Colour black( Colour(0,0,0) );
    view.Canvas().OutlineBox( white,vb );
    vb.Expand(-1);
    view.Canvas().OutlineBox( black,vb );
    vb.Expand(1);
    view.AddCursorDamage( vb );
*/
}

