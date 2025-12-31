#include <iostream>
#include <string>
#include <sstream>
#include <dirent.h>
#include <signal.h>
#include <unistd.h>
#include <fstream>
#include <vector>
#include <cstdlib>

using namespace std;

void KillProcessesByName(const string& baseName) {
    if (baseName.empty()) return;

    string target = baseName;

    DIR* procDir = opendir("/proc");
    if (!procDir) {
        cout << "Failed to open /proc" << endl;
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(procDir)) != nullptr) {
        if (entry->d_type != DT_DIR) continue;

        pid_t pid = atoi(entry->d_name);
        if (pid == 0) continue;

        string commPath = string("/proc/") + entry->d_name + "/comm";
        ifstream commFile(commPath);
        if (!commFile) continue;

        string procName;
        getline(commFile, procName);
        procName.erase(procName.find_last_not_of("\n\r") + 1);

        if (procName == target) {
            if (pid == getpid()) continue;

            if (kill(pid, SIGTERM) == 0) {
                cout << "Killed process: " << procName << " (PID: " << pid << ")" << endl;
            }
        }
    }
    closedir(procDir);
}

void KillProcessById(pid_t pid) {
    if (pid == 0 || pid == getpid()) {
        cout << "Invalid PID or attempt to kill self" << endl;
        return;
    }

    if (kill(pid, SIGTERM) == 0) {
        cout << "Killed process by PID: " << pid << endl;
    }
    else {
        cout << "Failed to kill PID: " << pid << endl;
    }
}

int main(int argc, char* argv[]) {
    pid_t targetPid = 0;
    string targetName = "";

    for (int i = 1; i < argc; ++i) {
        if (string(argv[i]) == "--id" && i + 1 < argc) {
            targetPid = atoi(argv[i + 1]);
            ++i;
        }
        else if (string(argv[i]) == "--name" && i + 1 < argc) {
            targetName = argv[i + 1];
            ++i;
        }
    }

    char* envValue = getenv("PROC_TO_KILL");
    if (envValue != nullptr) {
        string envStr = envValue;
        stringstream ss(envStr);
        string process;
        while (getline(ss, process, ',')) {
            if (!process.empty()) {
                KillProcessesByName(process);
            }
        }
    }

    if (targetPid != 0) {
        KillProcessById(targetPid);
    }
    if (!targetName.empty()) {
        KillProcessesByName(targetName);
    }

    return 0;
}