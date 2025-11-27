#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>

// WAV File structures and functions
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

// File dialog functions
std::string openFileDialog(const char* filter, const char* title) {
    char filename[MAX_PATH] = "";
    
    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = title;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    
    if (GetOpenFileName(&ofn)) {
        return std::string(filename);
    }
    return "";
}

std::string saveFileDialog(const char* filter, const char* title) {
    char filename[MAX_PATH] = "";
    
    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = title;
    ofn.lpstrDefExt = "wav";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    
    if (GetSaveFileName(&ofn)) {
        return std::string(filename);
    }
    return "";
}

std::string selectFolderDialog() {
    char folder[MAX_PATH] = "";
    
    BROWSEINFO bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.lpszTitle = "Select Output Folder";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    
    LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
    
    if (pidl != NULL) {
        if (SHGetPathFromIDList(pidl, folder)) {
            CoTaskMemFree(pidl);
            return std::string(folder);
        }
        CoTaskMemFree(pidl);
    }
    return "";
}

WAVFile readWAVFile(const std::string& filePath) {
    WAVFile wavFile;
    std::ifstream file(filePath, std::ios::binary);
    
    if (!file) {
        return wavFile;
    }

    file.read(reinterpret_cast<char*>(&wavFile.header), sizeof(WAVHeader));

    if (std::strncmp(wavFile.header.riff, "RIFF", 4) != 0 ||
        std::strncmp(wavFile.header.wav, "WAVE", 4) != 0) {
        return wavFile;
    }

    if (wavFile.header.audioFormat != 1 || wavFile.header.bitsPerSample != 16) {
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
        return {};
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
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

bool hideFile(WAVFile& wav, const std::string& filePath, std::string& errorMsg) {
    std::vector<uint8_t> fileData = readFile(filePath);
    
    if (fileData.empty()) {
        errorMsg = "Cannot read file to hide";
        return false;
    }

    std::string fileName = getFileName(filePath);
    
    if (fileName.length() > 255) {
        errorMsg = "Filename too long (max 255 chars)";
        return false;
    }

    uint32_t fileSize = fileData.size();
    uint8_t nameLen = fileName.length();
    size_t totalBits = (4 + 1 + nameLen + fileSize) * 8;

    if (totalBits > wav.samples.size()) {
        errorMsg = "File too large for this WAV file!";
        return false;
    }

    size_t idx = 0;

    for (int i = 0; i < 32; i++) {
        int bit = (fileSize >> i) & 1;
        if (bit) {
            wav.samples[idx] |= 1;
        } else {
            wav.samples[idx] &= ~1;
        }
        idx++;
    }

    for (int i = 0; i < 8; i++) {
        int bit = (nameLen >> i) & 1;
        if (bit) {
            wav.samples[idx] |= 1;
        } else {
            wav.samples[idx] &= ~1;
        }
        idx++;
    }

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

    return true;
}

bool extractFile(const WAVFile& wav, const std::string& outputDir, std::string& errorMsg, std::string& extractedFileName) {
    size_t idx = 0;

    uint32_t fileSize = 0;
    for (int i = 0; i < 32; i++) {
        int bit = wav.samples[idx] & 1;
        fileSize |= (bit << i);
        idx++;
    }

    if (fileSize == 0 || fileSize > wav.samples.size() / 8) {
        errorMsg = "No hidden file found or corrupted data";
        return false;
    }

    uint8_t nameLen = 0;
    for (int i = 0; i < 8; i++) {
        int bit = wav.samples[idx] & 1;
        nameLen |= (bit << i);
        idx++;
    }

    if (nameLen == 0 || nameLen > 255) {
        errorMsg = "Invalid filename length";
        return false;
    }

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

    extractedFileName = fileName;

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

    std::string outputPath = outputDir;
    if (!outputPath.empty() && outputPath.back() != '/' && outputPath.back() != '\\') {
        outputPath += "\\";
    }
    outputPath += fileName;

    std::ofstream outFile(outputPath, std::ios::binary);
    if (!outFile) {
        errorMsg = "Cannot create output file";
        return false;
    }

    outFile.write(reinterpret_cast<const char*>(extractedData.data()), fileSize);
    outFile.close();

    return true;
}

void writeWAVFile(const std::string& filePath, const WAVFile& wav) {
    std::ofstream file(filePath, std::ios::binary);
    if (!file) {
        return;
    }

    file.write(reinterpret_cast<const char*>(&wav.header), sizeof(WAVHeader));
    file.write(reinterpret_cast<const char*>(wav.samples.data()), wav.header.dataSize);
    file.close();
}

int main() {
    if (!glfwInit()) {
        return -1;
    }

    const char* glsl_version = "#version 330";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(700, 550, "Echocrypt", NULL, NULL);
    if (window == NULL) {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = NULL;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 8.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.WindowPadding = ImVec2(15, 15);
    style.FramePadding = ImVec2(8, 6);

    ImVec4* colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.12f, 0.12f, 0.18f, 1.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.19f, 0.19f, 0.25f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.54f, 0.71f, 0.98f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.64f, 0.81f, 1.00f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.44f, 0.61f, 0.88f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.19f, 0.19f, 0.25f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.54f, 0.71f, 0.98f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.64f, 0.81f, 1.00f, 1.00f);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    char hideWavPath[512] = "";
    char hideFilePath[512] = "";
    char outputWavPath[512] = "";
    char extractWavPath[512] = "";
    char extractOutputDir[512] = "";
    
    bool showSuccessPopup = false;
    bool showErrorPopup = false;
    std::string popupMessage = "";

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(700, 550));
        ImGui::Begin("Echocrypt Tool", NULL, 
                    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | 
                    ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

        ImGui::SetCursorPosY(20);
        ImGui::SetCursorPosX((700 - ImGui::CalcTextSize("Echocrypt").x) / 2);
        ImGui::TextColored(ImVec4(0.54f, 0.71f, 0.98f, 1.0f), "Echocrypt");
        
        ImGui::SetCursorPosX((700 - ImGui::CalcTextSize("Hide files inside audio files").x) / 2);
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Hide files inside audio files");
        
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::BeginTabBar("MainTabs")) {
            if (ImGui::BeginTabItem("Hide File")) {
                ImGui::Spacing();
                ImGui::Spacing();

                ImGui::Text("Audio File (WAV):");
                ImGui::SameLine(200);
                if (ImGui::Button("Browse##1", ImVec2(100, 0))) {
                    std::string path = openFileDialog("WAV Files\0*.wav\0All Files\0*.*\0", "Select WAV File");
                    if (!path.empty()) {
                        strncpy(hideWavPath, path.c_str(), 511);
                    }
                }
                ImGui::SetNextItemWidth(580);
                ImGui::InputText("##hideWav", hideWavPath, 512, ImGuiInputTextFlags_ReadOnly);

                ImGui::Spacing();
                ImGui::Text("File to Hide:");
                ImGui::SameLine(200);
                if (ImGui::Button("Browse##2", ImVec2(100, 0))) {
                    std::string path = openFileDialog("All Files\0*.*\0", "Select File to Hide");
                    if (!path.empty()) {
                        strncpy(hideFilePath, path.c_str(), 511);
                    }
                }
                ImGui::SetNextItemWidth(580);
                ImGui::InputText("##hideFile", hideFilePath, 512, ImGuiInputTextFlags_ReadOnly);

                ImGui::Spacing();
                ImGui::Text("Output Audio File:");
                ImGui::SameLine(200);
                if (ImGui::Button("Browse##3", ImVec2(100, 0))) {
                    std::string path = saveFileDialog("WAV Files\0*.wav\0All Files\0*.*\0", "Save Output WAV File");
                    if (!path.empty()) {
                        strncpy(outputWavPath, path.c_str(), 511);
                    }
                }
                ImGui::SetNextItemWidth(580);
                ImGui::InputText("##outputWav", outputWavPath, 512, ImGuiInputTextFlags_ReadOnly);

                ImGui::Spacing();
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::SetCursorPosX((700 - 150) / 2);
                if (ImGui::Button("Hide File", ImVec2(150, 40))) {
                    if (strlen(hideWavPath) == 0 || strlen(hideFilePath) == 0 || strlen(outputWavPath) == 0) {
                        popupMessage = "Please select all files!";
                        showErrorPopup = true;
                    } else {
                        WAVFile wav = readWAVFile(hideWavPath);
                        if (wav.samples.empty()) {
                            popupMessage = "Cannot read WAV file!";
                            showErrorPopup = true;
                        } else {
                            std::string errorMsg;
                            if (hideFile(wav, hideFilePath, errorMsg)) {
                                writeWAVFile(outputWavPath, wav);
                                popupMessage = "File hidden successfully!";
                                showSuccessPopup = true;
                                memset(hideWavPath, 0, 512);
                                memset(hideFilePath, 0, 512);
                                memset(outputWavPath, 0, 512);
                            } else {
                                popupMessage = errorMsg;
                                showErrorPopup = true;
                            }
                        }
                    }
                }

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Extract File")) {
                ImGui::Spacing();
                ImGui::Spacing();

                ImGui::Text("Audio File (with hidden data):");
                ImGui::SameLine(300);
                if (ImGui::Button("Browse##4", ImVec2(100, 0))) {
                    std::string path = openFileDialog("WAV Files\0*.wav\0All Files\0*.*\0", "Select WAV File");
                    if (!path.empty()) {
                        strncpy(extractWavPath, path.c_str(), 511);
                    }
                }
                ImGui::SetNextItemWidth(580);
                ImGui::InputText("##extractWav", extractWavPath, 512, ImGuiInputTextFlags_ReadOnly);

                ImGui::Spacing();
                ImGui::Text("Save Extracted File To:");
                ImGui::SameLine(300);
                if (ImGui::Button("Browse##5", ImVec2(100, 0))) {
                    std::string path = selectFolderDialog();
                    if (!path.empty()) {
                        strncpy(extractOutputDir, path.c_str(), 511);
                    }
                }
                ImGui::SetNextItemWidth(580);
                ImGui::InputText("##extractDir", extractOutputDir, 512, ImGuiInputTextFlags_ReadOnly);

                ImGui::Spacing();
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::SetCursorPosX((700 - 150) / 2);
                if (ImGui::Button("Extract File", ImVec2(150, 40))) {
                    if (strlen(extractWavPath) == 0 || strlen(extractOutputDir) == 0) {
                        popupMessage = "Please select all required items!";
                        showErrorPopup = true;
                    } else {
                        WAVFile wav = readWAVFile(extractWavPath);
                        if (wav.samples.empty()) {
                            popupMessage = "Cannot read WAV file!";
                            showErrorPopup = true;
                        } else {
                            std::string errorMsg;
                            std::string extractedFileName;
                            if (extractFile(wav, extractOutputDir, errorMsg, extractedFileName)) {
                                popupMessage = "File extracted: " + extractedFileName;
                                showSuccessPopup = true;
                                memset(extractWavPath, 0, 512);
                                memset(extractOutputDir, 0, 512);
                            } else {
                                popupMessage = errorMsg;
                                showErrorPopup = true;
                            }
                        }
                    }
                }

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::End();

        if (showSuccessPopup) {
            ImGui::OpenPopup("Success");
            showSuccessPopup = false;
        }

        if (ImGui::BeginPopupModal("Success", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%s", popupMessage.c_str());
            ImGui::Spacing();
            if (ImGui::Button("OK", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        if (showErrorPopup) {
            ImGui::OpenPopup("Error");
            showErrorPopup = false;
        }

        if (ImGui::BeginPopupModal("Error", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%s", popupMessage.c_str());
            ImGui::Spacing();
            if (ImGui::Button("OK", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.12f, 0.12f, 0.18f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}