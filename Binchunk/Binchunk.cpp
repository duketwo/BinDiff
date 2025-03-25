#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <iomanip>
#include <algorithm>
#include <cassert>
#include <sstream>

class FileReader {
private:
    const size_t BUFFER_SIZE = 4 * 1024;
    std::ifstream file_handle;
    int raw_pointer = 0;
    std::vector<char> raw_buffer;
    std::vector<char> buffer;
    char held_character = 0;
    bool has_held_character = false;
    bool eof_reached = false;

    void ingestData() {
        if (eof_reached) return;

        if (raw_pointer >= static_cast<int>(raw_buffer.size())) {
            raw_buffer.resize(BUFFER_SIZE);
            file_handle.read(raw_buffer.data(), BUFFER_SIZE);
            auto bytesRead = file_handle.gcount();

            if (bytesRead < 1) {
                eof_reached = true;
                raw_buffer.clear();  // Clear the buffer to avoid leftover data
            }
            else {
                raw_buffer.resize(bytesRead);
                raw_pointer = 0;
            }
        }
    }

    char nextByte() {
        ingestData();

        if (eof_reached && raw_pointer >= static_cast<int>(raw_buffer.size())) {
            return 0;
        }

        return raw_buffer[raw_pointer++];
    }

public:
    FileReader(const std::string& filename) {
        file_handle.open(filename, std::ios::binary);
        if (!file_handle.is_open()) {
            throw std::runtime_error("Failed to open file: " + filename);
        }
    }

    char peek(bool consume = false) {
        if (!has_held_character) {
            held_character = nextByte();
            has_held_character = true;
        }

        char c = held_character;

        if (consume) {
            has_held_character = false;
        }

        return c;
    }

    char read() {
        return peek(true);
    }

    std::vector<char> byteString(bool reset_buffer) {
        std::vector<char> data = buffer;

        if (reset_buffer) {
            buffer.clear();
        }

        return data;
    }

    bool isEOF() {
        return eof_reached && raw_pointer >= static_cast<int>(raw_buffer.size()) && !has_held_character;
    }

    void close() {
        file_handle.close();
    }

    std::vector<char> readUntil(const std::vector<char>& edgebytes = { '\n', '\r', '\0' }) {
        buffer.clear();  // Start with a clean buffer

        if (isEOF()) {
            return {};  // Return empty if already at EOF
        }

        // Read until we find an edge byte or EOF
        bool foundEdge = false;
        while (!isEOF() && !foundEdge) {
            char c = read();
            buffer.push_back(c);

            // Check if we hit an edge byte
            if (std::find(edgebytes.begin(), edgebytes.end(), c) != edgebytes.end()) {
                foundEdge = true;

                // Consume any consecutive edge bytes (e.g., \r\n)
                while (!isEOF()) {
                    char next = peek();
                    if (std::find(edgebytes.begin(), edgebytes.end(), next) != edgebytes.end()) {
                        buffer.push_back(read());
                    }
                    else {
                        break;
                    }
                }
            }
        }

        return byteString(true);
    }

    // Force read remaining data even without edge bytes
    std::vector<char> readRemaining() {
        buffer.clear();

        if (isEOF()) {
            return {};
        }

        while (!isEOF()) {
            buffer.push_back(read());
        }

        return byteString(true);
    }
};

class BytesPrinter {
private:
    size_t width;
    std::vector<char> to_process;
    std::vector<char> chunk;
    std::ofstream output_file;
    bool output_to_file;

    bool chomp() {
        size_t h_width = width / 2;
        chunk.clear();

        for (size_t i = 0; i < h_width && i < to_process.size(); i++) {
            chunk.push_back(to_process[i]);
        }

        to_process.erase(to_process.begin(), to_process.begin() + chunk.size());

        return !chunk.empty();
    }

    std::string chunkString() const {
        std::string chars;
        chars.reserve(chunk.size());

        for (char i : chunk) {
            if (i < 32 || i > 126) {
                chars.push_back('.');
            }
            else {
                chars.push_back(i);
            }
        }

        return chars;
    }

public:
    BytesPrinter(size_t w = 32) : width(w), output_to_file(false) {
        assert(width % 2 == 0 && "Width must be a multiple of two");
    }

    void setOutputFile(const std::string& filename) {
        output_file.open(filename, std::ios::out);
        if (!output_file.is_open()) {
            throw std::runtime_error("Failed to open output file: " + filename);
        }
        output_to_file = true;
    }

    std::vector<std::string> get_formatted_lines(const std::vector<char>& data_bytes) {
        to_process = data_bytes;
        std::string bar = "|";
        std::vector<std::string> lines;

        while (chomp()) {
            std::stringstream hex_stream;
            for (char c : chunk) {
                hex_stream << std::hex << std::setw(2) << std::setfill('0')
                    << static_cast<int>(static_cast<unsigned char>(c)) << " ";
            }

            std::string spaced = hex_stream.str();
            if (!spaced.empty()) {
                spaced.pop_back(); // Remove trailing space
            }

            std::string formatted = spaced;
            formatted.resize(width + width / 2 - 1, ' ');

            std::string cleansed = chunkString();
            cleansed.resize(width, ' ');

            lines.push_back(bar + " " + formatted + "  " + bar + "  " + cleansed + " " + bar);

            // Actually appears in case of multiline chunk
            bar = ":";
        }

        return lines;
    }

    void print(const std::vector<char>& data_bytes) {
        auto lines = get_formatted_lines(data_bytes);
        for (const auto& line : lines) {
            if (output_to_file) {
                output_file << line << std::endl;
            }
            else {
                std::cout << line << std::endl;
            }
        }
    }

    void close() {
        if (output_to_file) {
            output_file.close();
        }
    }
};

void showUsage(const char* programName) {
    std::cout << "Usage: " << programName << " <inputfile> [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -O <filename>  Write output to specified filename" << std::endl;
    std::cout << "  -o             Write output to <inputfile>.chunk" << std::endl;
    std::cout << "  --             Show this help message" << std::endl;
}

int main(int argc, char* argv[]) {
    // Show usage if no parameters given or -- option provided
    if (argc < 2 || (argc > 1 && std::string(argv[1]) == "--")) {
        showUsage(argv[0]);
        return 0;
    }

    std::string inputFilename = argv[1];
    std::string outputFilename = "";
    bool useOutputFile = false;

    // Parse command-line options
    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-O" && i + 1 < argc) {
            outputFilename = argv[++i];
            useOutputFile = true;
        }
        else if (arg == "-o") {
            outputFilename = inputFilename + ".chunk";
            useOutputFile = true;
        }
    }

    BytesPrinter bp;

    // Set output file if needed
    if (useOutputFile) {
        try {
            bp.setOutputFile(outputFilename);
        }
        catch (const std::exception& e) {
            std::cerr << e.what() << std::endl;
            return 1;
        }
    }

    try {
        FileReader fr(inputFilename);

        // Process file in chunks until no more edge bytes found
        while (true) {
            auto chunk = fr.readUntil();

            if (chunk.empty()) {
                break;
            }

            bp.print(chunk);
        }

        // Check if there's any remaining data without edge bytes
        auto remaining = fr.readRemaining();
        if (!remaining.empty()) {
            bp.print(remaining);
        }

        fr.close();
        bp.close();
    }
    catch (const std::exception& e) {
        std::cerr << "Error processing file " << inputFilename << ": " << e.what() << std::endl;
        return 1;
    }

    return 0;
}