// Chapter 16 - wavreader.cpp

#include "wavreader.h"

#include <fstream>
#include <iostream>
#include <iomanip>
#include <cstring>
#include <algorithm>

namespace
{
    uint16_t readU16LE(std::istream& in)
    {
        uint8_t b[2] = {};
        in.read(reinterpret_cast<char*>(b), 2);
        return static_cast<uint16_t>(b[0] | (b[1] << 8));
    }

    uint32_t readU32LE(std::istream& in)
    {
        uint8_t b[4] = {};
        in.read(reinterpret_cast<char*>(b), 4);
        return static_cast<uint32_t>(b[0])
             | (static_cast<uint32_t>(b[1]) <<  8)
             | (static_cast<uint32_t>(b[2]) << 16)
             | (static_cast<uint32_t>(b[3]) << 24);
    }

    bool readTag(std::istream& in, char out[5])
    {
        in.read(out, 4);
        out[4] = '\0';
        return in.gcount() == 4;
    }

    const char* formatName(int fmt)
    {
        switch (fmt)
        {
            case 1:      return "PCM integer";
            case 3:      return "IEEE float";
            case 6:      return "A-law";
            case 7:      return "mu-law";
            case 0xFFFE: return "EXTENSIBLE";
            default:     return "unknown";
        }
    }
}

WavFile WavReader::read(const std::string& path, std::string& error)
{
    WavFile result;
    error.clear();

    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        error = "Cannot open file: " + path;
        return result;
    }

    char tag[5] = {};
    if (!readTag(in, tag) || std::strcmp(tag, "RIFF") != 0)
    {
        error = "Not a RIFF file (found '" + std::string(tag) + "')";
        return result;
    }

    readU32LE(in);                          // RIFF size -- advisory only

    if (!readTag(in, tag) || std::strcmp(tag, "WAVE") != 0)
    {
        error = "Not a WAVE file (found '" + std::string(tag) + "')";
        return result;
    }

    // ---- walk the chunks ----
    bool           haveFmt    = false;
    bool           haveData   = false;
    std::streampos dataPos    = 0;
    uint32_t       dataSize   = 0;
    uint16_t       blockAlign = 0;

    while (in.good())
    {
        if (!readTag(in, tag))
            break;                          // clean end of file

        const uint32_t       chunkSize  = readU32LE(in);
        const std::streampos chunkStart = in.tellg();

        if (std::strcmp(tag, "fmt ") == 0)
        {
            result.audioFormat   = readU16LE(in);
            result.numChannels   = readU16LE(in);
            result.sampleRate    = static_cast<int>(readU32LE(in));
            readU32LE(in);                  // byte rate -- derivable, ignore
            blockAlign           = readU16LE(in);
            result.bitsPerSample = readU16LE(in);

            if (result.audioFormat == 0xFFFE && chunkSize >= 40)
            {
                readU16LE(in);              // cbSize
                readU16LE(in);              // valid bits per sample
                readU32LE(in);              // channel mask
                result.audioFormat = readU16LE(in);   // first 2 bytes of the GUID
            }

            haveFmt = true;
        }
        else if (std::strcmp(tag, "data") == 0)
        {
            dataPos  = chunkStart;
            dataSize = chunkSize;
            haveData = true;
        }
        // Everything else is skipped.

        std::streamoff advance = static_cast<std::streamoff>(chunkSize);
        if (chunkSize % 2 != 0)
            advance += 1;                   // pad byte, not counted in the size

        in.clear();                         // clear eof before seeking
        in.seekg(chunkStart + advance);

        if (in.peek() == EOF)
            break;
    }

    if (!haveFmt)  { error = "No 'fmt ' chunk found"; return result; }
    if (!haveData) { error = "No 'data' chunk found"; return result; }

    // Some recorders write 0xFFFFFFFF meaning "to end of file".
    if (dataSize == 0xFFFFFFFFu)
    {
        in.clear();
        in.seekg(0, std::ios::end);
        dataSize = static_cast<uint32_t>(in.tellg() - dataPos);
    }

    const int bytesPerSample = result.bitsPerSample / 8;
    if (bytesPerSample <= 0 || result.numChannels <= 0)
    {
        error = "Invalid format: " + std::to_string(result.bitsPerSample)
              + " bits, " + std::to_string(result.numChannels) + " channels";
        return WavFile{};
    }

    if (blockAlign == 0)
        blockAlign = static_cast<uint16_t>(bytesPerSample * result.numChannels);

    result.numFrames = dataSize / blockAlign;

    result.channels.assign(static_cast<size_t>(result.numChannels),
                           std::vector<float>(result.numFrames, 0.0f));

    in.clear();
    in.seekg(dataPos);

    std::vector<uint8_t> raw(dataSize);
    in.read(reinterpret_cast<char*>(raw.data()), dataSize);

    const size_t actuallyRead = static_cast<size_t>(in.gcount());
    if (actuallyRead < dataSize)
    {
        // Truncated file: keep what we got rather than failing outright.
        result.numFrames = actuallyRead / blockAlign;
        for (auto& ch : result.channels)
            ch.resize(result.numFrames);
    }

    for (size_t frame = 0; frame < result.numFrames; ++frame)
    {
        for (int c = 0; c < result.numChannels; ++c)
        {
            const size_t offset = frame * blockAlign
                                + static_cast<size_t>(c * bytesPerSample);

            float s = 0.0f;

            if (result.audioFormat == 3)                     // IEEE float
            {
                if (result.bitsPerSample == 32)
                {
                    float f = 0.0f;
                    std::memcpy(&f, &raw[offset], 4);        // NOT reinterpret_cast
                    s = f;
                }
                else if (result.bitsPerSample == 64)
                {
                    double d = 0.0;
                    std::memcpy(&d, &raw[offset], 8);
                    s = static_cast<float>(d);
                }
            }
            else                                             // integer PCM
            {
                switch (result.bitsPerSample)
                {
                    case 8:     // 8-bit WAV is UNSIGNED: 128 is silence
                        s = (static_cast<float>(raw[offset]) - 128.0f) / 128.0f;
                        break;

                    case 16:
                    {
                        const int16_t v = static_cast<int16_t>(
                            raw[offset] | (raw[offset + 1] << 8));
                        s = static_cast<float>(v) / 32768.0f;
                        break;
                    }

                    case 24:
                    {
                        // Shift into the top 24 bits then arithmetic-shift back
                        // down, letting the hardware sign-extend.
                        const int32_t v =
                              (static_cast<int32_t>(raw[offset    ]) <<  8)
                            | (static_cast<int32_t>(raw[offset + 1]) << 16)
                            | (static_cast<int32_t>(raw[offset + 2]) << 24);
                        s = static_cast<float>(v >> 8) / 8388608.0f;
                        break;
                    }

                    case 32:
                    {
                        int32_t v = 0;
                        std::memcpy(&v, &raw[offset], 4);
                        s = static_cast<float>(static_cast<double>(v) / 2147483648.0);
                        break;
                    }

                    default:
                        error = "Unsupported bit depth: "
                              + std::to_string(result.bitsPerSample);
                        return WavFile{};
                }
            }

            result.channels[static_cast<size_t>(c)][frame] = s;
        }
    }

    return result;
}

