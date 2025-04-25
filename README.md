# XQuant策略交易平台

- XQuant C++/Python版本使用SHMConnection C++版本与XMarketCenter、XRiskJudge、XTrader进行IPC通信：
    - 通过MarketServer内存通道从XMarketCenter读取行情数据，进行计算后触发交易信号，将报单通写入OrderServer内存通道；
    - XTrader从OrderServer内存通道读取报单请求，如果需要风控检查，则将报单请求写入RiskServer内存通道；如果不需要风控检查，则直接调用柜台API进行报单。
    - XRiskJudge从RiskServer内存通道读取报单，进行风控检查，并将检查结果写入内存通道；
    - XTrader从RiskServer内存通道读取报单检查结果，如果风控检查不通过，直接将订单状态信息返回监控系统；如果风控检查通过，则调用柜台API进行报单。



## Python版本
- Python环境安装：
    ```bash
    conda create -n XQuant python=3.9
    conda activate XQuant
    pip3 install HPSocket -i https://mirrors.aliyun.com/pypi/simple/
    pip3 install loguru -i https://mirrors.aliyun.com/pypi/simple/
    conda install -c conda-forge ta-lib
    ```

## K线闭合机制
- K线闭合机制如下：
    - 周期内时间序列的最后一个切片数据到达，则闭合。
    - 超时强制闭合。如果周期内间序列没有最后一个切片数据，则触发超时强制闭合。
    - 如果在超时强制闭合前，收到新周期的切片数据，则先闭合上一个周期K线，然后再创建新周期K线。


## 策略编写
- 所有策略类必须继承自BaseEngine基类，必须重新实现on_window_bar：
    ```python
    class SMAStrategy(engine.BaseEngine):
        def on_window_bar(self, bar: BarData)
    ```
- 可以根据需要重新实现如下接口：
    ```python
    # 切片数据回调
    def update_tick(self, msg: pack_message.PackMessage):
    # 切片数据回调
    def on_tick_data(self, data:dict):

    # 订单状态回调
    def notify_orderstatus(self, msg:pack_message.PackMessage):
        
    # 账户资金变动回调
    def notify_fund(self, msg:pack_message.PackMessage):
        
    # 账户仓位变动回调
    def notify_position(self, msg: pack_message.PackMessage):
        
    ```
- 订单构建过程如下：
    ```python
    order = pack_message.PackMessage()
    order.MessageType = pack_message.EMessageType.EOrderRequest
    order.OrderRequest.ExchangeID = msg.FutureMarketData.ExchangeID
    order.OrderRequest.Ticker = msg.FutureMarketData.Ticker
    order.OrderRequest.BusinessType = pack_message.EBusinessType.EFUTURE
    order.OrderRequest.OrderType = pack_message.EOrderType.ELIMIT
    order.OrderRequest.Price = msg.FutureMarketData.AskPrice1
    order.OrderRequest.Volume = 5
    order.OrderRequest.OrderToken = self.order_id
    self.order_id +=  1
    order.OrderRequest.Direction = pack_message.EOrderDirection.EBUY
    # order.OrderRequest.Offset = pack_message.EOrderOffset.EOPEN
    order.OrderRequest.Offset = 0
    order.OrderRequest.EngineID = self.strategy_id
    order.OrderRequest.RiskStatus = pack_message.ERiskStatusType.EPREPARE_CHECKED
    order.OrderRequest.RecvMarketTime = msg.FutureMarketData.RecvLocalTime
    order.OrderRequest.SendTime = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")
    ```

- 订单构建完成后需要将订单传递给框架，由框架进行发送：
    ```python
    self.order_request = order
    self.new_order = True  # 将self.new_order置为True
    ```
