#include "KLineGenerator.hpp"

static void OnWindowBar(const BarData& data)
{
    printf("%dmin K线回调 open:%.2f high:%.2f low:%.2f close:%.2f volume:%d\n",
            data.interval / 60, data.open, data.high, data.low, data.close, data.volume);
}

// 示例使用
int main(int argc, char* argv[]) 
{
    // 初始化生成器（1分钟和5分钟）
    uint16_t snapshot_interval = 0;
    uint16_t slice_per_sec = 2;
    KLineGenerator kline_generator("al2504", snapshot_interval, slice_per_sec, {60, 300});
    kline_generator.SetOnWindowBarFunc(OnWindowBar);
    // 模拟Tick数据生成
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    uint64_t start_time = ts.tv_sec * 1000;
    for (int i = 0; i < 1200; ++i) 
    {
        uint64_t timestamp = start_time + i * 500;
        double price = 100.0 + (i % 20 - 10) * 0.5;
        int volume = 100;
        kline_generator.ProcessTick(start_time, start_time + 1200 * 1000, timestamp, price, volume);
        
        if(timestamp % 60 == 0)
        {
            kline_generator.CloseKLine(start_time, start_time + 1200 * 1000, timestamp);
        }
    }


    // 输出历史数据统计
    std::vector<BarData>& data_list_1min = kline_generator.GetHistory(60);
    printf("1min 周期生成K线数量:%d\n", data_list_1min.size());
    for(int i = 0; i < data_list_1min.size(); i++)
    {
        BarData& data_1min = data_list_1min.at(i);
        printf("1min K线 open:%.2f high:%.2f low:%.2f close:%.2f volume:%d\n", 
                data_1min.open, data_1min.high, data_1min.low, data_1min.close, data_1min.volume);
    }
    std::vector<BarData>& data_list_5min = kline_generator.GetHistory(300);
    printf("5min 周期生成K线数量:%d\n", data_list_5min.size());
    for(int i = 0; i < data_list_5min.size(); i++)
    {
        BarData& data_5min = data_list_5min.at(i);
        printf("5min K线 open:%.2f high:%.2f low:%.2f close:%.2f volume:%d\n", 
                data_5min.open, data_5min.high, data_5min.low, data_5min.close, data_5min.volume);
    }

    return 0;
}

// g++ --std=c++17 -fPIC -lrt -g -O2 KLineGeneratorTest.cpp -o test