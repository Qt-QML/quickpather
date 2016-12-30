#include "box2dkineticsteeringagent.h"

#include <QQuickItem>

#include "quickentity.h"
#include "utils.h"

namespace QuickPather {

Box2DKineticSteeringAgent::Box2DKineticSteeringAgent()
{

}

static const qreal lenience = 0.5;

bool QuickPather::Box2DKineticSteeringAgent::steerTo(QuickPather::QuickEntity *entity, const QPointF &pos, qreal delta)
{
    // Assumes that the target QQuickItem has a "body" property.
    QObject *body = entity->item()->property("body").value<QObject*>();
    if (!body)
        return false;

    if (!body->property("linearVelocity").isValid())
        return false;

    // We want to get as close as possible...
    if (!Utils::isNextToTargetPos(entity, pos, lenience)) {
        const qreal angleToTarget = Utils::directionTo(entity->centrePos(), pos) + 90;
        entity->setRotation(angleToTarget);

        // ... without stopping too early. It's OK if we overshoot the target
        // with the "entity->speed() * delta" calculation, because we choose the
        // remaining distance instead in that case.
        qreal moveDistance = entity->speed() * delta;
        // TODO: this is fine for the default SteeringAgent which sets the item's position instantly,
        // but for Box2D, reducing the velocity makes the target visibly slow down as it gets to each node.
        // To account for this loss of accuracy, we bump the lenience up.
//        const QLineF lineToTarget(entity->centrePos(), pos);
//        if (lineToTarget.length() < moveDistance) {
//             moveDistance = lineToTarget.length();
//        }

        QPointF velocity = QTransform().rotate(angleToTarget).map(QPointF(0, moveDistance));
        body->setProperty("linearVelocity", -velocity);

        // TODO: does setting the linearVelocity result in instantaneous movement?
        // I assume not, so we have to wait for the next time step
        // in the physics engine, after which this function will have to be called again.
//        return Utils::isNextToTargetPos(entity, pos, lenience);
        return false;
    }

    body->setProperty("linearVelocity", QPointF(0, 0));
    // Forcing our position to the desired position eliminates the effect
    // where the target is constantly rotating and moving trying to get to the desired position.
    entity->setCentrePos(pos);
    return true;
}

}