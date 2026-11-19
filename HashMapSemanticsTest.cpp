#include "QuantEngine.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

namespace
{

constexpr int kKLineInterval = 60;

void Check(bool condition, const char* message)
{
    if(!condition)
    {
        std::fprintf(stderr, "XQuantHashMapSemanticsTest failed: %s\n", message);
        std::abort();
    }
}

template <std::size_t N>
void CopyText(char (&destination)[N], const std::string& source)
{
    std::snprintf(destination, N, "%s", source.c_str());
}

class TestStrategyEngine : public StrategyEngine
{
public:
    void Configure()
    {
        Utils::XQuantConfig config;
        config.SnapshotInterval = 0;
        config.SlicePerSec = 1;
        config.KLineIntervals = {kKLineInterval};
        LoadXQuantConfig(config);
    }

    bool ReadAccountFund(const std::string& account, Message::TAccountFund& fund) const
    {
        return m_AccountFundMap.if_contains(
            account,
            [&](const AccountFundMapT::value_type& item)
            {
                fund = item.second;
            });
    }

    bool ReadAccountPosition(const char* account, const char* ticker,
                             Message::TAccountPosition& position) const
    {
        const XQuant::AccountTickerKey key =
            XQuant::MakeAccountTickerKey(account, ticker);
        return m_AccountPositionMap.if_contains(
            key,
            [&](const AccountPositionMapT::value_type& item)
            {
                position = item.second;
            });
    }

    bool HasOrderStatus(const char* account, const char* orderRef,
                        Message::TOrderStatus& status) const
    {
        const XQuant::AccountOrderRefKey key =
            XQuant::MakeAccountOrderRefKey(account, orderRef);
        return m_OrderStatusMap.if_contains(
            key,
            [&](const OrderStatusMapT::value_type& item)
            {
                status = item.second;
            });
    }

    std::size_t KLineGeneratorCount() const
    {
        std::size_t count = 0;
        m_KLineGeneratorMap.for_each(
            [&](const KLineGeneratorMapT::value_type&)
            {
                ++count;
            });
        return count;
    }

    bool ReadKLineState(const std::string& ticker, BarData& current, std::size_t& historySize)
    {
        bool found = false;
        m_KLineGeneratorMap.modify_if(
            ticker,
            [&](KLineGeneratorMapT::value_type& item)
            {
                current = item.second.GetCurrentKLine(kKLineInterval);
                historySize = item.second.GetHistory(kKLineInterval).size();
                found = true;
            });
        return found;
    }

