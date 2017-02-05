#pragma once

#include <vector>
#include <fstream>

namespace file_helpers
{
    static std::vector<unsigned char> readFile(const std::string& fName) {
        std::ifstream file(fName, std::ios::ate | std::ios::binary);

        if (!file.is_open())
            return std::vector<unsigned char>();

        size_t fSize = (size_t)file.tellg();
        std::vector<unsigned char> buff(fSize);

        file.seekg(0);
        file.read((char*)buff.data(), buff.size());
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
    
    template<typename T>
    void encrypt(std::vector<T>& data)
    {
        static const std::string encryption_key = "sAfd@;sa34BY6fd:R4";
        const size_t keySize = encryption_key.size();

        for (size_t i = 0; i < data.size(); ++i)
        {
            data[i] ^= encryption_key[i % keySize];
        }
    }

    template<typename T>
    void decrypt(std::vector<T>& data)
    {
        encrypt<T>(data); // reversible
    }
}