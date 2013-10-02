#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>

class Logger : public QObject
{
Q_OBJECT

public:
    explicit Logger(QObject *parent = 0);
    void logDebug(const char *msg);
    void logWarning(const char *msg);
    void logCritical(const char *msg);
public slots:
   void toggleDebug(bool);
private:
    bool debugState;
};

#endif // LOGGER_H