- 常用消息类型如下：

    - 消息类型枚举：
        ```python
        pack_message.EMessageType.EOrderStatus  # 订单状态
        pack_message.EMessageType.EAccountFund  # 账户资金
        pack_message.EMessageType.EAccountPosition  # 账户仓位
        pack_message.EMessageType.EOrderRequest  # 报单请求
        pack_message.EMessageType.EActionRequest  # 撤单请求
        pack_message.EMessageType.ERiskReport  # 风控报告
        pack_message.EMessageType.EColoStatus  # 交易服务器状态
        pack_message.EMessageType.EAppStatus  # 交易App状态
        pack_message.EMessageType.EFutureMarketData  # 期货行情数据
        pack_message.EMessageType.EStockMarketData  # 股票行情数据
        ```

    - 业务类型枚举类型：
        ```python
        pack_message.EBusinessType.ESTOCK  # 股票现货
        pack_message.EBusinessType.ECREDIT  # 股票信用
        pack_message.EBusinessType.EFUTURE  # 期货
        pack_message.EBusinessType.ESPOT  # 指数现货
        ```

    - 订单类型枚举类型：
        ```python
        pack_message.EOrderType.EFAK # FAK订单
        pack_message.EOrderType.EFOK # FOK订单
        pack_message.EOrderType.ELIMIT # 限价单
        pack_message.EOrderType.EMARKET # 市价单
        ```

    - 订单方向枚举类型：
        ```python
        pack_message.EOrderDirection.EBUY  # 买
        pack_message.EOrderDirection.ESELL # 卖
        ```

    - 订单Offset枚举类型：
        ```python
        pack_message.EOrderOffset.EOPEN # 开仓
        pack_message.EOrderOffset.ECLOSE # 平仓
        pack_message.EOrderOffset.ECLOSE_TODAY # 平今仓
        pack_message.EOrderOffset.ECLOSE_YESTODAY # 平昨仓
        ```

    - 订单OrderSide枚举类型：
        ```python
        pack_message.EOrderSide.EOPEN_LONG  # 开多
        pack_message.EOrderSide.ECLOSE_TD_LONG  # 平今多
        pack_message.EOrderSide.ECLOSE_YD_LONG  # 平昨多
        pack_message.EOrderSide.EOPEN_SHORT  # 开空
        pack_message.EOrderSide.ECLOSE_TD_SHORT  # 平今空
        pack_message.EOrderSide.ECLOSE_YD_SHORT  # 平昨空
        pack_message.EOrderSide.ECLOSE_LONG  # 平多
        pack_message.EOrderSide.ECLOSE_SHORT  # 平空
        ```

    - 订单状态枚举类型：
        ```python
        pack_message.EOrderStatusType.EORDER_SENDED  # 订单已经发送
        pack_message.EOrderStatusType.EBROKER_ACK  # 收到柜台ACK确认
        pack_message.EOrderStatusType.EEXCHANGE_ACK  # 收到交易所ACK确认
        pack_message.EOrderStatusType.EPARTTRADED  # 部分成交
        pack_message.EOrderStatusType.EALLTRADED  # 全部成交
        pack_message.EOrderStatusType.ECANCELLING  # 撤单已经发送
        pack_message.EOrderStatusType.ECANCELLED  # 订单撤销
        pack_message.EOrderStatusType.EPARTTRADED_CANCELLED  # 部成部撤
        pack_message.EOrderStatusType.EBROKER_ERROR  # 柜台错误
        pack_message.EOrderStatusType.EEXCHANGE_ERROR  # 交易所错误
        pack_message.EOrderStatusType.EACTION_ERROR  # 撤单错误
        pack_message.EOrderStatusType.ERISK_ORDER_REJECTED  # 报单被风控拒绝
        pack_message.EOrderStatusType.ERISK_ACTION_REJECTED  # 撤单被风控拒绝
        pack_message.EOrderStatusType.ERISK_CHECK_INIT  # 风控初始化订单状态
        pack_message.EOrderStatusType.ERISK_CHECK_SELFMATCH  # 风控拒单-自成交
        pack_message.EOrderStatusType.ERISK_CHECK_CANCELLIMIT  # 风控拒单-撤单限制
        ```

    - 风控状态枚举类型：
        ```python
        pack_message.ERiskStatusType.EPREPARE_CHECKED # 等待风控检查
        pack_message.ERiskStatusType.ECHECKED_PASS # 风控检查通过
        pack_message.ERiskStatusType.ECHECKED_NOPASS # 风控检查未通过
        pack_message.ERiskStatusType.ENOCHECKED # 不需要风控检查
        pack_message.ERiskStatusType.ECHECK_INIT # 风控检查初始化
        ```

    - 报单请求消息结构体:
        ```cpp
        struct TOrderRequest
        {
            char Colo[16];
            char Broker[16];
            char Product[16];
            char Account[16];
            char Ticker[20];
            char ExchangeID[16];
            uint8_t BusinessType;
            uint8_t OrderType;
            uint8_t Direction;
            uint8_t Offset;
            uint8_t RiskStatus;
            int OrderToken;
            int EngineID;
            int UserReserved1;
            int UserReserved2;
            double Price;
            int Volume;
            char RecvMarketTime[32];
            char SendTime[32];
            char RiskID[16];
            char Trader[16];
            int ErrorID;
            char ErrorMsg[256];
            char UpdateTime[32];
        };
        ```
    - 撤单请求消息结构体:
        ```cpp
        struct TActionRequest
        {
            char Colo[16];
            char Account[16];
            char OrderRef[32];
            char ExchangeID[16];
            uint8_t BusinessType;
            int EngineID;
            uint8_t RiskStatus;
            char Trader[16];
            char RiskID[16];
            int ErrorID;
            char ErrorMsg[256];
            char UpdateTime[32];
        };
        ```

    - 账户资金信息消息结构体：
        ```cpp
        struct TAccountFund
        {
            char Colo[16];
            char Broker[16];
            char Product[16];
            char Account[16];
            uint8_t BusinessType;
            double Deposit; // 入金
            double Withdraw; // 出金
            double CurrMargin; // 当前保证金
            double Commission; // 手续费
            double CloseProfit; // 平仓盈亏
            double PositionProfit; // 持仓盈亏
            double Available; // 可用资金
            double WithdrawQuota; // 可取资金额度
            double ExchangeMargin; // 交易所保证金
            double Balance; // 总资产
            double PreBalance; // 日初总资产
            char UpdateTime[32]; 
        };
        ```
    - 账户仓位消息结构体：
        ```cpp
        struct TFuturePosition
        {
            int LongTdVolume;
            int LongYdVolume;
            int LongOpenVolume;
            int LongOpeningVolume;
            int LongClosingTdVolume;
            int LongClosingYdVolume;
            int ShortTdVolume;
            int ShortYdVolume;
            int ShortOpenVolume;
            int ShortOpeningVolume;
            int ShortClosingTdVolume;
            int ShortClosingYdVolume;
        };

        struct TStockPosition
        {
            int LongYdPosition; // 日初可用持仓
            int LongPosition; // 当前总持仓
            int LongTdBuy; // 今日买入量
            int LongTdSell; // 今日卖出量
            int MarginYdPosition; // 日初可用融资负债数量 
            int MarginPosition; // 融资负债数量;
            int MarginTdBuy; // 融资今日买入数量
            int MarginTdSell; // 今日卖券还款数量
            int ShortYdPosition; // 日初融券负债可用数量 
            int ShortPosition; // 融券负债数量
            int ShortTdSell; // 今日融券卖出数量
            int ShortTdBuy; // 今日买券还券数量
            int ShortDirectRepaid; // 直接还券数量
            int SpecialPositionAvl; // 融券专项证券头寸可用数量
        };

        struct TAccountPosition
        {
            char Colo[16];
            char Broker[16];
            char Product[16];
            char Account[16];
            char Ticker[20];
            char ExchangeID[16];
            uint8_t BusinessType;
            union
            {
                TFuturePosition FuturePosition;
                TStockPosition StockPosition;
            };
            char UpdateTime[32];
        };
        ```
    - 订单状态消息结构体：
        ```cpp
        struct TOrderStatus
        {
            char Colo[16];
            char Broker[16];
            char Product[16];
            char Account[16];
            char Ticker[20];
            char ExchangeID[16];
            uint8_t BusinessType;
            char OrderRef[32];
            char OrderSysID[32];
            char OrderLocalID[32];
            int OrderToken;
            int EngineID;
            int UserReserved1;
            int UserReserved2;
            uint8_t OrderType;
            uint8_t OrderSide;
            uint8_t OrderStatus;
            double SendPrice;
            unsigned int SendVolume;
            unsigned int TotalTradedVolume;
            double TradedAvgPrice;
            unsigned int TradedVolume;
            double TradedPrice;
            unsigned int CanceledVolume;
            double Commission;
            char RecvMarketTime[32];
            char SendTime[32];
            char InsertTime[32];
            char BrokerACKTime[32];
            char ExchangeACKTime[32];
            char RiskID[16];
            char Trader[16];
            int ErrorID;
            char ErrorMsg[256];
            char UpdateTime[32];
        };
        ```

    - 期货行情数据消息结构体：
        ```cpp
        struct TFutureMarketData
        {
            char Colo[16];
            char Broker[16];
            char Ticker[20]; // 商品组合合约长度可能大于16字节
            char ExchangeID[16];
            int LastTick;
            int Tick;
            char TradingDay[16];
            char ActionDay[16];
            char UpdateTime[32];
            int MillSec;
            double LastPrice;
            int Volume;
            double Turnover;
            double OpenPrice;
            double ClosePrice;
            double PreClosePrice;
            double SettlementPrice;
            double PreSettlementPrice;
            double OpenInterest;
            double PreOpenInterest;
            double CurrDelta;
            double PreDelta;
            double HighestPrice;
            double LowestPrice;
            double UpperLimitPrice;
            double LowerLimitPrice;
            double AveragePrice;
            double BidPrice1;
            int BidVolume1;
            double AskPrice1;
            int AskVolume1;
            double BidPrice2;
            int BidVolume2;
            double AskPrice2;
            int AskVolume2;
            double BidPrice3;
            int BidVolume3;
            double AskPrice3;
            int AskVolume3;
            double BidPrice4;
            int BidVolume4;
            double AskPrice4;
            int AskVolume4;
            double BidPrice5;
            int BidVolume5;
            double AskPrice5;
            int AskVolume5;
            int SectionFirstTick;
            int SectionLastTick;
            int TotalTick;
            int ErrorID;
            char RecvLocalTime[32];
        };

        ```

