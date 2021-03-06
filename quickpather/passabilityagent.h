#ifndef PASSABILITYAGENT_H
#define PASSABILITYAGENT_H

#include <QPointF>
#include <QObject>

#include "quickpather_global.h"

namespace QuickPather {

class QuickEntity;

// Not pure abstract, because we want it to be usable in the Q_PROPERTY macro
// and not force derived classes to multiply derive from it and QObject.
class QUICKPATHERSHARED_EXPORT PassabilityAgent : public QObject
{
    Q_OBJECT
public:
    virtual bool isPassable(const QPointF &pos, QuickEntity *entity);
};

}

#endif // PASSABILITYAGENT_H
