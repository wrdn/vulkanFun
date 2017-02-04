#pragma once

#include <vector>
#include <fstream>

namespace file_helpers
{
    static std::vector<char> readFile(const std::string& fName) {
        std::ifstream file(fName, std::ios::ate | std::ios::binary);

        if (!file.is_open())
            return std::vector<char>();

        size_t fSize = (size_t)file.tellg();
        std::vector<char> buff(fSize);

        file.seekg(0);
        file.read(buff.data(), buff.size());
        file.close();

        return buff;
    };

    static void writeFile(const std::string& fName, const std::vector<unsigned char>& data)
    {
        std::ofstream file(fName, std::ios::binary);
        if (!file.is_open())
            return;

        file.write((const char*)data.data(), data.size());

        file.close();
    }
}