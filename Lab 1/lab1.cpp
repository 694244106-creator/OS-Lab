/*
 * Lab 1 - OS Information Reporter (Linux)
 *
 * แสดงข้อมูลระบบ ได้แก่
 *   1. ชื่อ CPU
 *   2. จำนวนหน่วยความจำ (Memory)
 *   3. ชื่อและ version ของระบบปฏิบัติการ
 *   4. พื้นที่จัดเก็บข้อมูล (Storage)
 *   5. Process ที่ทำงานอยู่ในระบบ
 *
 * Build: g++ -std=c++17 -O2 -Wall -o lab1 lab1.cpp
 * Run  : ./lab1
 */

#include <dirent.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string trim(const std::string& text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

std::string humanBytes(unsigned long long bytes) {
    static const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < 4) {
        value /= 1024.0;
        ++unit;
    }
    std::ostringstream out;
    out << std::fixed << std::setprecision(unit == 0 ? 0 : 2) << value << ' ' << units[unit];
    return out.str();
}

void section(const std::string& title) {
    std::cout << "\n===== " << title << " =====\n";
}

// แปลง escape sequence ใน /proc/mounts เช่น "\040" -> " "
std::string unescapeMount(const std::string& text) {
    std::string out;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\\' && i + 3 < text.size() &&
            std::isdigit(static_cast<unsigned char>(text[i + 1])) &&
            std::isdigit(static_cast<unsigned char>(text[i + 2])) &&
            std::isdigit(static_cast<unsigned char>(text[i + 3]))) {
            out += static_cast<char>((text[i + 1] - '0') * 64 + (text[i + 2] - '0') * 8 + (text[i + 3] - '0'));
            i += 3;
        } else {
            out += text[i];
        }
    }
    return out;
}

// ---------- 1. CPU ----------
void showCpu() {
    section("1. CPU");

    std::ifstream cpuinfo("/proc/cpuinfo");
    if (!cpuinfo) {
        std::cout << "Cannot read /proc/cpuinfo\n";
        return;
    }

    std::string line;
    std::string model;
    int logicalCpus = 0;

    while (std::getline(cpuinfo, line)) {
        const auto colon = line.find(':');
        if (colon == std::string::npos) continue;

        const std::string key = trim(line.substr(0, colon));
        const std::string value = trim(line.substr(colon + 1));

        if (key == "processor") ++logicalCpus;
        if (model.empty() &&
            (key == "model name" || key == "Model" || key == "Hardware")) {
            model = value;
        }
    }

    std::cout << std::left << std::setw(14) << "CPU" << ": "
              << (model.empty() ? "Unknown" : model) << '\n';
    std::cout << std::left << std::setw(14) << "Logical CPUs" << ": "
              << logicalCpus << '\n';
}

// ---------- 2. Memory ----------
void showMemory() {
    section("2. Memory");

    std::ifstream meminfo("/proc/meminfo");
    if (!meminfo) {
        std::cout << "Cannot read /proc/meminfo\n";
        return;
    }

    unsigned long long totalKb = 0;
    unsigned long long availableKb = 0;
    unsigned long long freeKb = 0;

    std::string line;
    while (std::getline(meminfo, line)) {
        std::istringstream iss(line);
        std::string key;
        unsigned long long value = 0;
        iss >> key >> value;

        if (key == "MemTotal:") totalKb = value;
        else if (key == "MemAvailable:") availableKb = value;
        else if (key == "MemFree:") freeKb = value;
    }

    // ถ้า kernel เก่าไม่มี MemAvailable ให้ประมาณจาก MemFree
    if (availableKb == 0) availableKb = freeKb;

    const unsigned long long total = totalKb * 1024ULL;
    const unsigned long long available = availableKb * 1024ULL;
    const unsigned long long used = total > available ? total - available : 0;

    std::cout << std::left << std::setw(14) << "Total" << ": " << humanBytes(total) << '\n';
    std::cout << std::left << std::setw(14) << "Used" << ": " << humanBytes(used) << '\n';
    std::cout << std::left << std::setw(14) << "Available" << ": " << humanBytes(available) << '\n';
}

// ---------- 3. Operating System ----------
void showOs() {
    section("3. Operating System");

    std::string distro;
    std::ifstream osRelease("/etc/os-release");
    std::string line;
    while (std::getline(osRelease, line)) {
        if (line.rfind("PRETTY_NAME=", 0) == 0) {
            distro = trim(line.substr(12));
            if (distro.size() >= 2 && distro.front() == '"' && distro.back() == '"') {
                distro = distro.substr(1, distro.size() - 2);
            }
            break;
        }
    }

    struct utsname info {};
    const bool haveUname = uname(&info) == 0;

    std::cout << std::left << std::setw(14) << "OS" << ": "
              << (distro.empty() ? (haveUname ? info.sysname : "Unknown") : distro) << '\n';

    if (haveUname) {
        std::cout << std::left << std::setw(14) << "Kernel" << ": "
                  << info.sysname << ' ' << info.release << '\n';
        std::cout << std::left << std::setw(14) << "Version" << ": "
                  << info.version << '\n';
        std::cout << std::left << std::setw(14) << "Architecture" << ": "
                  << info.machine << '\n';
    }
}

