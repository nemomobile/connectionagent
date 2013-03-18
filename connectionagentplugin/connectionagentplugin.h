#ifndef CONNECTIONAGENTPLUGIN_H
#define CONNECTIONAGENTPLUGIN_H

#include <QDeclarativeItem>
#include "connectionagentplugin.h"
#include "connectionamanagerinterface.h"

class ConnectionAgentPlugin : public QObject
{
    Q_OBJECT
 //   Q_DISABLE_COPY(ConnectionAgentPlugin)
    
public:
    ConnectionAgentPlugin(QObject *parent = 0);
    ~ConnectionAgentPlugin();

public slots:
    void sendUserReply(const QVariantMap &input);
    void sendConnectReply(const QString &replyMessage, int timeout = 120);

signals:
    void userInputRequested(const QString &servicePath, const QVariantMap &fields);
    void userInputCanceled();
    void errorReported(const QString &error);
    void connectionRequest();

//    void userConnectRequested(const QDBusMessage &message);
private:
    com::jolla::Connectiond *connManagerInterface;
    QDBusServiceWatcher *connectiondWatcher;

private slots:
    void onErrorReported(const QString &error);
    void onRequestBrowser(const QString &url);
    void onUserInputRequested(const QString &service, const QVariantMap &fields);
    void onConnectionRequested();

    void connectToConnectiond(const QString = QString());
    void connectiondUnregistered(const QString = QString());
};

#endif // CONNECTIONAGENTPLUGIN_H

