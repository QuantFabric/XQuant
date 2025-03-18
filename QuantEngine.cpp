#include "QuantEngine.h"


QuantEngine::QuantEngine()
{
    m_pHPPackClient = NULL;
    m_pWorkThread = NULL;
    m_pDataClient = NULL;
    m_pStrategy = NULL;
}

QuantEngine::~QuantEngine()
{
    if(m_pHPPackClient)
    {
        delete m_pHPPackClient;
        m_pDataClient = NULL;
    }
    if(m_pWorkThread)
    {
        delete m_pHPPackClient;
        m_pWorkThread = NULL;
    }
    if(m_pDataClient)
    {
        delete m_pDataClient;
        m_pDataClient = NULL;
    }
    if(m_pStrategy)
    {
        delete m_pStrategy;
        m_pStrategy = NULL;
    }

    for(auto it = m_OrderClientMap.begin(); it != m_OrderClientMap.end(); it++)
    {   
        delete it->second;
        it->second = NULL;
    }
}

void QuantEngine::LoadConfig(const std::string& yml)
{
    std::string errorString;
    bool ret = Utils::LoadXQuantConfig(yml.c_str(), m_XQuantConfig, errorString);
    if(!ret)
    {
        FMTLOG(fmtlog::WRN, "QuantEngine::LoadXQuantConfig failed, {}", errorString);
    }
    else
    {
        FMTLOG(fmtlog::INF, "QuantEngine::LoadXQuantConfig {} successed {}", yml, m_XQuantConfig.AccountList.size());
        m_OrderMsg.MessageType = Message::EMessageType::EOrderRequest;
        m_OrderMsg.OrderRequest.EngineID = m_XQuantConfig.StrategyID;
        m_OrderMsg.OrderRequest.OrderType = Message::EOrderType::ELIMIT;
        m_OrderMsg.OrderRequest.Offset = 0;
        m_OrderMsg.OrderRequest.RiskStatus = Message::ERiskStatusType::EPREPARE_CHECKED;
    }
}

void QuantEngine::SetCommand(const std::string& cmd)
{
    m_Command = cmd;
    FMTLOG(fmtlog::INF, "QuantEngine::SetCommand cmd:{}", m_Command);
}

void QuantEngine::Start()
{
    // 登陆注册XWatcher
    RegisterClient(m_XQuantConfig.XWatcherIP.c_str(), m_XQuantConfig.XWatcherPort);
    std::string Accounts;
    for(int i = 0; i < m_XQuantConfig.AccountList.size(); i++)
    {
        Accounts += m_XQuantConfig.AccountList.at(i);
        Accounts += " ";
    }
    FMTLOG(fmtlog::INF, "QuantEngine::Start Strategy:{} StrategyID:{} MarketServer:{} OrderServer:{} Account:{}", 
            m_XQuantConfig.StrategyName, m_XQuantConfig.StrategyID, m_XQuantConfig.MarketServerName, m_XQuantConfig.OrderServerName, Accounts);
    // Update App Status
    InitAppStatus();
    
    // Connect to MarketServer
    m_pDataClient = new SHMIPC::SHMConnection<Message::PackMessage, DataClientConf>(m_XQuantConfig.StrategyName);
    m_pDataClient->Start(m_XQuantConfig.MarketServerName);
    if(m_pDataClient->IsConnected())
    {
        FMTLOG(fmtlog::INF, "QuantEngine::Start {} connected to {}", m_XQuantConfig.StrategyName, m_XQuantConfig.MarketServerName);
    }
    else
    {
        FMTLOG(fmtlog::WRN, "QuantEngine::Start {} connected to {} failed", m_XQuantConfig.StrategyName, m_XQuantConfig.MarketServerName);
    }

    // Connect to OrderServer
    for(int i = 0; i < m_XQuantConfig.AccountList.size(); i++)
    {
        FMTLOG(fmtlog::INF, "QuantEngine::Start {} connecting to {}", m_XQuantConfig.AccountList.at(i), m_XQuantConfig.OrderServerName + m_XQuantConfig.AccountList.at(i));
        SHMIPC::SHMConnection<Message::PackMessage, ClientConf>* Client = new SHMIPC::SHMConnection<Message::PackMessage, ClientConf>(m_XQuantConfig.AccountList.at(i));
        Client->Start(m_XQuantConfig.OrderServerName + m_XQuantConfig.AccountList.at(i));
        if(Client->IsConnected())
        {
            m_OrderClientMap[m_XQuantConfig.AccountList.at(i)] = Client;
            FMTLOG(fmtlog::INF, "QuantEngine::Start {} connected to {}", m_XQuantConfig.AccountList.at(i), m_XQuantConfig.OrderServerName + m_XQuantConfig.AccountList.at(i));
        }
        else
        {
            FMTLOG(fmtlog::WRN, "QuantEngine::Start {} connected to {} failed", m_XQuantConfig.AccountList.at(i), m_XQuantConfig.OrderServerName + m_XQuantConfig.AccountList.at(i));
        }
    }

    // 创建策略
    if(m_XQuantConfig.StrategyName == "TestStrategy")
    {
        m_pStrategy = new TestStrategy;
        FMTLOG(fmtlog::INF, "QuantEngine::Start created Strategy:{}", m_XQuantConfig.StrategyName);
    }

    if(m_pStrategy == NULL)
    {
        FMTLOG(fmtlog::ERR, "QuantEngine::Start Strategy:{} not implementation", m_XQuantConfig.StrategyName);
        memset(&m_PackMessage, 0, sizeof(m_PackMessage));
        m_PackMessage.MessageType = Message::EMessageType::EEventLog;
        m_PackMessage.EventLog.Level = Message::EEventLogLevel::EERROR;
        strncpy(m_PackMessage.EventLog.App, APP_NAME, sizeof(m_PackMessage.EventLog.App));
        strncpy(m_PackMessage.EventLog.Account, m_XQuantConfig.StrategyName.c_str(), sizeof(m_PackMessage.EventLog.Account));
        fmt::format_to_n(m_PackMessage.EventLog.Event, sizeof(m_PackMessage.EventLog.Event), 
                        "XQuant Strategy:{} not implementation", m_XQuantConfig.StrategyName);
        strncpy(m_PackMessage.EventLog.UpdateTime, Utils::getCurrentTimeUs(), sizeof(m_PackMessage.EventLog.UpdateTime));
        m_pHPPackClient->SendData((const unsigned char*)&m_PackMessage, sizeof(m_PackMessage));
    }
    
    m_pWorkThread = new std::thread(&QuantEngine::WorkThreadFunc, this);
    m_pDataClient->Join();
    m_pWorkThread->join();
    for(auto it = m_OrderClientMap.begin(); it != m_OrderClientMap.end(); it++)
    {   
        it->second->Join();
    }
}

