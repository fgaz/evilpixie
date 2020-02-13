#include "project.h"
#include "projectlistener.h"
#include "app.h"
#include "draw.h"
#include "colours.h"
#include "util.h"
#include "exception.h"
#include "global.h"

#include <assert.h>
#include <cstdio>


Project::Project( std::string const& filename ) :
    m_Expendable(false),
    m_Modified( false )
{
    Layer *l = new Layer();
    l->Load(filename.c_str());
    InsertLayer(l, 0);
    m_Filename = filename;
}


Project::Project() :
    m_Expendable(true),
    m_Modified( false )
{
    int w = 128;
    int h = 128;

    Palette* tmp = Palette::Load( JoinPath(g_App->DataPath(), "default.gpl").c_str());
    Layer *l = new Layer();
    l->SetPalette(*tmp);
    delete tmp;
    l->Append(new Img(FMT_I8,w,h));
    InsertLayer(l, 0);
}


Project::Project( PixelFormat fmt, int w, int h, Palette* palette, int num_frames ) :
    m_Expendable(false),
    m_Modified( false )
{
    assert(num_frames>=1);
    if(!palette) {
        palette = Palette::Load( JoinPath(g_App->DataPath(), "default.gpl").c_str());
    }
    Layer *l = new Layer();
    l->SetPalette(*palette);
    delete palette;
    int i;
    for(i = 0; i < num_frames; ++i) {
        l->Append(new Img(fmt, w, h));
    }
    InsertLayer(l,0);
}


Project::~Project()
{
    while (!m_Layers.empty()) {
        delete(m_Layers.back());
        m_Layers.pop_back();
    }
}

void Project::SetModifiedFlag( bool newmodifiedflag )
{
    if( m_Modified == newmodifiedflag )
        return;

    m_Expendable = false;
    m_Modified = newmodifiedflag;

    std::set<ProjectListener*>::iterator it;
    for( it=m_Listeners.begin(); it!=m_Listeners.end(); ++it )
    {
        (*it)->OnModifiedFlagChanged( newmodifiedflag );
    }
}

void Project::InsertLayer(Layer* layer, int pos)
{
    assert(pos >= 0 && pos <= (int)m_Layers.size());
    m_Layers.insert(m_Layers.begin() + pos, layer);
}

// KILL THIS!
void Project::ReplacePalette(Palette* newpalette)
{
    m_Layers[0]->SetPalette(*newpalette);
    delete newpalette;  // UGH!
    ImgID everything(-1,-1);
    NotifyPaletteReplaced(everything);
}


void Project::Save( std::string const& filename )
{
    m_Layers[0]->Save(filename.c_str());
    SetModifiedFlag(false);
    m_Filename = filename;
}



void Project::NotifyDamage(ImgID const& id, Box const& b )
{
    std::set<ProjectListener*>::iterator it;
    for( it=m_Listeners.begin(); it!=m_Listeners.end(); ++it )
    {
        (*it)->OnDamaged(id, b);
    }
}

void Project::NotifyFramesAdded(int first, int last)
{
    std::set<ProjectListener*>::iterator it;
    for( it=m_Listeners.begin(); it!=m_Listeners.end(); ++it )
    {
        (*it)->OnFramesAdded( first,last );
    }
}

void Project::NotifyFramesRemoved(int first, int last)
{
    std::set<ProjectListener*>::iterator it;
    for( it=m_Listeners.begin(); it!=m_Listeners.end(); ++it )
    {
        (*it)->OnFramesRemoved( first,last );
    }
}

void Project::NotifyLayerReplaced()
{
    std::set<ProjectListener*>::iterator it;
    for( it=m_Listeners.begin(); it!=m_Listeners.end(); ++it )
    {
        (*it)->OnLayerReplaced();
    }
}





void Project::NotifyPaletteChange( int first, int cnt )
{
    std::set<ProjectListener*>::iterator it;
    if (cnt == 1)
    {
        // TODO: which palette?
        Colour c = PaletteConst().GetColour(first);
        for (auto l : m_Listeners) {
            l->OnPaletteChanged(first,c);
        }
    } else {
        for (auto l : m_Listeners) {
            ImgID id(-1,-1);
            l->OnPaletteReplaced(id);
        }
    }
}

void Project::NotifyPaletteReplaced(ImgID const& id)
{
    for (auto l: m_Listeners) {
        l->OnPaletteReplaced(id);
    }
}




PenColour Project::PickUpPen(ImgID const& id, Point const& pt) const
{
    Img const& srcimg = GetImgConst(id);
    switch(srcimg.Fmt())
    {
    case FMT_I8:
        {
            I8 const* src = srcimg.PtrConst_I8(pt.x,pt.y);
            return PenColour(PaletteConst().GetColour(*src),*src);
        }
        break;
    case FMT_RGBX8:
        {
            RGBX8 const* src = srcimg.PtrConst_RGBX8(pt.x,pt.y);
            // TODO: could search for a palette index too...
            return PenColour(*src);
        }
        break;
    case FMT_RGBA8:
        {
            RGBA8 const* src = srcimg.PtrConst_RGBA8(pt.x,pt.y);
            // TODO: could search for a palette index too...
            RGBA8 c = *src;
            return PenColour(c);
        }
        break;
    default:
        assert(false);
        break;
    }
    return PenColour();
}