bool WavReader::printInfo(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        std::cerr << "Cannot open " << path << "\n";
        return false;
    }

    in.seekg(0, std::ios::end);
    const long long fileSize = static_cast<long long>(in.tellg());
    in.seekg(0);

    std::cout << "File: " << path << "   (" << fileSize << " bytes)\n\n";
    std::cout << "  offset       id      size   notes\n";
    std::cout << "  ------------------------------------------------------------------\n";

    char tag[5] = {};
    if (!readTag(in, tag) || std::strcmp(tag, "RIFF") != 0)
    {
        std::cerr << "  Not a RIFF file.\n";
        return false;
    }

    const uint32_t riffSize = readU32LE(in);
    char form[5] = {};
    readTag(in, form);

    std::cout << "  " << std::setw(6) << 0 << "    RIFF " << std::setw(9) << riffSize
              << "   form type: " << form << "\n";

    int fmtTag = 0, channels = 0, bits = 0, rate = 0;
    uint16_t blockAlign = 0;
    uint32_t byteRate = 0;
    long long dataOffset = -1;
    uint32_t dataSize = 0;

    while (in.good())
    {
        if (!readTag(in, tag)) break;

        const uint32_t       chunkSize  = readU32LE(in);
        const std::streampos chunkStart = in.tellg();

        std::cout << "  " << std::setw(6) << static_cast<long long>(chunkStart) - 8
                  << "    " << std::setw(4) << std::left << tag << std::right
                  << " " << std::setw(9) << chunkSize << "   ";

        if (std::strcmp(tag, "fmt ") == 0)
        {
            fmtTag   = readU16LE(in);
            channels = readU16LE(in);
            rate     = static_cast<int>(readU32LE(in));
            byteRate = readU32LE(in);
            blockAlign = readU16LE(in);
            bits     = readU16LE(in);

            std::cout << formatName(fmtTag) << ", " << channels << " ch, "
                      << rate << " Hz, " << bits << " bit";
        }
        else if (std::strcmp(tag, "data") == 0)
        {
            dataOffset = static_cast<long long>(chunkStart);
            dataSize   = chunkSize;
            const int ba = (blockAlign != 0) ? blockAlign : 1;
            std::cout << (chunkSize / static_cast<uint32_t>(ba)) << " frames, "
                      << std::fixed << std::setprecision(3)
                      << (rate > 0 ? static_cast<double>(chunkSize / ba) / rate : 0.0)
                      << " s";
        }
        else
        {
            std::cout << "(skipped)";
        }
        std::cout << "\n";

        std::streamoff advance = static_cast<std::streamoff>(chunkSize);
        if (chunkSize % 2 != 0) advance += 1;

        in.clear();
        in.seekg(chunkStart + advance);
        if (in.peek() == EOF) break;
    }

    std::cout << "  ------------------------------------------------------------------\n\n";
    std::cout << "  Format         : " << formatName(fmtTag) << "\n";
    std::cout << "  Channels       : " << channels << "\n";
    std::cout << "  Sample rate    : " << rate << " Hz\n";
    std::cout << "  Bit depth      : " << bits << "\n";
    std::cout << "  Data starts at : offset " << dataOffset
              << (dataOffset == 44 ? "" : "       <-- NOT 44") << "\n";

    const uint32_t computedByteRate = static_cast<uint32_t>(rate * channels * bits / 8);
    std::cout << "  Byte rate      : " << byteRate
              << (byteRate == computedByteRate ? "  (matches computed)"
                                               : "  (MISMATCH!)") << "\n";

    const uint16_t computedAlign = static_cast<uint16_t>(channels * bits / 8);
    std::cout << "  Block align    : " << blockAlign
              << (blockAlign == computedAlign ? "  (matches computed)"
                                              : "  (MISMATCH!)") << "\n\n";

    std::cout << "  Declared RIFF size : " << riffSize << "\n";
    std::cout << "  Actual file size   : " << fileSize
              << (static_cast<long long>(riffSize) + 8 == fileSize
                  ? "  (= declared + 8)  OK" : "  (MISMATCH)") << "\n";

    (void)dataSize;
    return true;
}
