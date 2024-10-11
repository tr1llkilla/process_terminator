/*
Copyright (C) 2024 tr1llkilla
    This program comes with ABSOLUTELY NO WARRANTY.
    This is free software, and you are welcome to redistribute it
    under legal conditions.
Author:
tr1llkilla

Author's note:
This program is unfinished, but does the trick to get the job done manually
Feel free to credit this work in any future projects!
*/
#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <psapi.h>
#include <string>
#include <vector>
#include <algorithm> // For sorting
#include <chrono>
#include <thread>

// Struct to hold process information, including memory and CPU usage
struct ProcessInfo {
    DWORD pid;
    std::wstring serviceName;
    SIZE_T memoryUsage;
    double cpuUsage;
};

// Function to display process information
void DisplayServiceInfo(const ProcessInfo& process) {
    std::wcout << L"PID: " << process.pid << L" | Service Name: " << process.serviceName << L" | Memory Usage: " 
               << process.memoryUsage / 1024 << L" KB" << L" | CPU Usage: " << process.cpuUsage << L" %" << std::endl;
}

// Helper function to get the CPU usage of a process
double CalculateCPUUsage(HANDLE hProcess, FILETIME ftPrevSysKernel, FILETIME ftPrevSysUser, FILETIME ftPrevProcKernel, FILETIME ftPrevProcUser, int intervalMs = 100) {
    FILETIME ftSysIdle, ftSysKernel, ftSysUser;
    FILETIME ftProcCreation, ftProcExit, ftProcKernel, ftProcUser;

    // Get system times
    if (!GetSystemTimes(&ftSysIdle, &ftSysKernel, &ftSysUser)) {
        return 0.0;
    }

    // Get process times
    if (!GetProcessTimes(hProcess, &ftProcCreation, &ftProcExit, &ftProcKernel, &ftProcUser)) {
        return 0.0;
    }

    // Convert times to 64-bit integers
    ULARGE_INTEGER sysKernel, sysUser, procKernel, procUser;
    sysKernel.LowPart = ftSysKernel.dwLowDateTime;
    sysKernel.HighPart = ftSysKernel.dwHighDateTime;
    sysUser.LowPart = ftSysUser.dwLowDateTime;
    sysUser.HighPart = ftSysUser.dwHighDateTime;
    procKernel.LowPart = ftProcKernel.dwLowDateTime;
    procKernel.HighPart = ftProcKernel.dwHighDateTime;
    procUser.LowPart = ftProcUser.dwLowDateTime;
    procUser.HighPart = ftProcUser.dwHighDateTime;

    ULARGE_INTEGER prevSysKernel, prevSysUser, prevProcKernel, prevProcUser;
    prevSysKernel.LowPart = ftPrevSysKernel.dwLowDateTime;
    prevSysKernel.HighPart = ftPrevSysKernel.dwHighDateTime;
    prevSysUser.LowPart = ftPrevSysUser.dwLowDateTime;
    prevSysUser.HighPart = ftPrevSysUser.dwHighDateTime;
    prevProcKernel.LowPart = ftPrevProcKernel.dwLowDateTime;
    prevProcKernel.HighPart = ftPrevProcKernel.dwHighDateTime;
    prevProcUser.LowPart = ftPrevProcUser.dwLowDateTime;
    prevProcUser.HighPart = ftPrevProcUser.dwHighDateTime;

    // Calculate CPU usage percentage
    ULONGLONG sysTime = (sysKernel.QuadPart - prevSysKernel.QuadPart) + (sysUser.QuadPart - prevSysUser.QuadPart);
    ULONGLONG procTime = (procKernel.QuadPart - prevProcKernel.QuadPart) + (procUser.QuadPart - prevProcUser.QuadPart);

    if (sysTime == 0) return 0.0;

    return (100.0 * procTime) / sysTime;
}

// Function to enumerate and sort processes by memory usage
void EnumerateProcesses() {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        std::cerr << "Error creating process snapshot." << std::endl;
        return;
    }

    PROCESSENTRY32 processEntry;
    processEntry.dwSize = sizeof(PROCESSENTRY32);

    std::vector<ProcessInfo> processes; // Vector to store processes

    if (Process32First(hSnapshot, &processEntry)) {
        do {
            // Convert ANSI string to wide string
            std::wstring serviceName(processEntry.szExeFile, processEntry.szExeFile + strlen(processEntry.szExeFile));

            // Get memory usage for the process
            PROCESS_MEMORY_COUNTERS_EX pmc;
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processEntry.th32ProcessID);
            if (hProcess != NULL) {
                if (GetProcessMemoryInfo(hProcess, (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
                    // Get initial CPU times
                    FILETIME ftPrevSysIdle, ftPrevSysKernel, ftPrevSysUser;
                    FILETIME ftPrevProcCreation, ftPrevProcExit, ftPrevProcKernel, ftPrevProcUser;

                    GetSystemTimes(&ftPrevSysIdle, &ftPrevSysKernel, &ftPrevSysUser);
                    GetProcessTimes(hProcess, &ftPrevProcCreation, &ftPrevProcExit, &ftPrevProcKernel, &ftPrevProcUser);

                    // Wait for a small interval to calculate CPU usage
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));

                    // Calculate CPU usage after the interval
                    double cpuUsage = CalculateCPUUsage(hProcess, ftPrevSysKernel, ftPrevSysUser, ftPrevProcKernel, ftPrevProcUser);

                    // Store the process information in the vector
                    processes.push_back({processEntry.th32ProcessID, serviceName, pmc.WorkingSetSize, cpuUsage});
                }
                CloseHandle(hProcess);
            }

        } while (Process32Next(hSnapshot, &processEntry));
    } else {
        std::cerr << "Error enumerating processes." << std::endl;
    }

    CloseHandle(hSnapshot);

    // Sort the processes by memory usage in descending order
    std::sort(processes.begin(), processes.end(), [](const ProcessInfo& a, const ProcessInfo& b) {
        return a.memoryUsage > b.memoryUsage;
    });

    // Display the sorted processes
    for (const auto& process : processes) {
        DisplayServiceInfo(process);
    }
}

// Function to terminate a process by PID
void TerminateProcessByPID(DWORD pid) {
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (hProcess == NULL) {
        std::cerr << "Unable to open process with PID: " << pid << ". Error: " << GetLastError() << std::endl;
        return;
    }

    // Try to terminate the process
    if (TerminateProcess(hProcess, 0)) {
        std::cout << "Process with PID " << pid << " has been terminated." << std::endl;
    } else {
        std::cerr << "Failed to terminate process with PID " << pid << ". Error: " << GetLastError() << std::endl;
    }

    CloseHandle(hProcess);
}

int main() {
    char choice;

    do {
        // Enumerate processes each time the loop runs, and sort by memory usage
        EnumerateProcesses();

        // Ask the user if they want to terminate a process
        std::cout << "\nWould you like to terminate a process? (Y/N): ";
        std::cin >> choice;

        if (choice == 'Y' || choice == 'y') {
            // If the user chooses to terminate a process, ask for the PID
            DWORD pidToTerminate;
            std::cout << "Enter the PID of the process you want to terminate: ";
            std::cin >> pidToTerminate;

            TerminateProcessByPID(pidToTerminate);
        }

    } while (choice == 'Y' || choice == 'y'); // Repeat while the user enters 'Y' or 'y'

    std::cout << "Exiting program." << std::endl;
    return 0;
}