void QuantEngine::RegisterClient(const char *ip, unsigned int port)
{
    m_pHPPackClient = new HPPackClient(ip, port);
    m_pHPPackClient->Start();
    sleep(1);
    Message::TLoginRequest login;
    login.ClientType = Message::EClientType::EXQUANT;
    strncpy(login.Account, m_XQuantConfig.StrategyName.c_str(), sizeof(login.Account));
    m_pHPPackClient->Login(login);
}

void QuantEngine::WorkThreadFunc()
{
    memset(&m_PackMessage, 0, sizeof(m_PackMessage));
    m_PackMessage.MessageType = Message::EMessageType::EEventLog;
    m_PackMessage.EventLog.Level = Message::EEventLogLevel::EINFO;
    strncpy(m_PackMessage.EventLog.App, APP_NAME, sizeof(m_PackMessage.EventLog.App));
    strncpy(m_PackMessage.EventLog.Account, m_XQuantConfig.StrategyName.c_str(), sizeof(m_PackMessage.EventLog.Account));
    fmt::format_to_n(m_PackMessage.EventLog.Event, sizeof(m_PackMessage.EventLog.Event), 
                    "XQuant {} Start, MarketServer:{} OrderServer:{}", 
                    m_XQuantConfig.StrategyName, m_XQuantConfig.MarketServerName, m_XQuantConfig.OrderServerName);
    strncpy(m_PackMessage.EventLog.UpdateTime, Utils::getCurrentTimeUs(), sizeof(m_PackMessage.EventLog.UpdateTime));
    m_pHPPackClient->SendData((const unsigned char*)&m_PackMessage, sizeof(m_PackMessage));

    FMTLOG(fmtlog::INF, "QuantEngine::WorkThreadFunc Strategy:{} Running", m_XQuantConfig.StrategyName);
    
    while (true)
    {
        // 收取行情数据
        m_pDataClient->HandleMsg();
        bool ok = m_pDataClient->Pop(m_PackMessage);
        if(ok)
        {
            bool NewOrder = false;
            if(Message::EMessageType::EFutureMarketData == m_PackMessage.MessageType)
            {
                NewOrder = m_pStrategy->Run(m_PackMessage, m_OrderMsg);
                FMTLOG(fmtlog::DBG, "QuantEngine::WorkThreadFunc recv data Ticker:{} UpdateTime:{} NewOrder:{}", 
                        m_PackMessage.FutureMarketData.Ticker, m_PackMessage.FutureMarketData.UpdateTime, NewOrder);
            }
            else if(Message::EMessageType::EStockMarketData == m_PackMessage.MessageType)
            {
                NewOrder = m_pStrategy->Run(m_PackMessage, m_OrderMsg);
                FMTLOG(fmtlog::DBG, "QuantEngine::WorkThreadFunc recv data Ticker:{} UpdateTime:{} NewOrder:{}", 
                        m_PackMessage.StockMarketData.Ticker, m_PackMessage.StockMarketData.UpdateTime, NewOrder);
            }
            if(NewOrder)
            {
                // 发送订单到内存队列
                for(auto it = m_OrderClientMap.begin(); it != m_OrderClientMap.end(); it++)
                {   
                    strncpy(m_OrderMsg.OrderRequest.Account, it->first.c_str(), sizeof(m_OrderMsg.OrderRequest.Account));
                    strncpy(m_OrderMsg.OrderRequest.SendTime, Utils::getCurrentTimeUs(), sizeof(m_OrderMsg.OrderRequest.SendTime));
                    it->second->Push(m_OrderMsg);
                    it->second->HandleMsg();
                }
            }
        }

        // 收取回报
        for(auto it = m_OrderClientMap.begin(); it != m_OrderClientMap.end(); it++)
        {   
            it->second->HandleMsg();
            bool ret = it->second->Pop(m_PackMessage);
            if(ret)
            {
                if(Message::EMessageType::EAccountFund == m_PackMessage.MessageType)
                {
                    m_pStrategy->OnAccountFund(m_PackMessage.AccountFund);
                    FMTLOG(fmtlog::DBG, "QuantEngine::WorkThreadFunc recv AccountFund Account:{} Balance:{}", 
                            m_PackMessage.AccountFund.Account, m_PackMessage.AccountFund.Balance);
                }
                else if(Message::EMessageType::EAccountPosition == m_PackMessage.MessageType)
                {
                    m_pStrategy->OnAccountPosition(m_PackMessage.AccountPosition);
                    FMTLOG(fmtlog::DBG, "QuantEngine::WorkThreadFunc recv AccountPosition Account:{} Ticker:{} UpdateTime:{}", 
                            m_PackMessage.AccountPosition.Account, m_PackMessage.AccountPosition.Ticker, m_PackMessage.AccountPosition.UpdateTime);
                }
                else if(Message::EMessageType::EOrderStatus == m_PackMessage.MessageType)
                {
                    m_pStrategy->OnOrderStatus(m_PackMessage.OrderStatus);
                    FMTLOG(fmtlog::DBG, "QuantEngine::WorkThreadFunc recv OrderStatus Account:{} Ticker:{} UpdateTime:{}", 
                            m_PackMessage.OrderStatus.Account, m_PackMessage.OrderStatus.Ticker, m_PackMessage.OrderStatus.UpdateTime);
                }
            }
        }
    }
}

