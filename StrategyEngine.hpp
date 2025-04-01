#ifndef STRATEGYENGINE_HPP
#define STRATEGYENGINE_HPP
#include <fmt/core.h>
#include "phmap.h"
#include <shared_mutex>
#include<time.h>

#include "FMTLogger.hpp"
#include "KLineGenerator.hpp"

class StrategyEngine
{
public:
    void LoadXQuantConfig(const Utils::XQuantConfig& config)
    {
        m_XQuantConfig = config;
    }

    bool Run(int64_t section_start, int64_t section_end, const Message::PackMessage& msg, Message::PackMessage& order)
    {
        bool ret = false;
        if(Message::EMessageType::EFutureMarketData == msg.MessageType)
        {
            std::string strTime = std::string(msg.FutureMarketData.ActionDay) + " " + msg.FutureMarketData.UpdateTime;
            struct tm timeStamp;
            if(strptime(strTime.c_str(), "%Y%m%d %H:%M:%S", &timeStamp) == NULL)
            {
                FMTLOG(fmtlog::WRN, "StrategyEngine::Run strptime error strTime:{}", strTime);
            }
            time_t value = mktime(&timeStamp);
            if(value == -1)
            {
                FMTLOG(fmtlog::WRN, "StrategyEngine::Run mktime error strTime:{}", strTime);
            }
            int64_t timestamp  = value * 1000 + msg.FutureMarketData.MillSec;
            auto it = m_KLineGeneratorMap.find(msg.FutureMarketData.Ticker);
            if(it != m_KLineGeneratorMap.end())
            {
                it->second.ProcessTick(section_start, section_end, timestamp, msg.FutureMarketData.LastPrice, msg.FutureMarketData.Volume);
            }
            else
            {
                KLineGenerator kline_generator = KLineGenerator(msg.FutureMarketData.Ticker, m_XQuantConfig.SnapshotInterval, m_XQuantConfig.SlicePerSec, m_XQuantConfig.KLineIntervals);
                RegisterCallBack(kline_generator);
                kline_generator.ProcessTick(section_start, section_end, timestamp, msg.FutureMarketData.LastPrice, msg.FutureMarketData.Volume);
                m_KLineGeneratorMap.insert(std::pair<std::string, KLineGenerator>(msg.FutureMarketData.Ticker, kline_generator));
            }
            if(timestamp + 10 * 1000 < section_end)
            {
                ret = Calculate(msg.FutureMarketData, order.OrderRequest);
            }
            FMTLOG(fmtlog::DBG, "StrategyEngine::Run ticker:{} UpdateTime:{} {}.{:03d} RecvMarketTime:{} timestamp:{}", 
                    msg.FutureMarketData.Ticker, msg.FutureMarketData.ActionDay, msg.FutureMarketData.UpdateTime, 
                    msg.FutureMarketData.MillSec, msg.FutureMarketData.RecvLocalTime, timestamp);
        }
        else if(Message::EMessageType::EStockMarketData == msg.MessageType)
        {
            std::string strTime = std::string(Utils::getCurrentDay()) + " " + msg.StockMarketData.UpdateTime;
            struct tm timeStamp;
            strptime(strTime.c_str(), "%Y-%m-%d %H:%M:%S", &timeStamp);
            int64_t timestamp  = mktime(&timeStamp) * 1000 + msg.StockMarketData.MillSec;
            auto it = m_KLineGeneratorMap.find(msg.StockMarketData.Ticker);
            if(it != m_KLineGeneratorMap.end())
            {
                it->second.ProcessTick(section_start, section_end, timestamp, msg.StockMarketData.LastPrice, msg.StockMarketData.Volume);
            }
            else
            {
                KLineGenerator kline_generator = KLineGenerator(msg.StockMarketData.Ticker, m_XQuantConfig.SnapshotInterval, m_XQuantConfig.SlicePerSec, m_XQuantConfig.KLineIntervals);
                RegisterCallBack(kline_generator);
                kline_generator.ProcessTick(section_start, section_end, timestamp, msg.StockMarketData.LastPrice, msg.FutureMarketData.Volume);
                m_KLineGeneratorMap.insert(std::pair<std::string, KLineGenerator>(msg.StockMarketData.Ticker, kline_generator));
            }
            if(timestamp + 10 * 1000 < section_end)
            {
                ret = Calculate(msg.StockMarketData, order.OrderRequest);
            }
            FMTLOG(fmtlog::DBG, "StrategyEngine::Run ticker:{} UpdateTime:{} {}.{:03d} RecvMarketTime:{} timestamp:{}", 
                    msg.FutureMarketData.Ticker, Utils::getCurrentDay(), msg.FutureMarketData.UpdateTime, 
                    msg.FutureMarketData.MillSec, msg.FutureMarketData.RecvLocalTime, timestamp);
        }
        return ret;
    }

