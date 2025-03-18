#ifndef QUANTENGINE_H
#define QUANTENGINE_H

#include <list>
#include <string.h>
#include <string>
#include <stdio.h>
#include <thread>
#include <mutex>
#include <fmt/core.h>
#include <unordered_map>
#include "PackMessage.hpp"
#include "Util.hpp"
#include "Singleton.hpp"
#include "FMTLogger.hpp"
#include "YMLConfig.hpp"
#include "HPPackClient.h"
#include "LockFreeQueue.hpp"
#include "SHMConnection.hpp"
#include "phmap.h"
#include <shared_mutex>
#include "TestStrategy.hpp"

#define APP_NAME "XQuant"

struct DataClientConf : public SHMIPC::CommonConf
{
    static const bool Publish = true;
    static const bool Performance = false;
};

struct ClientConf : public SHMIPC::CommonConf
{
    static const bool Publish = false;
    static const bool Performance = false;
};

class QuantEngine
{
    friend class Utils::Singleton<QuantEngine>;
public:
    explicit QuantEngine();
    virtual ~QuantEngine();
    void LoadConfig(const std::string& yml);
    void SetCommand(const std::string& cmd);
    void Start();
protected:
    void RegisterClient(const char *ip, unsigned int port);
    void WorkThreadFunc();

    void InitAppStatus();
    static void UpdateAppStatus(const std::string& cmd, Message::TAppStatus& AppStatus);
private:
    HPPackClient* m_pHPPackClient;
    Utils::XQuantConfig m_XQuantConfig;
    std::thread* m_pWorkThread;
    std::string m_Command;
    SHMIPC::SHMConnection<Message::PackMessage, DataClientConf>* m_pDataClient;
    typedef phmap::parallel_flat_hash_map<std::string, SHMIPC::SHMConnection<Message::PackMessage, ClientConf>*, 
                                        phmap::priv::hash_default_hash<std::string>,
                                        phmap::priv::hash_default_eq<std::string>,
                                        std::allocator<std::pair<const std::string, SHMIPC::SHMConnection<Message::PackMessage, ClientConf>*>>, 
                                        8, std::shared_mutex>
    OrderClientMapT;
    OrderClientMapT m_OrderClientMap;
    Message::PackMessage m_PackMessage;
    Message::PackMessage m_OrderMsg;
    StrategyEngine* m_pStrategy;
};


#endif // QUANTENGINE_H