#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>

struct WAVHeader {
    char riff[4];
    uint32_t fileSize;
    char wav[4];
    char fmt[4];
    uint32_t fmtSize;
    uint16_t audioFormat;
    uint16_t numberOfChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
    char data[4];
    uint32_t dataSize;
};

struct WAVFile {
    WAVHeader header;
    std::vector<int16_t> samples;
};

WAVFile readWAVFile(const std::string& filePath) {
    WAVFile wavFile;
    std::ifstream file(filePath, std::ios::binary);
    
    if (!file) {
        std::cerr << "ERROR: Cannot open file!" << std::endl;
        return wavFile;
    }

    file.read(reinterpret_cast<char*>(&wavFile.header), sizeof(WAVHeader));

    if (std::strncmp(wavFile.header.riff, "RIFF", 4) != 0 ||
        std::strncmp(wavFile.header.wav, "WAVE", 4) != 0) {
        std::cerr << "ERROR: Not a valid WAV file!" << std::endl;
        return wavFile;
    }

    if (wavFile.header.audioFormat != 1) {
        std::cerr << "ERROR: Only PCM format is supported!" << std::endl;
        return wavFile;
    }

    if (wavFile.header.bitsPerSample != 16) {
        std::cerr << "ERROR: Only 16-bit audio is supported!" << std::endl;
        return wavFile;
    }

    size_t numSamples = wavFile.header.dataSize / sizeof(int16_t);
    wavFile.samples.resize(numSamples);
    file.read(reinterpret_cast<char*>(wavFile.samples.data()), wavFile.header.dataSize);
    file.close();

    return wavFile;
}

std::vector<uint8_t> readFile(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "ERROR: Can't open input file" << std::endl;
        return {};
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        std::cerr << "ERROR: Failed reading file" << std::endl;
        return {};
    }

    return buffer;
}

std::string getFileName(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    if (pos != std::string::npos) {
        return path.substr(pos + 1);
    }
    return path;
}

bool hideFile(WAVFile& wav, const std::string& filePath) {
    std::vector<uint8_t> fileData = readFile(filePath);
    
    if (fileData.empty()) {
        return false;
    }

    std::string fileName = getFileName(filePath);
    
    if (fileName.length() > 255) {
        std::cerr << "ERROR: Filename too long (max 255 chars)" << std::endl;
        return false;
    }

    // Calculate total bits needed:
    // 4 bytes for file size + 1 byte for filename length + filename + file data
    uint32_t fileSize = fileData.size();
    uint8_t nameLen = fileName.length();
    size_t totalBits = (4 + 1 + nameLen + fileSize) * 8;

    if (totalBits > wav.samples.size()) {
        std::cerr << "ERROR: File too large! Need " << totalBits / 8 
                  << " bytes but only have " << wav.samples.size() / 8 << " bytes available." << std::endl;
        return false;
    }

    size_t idx = 0;

    // Embed file size (32 bits)
    for (int i = 0; i < 32; i++) {
        int bit = (fileSize >> i) & 1;
        if (bit) {
            wav.samples[idx] |= 1;
        } else {
            wav.samples[idx] &= ~1;
        }
        idx++;
    }

    // Embed filename length (8 bits)
    for (int i = 0; i < 8; i++) {
        int bit = (nameLen >> i) & 1;
        if (bit) {
            wav.samples[idx] |= 1;
        } else {
            wav.samples[idx] &= ~1;
        }
        idx++;
    }

    // Embed filename
    for (size_t i = 0; i < nameLen; i++) {
        uint8_t byte = fileName[i];
        for (int bitPos = 0; bitPos < 8; bitPos++) {
            int bit = (byte >> bitPos) & 1;
            if (bit) {
                wav.samples[idx] |= 1;
            } else {
                wav.samples[idx] &= ~1;
            }
            idx++;
        }
    }

    // Embed file data
    for (size_t i = 0; i < fileData.size(); i++) {
        uint8_t byte = fileData[i];
        for (int bitPos = 0; bitPos < 8; bitPos++) {
            int bit = (byte >> bitPos) & 1;
            if (bit) {
                wav.samples[idx] |= 1;
            } else {
                wav.samples[idx] &= ~1;
            }
            idx++;
        }
    }

    std::cout << "SUCCESS: Hidden " << fileSize << " bytes (" << fileName << ")" << std::endl;
    return true;
}

