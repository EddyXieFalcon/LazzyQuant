#include <QDebug>
#include <QCoreApplication>
#include <QCommandLineParser>

#include "config.h"
#include "connection_manager.h"
#include "multiple_timer.h"
#include "message_handler.h"
#include "option_helper.h"
#include "option_arbitrageur.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    QCoreApplication::setOrganizationName(ORGANIZATION);
    QCoreApplication::setApplicationName("option_arbitrageur_bundle");
    QCoreApplication::setApplicationVersion(VERSION_STR);

    QCommandLineParser parser;
    parser.setApplicationDescription("Option arbitrageur bundled with market watcher and trade executer.");
    parser.addHelpOption();
    parser.addVersionOption();

    parser.addOptions({
        {{"r", "replay"},
            QCoreApplication::translate("main", "Replay on a specified date"), "yyyyMMdd"},
        {{"u", "update"},
            QCoreApplication::translate("main", "Update subscribe list, should not be used with -r")},
        {{"f", "logtofile"},
            QCoreApplication::translate("main", "Save log to a file")},
    });

    parser.process(a);
    bool replayMode = parser.isSet("replay");
    QString replayDate = QString();
    if (replayMode) {
        replayDate = parser.value("replay");
    }
    bool update = parser.isSet("update");
    if (replayMode && update) {
        qCritical().noquote() << "Can not do update in replay mode!";
        return -1;
    }
    bool log2File = parser.isSet("logtofile");
    setupMessageHandler(true, log2File, "option_arbitrageur");

    com::lazzyquant::market_watcher *pWatcher = new com::lazzyquant::market_watcher(WATCHER_DBUS_SERVICE, WATCHER_DBUS_OBJECT, QDBusConnection::sessionBus());
    com::lazzyquant::trade_executer *pExecuter = nullptr;
    if (!replayMode) {
        pExecuter = new com::lazzyquant::trade_executer(EXECUTER_DBUS_SERVICE, EXECUTER_DBUS_OBJECT, QDBusConnection::sessionBus());
    }
    QStringList instruments;
    if (replayMode) {
        instruments = pWatcher->getSubscribeList();
    } else {
        instruments = pExecuter->getCachedInstruments();
    }
    OptionHelper helper(pExecuter);
    OptionArbitrageur arbitrageur(instruments, &helper);
    MultipleTimer *marketOpenTimer = nullptr;
    MultipleTimer *marketCloseTimer = nullptr;

    if (!replayMode) {
        if (update) {
            marketOpenTimer = new MultipleTimer({{8, 30}, {20, 30}});
            QObject::connect(marketOpenTimer, &MultipleTimer::timesUp, [pWatcher, pExecuter, &arbitrageur]() -> void {
                if ((pWatcher->getStatus() != "Ready") || (pExecuter->getStatus() != "Ready")) {
                    qCritical().noquote() << "Market watcher or trade executer is not ready!";
                    return;
                }
                const QStringList subscribedInstruments = pWatcher->getSubscribeList();
                const QStringList cachedInstruments = pExecuter->getCachedInstruments();
                const QSet<QString> underlyingIDs = arbitrageur.getUnderlyingIDs();
                QStringList instrumentsToSubscribe;
                for (const auto &item : cachedInstruments) {
                    if (!subscribedInstruments.contains(item)) {
                        for (const auto &underlyingID : qAsConst(underlyingIDs)) {
                            if (item.startsWith(underlyingID)) {
                                instrumentsToSubscribe << item;
                                break;
                            }
                        }
                    }
                }
                if (!instrumentsToSubscribe.empty()) {
                    pWatcher->subscribeInstruments(instrumentsToSubscribe);
                }
            });
        } else {
            buyLimit = std::bind((BUY_SELL_LIMIT_4_PARAM)&com::lazzyquant::trade_executer::buyLimit, pExecuter, _1, _2, _3, _4);
            sellLimit = std::bind((BUY_SELL_LIMIT_4_PARAM)&com::lazzyquant::trade_executer::sellLimit, pExecuter, _1, _2, _3, _4);
            marketOpenTimer = new MultipleTimer({{8, 30}, {20, 30}});
            QObject::connect(marketOpenTimer, &MultipleTimer::timesUp, [pWatcher, &arbitrageur]() -> void {
                if (pWatcher->getStatus() != "Ready") {
                    qCritical().noquote() << "Market watcher is not ready!";
                    return;
                }
                QString tradingDay = pWatcher->getTradingDay();
                arbitrageur.setTradingDay(tradingDay);
            });
            marketCloseTimer = new MultipleTimer({{15, 5}});
            QObject::connect(marketCloseTimer, &MultipleTimer::timesUp, &arbitrageur, &OptionArbitrageur::onMarketClose);
        }
    }
    ConnectionManager *pConnManager = nullptr;
    if (!update) {
        pConnManager = new ConnectionManager({pWatcher}, {&arbitrageur});
    }
    if (replayMode) {
        pWatcher->startReplay(replayDate);
    }

    int ret = a.exec();
    if (marketCloseTimer) {
        marketCloseTimer->disconnect();
        delete marketCloseTimer;
    }
    if (marketOpenTimer) {
        marketOpenTimer->disconnect();
        delete marketOpenTimer;
    }
    if (pConnManager)
        delete pConnManager;
    if (pExecuter)
        delete pExecuter;
    delete pWatcher;
    restoreMessageHandler();
    return ret;
}
