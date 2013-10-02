#include "logger.h"
#include <stdio.h>
#include <stdlib.h>

Logger::Logger(QObject *parent) :
    QObject(parent)
{
}

void Logger::toggleDebug(bool toggle)
{
   debugState = toggle;
}

void Logger::logDebug(const char *msg)
{
   if(debugState)
       fprintf(stderr, "%s\n", msg);
}

void Logger::logWarning(const char *msg)
{
   if(debugState)
       fprintf(stderr, "%s\n", msg);
}
void Logger::logCritical(const char *msg)
{
   if(debugState)
       fprintf(stderr, "%s\n", msg);
}
