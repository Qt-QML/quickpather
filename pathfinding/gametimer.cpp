#include "gametimer.h"

#include <QDebug>

GameTimer::GameTimer() :
    mRunning(false),
    mFps(100),
    mRemainder(0),
    mSimulatedTime(0),
    mTimeSpeedMultiplier(0)
{
}

void GameTimer::start(const QDateTime &gameStartDate)
{
    if (!gameStartDate.isValid()) {
        mGameStartDate = QDateTime::currentDateTime();
    }

    QObject::connect(&mTimer, SIGNAL(timeout()), this, SLOT(doUpdate()));

    setRunning(true);

    mTimer.start(5);
    mElapsedTimer.restart();
}

void GameTimer::pause()
{
    if (!mRunning) {
        qWarning() << "Timer already paused";
        return;
    }

    mTimer.disconnect(this);
}

void GameTimer::resume()
{
    if (mRunning) {
        qWarning() << "Timer already running";
        return;
    }

    // Disregard any time that's passed while the game was paused.
    mElapsedTimer.start();
    QObject::connect(&mTimer, SIGNAL(timeout()), this, SLOT(doUpdate()));
}

int GameTimer::fps() const
{
    return mFps;
}

QDateTime GameTimer::dateTime() const
{
    return mDateTime;
}

float GameTimer::timeSpeedMultiplier() const
{
    return mTimeSpeedMultiplier;
}

void GameTimer::setTimeSpeedMultiplier(float timeSpeedMultiplier)
{
    mTimeSpeedMultiplier = qMax(1.0f, timeSpeedMultiplier);
}

QDateTime GameTimer::dateFromSimulatedTime() const
{
    return mGameStartDate.addMSecs(mSimulatedTime * 1000 * mTimeSpeedMultiplier);
}

void GameTimer::setRunning(bool running)
{
    if (running == mRunning)
        return;

    mRunning = running;
}

void GameTimer::doUpdate()
{
    // Update by constant amount each loop until we've used the time elapsed since the last frame.
    static const float delta = 1.0 / mFps;
    // In seconds.
    float secondsSinceLastUpdate = mElapsedTimer.restart() * 0.001;
    mRemainder += secondsSinceLastUpdate;
    while (mRemainder > 0) {

        emit updated(delta);

        mDateTime = dateFromSimulatedTime();

        mRemainder -= delta;
        mSimulatedTime += delta;
    }
}


