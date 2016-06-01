/*****************************************************************************
* Copyright 2015-2016 Alexander Barthel albar965@mailbox.org
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*****************************************************************************/

#include "mappainterils.h"
#include "common/symbolpainter.h"

#include "mapgui/mapscale.h"
#include "mapgui/maplayer.h"
#include "mapgui/mapquery.h"
#include "geo/calculations.h"
#include "common/maptypes.h"
#include "common/mapcolors.h"
#include "mapgui/mapwidget.h"

#include <QElapsedTimer>

#include <marble/GeoDataLineString.h>
#include <marble/GeoPainter.h>
#include <marble/ViewportParams.h>

using namespace Marble;
using namespace atools::geo;

MapPainterIls::MapPainterIls(MapWidget *mapWidget, MapQuery *mapQuery, MapScale *mapScale,
                             bool verboseMsg)
  : MapPainter(mapWidget, mapQuery, mapScale, verboseMsg)
{
}

MapPainterIls::~MapPainterIls()
{
}

void MapPainterIls::render(const PaintContext *context)
{
  using namespace maptypes;

  if(!context->objectTypes.testFlag(ILS))
    return;

  if(context->mapLayerEffective->isAirportDiagram())
    return;

  if(context->mapLayer->isIls())
  {
  bool drawFast = mapWidget->viewContext() == Marble::Animation;

  const GeoDataLatLonBox& curBox = context->viewport->viewLatLonAltBox();
  QElapsedTimer t;
  t.start();

  const QList<MapIls> *ilss = query->getIls(curBox, context->mapLayer, drawFast);
  if(ilss != nullptr)
  {
    setRenderHints(context->painter);
    if(mapWidget->viewContext() == Marble::Still && verbose)
    {
      qDebug() << "Number of ils" << ilss->size();
      qDebug() << "Time for query" << t.elapsed() << " ms";
      qDebug() << curBox.toString();
      qDebug() << *context->mapLayer;
      t.restart();
    }

    for(const MapIls& ils : *ilss)
    {
      int x, y;
      bool visible = wToS(ils.position, x, y, scale->getScreeenSizeForRect(ils.bounding));

      if(!visible)
      {
        GeoDataLatLonBox ilsbox(ils.bounding.getNorth(), ils.bounding.getSouth(),
                                ils.bounding.getEast(), ils.bounding.getWest(),
                                DEG);
        visible = curBox.intersects(ilsbox);
      }

      if(visible)
        drawIlsSymbol(context->painter, ils, context->mapLayer, drawFast);
    }
  }
  if(mapWidget->viewContext() == Marble::Still && verbose)
    qDebug() << "Time for paint" << t.elapsed() << " ms";
  }
}

void MapPainterIls::drawIlsSymbol(GeoPainter *painter, const maptypes::MapIls& ils,
                                  const MapLayer *mapLayer, bool fast)
{
  painter->save();

  painter->setBackgroundMode(Qt::TransparentMode);

  painter->setBrush(Qt::NoBrush);
  painter->setPen(QPen(mapcolors::ilsSymbolColor, 2, Qt::SolidLine, Qt::FlatCap));

  QSize size = scale->getScreeenSizeForRect(ils.bounding);
  bool visible;
  QPoint pmid = wToS(ils.posmid, size, &visible);
  QPoint origin = wToS(ils.position, size, &visible);

  QPoint p1 = wToS(ils.pos1, size, &visible);
  QPoint p2 = wToS(ils.pos2, size, &visible);

  painter->drawLine(origin, p1);
  painter->drawLine(p1, pmid);
  painter->drawLine(pmid, p2);
  painter->drawLine(p2, origin);

  if(ils.slope > 0)
    painter->drawLine(p1, p2);

  if(!fast)
  {
    QString text;
    if(mapLayer->isIlsInfo())
    {
      text = ils.ident + " / " +
             QLocale().toString(ils.frequency / 1000., 'f', 2) + " / " +
             QLocale().toString(atools::geo::normalizeCourse(ils.heading + ils.magvar), 'f', 0) + "°M";

      if(ils.slope > 0)
        text += " / GS " + QLocale().toString(ils.slope, 'f', 1) + "°";
      if(ils.dme)
        text += " / DME";
    }
    else if(mapLayer->isIlsIdent())
      text = ils.ident;

    if(!text.isEmpty())
    {
      // painter->setBrush(mapcolors::textBoxColor);
      painter->setPen(QPen(mapcolors::ilsTextColor, 0.5f, Qt::SolidLine, Qt::FlatCap));
      painter->translate(origin);

      float rotate;
      if(ils.heading > 180)
        rotate = ils.heading + 90.f - ils.width / 2.f;
      else
        rotate = atools::geo::opposedCourseDeg(ils.heading) + 90.f + ils.width / 2.f;

      int featherLen =
        static_cast<int>(std::roundf(scale->getPixelForMeter(nmToMeter(ILS_FEATHER_LEN_METER), rotate)));

      if(featherLen > 40)
      {
        QFontMetrics metrics = painter->fontMetrics();
        int texth = metrics.descent();

        text = metrics.elidedText(text, Qt::ElideRight, featherLen);
        int textw = metrics.width(text);

        int textpos;
        if(ils.heading > 180)
          textpos = (featherLen - textw) / 2;
        else
          textpos = -(featherLen + textw) / 2;

        painter->rotate(rotate);
        // painter->drawRect(textpos - 2, 2, textw + 2, -metrics.height() - 2);
        painter->drawText(textpos, -texth, text);
        painter->resetTransform();
      }
    }
  }
  painter->restore();
}
