#include <assets/AssetBlob.h>
#include <assets/AssetFileIo.h>
#include <assets/AssetId.h>
#include <assets/AssetManifest.h>

#include <core/log/Log.h>

#include <cstddef>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    std::string HexId(mts::AssetId id)
    {
        std::ostringstream out;
        out << std::hex << std::setfill('0') << std::setw(16) << id.value;
        return out.str();
    }

    std::string NormalizeRoot(std::string root)
    {
        while (root.size() > 1 && (root.back() == '/' || root.back() == '\\'))
            root.pop_back();

        if (root.size() >= 2 && root[0] == '.' && (root[1] == '/' || root[1] == '\\'))
            root.erase(0, 2);

        return root;
    }

    bool IsShaderSource(const std::filesystem::path &path)
    {
        return path.extension() == ".slang";
    }

    // A cooked blob is only current if it was written by *this* cooker: an mtime
    // test alone means bumping kRawAssetContentVersion (or the blob format) skips
    // every file as up to date while the manifest is rewritten with the new
    // version, so every blob on disk disagrees with it and every runtime load
    // fails. Reading the 32-byte header is cheap next to recooking the asset.
    bool BlobMatchesCurrentFormat(const std::filesystem::path &cooked)
    {
        const std::optional<std::vector<std::byte>> prefix =
            mts::ReadFilePrefix(cooked, sizeof(mts::AssetBlobHeader));
        if (!prefix.has_value())
            return false;

        mts::AssetBlobHeader header{};
        std::memcpy(&header, prefix->data(), sizeof(header));

        return header.magic == mts::kAssetBlobMagic &&
               header.formatVersion == mts::kAssetBlobFormatVersion &&
               header.typeTag == mts::kRawAssetTypeTag &&
               header.contentVersion == mts::kRawAssetContentVersion;
    }

    bool IsUpToDate(const std::filesystem::path &source, const std::filesystem::path &cooked)
    {
        std::error_code ec;
        if (!std::filesystem::exists(cooked, ec))
            return false;

        if (!BlobMatchesCurrentFormat(cooked))
            return false;

        const auto sourceTime = std::filesystem::last_write_time(source, ec);
        if (ec)
            return false;

        const auto cookedTime = std::filesystem::last_write_time(cooked, ec);
        if (ec)
            return false;

        // strictly newer: an edit landing in the same filesystem timestamp tick as
        // the previous cook would otherwise read as current and never be recooked
        return cookedTime > sourceTime;
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

            if (IsShaderSource(entry.path()))
                continue;

            const std::filesystem::path relative = std::filesystem::relative(entry.path(), rootPath);
            const std::string idSource = (std::filesystem::path(NormalizeRoot(root)) / relative).generic_string();
            const mts::AssetId id = mts::MakeAssetId(idSource);

            const std::string fileName = HexId(id) + ".blob";
            const std::filesystem::path blobPath = outPath / fileName;

            if (IsUpToDate(entry.path(), blobPath))
            {
                MTS_LOG_INFO("up to date {} -> {}", idSource, fileName);
                cooked.push_back(CookedFile{id, fileName});
                continue;
            }

            const std::optional<std::vector<std::byte>> bytes = mts::ReadFileBytes(entry.path());
            if (!bytes.has_value())
            {
                MTS_LOG_ERROR("AssetCooker: failed to read {}", entry.path().string());
                mts::FlushLog();
                return 1;
            }

            const std::vector<std::byte> blob =
                mts::BuildAssetBlob(mts::kRawAssetTypeTag, mts::kRawAssetContentVersion, *bytes);
            if (!mts::WriteFileBytes(blobPath, blob))
            {
                MTS_LOG_ERROR("AssetCooker: failed to write {}", blobPath.string());
                mts::FlushLog();
                return 1;
            }

            MTS_LOG_INFO("cooked {} -> {}", idSource, fileName);
            cooked.push_back(CookedFile{id, fileName});
        }
    }

    std::vector<mts::AssetManifestSourceEntry> entries;
    entries.reserve(cooked.size());
    for (const CookedFile &file : cooked)
        entries.push_back(mts::AssetManifestSourceEntry{
            {file.id, mts::kRawAssetTypeTag, mts::kRawAssetContentVersion}, file.fileName});

    const std::vector<std::byte> manifestBlob = mts::BuildAssetManifestBlob(entries);
    if (!mts::WriteFileBytes(outPath / "manifest.blob", manifestBlob))
    {
        MTS_LOG_ERROR("AssetCooker: failed to write manifest.blob");
        mts::FlushLog();
        return 1;
    }

    MTS_LOG_INFO("AssetCooker: cooked {} assets into {}", cooked.size(), outDir);
    mts::FlushLog();
    return 0;
}
