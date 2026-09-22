// Chapter 8 - bytes.cpp
// Writes a small binary file with little-endian fields, then hex-dumps it
// and reads the header back.
// Build:  g++ -std=c++17 -Wall -Wextra -O2 bytes.cpp -o bytes.exe
// Run:    ./bytes.exe

#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <string>
#include <cstdint>

// ---------------------------------------------------------------- writing

void writeU16LE(std::ostream& out, uint16_t value)
{
    out.put(static_cast<char>( value       & 0xFF));   // low byte first
    out.put(static_cast<char>((value >> 8) & 0xFF));   // then high byte
}

void writeU32LE(std::ostream& out, uint32_t value)
{
    out.put(static_cast<char>( value        & 0xFF));
    out.put(static_cast<char>((value >>  8) & 0xFF));
    out.put(static_cast<char>((value >> 16) & 0xFF));
    out.put(static_cast<char>((value >> 24) & 0xFF));
}

// Write four ASCII characters, as RIFF chunk identifiers require.
void writeTag(std::ostream& out, const char* tag)
{
    out.write(tag, 4);
}

// ---------------------------------------------------------------- dumping

void hexDump(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        std::cerr << "Could not open " << path << "\n";
        return;
    }

    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());

    std::cout << "File: " << path << "  (" << bytes.size() << " bytes)\n\n";

    for (size_t offset = 0; offset < bytes.size(); offset += 16)
    {
        std::cout << std::setw(8) << std::setfill('0') << std::hex
                  << offset << "  " << std::setfill(' ');

        for (size_t i = 0; i < 16; ++i)
        {
            if (offset + i < bytes.size())
                std::cout << std::setw(2) << std::setfill('0') << std::hex
                          << static_cast<int>(bytes[offset + i]) << ' ';
            else
                std::cout << "   ";

            if (i == 7)
                std::cout << ' ';
        }

        std::cout << " |";
        for (size_t i = 0; i < 16 && offset + i < bytes.size(); ++i)
        {
            const uint8_t b = bytes[offset + i];
            std::cout << (b >= 32 && b < 127 ? static_cast<char>(b) : '.');
        }
        std::cout << "|\n";
    }

    std::cout << std::dec << std::setfill(' ') << "\n";
}

// ---------------------------------------------------------------- main

int main()
{
    const std::string path = "bytes_demo.bin";

    {
        std::ofstream out(path, std::ios::binary);   // BINARY -- always
        if (!out)
        {
            std::cerr << "Could not create " << path << "\n";
            return 1;
        }

        writeTag(out, "DEMO");              // 4 bytes of ASCII
        writeU32LE(out, 44100);             // sample rate
        writeU16LE(out, 2);                 // channel count
        writeU16LE(out, 16);                // bits per sample

        const int16_t samples[6] = { 0, 16384, 32767, 0, -16384, -32768 };
        out.write(reinterpret_cast<const char*>(samples), sizeof(samples));
    }   // `out` destroyed here -> flushed and closed

    hexDump(path);

    std::ifstream in(path, std::ios::binary);
    char tag[5] = {};
    in.read(tag, 4);

    auto readU16LE = [&in]() -> uint16_t {
        const uint8_t b0 = static_cast<uint8_t>(in.get());
        const uint8_t b1 = static_cast<uint8_t>(in.get());
        return static_cast<uint16_t>(b0 | (b1 << 8));
    };
    auto readU32LE = [&in]() -> uint32_t {
        const uint8_t b0 = static_cast<uint8_t>(in.get());
        const uint8_t b1 = static_cast<uint8_t>(in.get());
        const uint8_t b2 = static_cast<uint8_t>(in.get());
        const uint8_t b3 = static_cast<uint8_t>(in.get());
        return static_cast<uint32_t>(b0)
             | (static_cast<uint32_t>(b1) <<  8)
             | (static_cast<uint32_t>(b2) << 16)
             | (static_cast<uint32_t>(b3) << 24);
    };

    const uint32_t rate     = readU32LE();
    const uint16_t channels = readU16LE();
    const uint16_t bits     = readU16LE();

    std::cout << "Read back:\n";
    std::cout << "  tag      = " << tag << "\n";
    std::cout << "  rate     = " << rate << "\n";
    std::cout << "  channels = " << channels << "\n";
    std::cout << "  bits     = " << bits << "\n";

    return 0;
}
