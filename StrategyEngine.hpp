#ifndef STRATEGYENGINE_HPP
#define STRATEGYENGINE_HPP

#include "phmap.h"
#include <shared_mutex>

class StrategyEngine
{
public:
    bool Run(const Message::PackMessage& msg, Message::PackMessage& order)
    {
        bool ret = false;
        if(Message::EMessageType::EFutureMarketData == msg.MessageType)
        {
            ret = Calculate(msg.FutureMarketData, order.OrderRequest);
        }
        else if(Message::EMessageType::EStockMarketData == msg.MessageType)
        {
            ret = Calculate(msg.StockMarketData, order.OrderRequest);
        }
        return ret;
    }

    void OnAccountFund(const Message::TAccountFund& data)
    {

    }

    void OnAccountPosition(const Message::TAccountPosition& data)
    {

    }

    void OnOrderStatus(const Message::TOrderStatus& data)
    {

    }
protected:
    virtual bool Calculate(const MarketData::TFutureMarketData& data, Message::TOrderRequest& order)
    {
        return false;
    }

    virtual bool Calculate(const MarketData::TStockMarketData& data, Message::TOrderRequest& order)
    {
        return false;
    }

protected:
    typedef phmap::parallel_flat_hash_map<std::string, Message::TAccountFund, 
                                        phmap::priv::hash_default_hash<std::string>,
                                        phmap::priv::hash_default_eq<std::string>,
                                        std::allocator<std::pair<std::string, Message::TAccountFund>>, 
                                        8, std::shared_mutex>
    AccountFundMapT;
    AccountFundMapT m_AccountFundMap;
    typedef phmap::parallel_flat_hash_map<std::string, Message::TAccountPosition, 
                                        phmap::priv::hash_default_hash<std::string>,
                                        phmap::priv::hash_default_eq<std::string>,
                                        std::allocator<std::pair<std::string, Message::TAccountPosition>>, 
                                        8, std::shared_mutex>
    AccountPositionMapT;
    AccountPositionMapT m_AccountPositionMap;
    typedef phmap::parallel_flat_hash_map<std::string, Message::TOrderStatus, 
                                        phmap::priv::hash_default_hash<std::string>,
                                        phmap::priv::hash_default_eq<std::string>,
                                        std::allocator<std::pair<std::string, Message::TOrderStatus>>, 
                                        8, std::shared_mutex>
    OrderStatusMapT;
    OrderStatusMapT m_OrderStatusMap;
};

#endif // STRATEGYENGINE_HPP