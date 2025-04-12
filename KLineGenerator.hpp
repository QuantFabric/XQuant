#include <iostream>
#include <vector>
#include <unordered_map>
#include <cmath>
#include <algorithm>
#include <mutex>
#include <string.h>
#include <functional>


// K线数据结构
typedef struct BarData
{
    char ticker[20];
    int64_t start_time;
    int64_t end_time;
    int interval;
    double open;
    double high;
    double low;
    double close;
    int volume;
    BarData()
    {
        start_time = 0;
        end_time = 0;
        open = 0;
        high = 0;
        low = 0;
        close = 0;
        volume = 0;
    }
};

using OnWindowBarFunc = std::function<void(const BarData&)>;

class KLineGenerator 
{
public:
    explicit KLineGenerator(const char* ticker, uint16_t snapshot_interval, uint16_t slice_per_sec, const std::vector<int>& intervals):m_IntervalsVec(intervals) 
    {
        m_Ticker = ticker;
        m_SnapshotInterval = snapshot_interval;
        m_slice_per_sec = slice_per_sec;
        for(int interval : m_IntervalsVec) 
        {
            m_CurrentKLineMap[interval] = BarData();
            m_HistoryKLineMap[interval] = std::vector<BarData>();
        }
    }

    void SetOnWindowBarFunc(OnWindowBarFunc OnWindowBar)
    {
        m_OnWindowBar = OnWindowBar;
    }

    // 处理Tick数据
    void ProcessTick(int64_t section_start, int64_t section_end, int64_t timestamp, double price, int volume) 
    {
        for(int interval : m_IntervalsVec) 
        {
            int64_t window_start = calculate_window_start(timestamp, interval);
            int64_t window_end = 0;
            if(m_slice_per_sec > 0)
            {
                window_end = window_start + (interval - 1) * 1000 + (1000 / m_slice_per_sec) * (m_slice_per_sec - 1);
            }
            else
            {
                window_end = window_start + (interval - m_SnapshotInterval) * 1000;
            }
            BarData& current_kline = m_CurrentKLineMap[interval];
            // Tick数据在周期内
            if(current_kline.end_time > window_start)
            {
                // 更新当前K线
                if(current_kline.volume > 0)
                {
                    current_kline.high = std::max(current_kline.high, price);
                    current_kline.low = std::min(current_kline.low, price);
                    current_kline.close = price;
                    current_kline.volume += volume;
                }
                else // 周期内收到的第一个Tick数据切片
                {
                    current_kline.open = price;
                    current_kline.high = price;
                    current_kline.low = price;
                    current_kline.close = price;
                    current_kline.volume = volume;
                }
                // 周期内最后一个Tick切片数据, 关闭K线
                if(timestamp >= current_kline.end_time)
                {
                    if(section_start + 59 * 1000 <= timestamp && timestamp < section_end + 59 * 1000)
                    {
                        m_HistoryKLineMap[interval].push_back(current_kline);
                        m_OnWindowBar(current_kline);
                    }
                    // 初始化新的K线
                    strncpy(current_kline.ticker, m_Ticker.c_str(), sizeof(current_kline.ticker));
                    current_kline.start_time = calculate_window_start(timestamp + 1000, interval);
                    if(m_slice_per_sec > 0)
                    {
                        current_kline.end_time = current_kline.start_time + (interval - 1) * 1000 + (1000 / m_slice_per_sec) * (m_slice_per_sec - 1);
                    }
                    else
                    {
                        current_kline.end_time = current_kline.start_time + (interval - m_SnapshotInterval) * 1000;
                    }
                    current_kline.interval = interval;
                    current_kline.open = 0;
                    current_kline.high = 0;
                    current_kline.low = 0;
                    current_kline.close = 0;
                    current_kline.volume = 0;
                }
            }
            // 第一次初始化K线
            else if(current_kline.end_time == 0)
            {
                // 初始化新K线
                strncpy(current_kline.ticker, m_Ticker.c_str(), sizeof(current_kline.ticker));
                current_kline.start_time = window_start;
                current_kline.end_time = window_end;
                current_kline.interval = interval;
                current_kline.open = price;
                current_kline.high = price;
                current_kline.low = price;
                current_kline.close = price;
                current_kline.volume = volume;
            }
            // 超时关闭K线
            else if(current_kline.end_time < window_start)
            {
                if(section_start + 59 * 1000 <= timestamp && timestamp < section_end + 59 * 1000)
                {
                    // 关闭当前K线
                    m_HistoryKLineMap[interval].push_back(current_kline);
                    m_OnWindowBar(current_kline);
                }
                // 初始化新K线
                strncpy(current_kline.ticker, m_Ticker.c_str(), sizeof(current_kline.ticker));
                current_kline.start_time = window_start;
                current_kline.end_time = window_end;
                current_kline.interval = interval;
                current_kline.open = price;
                current_kline.high = price;
                current_kline.low = price;
                current_kline.close = price;
                current_kline.volume = volume;
            } 
        }
    }