    int WindowBarCount() const
    {
        return m_WindowBarCount.load();
    }

protected:
    void OnWindowBar(const BarData&) override
    {
        ++m_WindowBarCount;
    }

private:
    mutable std::atomic<int> m_WindowBarCount{0};
};

class TestableTestStrategy : public TestStrategy
{
public:
    using TestStrategy::Calculate;
};

void SetActiveOrderStatus(TestableTestStrategy& strategy,
                          const std::string& account,
                          const std::string& ticker,
                          const std::string& orderRef,
                          const std::string& recvMarketTime)
{
    Message::TOrderStatus status;
    std::memset(&status, 0, sizeof(status));
    CopyText(status.Account, account);
    CopyText(status.Ticker, ticker);
    CopyText(status.OrderRef, orderRef);
    CopyText(status.ExchangeID, std::string("SHFE"));
    CopyText(status.RecvMarketTime, recvMarketTime);
    status.BusinessType = Message::EBusinessType::EFUTURE;
    status.EngineID = 1;
    status.OrderStatus = Message::EOrderStatusType::EBROKER_ACK;
    strategy.OnOrderStatus(status);
}

struct WindowTime
{
    std::time_t start;
    char actionDay[16];
    char updateTime[32];
};

WindowTime MakeWindowTime(std::time_t timestamp)
{
    WindowTime window;
    window.start = timestamp;

    std::tm localTime;
    localtime_r(&window.start, &localTime);
    std::strftime(window.actionDay, sizeof(window.actionDay), "%Y%m%d", &localTime);
    std::strftime(window.updateTime, sizeof(window.updateTime), "%H:%M:%S", &localTime);
    return window;
}

WindowTime MakeCurrentWindowTime()
{
    const std::time_t now = std::time(nullptr);
    return MakeWindowTime((now / 60) * 60);
}

Message::PackMessage MakeFutureMarketData(const WindowTime& window, const std::string& ticker)
{
    Message::PackMessage message;
    std::memset(&message, 0, sizeof(message));
    message.MessageType = Message::EMessageType::EFutureMarketData;
    CopyText(message.FutureMarketData.Ticker, ticker);
    CopyText(message.FutureMarketData.ActionDay, window.actionDay);
    CopyText(message.FutureMarketData.UpdateTime, window.updateTime);
    message.FutureMarketData.LastPrice = 100.0;
    message.FutureMarketData.Volume = 1;
    message.FutureMarketData.Turnover = 1.0;
    return message;
}

void TestAccountAndOrderSemantics()
{
    TestStrategyEngine engine;
    static_assert(std::is_trivial<XQuant::AccountTickerKey>::value,
                  "AccountTickerKey must be trivial");
    static_assert(std::is_standard_layout<XQuant::AccountTickerKey>::value,
                  "AccountTickerKey must be standard layout");
    static_assert(std::is_trivial<XQuant::AccountOrderRefKey>::value,
                  "AccountOrderRefKey must be trivial");
    static_assert(std::is_standard_layout<XQuant::AccountOrderRefKey>::value,
                  "AccountOrderRefKey must be standard layout");

    Message::TAccountFund fund;
    std::memset(&fund, 0, sizeof(fund));
    CopyText(fund.Account, std::string("account"));
    fund.Balance = 10.0;
    engine.OnAccountFund(fund);
    fund.Balance = 20.0;
    engine.OnAccountFund(fund);

    Message::TAccountFund storedFund;
    std::memset(&storedFund, 0, sizeof(storedFund));
    Check(engine.ReadAccountFund("account", storedFund), "account fund was stored");
    Check(storedFund.Balance == 20.0, "account fund upsert uses latest value");

    Message::TAccountPosition position;
    std::memset(&position, 0, sizeof(position));
    CopyText(position.Account, std::string("account"));
    CopyText(position.Ticker, std::string("future"));
    position.FuturePosition.LongTdVolume = 1;
    engine.OnAccountPosition(position);
    position.FuturePosition.LongTdVolume = 2;
    engine.OnAccountPosition(position);

    Message::TAccountPosition storedPosition;
    std::memset(&storedPosition, 0, sizeof(storedPosition));
    Check(engine.ReadAccountPosition("account", "future", storedPosition), "account position was stored");
    Check(storedPosition.FuturePosition.LongTdVolume == 2, "account position upsert uses latest value");

    position.FuturePosition.LongTdVolume = 3;
    CopyText(position.Account, std::string("A:B"));
    CopyText(position.Ticker, std::string("C"));
    engine.OnAccountPosition(position);
    position.FuturePosition.LongTdVolume = 4;
    CopyText(position.Account, std::string("A"));
    CopyText(position.Ticker, std::string("B:C"));
    engine.OnAccountPosition(position);
    Check(engine.ReadAccountPosition("A:B", "C", storedPosition),
          "account/ticker key with colon in account was stored");
    Check(storedPosition.FuturePosition.LongTdVolume == 3,
          "account/ticker keys do not collide across fields");
    Check(engine.ReadAccountPosition("A", "B:C", storedPosition),
          "account/ticker key with colon in ticker was stored");
    Check(storedPosition.FuturePosition.LongTdVolume == 4,
          "account/ticker key field boundaries are preserved");

    Message::TOrderStatus orderStatus;
    std::memset(&orderStatus, 0, sizeof(orderStatus));
    CopyText(orderStatus.Account, std::string("account"));
    CopyText(orderStatus.OrderRef, std::string("order-1"));
    orderStatus.OrderStatus = Message::EOrderStatusType::EORDER_SENDED;
    engine.OnOrderStatus(orderStatus);

    Message::TOrderStatus storedStatus;
    std::memset(&storedStatus, 0, sizeof(storedStatus));
    Check(engine.HasOrderStatus("account", "order-1", storedStatus), "active order status was stored");
    Check(storedStatus.OrderStatus == Message::EOrderStatusType::EORDER_SENDED, "active order status value");

    orderStatus.OrderStatus = Message::EOrderStatusType::EBROKER_ACK;
    engine.OnOrderStatus(orderStatus);
    Check(engine.HasOrderStatus("account", "order-1", storedStatus), "updated active order status was stored");
    Check(storedStatus.OrderStatus == Message::EOrderStatusType::EBROKER_ACK, "active order status upsert");

    orderStatus.OrderStatus = Message::EOrderStatusType::EALLTRADED;
    engine.OnOrderStatus(orderStatus);
    Check(!engine.HasOrderStatus("account", "order-1", storedStatus), "terminal order status was erased");

    engine.OnOrderStatus(orderStatus);
    Check(!engine.HasOrderStatus("account", "order-1", storedStatus), "erasing a missing terminal order is a no-op");

    orderStatus.OrderStatus = Message::EOrderStatusType::EORDER_SENDED;
    CopyText(orderStatus.Account, std::string("A:B"));
    CopyText(orderStatus.OrderRef, std::string("C"));
    orderStatus.OrderToken = 101;
    engine.OnOrderStatus(orderStatus);
    CopyText(orderStatus.Account, std::string("A"));
    CopyText(orderStatus.OrderRef, std::string("B:C"));
    orderStatus.OrderToken = 202;
    engine.OnOrderStatus(orderStatus);
    Check(engine.HasOrderStatus("A:B", "C", storedStatus),
          "account/order key with colon in account was stored");
    Check(storedStatus.OrderToken == 101,
          "account/order keys do not collide across fields");
    Check(engine.HasOrderStatus("A", "B:C", storedStatus),
          "account/order key with colon in order reference was stored");
    Check(storedStatus.OrderToken == 202,
          "account/order key field boundaries are preserved");
}

void TestFirstTickAndCloseKLine()
{
    TestStrategyEngine engine;
    engine.Configure();

    const WindowTime window = MakeCurrentWindowTime();
    const int64_t sectionStart = static_cast<int64_t>(window.start) * 1000;
    const int64_t sectionEnd = sectionStart + 3600 * 1000;
    Message::PackMessage marketData = MakeFutureMarketData(window, "future");
    Message::PackMessage order;
    std::memset(&order, 0, sizeof(order));

    engine.Run(sectionStart, sectionEnd, marketData, order);
    Check(engine.KLineGeneratorCount() == 1, "first tick creates one K-line generator");

    BarData current;
    std::size_t historySize = 0;
    Check(engine.ReadKLineState("future", current, historySize), "K-line generator is readable");
    Check(current.start_time == sectionStart, "first tick initializes the current K-line");
    Check(current.volume == 1, "first tick is applied to the new K-line");
    Check(historySize == 0, "open K-line is not in history");

    const int64_t closeTimestamp = sectionStart + (kKLineInterval - 1) * 1000;
    engine.CloseKLine(sectionStart, sectionEnd, closeTimestamp);
    Check(engine.WindowBarCount() == 1, "CloseKLine invokes the bar callback once");
    Check(engine.ReadKLineState("future", current, historySize), "closed K-line generator is readable");
    Check(historySize == 1, "CloseKLine stores one completed K-line");

    engine.CloseKLine(sectionStart, sectionEnd, closeTimestamp);
    Check(engine.WindowBarCount() == 1, "repeated CloseKLine does not close the next K-line");
}

void TestConcurrentKLineInitialization()
{
    TestStrategyEngine engine;
    engine.Configure();

    const WindowTime window = MakeCurrentWindowTime();
    const int64_t sectionStart = static_cast<int64_t>(window.start) * 1000;
    const int64_t sectionEnd = sectionStart + 3600 * 1000;
    Message::PackMessage baseMarketData = MakeFutureMarketData(window, "concurrent-future");

    constexpr int kThreadCount = 8;
    std::atomic<bool> start{false};
    std::atomic<int> failures{0};
    std::vector<std::thread> workers;
    workers.reserve(kThreadCount);
    for(int index = 0; index < kThreadCount; ++index)
    {
        workers.emplace_back([&, index]()
        {
            Message::PackMessage marketData = baseMarketData;
            marketData.FutureMarketData.LastPrice = 100.0 + index;
            Message::PackMessage order;
            std::memset(&order, 0, sizeof(order));

            while(!start.load())
            {
                std::this_thread::yield();
            }
            try
            {
                engine.Run(sectionStart, sectionEnd, marketData, order);
            }
            catch(...)
            {
                ++failures;
            }
        });
    }

    start.store(true);
    for(std::thread& worker : workers)
    {
        worker.join();
    }

    Check(failures.load() == 0, "concurrent K-line runs do not throw");
    Check(engine.KLineGeneratorCount() == 1, "concurrent first ticks create one K-line generator");

    const int64_t closeTimestamp = sectionStart + (kKLineInterval - 1) * 1000;
    engine.CloseKLine(sectionStart, sectionEnd, closeTimestamp);
    Check(engine.WindowBarCount() == 1, "concurrent K-line generator is closed exactly once");

    BarData current;
    std::size_t historySize = 0;
    Check(engine.ReadKLineState("concurrent-future", current, historySize), "concurrent generator state is readable");
    Check(historySize == 1, "concurrent first ticks do not lose the initial K-line");
}

void TestConcurrentReadModifyErase()
{
    TestStrategyEngine engine;
    std::atomic<bool> done{false};

    std::thread reader([&]()
    {
        while(!done.load())
        {
            Message::TAccountFund fund;
            Message::TAccountPosition position;
            Message::TOrderStatus status;
            (void)engine.ReadAccountFund("p-account", fund);
            (void)engine.ReadAccountPosition("p-account", "p-ticker", position);
            (void)engine.HasOrderStatus("p-account", "p-order", status);
        }
    });

    constexpr int kWriterCount = 4;
    constexpr int kIterations = 2000;
    std::vector<std::thread> writers;
    writers.reserve(kWriterCount);
    for(int writerIndex = 0; writerIndex < kWriterCount; ++writerIndex)
    {
        writers.emplace_back([&, writerIndex]()
        {
            for(int iteration = 0; iteration < kIterations; ++iteration)
            {
                Message::TAccountFund fund;
                std::memset(&fund, 0, sizeof(fund));
                CopyText(fund.Account, std::string("p-account"));
                fund.Balance = writerIndex * kIterations + iteration;
                engine.OnAccountFund(fund);

                Message::TAccountPosition position;
                std::memset(&position, 0, sizeof(position));
                CopyText(position.Account, std::string("p-account"));
                CopyText(position.Ticker, std::string("p-ticker"));
                position.FuturePosition.LongTdVolume = iteration;
                engine.OnAccountPosition(position);

                Message::TOrderStatus status;
                std::memset(&status, 0, sizeof(status));
                CopyText(status.Account, std::string("p-account"));
                CopyText(status.OrderRef, std::string("p-order"));
                status.OrderStatus = Message::EOrderStatusType::EORDER_SENDED;
                engine.OnOrderStatus(status);
                status.OrderStatus = Message::EOrderStatusType::EALLTRADED;
                engine.OnOrderStatus(status);
            }
        });
    }

    for(std::thread& writer : writers)
    {
        writer.join();
    }
    done.store(true);
    reader.join();

    Message::TAccountFund finalFund;
    std::memset(&finalFund, 0, sizeof(finalFund));
    CopyText(finalFund.Account, std::string("p-account"));
    finalFund.Balance = 42.0;
    engine.OnAccountFund(finalFund);

    Message::TAccountFund storedFund;
    std::memset(&storedFund, 0, sizeof(storedFund));
    Check(engine.ReadAccountFund("p-account", storedFund), "parallel account fund was stored");
    Check(storedFund.Balance == 42.0, "parallel account fund retains latest value");

    Message::TOrderStatus missingStatus;
    std::memset(&missingStatus, 0, sizeof(missingStatus));
    Check(!engine.HasOrderStatus("p-account", "p-order", missingStatus), "parallel order reaches erased state");
}

void TestTestStrategyOrderGeneration()
{
    TestableTestStrategy strategy;
    MarketData::TFutureMarketData data;
    std::memset(&data, 0, sizeof(data));
    CopyText(data.Ticker, std::string("test-order"));
    CopyText(data.ExchangeID, std::string("SHFE"));
    CopyText(data.RecvLocalTime, std::string("2026-11-19 09:30:00"));
    data.BidPrice1 = 99.0;
    data.AskPrice1 = 101.0;

    Message::TOrderRequest order;
    std::memset(&order, 0, sizeof(order));
    data.BidVolume1 = 30;
    data.AskVolume1 = 10;
    Check(!strategy.Calculate(data, order),
          "TestStrategy does not order at an imbalance of exactly twenty");

    data.BidVolume1 = 100;
    std::vector<uint8_t> orderTypes;
    for(int index = 0; index < 11; ++index)
    {
        std::memset(&order, 0, sizeof(order));
        Check(strategy.Calculate(data, order), "TestStrategy buy imbalance generates an order");
        Check(order.Direction == Message::EOrderDirection::EBUY,
              "TestStrategy bid imbalance generates a buy");
        Check(order.OrderToken == index + 1, "TestStrategy uses one unified order id sequence");
        Check(order.Price == 100.0, "TestStrategy uses bid/ask midpoint");
        Check(order.Volume == 1, "TestStrategy order volume");
        Check(order.EngineID == 1, "TestStrategy engine id");
        orderTypes.push_back(order.OrderType);
    }

    Check(orderTypes[0] == Message::EOrderType::EFAK, "order id modulo one uses FAK");
    Check(orderTypes[1] == Message::EOrderType::EFOK, "order id modulo two uses FOK");
    for(std::size_t index = 2; index < 10; ++index)
    {
        Check(orderTypes[index] == Message::EOrderType::ELIMIT,
              "other order ids use LIMIT");
    }
    Check(orderTypes[10] == Message::EOrderType::EFAK, "order type cycle repeats every ten orders");

    data.BidVolume1 = 10;
    data.AskVolume1 = 100;
    std::memset(&order, 0, sizeof(order));
    Check(strategy.Calculate(data, order), "TestStrategy sell imbalance generates an order");
    Check(order.Direction == Message::EOrderDirection::ESELL,
          "TestStrategy ask imbalance generates a sell");
    Check(order.OrderToken == 12, "sell order continues the unified order id sequence");
    Check(order.OrderType == Message::EOrderType::EFOK, "sell order uses its order id order type");
}

void TestTestStrategyCancelExpiredOrders()
{
    TestableTestStrategy strategy;
    SetActiveOrderStatus(strategy, "account-a", "cancel-order", "order-1",
                         "2026-11-19 09:30:00.000000");

    MarketData::TFutureMarketData data;
    std::memset(&data, 0, sizeof(data));
    CopyText(data.Ticker, std::string("cancel-order"));
    CopyText(data.ExchangeID, std::string("SHFE"));
    CopyText(data.RecvLocalTime, std::string("2026-11-19 09:30:10.000000"));
    data.BidPrice1 = 99.0;
    data.AskPrice1 = 101.0;
    data.BidVolume1 = 10;
    data.AskVolume1 = 10;

    Message::TOrderRequest order;
    std::memset(&order, 0, sizeof(order));
    Check(!strategy.Calculate(data, order), "balanced book does not create an order");
    Check(strategy.TakeCancelRequests().empty(), "order at exactly ten seconds is not cancelled");

    CopyText(data.RecvLocalTime, std::string("2026-11-19 09:30:10.000001"));
    Check(!strategy.Calculate(data, order), "expired order does not create a new order");
    std::vector<Message::TActionRequest> cancels = strategy.TakeCancelRequests();
    Check(cancels.size() == 1, "order older than ten seconds is cancelled");
    Check(std::strcmp(cancels[0].Account, "account-a") == 0, "cancel account");
    Check(std::strcmp(cancels[0].OrderRef, "order-1") == 0, "cancel order reference");
    Check(std::strcmp(cancels[0].ExchangeID, "SHFE") == 0, "cancel exchange");
    Check(cancels[0].BusinessType == Message::EBusinessType::EFUTURE,
          "cancel business type");
    Check(cancels[0].EngineID == 1, "cancel engine id");
    Check(cancels[0].RiskStatus == Message::ERiskStatusType::EPREPARE_CHECKED,
          "cancel uses risk checking");

    Check(!strategy.Calculate(data, order), "cancelled order does not create a new order");
    Check(strategy.TakeCancelRequests().empty(), "ECANCELLING order is not cancelled again");

    TestableTestStrategy filteredStrategy;
    SetActiveOrderStatus(filteredStrategy, "account-a", "other-order", "order-2",
                         "2026-11-19 09:30:00.000000");
    Check(!filteredStrategy.Calculate(data, order), "different ticker does not create an order");
    Check(filteredStrategy.TakeCancelRequests().empty(),
          "different ticker order is not cancelled");

    TestableTestStrategy multipleStrategy;
    SetActiveOrderStatus(multipleStrategy, "account-a", "cancel-order", "order-3",
                         "2026-11-19 09:30:00.000000");
    SetActiveOrderStatus(multipleStrategy, "account-b", "cancel-order", "order-4",
                         "2026-11-19 09:30:00.000000");
    SetActiveOrderStatus(multipleStrategy, "account-a", "other-order", "order-5",
                         "2026-11-19 09:30:00.000000");
    Check(!multipleStrategy.Calculate(data, order), "multiple expired orders do not create a new order");
    cancels = multipleStrategy.TakeCancelRequests();
    Check(cancels.size() == 2, "all expired orders for the ticker are queued");

    TestableTestStrategy combinedStrategy;
    SetActiveOrderStatus(combinedStrategy, "account-a", "cancel-order", "order-6",
                         "2026-11-19 09:30:00.000000");
    data.BidVolume1 = 100;
    data.AskVolume1 = 10;
    std::memset(&order, 0, sizeof(order));
    Check(combinedStrategy.Calculate(data, order),
          "expired order and new order can be generated on the same Tick");
    Check(order.Direction == Message::EOrderDirection::EBUY,
          "combined Tick keeps the new buy order");
    cancels = combinedStrategy.TakeCancelRequests();
    Check(cancels.size() == 1, "combined Tick keeps the cancel request");
}

} // namespace

int main()
{
    TestAccountAndOrderSemantics();
    TestFirstTickAndCloseKLine();
    TestConcurrentKLineInitialization();
    TestConcurrentReadModifyErase();
    TestTestStrategyOrderGeneration();
    TestTestStrategyCancelExpiredOrders();
    return 0;
}
