#ifndef RANGESWIDGET_H
#define RANGESWIDGET_H

#include <QtWidgets/QWidget>

#include <cstdio>

#include "../colours.h"
#include "../layer.h"  // for NodePath
#include "../point.h"
#include "../projectlistener.h"
#include "../range.h"

class Project;

// Widget for picking and editing colour ranges.
class RangesWidget : public QWidget, ProjectListener
{
    Q_OBJECT
public:
	RangesWidget(QWidget *parent, Project& proj, NodePath const& target, int frame);
    ~RangesWidget();

    // Reconfigure for another layer/frame.
    // (doesn't emit any signals)
    void SetFocus(NodePath const& target, int frame);

signals:
    // upon sucessful drop
    void colourDropped(int idx, Colour const& c);

    void pickedFGPen(PenColour fg);
    void pickedBGPen(PenColour bg);
protected:
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void wheelEvent(QWheelEvent *event);
    void paintEvent(QPaintEvent *event);
    void resizeEvent(QResizeEvent *event);
    void leaveEvent( QEvent *event );
#if 0
    QSize sizeHint () const;
    QSize minimumSizeHint () const;
#endif

    void dragEnterEvent(QDragEnterEvent *event);
    void dragMoveEvent(QDragMoveEvent *event);
    void dropEvent(QDropEvent *event);

    // ProjectListener implementation
    // (we're only interested in palette-related stuff)
    virtual void OnPaletteChanged(NodePath const& target, int frame, int index, Colour const& c) override;
    virtual void OnPaletteReplaced(NodePath const& target, int frame) override; 
    virtual void OnRangesBlatted(NodePath const& target, int frame) override;

private:
    RangesWidget();    // disallowed
    Point PickCell(QPoint const& pos) const;
    void CalcCellRect(int col, int row, QRect& r) const;
    Project& m_Proj;
    NodePath m_Focus;
    int m_Frame;

    int m_Rows;
    int m_Cols;

    PenColour m_FGPen;
    PenColour m_BGPen;
};

#endif // RANGESWIDGET_H

