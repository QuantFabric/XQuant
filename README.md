# XQuant策略交易平台

- XQuant C++/Python版本使用SHMConnection C++版本与XMarketCenter、XRiskJudge、XTrader进行IPC通信：
    - 通过MarketServer内存通道从XMarketCenter读取行情数据，进行计算后触发交易信号，将报单通写入OrderServer内存通道；
    - XTrader从OrderServer内存通道读取报单请求，如果需要风控检查，则将报单请求写入RiskServer内存通道；如果不需要风控检查，则直接调用柜台API进行报单。
    - XRiskJudge从RiskServer内存通道读取报单，进行风控检查，并将检查结果写入内存通道；
    - XTrader从RiskServer内存通道读取报单检查结果，如果风控检查不通过，直接将订单状态信息返回监控系统；如果风控检查通过，则调用柜台API进行报单。
