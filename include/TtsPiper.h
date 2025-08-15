#pragma once

#include <string>
#include <vector>
#include <cstdint>

class TtsPiper {
public:
    TtsPiper(const std::string& modelPath, const std::string& piperBin = "piper");
    bool isAvailable() const;
    std::vector<int16_t> synthesize(const std::string& text, double& sampleRate);
    const std::string& lastError() const;

private:
    std::string bin_, model_;
    mutable std::string lastErr_;
};