// ---------- 4. Storage ----------
void showStorage() {
    section("4. Storage");

    std::ifstream mounts("/proc/mounts");
    if (!mounts) {
        std::cout << "Cannot read /proc/mounts\n";
        return;
    }

    // ระบบไฟล์เสมือนที่ไม่ใช่พื้นที่จัดเก็บจริง
    static const std::set<std::string> pseudoFs = {
        "proc", "sysfs", "devtmpfs", "devpts", "tmpfs", "cgroup", "cgroup2",
        "securityfs", "pstore", "debugfs", "tracefs", "configfs", "fusectl",
        "mqueue", "bpf", "hugetlbfs", "autofs", "ramfs", "nsfs", "binfmt_misc",
        "efivarfs", "rpc_pipefs", "fuse.lxcfs", "fuse.gvfsd-fuse", "squashfs"};

    std::cout << std::left << std::setw(22) << "Filesystem"
              << std::setw(8) << "Type"
              << std::right << std::setw(12) << "Size"
              << std::setw(12) << "Used"
              << std::setw(12) << "Avail"
              << std::setw(6) << "Use%"
              << "  Mounted on\n";

    std::string line;
    std::set<std::string> seenDevices;

    while (std::getline(mounts, line)) {
        std::istringstream iss(line);
        std::string device, mountPoint, type;
        if (!(iss >> device >> mountPoint >> type)) continue;

        if (pseudoFs.count(type)) continue;
        if (!seenDevices.insert(device).second) continue;

        mountPoint = unescapeMount(mountPoint);

        struct statvfs fs {};
        if (statvfs(mountPoint.c_str(), &fs) != 0) continue;
        if (fs.f_blocks == 0) continue;

        const unsigned long long total = static_cast<unsigned long long>(fs.f_blocks) * fs.f_frsize;
        const unsigned long long free = static_cast<unsigned long long>(fs.f_bfree) * fs.f_frsize;
        const unsigned long long avail = static_cast<unsigned long long>(fs.f_bavail) * fs.f_frsize;
        const unsigned long long used = total - free;
        const int percent = total == 0 ? 0 : static_cast<int>(used * 100 / total);

        std::cout << std::left << std::setw(22) << device
                  << std::setw(8) << type
                  << std::right << std::setw(12) << humanBytes(total)
                  << std::setw(12) << humanBytes(used)
                  << std::setw(12) << humanBytes(avail)
                  << std::setw(5) << percent << '%'
                  << "  " << mountPoint << '\n';
    }
}

// ---------- 5. Processes ----------
std::string processState(char state) {
    switch (state) {
        case 'R': return "Running";
        case 'S': return "Sleeping";
        case 'D': return "Disk sleep";
        case 'T': return "Stopped";
        case 't': return "Tracing";
        case 'Z': return "Zombie";
        case 'I': return "Idle";
        case 'X': return "Dead";
        default: return std::string(1, state);
    }
}

void showProcesses() {
    section("5. Processes");

    DIR* proc = opendir("/proc");
    if (!proc) {
        std::cout << "Cannot open /proc\n";
        return;
    }

    struct ProcInfo {
        int pid;
        std::string name;
        char state;
    };

    std::vector<ProcInfo> processes;
    while (dirent* entry = readdir(proc)) {
        const std::string name = entry->d_name;
        if (name.empty() ||
            !std::all_of(name.begin(), name.end(), [](unsigned char c) { return std::isdigit(c); })) {
            continue;
        }

        const int pid = std::stoi(name);
        const std::string base = "/proc/" + name;

        std::string command;
        std::ifstream commFile(base + "/comm");
        if (commFile) std::getline(commFile, command);

        char state = '?';
        std::ifstream statFile(base + "/stat");
        std::string stat;
        if (statFile && std::getline(statFile, stat)) {
            const auto close = stat.rfind(')');
            if (close != std::string::npos && close + 2 < stat.size()) {
                state = stat[close + 2];
            }
        }

        processes.push_back({pid, command.empty() ? "?" : command, state});
    }
    closedir(proc);

    std::sort(processes.begin(), processes.end(),
              [](const ProcInfo& a, const ProcInfo& b) { return a.pid < b.pid; });

    std::cout << std::left << std::setw(8) << "PID"
              << std::setw(14) << "State"
              << "Command\n";

    for (const auto& proc : processes) {
        std::cout << std::left << std::setw(8) << proc.pid
                  << std::setw(14) << processState(proc.state)
                  << proc.name << '\n';
    }

    std::cout << "\nTotal processes: " << processes.size() << '\n';
}

}  // namespace

int main() {
    std::cout << "System Information\n";

    showCpu();
    showMemory();
    showOs();
    showStorage();
    showProcesses();

    return 0;
}
