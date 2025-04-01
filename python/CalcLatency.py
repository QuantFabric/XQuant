import os
import pandas as pd
import time
import datetime


class CalcLatency(object):
    """
    Colo|Broker|Product|Account|Ticker|Exchange|Volume|Price|Status|OrderSide|OrderType|EngineID|OrderRef|OrderSysID|OrderLocalID|OrderToken|RiskID|ErrorID|ErrorMsg|RecvMarketTime|SendTime|InsertTime|BrokerACKTime|ExchACKTime|UpdateTime
    'XServer'|'CTP'|'Test1'|'188795'|'al2504'|'SHFE'|'0/0'|'0.000/0.000'|'初始化检查'|'OpenLong'|'LIMIT'|'0'|'571920007'|''|''|'0'|'Risk-1'|'0'|'API Connect Successed'|''|''|'08:53:12.007669'|''|''|'08:53:12.007701'
    'XServer'|'CTP'|'Test2'|'237477'|'al2504'|'SHFE'|'0/0'|'0.000/0.000'|'初始化检查'|'OpenLong'|'LIMIT'|'0'|'571940007'|''|''|'0'|'Risk-1'|'0'|'API Connect Successed'|''|''|'08:53:14.022988'|''|''|'08:53:14.023003'
    'XServer'|'CTP'|'Test2'|'237477'|'al2505'|'SHFE'|'1/1'|'20855.000/20855.000'|'全部成交'|'CloseYdLong'|'LIMIT'|'1'|'576110092'|'155122'|'8666003'|'1'|'Risk-1'|'0'|'全部成交报单已提交'|'09:00:11.123190'|'09:00:11.123250'|'09:00:11.123368'|'09:00:11.140066'|'09:00:11.140181'|'09:00:11.140217'
    'XServer'|'CTP'|'Test2'|'237477'|'al2505'|'SHFE'|'1/1'|'20855.000/20855.000'|'全部成交'|'CloseYdLong'|'LIMIT'|'2'|'576110093'|'155125'|'8666006'|'1'|'Risk-1'|'0'|'全部成交报单已提交'|'09:00:11.123190'|'09:00:11.123631'|'09:00:11.123740'|'09:00:11.140156'|'09:00:11.140248'|'09:00:11.140268'
    'XServer'|'CTP'|'Test1'|'188795'|'al2505'|'SHFE'|'1/1'|'20855.000/20855.000'|'全部成交'|'CloseYdLong'|'LIMIT'|'1'|'576110092'|'155123'|'8666004'|'1'|'Risk-1'|'0'|'全部成交报单已提交'|'09:00:11.123190'|'09:00:11.123262'|'09:00:11.123351'|'09:00:11.140057'|'09:00:11.142429'|'09:00:11.142494'
    'XServer'|'CTP'|'Test1'|'188795'|'al2505'|'SHFE'|'1/1'|'20855.000/20855.000'|'全部成交'|'CloseYdLong'|'LIMIT'|'2'|'576110093'|'155124'|'8666005'|'1'|'Risk-1'|'0'|'全部成交报单已提交'|'09:00:11.123190'|'09:00:11.123631'|'09:00:11.123660'|'09:00:11.140126'|'09:00:11.142519'|'09:00:11.142540'
    """
    def __init__(self, csv_file_path:str):
        self.csv_file_path = csv_file_path
        self.data = pd.read_csv(self.csv_file_path, low_memory=False, sep="|")
        self.data = self.data[self.data['ExchACKTime'].str.len() > 0]
        filter = ['RecvMarketTime', 'SendTime', 'InsertTime', 'BrokerACKTime', 'ExchACKTime']
        # 根据EngineID过滤C++版本的订单
        self.latency1_data = self.data[self.data['EngineID'] == "'1'"]
        self.latency1_data = self.latency1_data[filter]
        # 根据EngineID过滤Python版本的订单
        self.latency2_data = self.data[self.data['EngineID'] == "'2'"]
        self.latency2_data = self.latency2_data[filter]

        self.latency1_data = self.latency1_data[self.latency1_data['BrokerACKTime'].str.len() > 10]
        self.latency2_data = self.latency2_data[self.latency2_data['BrokerACKTime'].str.len() > 10]

        self.latency1_data = self.latency1_data[self.latency1_data['ExchACKTime'].str.len() > 10]
        self.latency2_data = self.latency2_data[self.latency2_data['ExchACKTime'].str.len() > 10]

    def calculate_latency(self):
        def timestamp(strTime: str):
            strTime = strTime.strip("'")
            ret_list = strTime.split(".", 1)
            if len(ret_list) < 2:
                raise ValueError(f"{strTime}")
            ret:int = time.mktime(time.strptime(f"{datetime.datetime.now().strftime('%Y-%m-%d')} {ret_list[0]}", "%Y-%m-%d %H:%M:%S")) * 1000000 + int(ret_list[1])
            return ret

        self.latency1_data['Tick2Order'] = self.latency1_data['InsertTime'].apply(lambda x: timestamp(x)) - self.latency1_data['RecvMarketTime'].apply(lambda x: timestamp(x))
        self.latency2_data['Tick2Order'] = self.latency2_data['InsertTime'].apply(lambda x: timestamp(x)) - self.latency2_data['RecvMarketTime'].apply(lambda x: timestamp(x))

        self.latency1_data['BrokerLatency'] = self.latency1_data['BrokerACKTime'].apply(lambda x: timestamp(x)) - self.latency1_data['InsertTime'].apply(lambda x: timestamp(x))
        self.latency2_data['BrokerLatency'] = self.latency2_data['BrokerACKTime'].apply(lambda x: timestamp(x)) - self.latency2_data['InsertTime'].apply(lambda x: timestamp(x))

        self.latency1_data['ExchangeLatency'] = self.latency1_data['ExchACKTime'].apply(lambda x: timestamp(x)) - self.latency1_data['InsertTime'].apply(lambda x: timestamp(x))
        self.latency2_data['ExchangeLatency'] = self.latency2_data['ExchACKTime'].apply(lambda x: timestamp(x)) - self.latency2_data['InsertTime'].apply(lambda x: timestamp(x))

        self.latency1_data['Quant'] = self.latency1_data['SendTime'].apply(lambda x: timestamp(x)) - self.latency1_data['RecvMarketTime'].apply(lambda x: timestamp(x))
        self.latency2_data['Quant'] = self.latency2_data['SendTime'].apply(lambda x: timestamp(x)) - self.latency2_data['RecvMarketTime'].apply(lambda x: timestamp(x))

        self.latency1_data['Trader'] = self.latency1_data['InsertTime'].apply(lambda x: timestamp(x)) - self.latency1_data['SendTime'].apply(lambda x: timestamp(x))
        self.latency2_data['Trader'] = self.latency2_data['InsertTime'].apply(lambda x: timestamp(x)) - self.latency2_data['SendTime'].apply(lambda x: timestamp(x))

        percentiles = [0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.65, 0.7, 0.75, 0.8, 0.85, 0.9, 0.95, 0.99]

        print(self.latency1_data['Tick2Order'].describe(percentiles=percentiles))
        print(self.latency2_data['Tick2Order'].describe(percentiles=percentiles))

        print(self.latency1_data['BrokerLatency'].describe(percentiles=percentiles))
        print(self.latency2_data['BrokerLatency'].describe(percentiles=percentiles))

        print(self.latency1_data['ExchangeLatency'].describe(percentiles=percentiles))
        print(self.latency2_data['ExchangeLatency'].describe(percentiles=percentiles))

        print(self.latency1_data['Quant'].describe(percentiles=percentiles))
        print(self.latency2_data['Quant'].describe(percentiles=percentiles))

        print(self.latency1_data['Trader'].describe(percentiles=percentiles))
        print(self.latency2_data['Trader'].describe(percentiles=percentiles))
        


if __name__ == "__main__":
    csv_file_path = "/home/yangyl/XOS/QuantFabric/XQuant/python/HistoryOrderTable20250325.csv"
    calculator = CalcLatency(csv_file_path=csv_file_path)
    calculator.calculate_latency()