- 不同消息数据使用如下：
    ```python
    def print_msg(func:str, msg):
        logger.debug(f"{func} MessageType {msg.MessageType:#X}")
        if msg.MessageType == pack_message.EMessageType.EFutureMarketData:
            logger.debug("Colo:{} Ticker:{} ExchangeID:{} TradingDay:{} ActionDay:{} UpdateTime:{} MillSec:{} LastPrice:{} "
                        "Volume:{} Turnover:{} OpenPrice:{} ClosePrice:{} PreClosePrice:{} SettlementPrice:{} PreSettlementPrice:{} "
                        "OpenInterest:{} PreOpenInterest:{} HighestPrice:{} LowestPrice:{} UpperLimitPrice:{} LowerLimitPrice:{} "
                        "BidPrice1:{} BidVolume1:{} AskPrice1:{} AskVolume1:{} RecvLocalTime:{} CurrentTime:{}", 
                        msg.FutureMarketData.Colo, msg.FutureMarketData.Ticker, msg.FutureMarketData.ExchangeID, msg.FutureMarketData.TradingDay, 
                        msg.FutureMarketData.ActionDay, msg.FutureMarketData.UpdateTime, msg.FutureMarketData.MillSec, msg.FutureMarketData.LastPrice,
                        msg.FutureMarketData.Volume, msg.FutureMarketData.Turnover, msg.FutureMarketData.OpenPrice, msg.FutureMarketData.ClosePrice,
                        msg.FutureMarketData.PreClosePrice, msg.FutureMarketData.SettlementPrice, msg.FutureMarketData.PreSettlementPrice,
                        msg.FutureMarketData.OpenInterest, msg.FutureMarketData.PreOpenInterest, msg.FutureMarketData.HighestPrice, 
                        msg.FutureMarketData.LowestPrice, msg.FutureMarketData.UpperLimitPrice, msg.FutureMarketData.LowerLimitPrice,
                        msg.FutureMarketData.BidPrice1, msg.FutureMarketData.BidVolume1, msg.FutureMarketData.AskPrice1, 
                        msg.FutureMarketData.AskVolume1, msg.FutureMarketData.RecvLocalTime, datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f"))
        elif msg.MessageType == pack_message.EMessageType.EOrderStatus:
            logger.debug("Colo:{} Broker:{} Product:{} Account:{} Ticker:{} ExchangeID:{} BusinessType:{} OrderRef:{} "
                        "OrderSysID:{} OrderLocalID:{} OrderToken:{} EngineID:{} UserReserved1:{} UserReserved2:{} "
                        "OrderType:{} OrderSide:{} OrderStatus:{} SendPrice:{} SendVolume:{} TotalTradedVolume:{} "
                        "TradedAvgPrice:{} TradedVolume:{} TradedPrice:{} CanceledVolume:{} Commission:{} RecvMarketTime:{} "
                        "SendTime:{} InsertTime:{} BrokerACKTime:{} ExchangeACKTime:{} RiskID:{} Trader:{} ErrorID:{} "
                        "ErrorMsg:{} UpdateTime:{}", 
                        msg.OrderStatus.Colo, msg.OrderStatus.Broker, msg.OrderStatus.Product, msg.OrderStatus.Account,
                        msg.OrderStatus.Ticker, msg.OrderStatus.ExchangeID, msg.OrderStatus.BusinessType, msg.OrderStatus.OrderRef,
                        msg.OrderStatus.OrderSysID, msg.OrderStatus.OrderLocalID, msg.OrderStatus.OrderToken, msg.OrderStatus.EngineID,
                        msg.OrderStatus.UserReserved1, msg.OrderStatus.UserReserved2, msg.OrderStatus.OrderType, msg.OrderStatus.OrderSide,
                        msg.OrderStatus.OrderStatus, msg.OrderStatus.SendPrice, msg.OrderStatus.SendVolume, msg.OrderStatus.TotalTradedVolume,
                        msg.OrderStatus.TradedAvgPrice, msg.OrderStatus.TradedVolume, msg.OrderStatus.TradedPrice, msg.OrderStatus.CanceledVolume,
                        msg.OrderStatus.Commission, msg.OrderStatus.RecvMarketTime, msg.OrderStatus.SendTime, msg.OrderStatus.InsertTime,
                        msg.OrderStatus.BrokerACKTime, msg.OrderStatus.ExchangeACKTime, msg.OrderStatus.RiskID, msg.OrderStatus.Trader,
                        msg.OrderStatus.ErrorID, msg.OrderStatus.ErrorMsg, msg.OrderStatus.UpdateTime)
        elif msg.MessageType == pack_message.EMessageType.EAccountFund:
            logger.debug("Colo:{} Broker:{} Product:{} Account:{} BusinessType:{} Deposit:{} Withdraw:{} CurrMargin:{} "
                        "Commission:{} CloseProfit:{} PositionProfit:{} Available:{} WithdrawQuota:{} ExchangeMargin:{} "
                        "Balance:{} PreBalance:{} UpdateTime:{}", 
                        msg.AccountFund.Colo, msg.AccountFund.Broker, msg.AccountFund.Product, msg.AccountFund.Account,
                        msg.AccountFund.BusinessType, msg.AccountFund.Deposit, msg.AccountFund.Withdraw, msg.AccountFund.CurrMargin,
                        msg.AccountFund.Commission, msg.AccountFund.CloseProfit, msg.AccountFund.PositionProfit, 
                        msg.AccountFund.Available, msg.AccountFund.WithdrawQuota, msg.AccountFund.ExchangeMargin,
                        msg.AccountFund.Balance, msg.AccountFund.PreBalance, msg.AccountFund.UpdateTime)
        elif msg.MessageType == pack_message.EMessageType.EAccountPosition:
            if msg.AccountPosition.BusinessType == pack_message.EBusinessType.EFUTURE:
                logger.debug("Colo:{} Broker:{} Product:{} Account:{} Ticker:{} ExchangeID:{} BusinessType:{} "
                            "LongTdVolume:{} LongYdVolume:{} LongOpenVolume:{} LongOpeningVolume:{} "
                            "LongClosingTdVolume:{} LongClosingYdVolume:{} ShortTdVolume:{} ShortYdVolume:{} "
                            "ShortOpenVolume:{} ShortOpeningVolume:{} ShortClosingTdVolume:{} "
                            "ShortClosingYdVolume:{} UpdateTime:{}", 
                            msg.AccountPosition.Colo, msg.AccountPosition.Broker, msg.AccountPosition.Product, 
                            msg.AccountPosition.Account, msg.AccountPosition.Ticker, msg.AccountPosition.ExchangeID,
                            msg.AccountPosition.BusinessType, msg.AccountPosition.FuturePosition.LongTdVolume,
                            msg.AccountPosition.FuturePosition.LongYdVolume, msg.AccountPosition.FuturePosition.LongOpenVolume,
                            msg.AccountPosition.FuturePosition.LongOpeningVolume, msg.AccountPosition.FuturePosition.LongClosingTdVolume,
                            msg.AccountPosition.FuturePosition.LongClosingYdVolume, msg.AccountPosition.FuturePosition.ShortTdVolume,
                            msg.AccountPosition.FuturePosition.ShortYdVolume, msg.AccountPosition.FuturePosition.ShortOpenVolume,
                            msg.AccountPosition.FuturePosition.ShortOpeningVolume, msg.AccountPosition.FuturePosition.ShortClosingTdVolume,
                            msg.AccountPosition.FuturePosition.ShortClosingYdVolume, msg.AccountPosition.UpdateTime)
            elif msg.AccountPosition.BusinessType == pack_message.EBusinessType.ESTOCK:
                logger.debug("Colo:{} Broker:{} Product:{} Account:{} Ticker:{} ExchangeID:{} BusinessType:{} "
                            "LongYdPosition:{} LongPosition:{} LongTdBuy:{} LongTdSell:{} "
                            "MarginYdPosition:{} MarginPosition:{} MarginTdBuy:{} MarginTdSell:{} "
                            "ShortYdPosition:{} ShortPosition:{} ShortTdBuy:{} ShortTdSell:{} "
                            "ShortDirectRepaid:{} SpecialPositionAvl:{} UpdateTime:{}", 
                            msg.AccountPosition.Colo, msg.AccountPosition.Broker, msg.AccountPosition.Product, 
                            msg.AccountPosition.Account, msg.AccountPosition.Ticker, msg.AccountPosition.ExchangeID,
                            msg.AccountPosition.BusinessType, msg.AccountPosition.StockPosition.LongYdPosition, 
                            msg.AccountPosition.StockPosition.LongPosition, msg.AccountPosition.StockPosition.LongTdBuy,
                            msg.AccountPosition.StockPosition.LongTdSell, msg.AccountPosition.StockPosition.MarginYdPosition,
                            msg.AccountPosition.StockPosition.MarginPosition, msg.AccountPosition.StockPosition.MarginTdBuy,
                            msg.AccountPosition.StockPosition.MarginTdSell, msg.AccountPosition.StockPosition.ShortYdPosition,
                            msg.AccountPosition.StockPosition.ShortPosition, msg.AccountPosition.StockPosition.ShortTdBuy,
                            msg.AccountPosition.StockPosition.ShortTdSell, msg.AccountPosition.StockPosition.ShortDirectRepaid,
                            msg.AccountPosition.StockPosition.SpecialPositionAvl, msg.AccountPosition.UpdateTime)

    ```