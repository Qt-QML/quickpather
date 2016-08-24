#include "quickgridpather.h"

#include "quickentity.h"

QuickGridPather::QuickGridPather(QObject *parent) :
    GridPather(parent)
{
}

void QuickGridPather::moveTo(QuickEntity *entity, const QPointF &pos)
{
    moveEntityTo(entity, pos);
}

void QuickGridPather::cancel(QuickEntity *entity)
{
    cancelEntityMovement(entity);
}

void QuickGridPather::onCellSizeChanged(int, int)
{
    emit cellSizeChanged();
}

void QuickGridPather::onTimerChanged(GameTimer *, GameTimer *)
{
    emit timerChanged();
}

#ifdef EXPOSE_VISUALISATION_API
void QuickGridPather::onNodeAddedToClosedList(const QPointF &centrePos)
{
    emit nodeAddedToClosedList(centrePos);
}

void QuickGridPather::onNodeAddedToOpenList(const QPointF &centrePos)
{
    emit nodeAddedToOpenList(centrePos);
}

void QuickGridPather::onNodeChosen(const QPointF &centrePos)
{
    emit nodeChosen(centrePos);
}
#endif