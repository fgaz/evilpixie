#include "rgbpickerwidget.h"

#include "../project.h"

#include <algorithm>
#include <cassert>
#include <cmath>

#include <QColor>
#include <QPainter>
#include <QMouseEvent>


RGBPickerWidget::RGBPickerWidget() :
    m_Backing(0),
    m_LeftSel(),
    m_RightSel()
{
    setMouseTracking(true);
}

RGBPickerWidget::~RGBPickerWidget()
{
    if( m_Backing)
        delete m_Backing;
}


void RGBPickerWidget::UpdateBacking()
{
    if(m_Backing)
        delete m_Backing;
    m_Backing = new QImage(size(),QImage::Format_RGB32);


    // draw
    const int W = m_Backing->width();
    const int H = m_Backing->height();
    int y;
    for(y=0;y<H; ++y)
    {
        QRgb* dest = (QRgb*)m_Backing->scanLine(y);
        int x;
        for(x=0;x<W; ++x)
        {
            RGBx c = PointToRGB(QPoint(x,y));
            *dest++ = qRgb(c.r, c.g, c.b);
        }
    }
}


void RGBPickerWidget::SetLeftSelected( RGBx c )
{
    m_LeftSel = RGBToPoint(c);
    update();
}

void RGBPickerWidget::SetRightSelected( RGBx c )
{
    m_RightSel = RGBToPoint(c);
    update();
}


QPoint RGBPickerWidget::RGBToPoint( RGBx c ) const
{
    return QPoint(0,0);
}

RGBx RGBPickerWidget::PointToRGB( QPoint const& p ) const
{
    float x = (float)p.x() / (float)size().width();
    float y = (float)p.y() / (float)size().height();
    int zone = (int)(y*6.0f);
    float t = fmod(y,1.0f/6.0f) * 6.0f;
    assert(zone>=0 && zone<6);
    RGBx a,b;
    switch(zone)
    {
        case 0: a=RGBx(255,0,0); b=RGBx(255,255,0); break;
        case 1: a=RGBx(255,255,0); b=RGBx(0,255,0); break;
        case 2: a=RGBx(0,255,0); b=RGBx(0,255,255); break;
        case 3: a=RGBx(0,255,255); b=RGBx(0,0,255); break;
        case 4: a=RGBx(0,0,255); b=RGBx(255,0,255); break;
        case 5: a=RGBx(255,0,255); b=RGBx(255,0,0); break;
    } 

    if( x<0.5) {
        RGBx white(255,255,255);
        return Lerp(white,Lerp(a,b,t),x*2.0f);
    } else {
        RGBx black(0,0,0);
        return Lerp(Lerp(a,b,t),black,(x-0.5f)*2.0f);
    }

}

QSize RGBPickerWidget::sizeHint () const
    { return QSize( 128,256); }

QSize RGBPickerWidget::minimumSizeHint () const
    { return QSize( 32,64); }

void RGBPickerWidget::mousePressEvent(QMouseEvent *event)
{
    if( event->button() == Qt::LeftButton )
    {
        QPoint p = event->pos();
        m_LeftSel = p;
        RGBx c = PointToRGB(p);
        emit pickedLeftButton( c );
    }
    if( event->button() == Qt::RightButton )
    {
        QPoint p = event->pos();
        m_RightSel = p;
        RGBx c = PointToRGB(p);
        emit pickedRightButton( c );
    }
    update();
}

void RGBPickerWidget::mouseMoveEvent(QMouseEvent *event)
{
    update();
}

void RGBPickerWidget::mouseReleaseEvent(QMouseEvent *)
{
    update();
}

void RGBPickerWidget::leaveEvent( QEvent *event )
{
    update();
}

/*
void RGBPickerWidget::wheelEvent(QWheelEvent *)
{
}
*/

void RGBPickerWidget::resizeEvent(QResizeEvent *)
{
    UpdateBacking();
}


void RGBPickerWidget::paintEvent(QPaintEvent *)
{
    if(!m_Backing)
        UpdateBacking();
    QPainter painter(this);
   	painter.setPen(Qt::NoPen);
    painter.setBrush( QColor( 0,0,0) );
   	painter.drawRect( rect() );

    painter.drawImage( QPoint(0,0), *m_Backing);

}


