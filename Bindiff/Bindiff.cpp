#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

// ANSI color codes
const std::string RESET = "\033[0m";
const std::string RED = "\033[31m";
const std::string GREEN = "\033[32m";

void setupConsole() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
#endif
}

// Efficient line-by-line comparison using a sliding window approach
void compareFiles(const std::string& file1, const std::string& file2) {
    std::ifstream f1(file1);
    std::ifstream f2(file2);

    if (!f1.is_open() || !f2.is_open()) {
        std::cerr << "Error opening files\n";
        return;
    }

    // Use a fixed-size window for context
    const int CONTEXT_SIZE = 100;
    std::vector<std::string> f1Buffer(CONTEXT_SIZE);
    std::vector<std::string> f2Buffer(CONTEXT_SIZE);
    std::vector<bool> f1Matched(CONTEXT_SIZE, false);
    std::vector<bool> f2Matched(CONTEXT_SIZE, false);

    // Fill initial buffers
    int f1Count = 0, f2Count = 0;
    std::string line;

    for (int i = 0; i < CONTEXT_SIZE && std::getline(f1, line); i++) {
        f1Buffer[i] = line;
        f1Count++;
    }

    for (int i = 0; i < CONTEXT_SIZE && std::getline(f2, line); i++) {
        f2Buffer[i] = line;
        f2Count++;
    }

    // Process files in chunks
    while (f1Count > 0 || f2Count > 0) {
        // Find matches within the current window
        for (int i = 0; i < f1Count; i++) {
            if (f1Matched[i]) continue;

            for (int j = 0; j < f2Count; j++) {
                if (f2Matched[j]) continue;

                if (f1Buffer[i] == f2Buffer[j]) {
                    f1Matched[i] = true;
                    f2Matched[j] = true;
                    break;
                }
            }
        }

        // Output lines that can be determined
        int f1ProcessedCount = 0;
        int f2ProcessedCount = 0;

        // Process unmatched lines at the beginning of file1
        while (f1ProcessedCount < f1Count && !f1Matched[f1ProcessedCount]) {
            std::cout << RED << "- " << f1Buffer[f1ProcessedCount] << RESET << '\n';
            f1ProcessedCount++;
        }

        // Process unmatched lines at the beginning of file2
        while (f2ProcessedCount < f2Count && !f2Matched[f2ProcessedCount]) {
            std::cout << GREEN << "+ " << f2Buffer[f2ProcessedCount] << RESET << '\n';
            f2ProcessedCount++;
        }

        // Process matched lines
        if (f1ProcessedCount < f1Count && f2ProcessedCount < f2Count) {
            // Find the first match
            while (f1ProcessedCount < f1Count &&
                (!f1Matched[f1ProcessedCount] ||
                    f1Buffer[f1ProcessedCount] != f2Buffer[f2ProcessedCount])) {
                std::cout << RED << "- " << f1Buffer[f1ProcessedCount] << RESET << '\n';
                f1ProcessedCount++;
            }

            if (f1ProcessedCount < f1Count) {
                std::cout << "  " << f1Buffer[f1ProcessedCount] << '\n';
                f1ProcessedCount++;
                f2ProcessedCount++;
            }
        }

        // Shift buffers
        if (f1ProcessedCount > 0) {
            // Shift file1 buffer
            for (int i = 0; i < f1Count - f1ProcessedCount; i++) {
                f1Buffer[i] = f1Buffer[i + f1ProcessedCount];
                f1Matched[i] = f1Matched[i + f1ProcessedCount];
            }

            // Fill in new lines
            for (int i = f1Count - f1ProcessedCount; i < CONTEXT_SIZE; i++) {
                if (std::getline(f1, line)) {
                    f1Buffer[i] = line;
                    f1Matched[i] = false;
                }
                else {
                    f1Count = i;
                    break;
                }
            }

            f1Count = std::max(0, f1Count - f1ProcessedCount);
        }

        if (f2ProcessedCount > 0) {
            // Shift file2 buffer
            for (int i = 0; i < f2Count - f2ProcessedCount; i++) {
                f2Buffer[i] = f2Buffer[i + f2ProcessedCount];
                f2Matched[i] = f2Matched[i + f2ProcessedCount];
            }

            // Fill in new lines
            for (int i = f2Count - f2ProcessedCount; i < CONTEXT_SIZE; i++) {
                if (std::getline(f2, line)) {
                    f2Buffer[i] = line;
                    f2Matched[i] = false;
                }
                else {
                    f2Count = i;
                    break;
                }
            }

            f2Count = std::max(0, f2Count - f2ProcessedCount);
        }

        // If no progress was made but we still have lines, force output of one line
        if (f1ProcessedCount == 0 && f2ProcessedCount == 0 && (f1Count > 0 || f2Count > 0)) {
            if (f1Count > 0) {
                std::cout << RED << "- " << f1Buffer[0] << RESET << '\n';

                // Shift file1 buffer
                for (int i = 0; i < f1Count - 1; i++) {
                    f1Buffer[i] = f1Buffer[i + 1];
                    f1Matched[i] = f1Matched[i + 1];
                }

                if (std::getline(f1, line)) {
                    f1Buffer[f1Count - 1] = line;
                    f1Matched[f1Count - 1] = false;
                }
                else {
                    f1Count--;
                }
            }

            if (f2Count > 0) {
                std::cout << GREEN << "+ " << f2Buffer[0] << RESET << '\n';

                // Shift file2 buffer
                for (int i = 0; i < f2Count - 1; i++) {
                    f2Buffer[i] = f2Buffer[i + 1];
                    f2Matched[i] = f2Matched[i + 1];
                }

                if (std::getline(f2, line)) {
                    f2Buffer[f2Count - 1] = line;
                    f2Matched[f2Count - 1] = false;
                }
                else {
                    f2Count--;
                }
            }
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <file1> <file2>\n";
        return 1;
    }

    setupConsole();
    compareFiles(argv[1], argv[2]);

    return 0;
}