bool extractFile(const WAVFile& wav, const std::string& outputDir) {
    size_t idx = 0;

    // Extract file size
    uint32_t fileSize = 0;
    for (int i = 0; i < 32; i++) {
        int bit = wav.samples[idx] & 1;
        fileSize |= (bit << i);
        idx++;
    }

    if (fileSize == 0 || fileSize > wav.samples.size() / 8) {
        std::cerr << "ERROR: Invalid file size. No hidden file or corrupted data." << std::endl;
        return false;
    }

    // Extract filename length
    uint8_t nameLen = 0;
    for (int i = 0; i < 8; i++) {
        int bit = wav.samples[idx] & 1;
        nameLen |= (bit << i);
        idx++;
    }

    if (nameLen == 0 || nameLen > 255) {
        std::cerr << "ERROR: Invalid filename length." << std::endl;
        return false;
    }

    // Extract filename
    std::string fileName;
    for (size_t i = 0; i < nameLen; i++) {
        uint8_t byte = 0;
        for (int bitPos = 0; bitPos < 8; bitPos++) {
            int bit = wav.samples[idx] & 1;
            byte |= (bit << bitPos);
            idx++;
        }
        fileName += static_cast<char>(byte);
    }

    // Extract file data
    std::vector<uint8_t> extractedData(fileSize);
    for (size_t i = 0; i < fileSize; i++) {
        uint8_t byte = 0;
        for (int bitPos = 0; bitPos < 8; bitPos++) {
            int bit = wav.samples[idx] & 1;
            byte |= (bit << bitPos);
            idx++;
        }
        extractedData[i] = byte;
    }

    // Create output path
    std::string outputPath = outputDir;
    if (!outputPath.empty() && outputPath.back() != '/' && outputPath.back() != '\\') {
        outputPath += "/";
    }
    outputPath += fileName;

    // Write to file
    std::ofstream outFile(outputPath, std::ios::binary);
    if (!outFile) {
        std::cerr << "ERROR: Can't create output file" << std::endl;
        return false;
    }

    outFile.write(reinterpret_cast<const char*>(extractedData.data()), fileSize);
    outFile.close();

    std::cout << "SUCCESS: Extracted " << fileSize << " bytes to " << outputPath << std::endl;
    return true;
}

void writeWAVFile(const std::string& filePath, const WAVFile& wav) {
    std::ofstream file(filePath, std::ios::binary);
    if (!file) {
        std::cerr << "ERROR: Cannot create file!" << std::endl;
        return;
    }

    file.write(reinterpret_cast<const char*>(&wav.header), sizeof(WAVHeader));
    file.write(reinterpret_cast<const char*>(wav.samples.data()), wav.header.dataSize);
    file.close();

    std::cout << "SUCCESS: WAV file written" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "ERROR: Missing operation argument" << std::endl;
        return 1;
    }

    std::string operation = argv[1];

    if (operation == "hide") {
        if (argc != 5) {
            std::cerr << "ERROR: Usage: program hide <wav_file> <file_to_hide> <output_wav>" << std::endl;
            return 1;
        }

        std::string wavPath = argv[2];
        std::string filePath = argv[3];
        std::string outputPath = argv[4];

        WAVFile wav = readWAVFile(wavPath);
        if (wav.samples.empty()) {
            return 1;
        }

        if (hideFile(wav, filePath)) {
            writeWAVFile(outputPath, wav);
            return 0;
        }
        return 1;

    } else if (operation == "extract") {
        if (argc != 4) {
            std::cerr << "ERROR: Usage: program extract <wav_file> <output_directory>" << std::endl;
            return 1;
        }

        std::string wavPath = argv[2];
        std::string outputDir = argv[3];

        WAVFile wav = readWAVFile(wavPath);
        if (wav.samples.empty()) {
            return 1;
        }

        if (extractFile(wav, outputDir)) {
            return 0;
        }
        return 1;

    } else {
        std::cerr << "ERROR: Unknown operation. Use 'hide' or 'extract'" << std::endl;
        return 1;
    }

    return 0;
}