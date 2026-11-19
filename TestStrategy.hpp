#ifndef TESTSTRATEGY_HPP
#define TESTSTRATEGY_HPP

#include "StrategyEngine.hpp"

#include <vector>

class TestStrategy : public StrategyEngine
{
public:
    TestStrategy()
        : m_OrderCounter(0)
    {
        m_StrategyID = 1;
    }

    std::vector<Message::TActionRequest> TakeCancelRequests()
    {
        std::vector<Message::TActionRequest> requests;
        requests.swap(m_PendingCancelRequests);
        return requests;
    }

protected:
    virtual void RegisterCallBack(KLineGenerator& kline_generator)
    {
        kline_generator.SetOnWindowBarFunc(std::bind(&TestStrategy::OnWindowBar, this, std::placeholders::_1));
    }

    virtual bool Calculate(const MarketData::TFutureMarketData& data, Message::TOrderRequest& order)
    {
        CancelExpiredOrders(data);

        uint8_t direction = 0;
        if(data.BidVolume1 - data.AskVolume1 > 20)
        {
            direction = Message::EOrderDirection::EBUY;
        }
        else if(data.AskVolume1 - data.BidVolume1 > 20)
        {
            direction = Message::EOrderDirection::ESELL;
        }
        else
        {
            return false;
        }

        const uint32_t orderID = ++m_OrderCounter;
        strncpy(order.ExchangeID, data.ExchangeID, sizeof(order.ExchangeID));
        strncpy(order.Ticker, data.Ticker, sizeof(order.Ticker));
        strncpy(order.RecvMarketTime, data.RecvLocalTime, sizeof(order.RecvMarketTime));
        order.BusinessType = Message::EBusinessType::EFUTURE;
        order.Price = (data.BidPrice1 + data.AskPrice1) / 2.0;
        order.Volume = 1;
        order.Direction = direction;
        order.Offset = 0;
        order.OrderToken = static_cast<int>(orderID);
        order.EngineID = m_StrategyID;

        if(orderID % 10 == 1)
        {
            order.OrderType = Message::EOrderType::EFAK;
        }
        else if(orderID % 10 == 2)
        {
            order.OrderType = Message::EOrderType::EFOK;
        }
        else
        {
            order.OrderType = Message::EOrderType::ELIMIT;
        }
        return true;
    }

    void CancelExpiredOrders(const MarketData::TFutureMarketData& data)
    {
        const int64_t currentTimeUs = ParseRecvLocalTimeUs(data.RecvLocalTime);
        if(currentTimeUs < 0)
        {
            return;
        }

        m_OrderStatusMap.for_each_m(
            [&](OrderStatusMapT::value_type& item)
            {
                Message::TOrderStatus& orderStatus = item.second;
                if(orderStatus.OrderStatus == Message::EOrderStatusType::ECANCELLING ||
                   std::memcmp(orderStatus.Ticker, data.Ticker, sizeof(data.Ticker)) != 0)
                {
                    return;
                }

                const int64_t orderTimeUs =
                    ParseRecvLocalTimeUs(orderStatus.RecvMarketTime);
                if(orderTimeUs < 0 || currentTimeUs <= orderTimeUs ||
                   currentTimeUs - orderTimeUs <= 10 * 1000000)
                {
                    return;
                }

                orderStatus.OrderStatus = Message::EOrderStatusType::ECANCELLING;
                CancelOrder(orderStatus);
            });
    }

    static int64_t ParseRecvLocalTimeUs(const char* recvTime)
    {
        if(recvTime == nullptr)
        {
            return -1;
        }

        struct tm timeStamp = {};
        char* end = strptime(recvTime, "%Y-%m-%d %H:%M:%S", &timeStamp);
        if(end == nullptr)
        {
            return -1;
        }

        const time_t seconds = mktime(&timeStamp);
        if(seconds == -1)
        {
            return -1;
        }

        int64_t microseconds = 0;
        if(*end == '.')
        {
            ++end;
            int digits = 0;
            while(digits < 6 && *end >= '0' && *end <= '9')
            {
                microseconds = microseconds * 10 + (*end - '0');
                ++end;
                ++digits;
            }
            while(digits < 6)
            {
                microseconds *= 10;
                ++digits;
            }
        }
        return static_cast<int64_t>(seconds) * 1000000 + microseconds;
    }

    void CancelOrder(const Message::TOrderStatus& orderStatus)
    {
        Message::TActionRequest request;
        memset(&request, 0, sizeof(request));
        strncpy(request.Account, orderStatus.Account, sizeof(request.Account));
        strncpy(request.OrderRef, orderStatus.OrderRef, sizeof(request.OrderRef));
        strncpy(request.ExchangeID, orderStatus.ExchangeID, sizeof(request.ExchangeID));
        strncpy(request.UpdateTime, Utils::getCurrentTimeUs(), sizeof(request.UpdateTime));
        request.BusinessType = orderStatus.BusinessType;
        request.EngineID = orderStatus.EngineID;
        request.RiskStatus = Message::ERiskStatusType::EPREPARE_CHECKED;
        m_PendingCancelRequests.push_back(request);
    }

    virtual void OnWindowBar(const BarData& data)
    {
        FMTLOG(fmtlog::DBG, "TestStrategy::OnWindowBar ticker:{} {}min close:{} volume:{} turnover:{} start_time:{} end_time:{}", 
                data.ticker, data.interval/60, data.close, data.volume, data.turnover, data.start_time, data.end_time);
    }
protected:
    uint32_t m_OrderCounter;
    std::vector<Message::TActionRequest> m_PendingCancelRequests;
};

#endif // TESTSTRATEGY_HPP
