#include <iostream>
#include <iomanip>
#include <unistd.h>
#include <dirent.h>
#include <fstream>
#include <vector>
#include <algorithm>
#include <signal.h>
#include <string>

using namespace std;

struct Process {
    int pid;
    string user;
    float cpu;
    float ram;
    string command;
};

// ---------------- CPU USAGE ----------------
float getCPUUsage() {
    static long prevIdle = 0, prevTotal = 0;

    ifstream file("/proc/stat");
    string cpu;
    long user, nice, system, idle, iowait, irq, softirq;
    file >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq;

    long idleTime = idle + iowait;
    long total = user + nice + system + idle + iowait + irq + softirq;

    long diffIdle = idleTime - prevIdle;
    long diffTotal = total - prevTotal;

    prevIdle = idleTime;
    prevTotal = total;

    if(diffTotal == 0) return 0.0;
    return (1.0 - (float)diffIdle / diffTotal) * 100;
}

// ---------------- MEMORY USAGE ----------------
float getMemoryUsage() {
    ifstream file("/proc/meminfo");
    string key;
    long total = 0, free = 0;

    while(file >> key) {
        if(key == "MemTotal:") file >> total;
        if(key == "MemAvailable:") {
            file >> free;
            break;
        }
    }
    return (1 - (float)free / total) * 100;
}

// ---------------- PROCESS LIST ----------------
vector<Process> getProcesses() {
    vector<Process> procs;
    DIR* dir = opendir("/proc");
    struct dirent* entry;

    while((entry = readdir(dir)) != nullptr) {
        if(!isdigit(entry->d_name[0])) continue;

        int pid = stoi(entry->d_name);
        string basePath = "/proc/" + string(entry->d_name);

        // Read process name
        ifstream cmdFile(basePath + "/comm");
        if(!cmdFile) continue;
        string command;
        getline(cmdFile, command);

        // Read CPU stats
        ifstream statFile(basePath + "/stat");
        if(!statFile) continue;

        string skip;
        long utime, stime;
        for(int i = 0; i < 13; i++) statFile >> skip;
        statFile >> utime >> stime;
        float cpuLoad = float(utime + stime) / sysconf(_SC_CLK_TCK);

        // Read memory usage
        ifstream statusFile(basePath + "/status");
        if(!statusFile) continue;
        string key;
        float ram = 0;
        while(statusFile >> key) {
            if(key == "VmRSS:") {
                statusFile >> ram;
                ram /= 1024.0; // KB → MB
                break;
            }
        }

        procs.push_back({pid, "user", cpuLoad, ram, command});
    }
    closedir(dir);

    sort(procs.begin(), procs.end(),
         [](auto &a, auto &b){ return a.cpu > b.cpu; });

    return procs;
}

// ---------------- KILL PROCESS ----------------
void killProc(int pid) {
    if(kill(pid, SIGTERM) == 0)
        cout << "✅ Process " << pid << " killed.\n";
    else
        cout << "❌ Failed to kill process.\n";
}

// ---------------- MAIN UI LOOP ----------------
int main() {
    while(true) {
        system("clear");
        cout << "\033[1;36m===== SYSTEM MONITOR TOOL =====\033[0m\n";
        cout << "CPU Usage: " << fixed << setprecision(1) << getCPUUsage() << "%\n";
        cout << "Memory Usage: " << getMemoryUsage() << "%\n\n";

        cout << " PID   CPU%   RAM(MB)   COMMAND\n";
        cout << "--------------------------------------\n";

        auto processes = getProcesses();
        for(int i = 0; i < min(10, (int)processes.size()); i++) {
            cout << setw(5) << processes[i].pid << "  "
                 << setw(5) << processes[i].cpu << "  "
                 << setw(7) << processes[i].ram << "   "
                 << processes[i].command << "\n";
        }

        cout << "\nEnter PID to kill or 0 to refresh: ";
        int pid;
        cin >> pid;
        if(pid > 0) killProc(pid);
    }
    return 0;
}
