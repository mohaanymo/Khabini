#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <bitset> 


struct WAVHeader {
    // RIFF Header
    char riff[4];
    uint32_t fileSize;
    char wav[4];

    // Format Chunk
    char fmt[4];
    uint32_t fmtSize;
    uint16_t audioFormat;
    uint16_t numberOfChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;

    // Data Chunk
    char data[4];
    uint32_t dataSize;

};

struct WAVFile {
    WAVHeader header;
    std::vector<int16_t> samples;  // Audio samples (16-bit signed integers)
};

WAVFile readWAVFile(const std::string& filePath) {
    WAVFile wavFile;

    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        std::cerr << "Cannot open file!" << std::endl;
        return wavFile;
    }

    // Read header
    file.read(reinterpret_cast<char*>(&wavFile.header), sizeof(WAVHeader));

    // Validate
    if (std::strncmp(wavFile.header.riff, "RIFF", 4) != 0 ||
        std::strncmp(wavFile.header.wav, "WAVE", 4) != 0) {
        std::cerr << "Not a valid WAV file!" << std::endl;
        return wavFile;
    }

    if (wavFile.header.audioFormat != 1) {
        std::cerr << "Only PCM format is supported!" << std::endl;
        return wavFile;
    }

    if (wavFile.header.bitsPerSample != 16) {
        std::cerr << "Only 16-bit audio is supported!" << std::endl;
        return wavFile;
    }

    // Calculate number of samples
    size_t numSamples = wavFile.header.dataSize / sizeof(int16_t);
    wavFile.samples.resize(numSamples);

    // Read all audio samples
    file.read(reinterpret_cast<char*>(wavFile.samples.data()),
        wavFile.header.dataSize);

    file.close();

    std::cout << "Successfully read " << numSamples << " samples" << std::endl;

    return wavFile;
}

void displayWAVInfo(const WAVFile& wav) {
    std::cout << "=== WAV File Information ===" << std::endl;
    std::cout << "Channels: " << wav.header.numberOfChannels << std::endl;
    std::cout << "Sample Rate: " << wav.header.sampleRate << " Hz" << std::endl;
    std::cout << "Bits Per Sample: " << wav.header.bitsPerSample << std::endl;
    std::cout << "Total Samples: " << wav.samples.size() << std::endl;

    float duration = (float)wav.header.dataSize / wav.header.byteRate;
    std::cout << "Duration: " << duration << " seconds" << std::endl;

    std::cout << "\nFirst 10 samples: ";
    for (size_t i = 0; i < 10 && i < wav.samples.size(); i++) {
        std::cout << wav.samples[i] << " ";
    }
    std::cout << std::endl;
}

// Function to hide a message in the audio samples
void hideMessage(WAVFile& wav, const std::string& message) {
    // Calculate how many bits we need to hide
    size_t messageBits = message.size() * 8;  // Each character = 8 bits

    // Check if we have enough samples
    if (messageBits > wav.samples.size()) {
        std::cerr << "Error: Message too long for this audio file!" << std::endl;
        return;
    }


    size_t sampleIndex = 0; 

    for (size_t i = 0; i < message.size(); i++) {
        char c = message[i];

        // Loop through each bit in the character (8 bits per character)
        for (int bitPos = 0; bitPos < 8; bitPos++) {

            // This expression >> shifts the binary to right N number of times
            // Example: c = 10011, bitPos = 3, c >> bitPos = 00100
            int bit = (c >> bitPos) & 1;

            // Modify the LSB of the current sample
            if (bit == 1) {
                // Set LSB to 1 with | 1
                wav.samples[sampleIndex] = wav.samples[sampleIndex] | 1;
            }
            else {
                // This & ~1 clears LSB
                wav.samples[sampleIndex] = wav.samples[sampleIndex] & ~1;
            }

            sampleIndex++;
        }
    }

    std::cout << "Message hidden successfully!" << std::endl;
}

// Function to extract a message from the audio samples
std::string extractMessage(const WAVFile& wav, size_t messageLength) {
    std::string message = "";

    size_t sampleIndex = 0; 

    for (size_t i = 0; i < messageLength; i++) {
        char c = 0;  // Start with empty character

        for (int bitPos = 0; bitPos < 8; bitPos++) {
            int bit = wav.samples[sampleIndex] & 1;


            c = c | (bit << bitPos);

            sampleIndex++; 
        }

        message += c;
    }

    return message;
}

// Function to write WAV file back to disk
void writeWAVFile(const std::string& filePath, const WAVFile& wav) {
    std::ofstream file(filePath, std::ios::binary);

    if (!file) {
        std::cerr << "Cannot create file!" << std::endl;
        return;
    }

    // Write header
    file.write(reinterpret_cast<const char*>(&wav.header), sizeof(WAVHeader));

    // Write samples
    file.write(reinterpret_cast<const char*>(wav.samples.data()),
        wav.header.dataSize);

    file.close();

    std::cout << "WAV file written successfully!" << std::endl;
}

int main() {
    // Read the original WAV file
    WAVFile wav = readWAVFile("test.wav");

    if (wav.samples.empty()) {
        std::cerr << "Failed to read WAV file!" << std::endl;
        return 1;
    }

    std::string secretMessage = "mohamed";
    hideMessage(wav, secretMessage);
    writeWAVFile("output.wav", wav);
    WAVFile modifiedWav = readWAVFile("output.wav");
    std::string extracted = extractMessage(modifiedWav, secretMessage.size());

    std::cout << "\nExtracted message: \"" << extracted << "\"" << std::endl;

    if (extracted == secretMessage) {
        std::cout << "SUCCESS! Message matches!" << std::endl;
    }
    else {
        std::cout << "ERROR! Message doesn't match!" << std::endl;
    }

    return 0;
}