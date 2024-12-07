#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unistd.h>
#include <sstream>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/inotify.h>

#define CORE_COUNT 8

std::vector<std::vector<long>> readCpuStats() {
    std::ifstream file("/proc/stat");
    std::string line;
    std::vector<std::vector<long>> cpuStats(CORE_COUNT, std::vector<long>(4));

    int core = 0;
    while (std::getline(file, line)) {
        if (line.find("cpu") == 0 && line.find("cpu ") != 0) { 
            std::istringstream iss(line);
            std::string cpu;
            iss >> cpu;
            for (int i = 0; i < 4; ++i) {
                iss >> cpuStats[core][i];
            }
            core++;
            if (core >= CORE_COUNT) break;
        }
    }
    return cpuStats;
}

int calculateTotalCpuLoad(const std::vector<std::vector<long>>& stats1, const std::vector<std::vector<long>>& stats2) {
    long total1 = 0, idle1 = 0;
    long total2 = 0, idle2 = 0;

    for (const auto& coreStats : stats1) {
        total1 += coreStats[0] + coreStats[1] + coreStats[2] + coreStats[3];
        idle1 += coreStats[3];
    }

    for (const auto& coreStats : stats2) {
        total2 += coreStats[0] + coreStats[1] + coreStats[2] + coreStats[3];
        idle2 += coreStats[3];
    }

    long totalDiff = total2 - total1;
    long idleDiff = idle2 - idle1;

    if (totalDiff == 0) {
        return 0;
    }

    return static_cast<int>((totalDiff - idleDiff) * 100 / totalDiff);
}
void WriteFile(const std::string& filePath, const std::string& content) noexcept {
    int fd = open(filePath.c_str(), O_WRONLY | O_NONBLOCK);

    if (fd < 0) {
        chmod(filePath.c_str(), 0666);
        fd = open(filePath.c_str(), O_WRONLY | O_NONBLOCK); 
    }

    if (fd >= 0) {
        write(fd, content.data(), content.size());
        close(fd);
        chmod(filePath.c_str(), 0444);
    }
}

void MaxCpuFreq(){
    WriteFile("/sys/devices/system/cpu/cpufreq/policy0/scaling_max_freq", "2147483647");
    for(int i = 1; i <= 7; ++i){
        WriteFile("/sys/devices/system/cpu/cpufreq/policy" + std::to_string(i)+ "/scaling_max_freq", "2147483647");
    }
}

void MixCpuFreq(){
    WriteFile("/sys/devices/system/cpu/cpufreq/policy0/scaling_max_freq", "1700000");
    for(int i = 1; i <= 7; ++i){
        WriteFile("/sys/devices/system/cpu/cpufreq/policy" + std::to_string(i)+ "/scaling_max_freq", "1600000");
    }
}
void Disable_Eas_Scheduler(){
    WriteFile("/proc/sys/kernel/sched_energy_aware", "0");
}
int main() {
    Disable_Eas_Scheduler();
    while(1){
        auto initialStats = readCpuStats();
        sleep(1); 
        auto finalStats = readCpuStats();

        if (initialStats.size() != finalStats.size()) {
            printf("错误: CPU统计大小不匹配\n");
            return 1;
        }

        int totalLoad = calculateTotalCpuLoad(initialStats, finalStats);

        if (totalLoad >= 75){
            MaxCpuFreq();
        } else {
            MixCpuFreq();
        }
        usleep(200000); // 防止频繁运行导致的性能消耗
    }
    return 0;
}