void QuantEngine::InitAppStatus()
{
    Message::PackMessage message;
    message.MessageType = Message::EMessageType::EAppStatus;
    QuantEngine::UpdateAppStatus(m_Command, message.AppStatus);
    m_pHPPackClient->SendData((const unsigned char*)&message, sizeof(message));
}

void QuantEngine::UpdateAppStatus(const std::string& cmd, Message::TAppStatus& AppStatus)
{
    std::vector<std::string> ItemVec;
    Utils::Split(cmd, " ", ItemVec);
    std::string Account;
    for(int i = 0; i < ItemVec.size(); i++)
    {
        if(Utils::equalWith(ItemVec.at(i), "-a"))
        {
            Account = ItemVec.at(i + 1);
            break;
        }
    }
    strncpy(AppStatus.Account, Account.c_str(), sizeof(AppStatus.Account));

    std::vector<std::string> Vec;
    Utils::Split(ItemVec.at(0), "/", Vec);
    std::string AppName = Vec.at(Vec.size() - 1);
    strncpy(AppStatus.AppName, AppName.c_str(), sizeof(AppStatus.AppName));
    AppStatus.PID = getpid();
    strncpy(AppStatus.Status, "Start", sizeof(AppStatus.Status));

    char command[256] = {0};
    std::string AppLogPath;
    char* p = getenv("APP_LOG_PATH");
    if(p == NULL)
    {
        AppLogPath = "./log/";
    }
    else
    {
        AppLogPath = p;
    }
    fmt::format_to_n(AppStatus.StartScript, sizeof(AppStatus.StartScript), "nohup {} > {}/{}_{}_run.log 2>&1 &", 
                    cmd, AppLogPath, AppName, AppStatus.Account);
    std::string CommitID = std::string(APP_COMMITID) + ":" + SHMSERVER_COMMITID;
    strncpy(AppStatus.CommitID, CommitID.c_str(), sizeof(AppStatus.CommitID));
    strncpy(AppStatus.UtilsCommitID, UTILS_COMMITID, sizeof(AppStatus.UtilsCommitID));
    strncpy(AppStatus.APIVersion, API_VERSION, sizeof(AppStatus.APIVersion));
    strncpy(AppStatus.StartTime, Utils::getCurrentTimeUs(), sizeof(AppStatus.StartTime));
    strncpy(AppStatus.LastStartTime, Utils::getCurrentTimeUs(), sizeof(AppStatus.LastStartTime));
    strncpy(AppStatus.UpdateTime, Utils::getCurrentTimeUs(), sizeof(AppStatus.UpdateTime));
}
