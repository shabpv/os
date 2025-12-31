#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>
#include <sstream>
#include <cstdlib>

using namespace std;

static void KillProcessesByName(string baseName) {
    if (baseName.empty()) return;

    string target = baseName;
    size_t len = target.length();
    if (len < 4 || _stricmp(target.c_str() + len - 4, ".exe") != 0) {
        target += ".exe";
    }

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        cout << "Failed to create process snapshot" << endl;
        return;
    }

    PROCESSENTRY32 pe32 = { 0 };
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnapshot, &pe32)) {
        do {
            if (_stricmp(pe32.szExeFile, target.c_str()) == 0) {
                if (pe32.th32ProcessID == GetCurrentProcessId()) continue;

                HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe32.th32ProcessID);
                if (hProcess != NULL) {
                    TerminateProcess(hProcess, 0);
                    cout << "Killed process: " << pe32.szExeFile << " (PID: " << pe32.th32ProcessID << ")" << endl;
                    CloseHandle(hProcess);
                }
            }
        } while (Process32Next(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);
}

static void KillProcessById(DWORD pid) {
    if (pid == 0 || pid == GetCurrentProcessId()) {
        cout << "Invalid PID or attempt to kill self" << endl;
        return;
    }

    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (hProcess != NULL) {
        TerminateProcess(hProcess, 0);
        cout << "Killed process by PID: " << pid << endl;
        CloseHandle(hProcess);
    }
    else {
        cout << "Failed to open process with PID: " << pid << endl;
    }
}

int main(int argc, char* argv[]) {
    DWORD targetPid = 0;
    string targetName = "";

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--id") == 0 && i + 1 < argc) {
            targetPid = stoul(argv[i + 1]);
            ++i;
        }
        else if (strcmp(argv[i], "--name") == 0 && i + 1 < argc) {
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