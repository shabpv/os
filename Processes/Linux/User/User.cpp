#include <iostream>
#include <string>
#include <sstream>
#include <dirent.h>
#include <unistd.h>
#include <fstream>
#include <sys/wait.h>
#include <vector>
#include <cstdlib>  

using namespace std;

int GetProcessCount(const string& baseName) {
    if (baseName.empty()) return 0;

    int count = 0;
    DIR* procDir = opendir("/proc");
    if (!procDir) return 0;

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

        if (procName == baseName) {
            ++count;
        }
    }
    closedir(procDir);
    return count;
}

bool IsProcessAlive(pid_t pid) {
    return kill(pid, 0) == 0;
}

pid_t LaunchApp(const vector<string>& args) {
    pid_t pid = fork();
    if (pid == 0) {
        vector<char*> cargs;
        for (const auto& arg : args) cargs.push_back(const_cast<char*>(arg.c_str()));
        cargs.push_back(nullptr);

        execvp(cargs[0], cargs.data());
        perror("Failed to launch app");  
        exit(1);
    } else if (pid > 0) {
        return pid;
    }
    return 0;
}

void RunKiller(const string& args_str) {
    pid_t pid = fork();
    if (pid == 0) {
        vector<string> args;
        args.push_back("./Killer");

        if (!args_str.empty()) {
            stringstream ss(args_str);
            string token;
            while (ss >> token) {
                args.push_back(token);
            }
        }

        vector<char*> cargs;
        for (const auto& arg : args) {
            cargs.push_back(const_cast<char*>(arg.c_str()));
        }
        cargs.push_back(nullptr);

        execvp("./Killer", cargs.data());
        perror("Failed to launch Killer");  
        exit(1);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
    } else {
        cout << "Fork failed for Killer" << endl;
    }
}

int main() {
    setenv("PROC_TO_KILL", "sleep", 1);

    cout << "Environment variable PROC_TO_KILL set to \"sleep\"" << endl;

    cout << "\n--- Test killing by environment variable ---\n";
    cout << "Launching several sleep processes...\n";
    LaunchApp({"sleep", "60"});
    LaunchApp({"sleep", "60"});
    LaunchApp({"sleep", "60"});
    sleep(1);

    int count = GetProcessCount("sleep");
    cout << "sleep count before Killer: " << count << endl;

    RunKiller("");

    sleep(1);
    count = GetProcessCount("sleep");
    cout << "sleep count after Killer: " << count << " (should be 0)\n";

    cout << "\n--- Test with --name parameter ---\n";
    cout << "Launching several sleep processes...\n";
    LaunchApp({"sleep", "60"});
    LaunchApp({"sleep", "60"});
    sleep(1);

    count = GetProcessCount("sleep");
    cout << "sleep count before Killer: " << count << endl;

    RunKiller("--name sleep");

    sleep(1);
    count = GetProcessCount("sleep");
    cout << "sleep count after Killer: " << count << " (should be 0)\n";

    cout << "\n--- Test with --id parameter ---\n";
    cout << "Launching sleep process...\n";
    pid_t testPid = LaunchApp({"sleep", "60"});
    if (testPid != 0) {
        sleep(1);
        if (IsProcessAlive(testPid)) {
            cout << "sleep launched, PID: " << testPid << endl;
        }

        stringstream ss;
        ss << "--id " << testPid;
        RunKiller(ss.str());

        sleep(1);
        if (!IsProcessAlive(testPid)) {
            cout << "sleep successfully killed by PID\n";
        }
    } else {
        cout << "Failed to launch sleep\n";
    }

    unsetenv("PROC_TO_KILL");
    cout << "\nEnvironment variable PROC_TO_KILL removed\n";

    cout << "\nPress Enter to exit...";
    cin.get();
    return 0;
}