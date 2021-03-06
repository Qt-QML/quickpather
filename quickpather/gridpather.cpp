#include "gridpather.h"

#include <QDebug>
#include <QLoggingCategory>
#include <QQuickItem>

#include "passabilityagent.h"
#include "pathcache.h"
#include "gametimer.h"
#include "gridpathagent.h"
#include "quickentity.h"
#include "steeringagent.h"
#include "utils.h"

Q_LOGGING_CATEGORY(lcGridPather, "quickpather.gridpather")

namespace QuickPather {

GridPathData::GridPathData() :
    mCurrentNodeIndex(-1)
{
}

bool GridPathData::isValid() const
{
    return mCurrentNodeIndex != -1;
}

QPointF GridPathData::targetPos() const
{
    return mTargetPos;
}

QVector<QSharedPointer<GridPathNode> > GridPathData::nodes() const
{
    return mNodes;
}

int GridPathData::currentNodeIndex() const
{
    return mCurrentNodeIndex;
}

GridPather::GridPather(QObject *parent) :
    QObject(parent),
    mCellSize(32),
    mTimer(nullptr),
    mPassabilityAgent(nullptr),
    mSteeringAgent(nullptr),
    mPathCache(new PathCache(this))
{
}

static int iterationLimit = 1000;

static const QPointF north(0, -1);
static const QPointF south(0, 1);
static const QPointF east(1, 0);
static const QPointF west(-1, 0);

static const qreal northRotation = 0;
static const qreal southRotation = 180;
static const qreal eastRotation = 90;
static const qreal westRotation = 270;

static const int qtyStraightDirections = 4;

const QPointF straightDirections[qtyStraightDirections] = {
    north, south, east, west
};

/*
   Moves \a entity to \a pos.

   If a path to \a pos was found, any existing movement instructions are removed.
*/
bool GridPather::moveEntityTo(QuickEntity *entity, const QPointF &pos)
{
    if (!entity) {
        qWarning() << "GridPather: entity cannot be null";
        return false;
    }

    if (!mTimer) {
        qWarning() << "GridPather: no timer set";
        return false;
    }

    if (!mPassabilityAgent) {
        qWarning() << "GridPather: no passability agent set";
        return false;
    }

    if (!mSteeringAgent) {
        qWarning() << "GridPather: no steering agent set";
        return false;
    }

    const QPointF startPos = entity->centrePos();
    if (!mPassabilityAgent->isPassable(startPos, entity)) {
        qCDebug(lcGridPather) << "Starting position" << pos << "isn't passable for" << entity;
        return false;
    }

    // TODO: ensure target ends up precisely over node
    const int xDiff = qRound(startPos.x() - mCellSize / 2) % mCellSize;
    const int yDiff = qRound(startPos.y() - mCellSize / 2) % mCellSize;
    if (xDiff != 0 || yDiff != 0) {
        qWarning().nospace() << "GridPather: currently incapable of dealing with non-cell-centered positions ("
            << "the start position " << startPos << " does not result in the entity " << entity << " being centred in a cell)";
        // (Users of this API should simply wait until the target is centered before moving)
        return false;
    }

    qCDebug(lcGridPather) << "Looking for path to target pos" << pos;
    if (mPathCache) {
        const GridPathData cachedData = mPathCache->cachedData(entity, pos);
        if (cachedData.isValid()) {
            qCDebug(lcGridPather) << "Found cached path for" << entity << "to target pos" << pos;
            addEntity(entity, cachedData);
            return true;
        }
    }

    QVector<QSharedPointer<GridPathNode> > openList;
    QVector<QSharedPointer<GridPathNode> > closedList;
    QVector<QSharedPointer<GridPathNode> > shortestPath;

    // Starting square is automatically in the open list.
    QSharedPointer<GridPathNode> startNode(new GridPathNode(startPos));
    openList.push_back(startNode);

    // The selected node is the last selected node that was put on the closed list.
    QSharedPointer<GridPathNode> selectedNode;
    int iterations = 0;
    const QPointF nodeDist(mCellSize, mCellSize);
//    const QPointF boundsEnlargement = characterBoundsEnlargement();

    GridPathAgent pathAgent(pos);

    typedef QVector<QSharedPointer<GridPathNode> > NodeVector;
    typedef NodeVector::iterator node_it;
    typedef NodeVector::const_iterator const_node_it;

    int isPassableChecks = 0;

    do {
        // Look for the lowest "total cost" square on the open list. We refer to this as the selected node.
        node_it lowest = std::min_element(openList.begin(), openList.end(), totalScoreLessThan);
        // Switch it to the closed list.
        closedList.push_back(*lowest);
        onNodeAddedToClosedList((*lowest)->pos());
        selectedNode = *lowest;
        openList.erase(lowest);

        // The search is finished when ...
        if (pathAgent.isPathComplete(*entity, startPos, *selectedNode)) {
            break;
        }

        // For each of the 4 squares adjacent to this current square...
        for(int dirIndex = 0; dirIndex < qtyStraightDirections; ++dirIndex) {
            // Centre of the node.
            const QPointF nodePos(selectedNode->pos() + straightDirections[dirIndex] * nodeDist.x());

            QSharedPointer<GridPathNode> adjNode(new GridPathNode(nodePos));

            bool walkable = mPassabilityAgent->isPassable(adjNode->pos(), entity/*, &boundsEnlargement*/);
            bool isOnClosedList = std::find_if(closedList.begin(), closedList.end(),
                PathNodePosComp(adjNode)) != closedList.end();
            ++isPassableChecks;

            if(walkable && !isOnClosedList) {
                node_it onOpenList = std::find_if(openList.begin(), openList.end(), PathNodePosComp(adjNode));
                bool isOnOpenList = onOpenList != openList.end();

                if(isOnOpenList) {
                    // Check to see if this path to adj_node is better, using "start cost" as the measure.
                    QSharedPointer<GridPathNode> nodeCopy(new GridPathNode(*(*onOpenList)));

                    nodeCopy->setParent(selectedNode);
                    nodeCopy->setStartCost(pathAgent.calculateStartCost(*nodeCopy));
                    nodeCopy->setTargetCost(pathAgent.calculateTargetCost(*nodeCopy));

                    if((*onOpenList)->startCost() < nodeCopy->startCost()) {
                        // Is improved; change its parent to selected.
                        adjNode->setParent(selectedNode);
                        adjNode->setStartCost(pathAgent.calculateStartCost(*adjNode));
                        adjNode->setTargetCost(pathAgent.calculateTargetCost(*adjNode));
                    }
                } else {
                    // Not on open list; add it.
                    adjNode->setParent(selectedNode);
                    adjNode->setStartCost(pathAgent.calculateStartCost(*adjNode));
                    adjNode->setTargetCost(pathAgent.calculateTargetCost(*adjNode));

                    openList.push_back(adjNode);
                    onNodeAddedToOpenList(adjNode->pos());
                }
            }
        }

        ++iterations;
    } while(iterations < iterationLimit && !openList.empty());

    if (iterations == iterationLimit) {
        qWarning() << "iteration limit (" << iterationLimit << ") reached.";
        return false;
    }

    if(closedList.empty()) {
        qCDebug(lcGridPather) << "Impossible for" << entity << "to reach target pos" << pos;
        return false;
    }

    // To get the actual shortest path: working backwards from the target square,
    // we go from each node to its parent node until we reach the starting node.
    QSharedPointer<GridPathNode> node = closedList.back();
    QVector<QSharedPointer<GridPathNode> > test;
    while(node) {
        onNodeChosen(node->pos());

        shortestPath.push_front(node);
        test.insert(test.begin(), node);
        node = node->parent();
    }

    if(!shortestPath.isEmpty()) {
        shortestPath.pop_front();
    }

    GridPathData pathData;
    pathData.mTargetPos = pos;
    pathData.mNodes = shortestPath;
    pathData.mCurrentNodeIndex = 0;

    addEntity(entity, pathData);

    if (mPathCache) {
        // If we got this far, the path wasn't cached;
        // add it to the cache so it's faster next time.
        mPathCache->addCachedData(entity, pos, pathData);
    }

    qCDebug(lcGridPather).nospace() << "Successfully found path (" << shortestPath.size() << " nodes) for "
        << entity << " to target pos " << pos << " after " << isPassableChecks << " isPassable() checks";
    return true;
}

void GridPather::addEntity(QuickEntity *entity, const GridPathData &pathData)
{
    mData.insert(entity, pathData);
    connectToEntity(entity);
}

void GridPather::cancelEntityMovement(QuickEntity *entity)
{
    mData.remove(entity);
    disconnectFromEntity(entity);
}

void GridPather::connectToEntity(QuickEntity *entity)
{
    QObject::connect(entity, &QuickEntity::entityDestroyed, this, &GridPather::cancelEntityMovement);
}

void GridPather::disconnectFromEntity(QuickEntity *entity)
{
    QObject::disconnect(entity, &QuickEntity::entityDestroyed, this, &GridPather::cancelEntityMovement);
}

int GridPather::cellSize() const
{
    return mCellSize;
}

void GridPather::setCellSize(int cellSize)
{
    if (!mData.isEmpty()) {
        qWarning() << "Cannot set cell size while pathing active";
        return;
    }

    if (cellSize == mCellSize)
        return;

    mCellSize = cellSize;
    emit cellSizeChanged();
}

GameTimer *GridPather::timer() const
{
    return mTimer;
}

void GridPather::setTimer(GameTimer *timer)
{
    if (timer == mTimer)
        return;

    if (mTimer)
        mTimer->disconnect(this);

    mTimer = timer;
    emit timerChanged();

    if (mTimer)
        connect(mTimer, &GameTimer::updated, this, &GridPather::timerUpdated);
}

PassabilityAgent *GridPather::passabilityAgent()
{
    return mPassabilityAgent;
}

void GridPather::setPassabilityAgent(PassabilityAgent *passabilityAgent)
{
    if (!mData.isEmpty()) {
        qWarning() << "Cannot set passability agent while pathing active";
        return;
    }

    if (passabilityAgent == mPassabilityAgent)
        return;

    mPassabilityAgent = passabilityAgent;
    emit passabilityAgentChanged();
}

SteeringAgent *GridPather::steeringAgent()
{
    return mSteeringAgent;
}

void GridPather::setSteeringAgent(SteeringAgent *steeringAgent)
{
    if (!mData.isEmpty()) {
        qWarning() << "Cannot set steering agent while pathing active";
        return;
    }

    if (steeringAgent == mSteeringAgent)
        return;

    mSteeringAgent = steeringAgent;
    emit steeringAgentChanged();
}

PathCache *GridPather::pathCache()
{
    return mPathCache;
}

void GridPather::setPathCache(PathCache *cache)
{
    if (cache == mPathCache)
        return;

    mPathCache = cache;
    emit pathCacheChanged();
}

GridPathData GridPather::pathData(QuickEntity *entity) const
{
    GridPathData data;

    auto dataIt = mData.constFind(entity);
    if (dataIt != mData.constEnd()) {
        data = *dataIt;
    }

    return data;
}

#ifdef EXPOSE_VISUALISATION_API
void GridPather::onNodeAddedToClosedList(const QPointF &centrePos)
{
    emit nodeAddedToClosedList(centrePos);
}

void GridPather::onNodeAddedToOpenList(const QPointF &centrePos)
{
    emit nodeAddedToOpenList(centrePos);
}

void GridPather::onNodeChosen(const QPointF &centrePos)
{
    emit nodeChosen(centrePos);
}
#endif

void GridPather::onCellSizeChanged(int, int)
{
}

void GridPather::timerUpdated(qreal delta)
{
    QHashIterator<QuickEntity*, GridPathData> it(mData);
    while (it.hasNext()) {
        it.next();
        QuickEntity *entity = it.key();

        GridPathData &pathData = mData[entity];
        if (pathData.mCurrentNodeIndex < pathData.mNodes.size()) {
            const GridPathNode currentNode = *pathData.mNodes.at(pathData.mCurrentNodeIndex);
            if (mSteeringAgent->steerTo(entity, currentNode.pos(), delta)) {
                // We've reached the current node.
                ++pathData.mCurrentNodeIndex;
            }
        } else {
            mData.remove(entity);
        }
    }
}

}
