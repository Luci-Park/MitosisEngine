#include <assets/AssetBlob.h>
#include <assets/AssetId.h>
#include <assets/AssetManifest.h>

#include <core/log/Log.h>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    std::optional<std::vector<std::byte>> ReadFileBytes(const std::filesystem::path &path)
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file)
            return std::nullopt;

        const std::streamsize size = file.tellg();
        if (size < 0)
            return std::nullopt;
        file.seekg(0);

        std::vector<std::byte> bytes(static_cast<std::size_t>(size));
        if (!bytes.empty() && !file.read(reinterpret_cast<char *>(bytes.data()), size))
            return std::nullopt;

        return bytes;
    }

    void WriteFileBytes(const std::filesystem::path &path, const std::vector<std::byte> &bytes)
    {
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }

    std::string HexId(mts::AssetId id)
    {
        std::ostringstream out;
        out << std::hex << std::setfill('0') << std::setw(16) << id.value;
        return out.str();
    }

    struct CookedFile
    {
        mts::AssetId id;
        std::string fileName;
    };
}

int main(int argc, char **argv)
{
    std::vector<std::string> sourceRoots;
    std::string outDir;

    for (int i = 1; i < argc; ++i)
    {
        const std::string_view arg = argv[i];
        if (arg == "--source" && i + 1 < argc)
        {
            sourceRoots.emplace_back(argv[++i]);
        }
        else if (arg == "--out" && i + 1 < argc)
        {
            outDir = argv[++i];
        }
        else
        {
            std::cerr << "AssetCooker: unrecognized argument " << arg << "\n";
            return 1;
        }
    }

    if (sourceRoots.empty() || outDir.empty())
    {
        std::cerr << "usage: AssetCooker --source <dir> [--source <dir> ...] --out <dir>\n";
        return 1;
    }

    mts::InitLog();

    const std::filesystem::path outPath = outDir;
    std::filesystem::create_directories(outPath);

    std::vector<CookedFile> cooked;

    for (const std::string &root : sourceRoots)
    {
        const std::filesystem::path rootPath = root;
        if (!std::filesystem::exists(rootPath))
        {
            MTS_LOG_WARN("AssetCooker: source root does not exist, skipping: {}", root);
            continue;
        }

        for (const std::filesystem::directory_entry &entry : std::filesystem::recursive_directory_iterator(rootPath))
        {
            if (!entry.is_regular_file())
                continue;

            const std::filesystem::path relative = std::filesystem::relative(entry.path(), rootPath);
            const std::string idSource = root + "/" + relative.generic_string();
            const mts::AssetId id = mts::MakeAssetId(idSource);

            const std::optional<std::vector<std::byte>> bytes = ReadFileBytes(entry.path());
            if (!bytes.has_value())
            {
                MTS_LOG_ERROR("AssetCooker: failed to read {}", entry.path().string());
                mts::FlushLog();
                return 1;
            }

            const std::vector<std::byte> blob =
                mts::BuildAssetBlob(mts::kRawAssetTypeTag, mts::kRawAssetContentVersion, *bytes);
            const std::string fileName = HexId(id) + ".blob";
            WriteFileBytes(outPath / fileName, blob);

            MTS_LOG_INFO("cooked {} -> {}", idSource, fileName);
            cooked.push_back(CookedFile{id, fileName});
        }
    }

    std::vector<mts::AssetManifestSourceEntry> entries;
    entries.reserve(cooked.size());
    for (const CookedFile &file : cooked)
        entries.push_back(mts::AssetManifestSourceEntry{file.id, mts::kRawAssetTypeTag, mts::kRawAssetContentVersion,
                                                          file.fileName});

    const std::vector<std::byte> manifestBlob = mts::BuildAssetManifestBlob(entries);
    WriteFileBytes(outPath / "manifest.blob", manifestBlob);

    MTS_LOG_INFO("AssetCooker: cooked {} assets into {}", cooked.size(), outDir);
    mts::FlushLog();
    return 0;
}