    void CloseKLine(int64_t section_start, int64_t section_end, int64_t timestamp)
    {
        for(int interval : m_IntervalsVec) 
        {
            BarData& current_kline = m_CurrentKLineMap[interval];
            // 超时关闭K线
            if(timestamp >= current_kline.end_time) 
            {
                m_HistoryKLineMap[interval].push_back(current_kline);
                m_OnWindowBar(current_kline);

                timestamp = timestamp + 1000;
                int64_t window_start = calculate_window_start(timestamp, interval);
                int64_t window_end = 0;
                if(m_slice_per_sec > 0)
                {
                    window_end = window_start + (interval - 1) * 1000 + (1000 / m_slice_per_sec) * (m_slice_per_sec - 1);
                }
                else
                {
                    window_end = window_start + (interval - m_SnapshotInterval) * 1000;
                } 
                // 初始化新K线
                strncpy(current_kline.ticker, m_Ticker.c_str(), sizeof(current_kline.ticker));
                current_kline.start_time = window_start;
                current_kline.end_time = window_end;
                current_kline.interval = interval;
                current_kline.open = 0;
                current_kline.high = 0;
                current_kline.low = 0;
                current_kline.close = 0;
                current_kline.volume = 0;
            } 
        }
    }

    // 获取当前K线
    BarData& GetCurrentKLine(int interval) 
    {
        return m_CurrentKLineMap[interval];
    }

    // 获取历史数据
    std::vector<BarData>& GetHistory(int interval) 
    {
        return m_HistoryKLineMap[interval];
    }
protected:
    // 时间窗口对齐计算
    int64_t calculate_window_start(int64_t timestamp, int interval) 
    {
        int64_t timestamp_sec = timestamp / 1000;
        tm local_tm = *localtime(&timestamp_sec);
        if (interval >= 86400) 
        { 
            // 日线及以上
            local_tm.tm_hour = 0;
            local_tm.tm_min = 0;
            local_tm.tm_sec = 0;
        } 
        else 
        {
            int total_sec = local_tm.tm_hour * 3600 
                          + local_tm.tm_min * 60 
                          + local_tm.tm_sec;
            int aligned_sec = (total_sec / interval) * interval;

            local_tm.tm_hour = aligned_sec / 3600;
            local_tm.tm_min = (aligned_sec % 3600) / 60;
            local_tm.tm_sec = aligned_sec % 60;
        }

        return mktime(&local_tm) * 1000;
    }
private:
    std::vector<int> m_IntervalsVec;           // 支持的周期列表（秒）
    std::unordered_map<int, BarData> m_CurrentKLineMap;      // 当前未闭合的K线
    std::unordered_map<int, std::vector<BarData>> m_HistoryKLineMap;   // 历史K线存储
    uint16_t m_slice_per_sec; // 每秒切片数量
    uint16_t m_SnapshotInterval; // 快照周期
    OnWindowBarFunc m_OnWindowBar;
    std::string m_Ticker;
};

