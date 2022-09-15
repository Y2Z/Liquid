#pragma once

#include <QSharedMemory>
#include <QSystemSemaphore>
#include <QWidget>

class SingleInstance : public QObject
{
    Q_OBJECT

public:
    explicit SingleInstance(QWidget *parent = Q_NULLPTR, QString *instanceId = new QString());
    ~SingleInstance();

    bool isAlreadyRunning(const bool raiseExisting = false);
    static void raiseWindow(QWidget *window);

private:
    struct SharedData
    {
        bool needToRaiseExistingWindow = false;
    };

    QSharedMemory *shMem;
    QString shmemName;
    QString smphorName;
};