    void CloseKLine(int64_t section_start, int64_t section_end, int64_t timestamp)
    {
        // 交易小节延后1min的时间区间强制闭合K线
        if((section_start + 60 * 1000) <= timestamp && timestamp < (section_end + 60 * 1000))
        {
            for(auto it = m_KLineGeneratorMap.begin(); it != m_KLineGeneratorMap.end(); it++)
            {
                it->second.CloseKLine(section_start, section_end, timestamp);
            }
            FMTLOG(fmtlog::DBG, "StrategyEngine::CloseKLine section_start:{} section_end:{} timestamp:{}", section_start, section_end, timestamp);
        }
    }

    virtual void CloseAllOrder(int64_t section_start, int64_t section_end, int64_t timestamp)
    {
        // 关闭所有订单
    }

    virtual void OnAccountFund(const Message::TAccountFund& data)
    {
        m_AccountFundMap[data.Account] = data;
    }

    virtual void OnAccountPosition(const Message::TAccountPosition& data)
    {
        std::string key = std::string(data.Account) + ":" + data.Ticker;
        m_AccountPositionMap[key] = data;
    }

    virtual void OnOrderStatus(const Message::TOrderStatus& data)
    {
        std::string key = std::string(data.Account) + ":" + data.OrderRef;
        if(Message::EOrderStatusType::EORDER_SENDED == data.OrderStatus ||
            Message::EOrderStatusType::EBROKER_ACK == data.OrderStatus ||
            Message::EOrderStatusType::EEXCHANGE_ACK == data.OrderStatus ||
            Message::EOrderStatusType::EPARTTRADED == data.OrderStatus ||
            Message::EOrderStatusType::EACTION_ERROR == data.OrderStatus ||
            Message::EOrderStatusType::ERISK_ORDER_REJECTED == data.OrderStatus ||
            Message::EOrderStatusType::ERISK_ACTION_REJECTED == data.OrderStatus)
        {
            m_OrderStatusMap[key] = data;
        }
        else if(Message::EOrderStatusType::EALLTRADED == data.OrderStatus ||
                Message::EOrderStatusType::ECANCELLED == data.OrderStatus ||
                Message::EOrderStatusType::EPARTTRADED_CANCELLED == data.OrderStatus ||
                Message::EOrderStatusType::EBROKER_ERROR == data.OrderStatus ||
                Message::EOrderStatusType::EEXCHANGE_ERROR == data.OrderStatus ||
                Message::EOrderStatusType::ERISK_CHECK_SELFMATCH == data.OrderStatus ||
                Message::EOrderStatusType::ERISK_CHECK_CANCELLIMIT == data.OrderStatus)
        {
            m_OrderStatusMap.erase(key);
        }
    }

protected:
    virtual void RegisterCallBack(KLineGenerator& kline_generator)
    {
        kline_generator.SetOnWindowBarFunc(std::bind(&StrategyEngine::OnWindowBar, this, std::placeholders::_1));
    }

    virtual bool Calculate(const MarketData::TFutureMarketData& data, Message::TOrderRequest& order)
    {
        return false;
    }

    virtual bool Calculate(const MarketData::TStockMarketData& data, Message::TOrderRequest& order)
    {
        return false;
    }

    virtual void OnWindowBar(const BarData& data)
    {
        FMTLOG(fmtlog::DBG, "StrategyEngine::OnWindowBar ticker:{} {}min close:{} start_time:{} end_time:{}", 
                data.ticker, data.interval/60, data.close, data.start_time, data.end_time);
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
    int32_t m_StrategyID;
    typedef phmap::parallel_flat_hash_map<std::string, KLineGenerator, 
                                        phmap::priv::hash_default_hash<std::string>,
                                        phmap::priv::hash_default_eq<std::string>,
                                        std::allocator<std::pair<std::string, KLineGenerator>>, 
                                        8, std::shared_mutex>
    KLineGeneratorMapT;
    KLineGeneratorMapT m_KLineGeneratorMap;
    Utils::XQuantConfig m_XQuantConfig;
};

#endif // STRATEGYENGINE_HPP