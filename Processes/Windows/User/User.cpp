#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>
#include <sstream>

using namespace std;

static int GetProcessCount(string baseName) {
    if (baseName.empty()) return 0;

    string target = baseName;
    size_t len = target.length();
    if (len < 4 || _stricmp(target.c_str() + len - 4, ".exe") != 0) {
        target += ".exe";
    }

    int count = 0;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32 pe32 = { 0 };
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnapshot, &pe32)) {
        do {
            if (_stricmp(pe32.szExeFile, target.c_str()) == 0) {
                ++count;
            }
        } while (Process32Next(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);
    return count;
}

static bool IsProcessAlive(DWORD pid) {
    if (pid == 0) return false;
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (hProcess == NULL) return false;

    DWORD exitCode;
    bool alive = (GetExitCodeProcess(hProcess, &exitCode) && exitCode == STILL_ACTIVE);
    CloseHandle(hProcess);
    return alive;
}

static DWORD LaunchApp(const string& appName) {
    string cmd = appName;
    if (_stricmp(appName.c_str(), "calc") != 0 && cmd.find(".exe") == string::npos) {
        cmd += ".exe";
    }

    STARTUPINFOA si = { 0 };
    si.cb = sizeof(STARTUPINFOA);
    PROCESS_INFORMATION pi = { 0 };

    char cmdBuffer[256];
    strcpy_s(cmdBuffer, cmd.c_str());

    if (CreateProcessA(NULL, cmdBuffer, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hThread);
        return pi.dwProcessId;
    }
    else {
        cout << "Failed to launch " << appName << " (error: " << GetLastError() << ")" << endl;
        return 0;
    }
}

static void RunKiller(const string& args) {
    string cmd = "Killer.exe " + args;

    STARTUPINFOA si = { 0 };
    si.cb = sizeof(STARTUPINFOA);
    PROCESS_INFORMATION pi = { 0 };

    char cmdBuffer[512];
    strcpy_s(cmdBuffer, cmd.c_str());

    if (CreateProcessA(NULL, cmdBuffer, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
    else {
        cout << "Failed to launch Killer.exe with args: " << args << endl;
    }
}

int main() {
    SetEnvironmentVariableA("PROC_TO_KILL", "notepad");

    cout << "Environment variable PROC_TO_KILL set to \"notepad\"" << endl;

    cout << "\n--- Test killing by environment variable ---\n";
    cout << "Launching several notepad.exe...\n";
    LaunchApp("notepad");
    LaunchApp("notepad");
    LaunchApp("notepad");
    Sleep(2000);

    int count = GetProcessCount("notepad");
    cout << "notepad.exe count before Killer: " << count << endl;

    RunKiller("");

    Sleep(1000);
    count = GetProcessCount("notepad");
    cout << "notepad.exe count after Killer: " << count << " (should be 0)\n";

    cout << "\n--- Test with --name parameter ---\n";
    cout << "Launching several Calculator.exe (via calc)...\n";
    LaunchApp("calc");
    LaunchApp("calc");
    Sleep(3000);  

    count = GetProcessCount("Calculator");
    cout << "Calculator.exe count before Killer: " << count << endl;

    RunKiller("--name Calculator");

    Sleep(1000);
    count = GetProcessCount("Calculator");
    cout << "Calculator.exe count after Killer: " << count << " (should be 0)\n";

    cout << "\n--- Test with --id parameter ---\n";
    cout << "Launching mspaint.exe...\n";
    DWORD paintPid = LaunchApp("mspaint");
    if (paintPid != 0) {
        Sleep(2000);
        if (IsProcessAlive(paintPid)) {
            cout << "mspaint.exe launched, PID: " << paintPid << endl;
        }

        stringstream ss;
        ss << "--id " << paintPid;
        RunKiller(ss.str());

        Sleep(1000);
        if (!IsProcessAlive(paintPid)) {
            cout << "mspaint.exe successfully killed by PID\n";
        }
    }
    else {
        cout << "Failed to launch mspaint.exe\n";
    }

    SetEnvironmentVariableA("PROC_TO_KILL", NULL);
    cout << "\nEnvironment variable PROC_TO_KILL removed\n";

    system("pause");
    return 0;
}