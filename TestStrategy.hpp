#ifndef TESTSTRATEGY_HPP
#define TESTSTRATEGY_HPP

#include "StrategyEngine.hpp"


class TestStrategy : public StrategyEngine
{
public:

protected:
    virtual bool Calculate(const MarketData::TFutureMarketData& data, Message::TOrderRequest& order)
    {
        static uint32_t OrderID = 1;
        bool ret = false;
        if(data.BidVolume1 > 100 && data.AskVolume1 < 10)
        {
            strncpy(order.ExchangeID, data.ExchangeID, sizeof(order.ExchangeID));
            strncpy(order.Ticker, data.Ticker, sizeof(order.Ticker));
            order.BusinessType = Message::EBusinessType::EFUTURE;
            order.Price = data.AskPrice1;
            order.Volume = 1;
            order.Direction = Message::EOrderDirection::EBUY;
            order.OrderToken = OrderID++;
            strncpy(order.RecvMarketTime, data.RecvLocalTime, sizeof(order.RecvMarketTime));
            ret = true;
        }
        else if(data.BidVolume1 < 10 && data.AskVolume1 > 100)
        {
            strncpy(order.ExchangeID, data.ExchangeID, sizeof(order.ExchangeID));
            strncpy(order.Ticker, data.Ticker, sizeof(order.Ticker));
            order.BusinessType = Message::EBusinessType::EFUTURE;
            order.Price = data.BidPrice1;
            order.Volume = 1;
            order.Direction = Message::EOrderDirection::ESELL;
            order.OrderToken = OrderID++;
            strncpy(order.RecvMarketTime, data.RecvLocalTime, sizeof(order.RecvMarketTime));
            ret = true;
        }
        return ret;
    }
};

#endif // TESTSTRATEGY_HPP