#ifndef PROJECTLISTENER_H
#define PROJECTLISTENER_H

class Colour;
class Box;

// base class for things that want to be informed of
// changes to the Project.
//
class ProjectListener
{
public:
    virtual ~ProjectListener() {}
	virtual void OnDamaged( int frame, Box const& projdmg ) = 0;
    virtual void OnPaletteChanged( int n, Colour const& c ) = 0;
    virtual void OnPaletteReplaced() {}
    virtual void OnModifiedFlagChanged( bool ) {}
    virtual void OnPenChange() {}
    virtual void OnFramesAdded(int /*first*/, int /*last*/) {}
    virtual void OnFramesRemoved(int /*first*/, int /*last*/) {}

private:

};

#endif // PROJECTLISTENER_H

