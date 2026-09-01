#include <assets/AssetCache.h>

#include <assets/AssetBlob.h>
#include <assets/AssetManifest.h>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cstring>
#include <fstream>
#include <string>
#include <type_traits>

namespace
{
    struct ScratchDir
    {
        std::filesystem::path path;

        ScratchDir()
        {
            static std::atomic<int> counter{0};
            path = std::filesystem::temp_directory_path() /
                   ("mitosis_assetcache_test_" + std::to_string(counter.fetch_add(1)));
            std::filesystem::create_directories(path);
        }

        ~ScratchDir()
        {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
        }

        ScratchDir(const ScratchDir &) = delete;
        ScratchDir &operator=(const ScratchDir &) = delete;
    };

    void WriteFile(const std::filesystem::path &path, const std::vector<std::byte> &bytes)
    {
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }

    std::vector<std::byte> MakeContent(std::string_view text)
    {
        std::vector<std::byte> bytes(text.size());
        std::memcpy(bytes.data(), text.data(), text.size());
        return bytes;
    }
}

TEST_CASE("AssetCache loads a cooked blob through the manifest", "[assets][cache]")
{
    ScratchDir dir;
    const mts::AssetId id = mts::MakeAssetId("greeting.raw");
    const std::vector<std::byte> content = MakeContent("hello asset cache");
    WriteFile(dir.path / "greeting.blob", mts::BuildAssetBlob(1, 1, content));

    const mts::AssetManifestSourceEntry sourceEntry{{id, 1, 1}, "greeting.blob"};
    const std::optional<mts::AssetManifest> manifest =
        mts::AssetManifest::Parse(mts::BuildAssetManifestBlob({&sourceEntry, 1}));
    REQUIRE(manifest.has_value());

    mts::AssetCache cache(&*manifest, dir.path);
    REQUIRE(cache.Get(id) == nullptr);

    const mts::AssetBlobView *view = cache.Load(id);
    REQUIRE(view != nullptr);
    REQUIRE(view->content.size() == content.size());
    CHECK(std::memcmp(view->content.data(), content.data(), content.size()) == 0);

    CHECK(cache.Get(id) == view);
    CHECK(cache.Load(id) == view);
}

TEST_CASE("AssetCache Load returns nullptr for an unknown id", "[assets][cache]")
{
    ScratchDir dir;
    const std::optional<mts::AssetManifest> manifest = mts::AssetManifest::Parse(mts::BuildAssetManifestBlob({}));
    REQUIRE(manifest.has_value());

    mts::AssetCache cache(&*manifest, dir.path);
    CHECK(cache.Load(mts::MakeAssetId("missing")) == nullptr);
}

TEST_CASE("AssetCache Load returns nullptr when the file is missing", "[assets][cache]")
{
    ScratchDir dir;
    const mts::AssetId id = mts::MakeAssetId("ghost.raw");
    const mts::AssetManifestSourceEntry sourceEntry{{id, 1, 1}, "ghost.blob"};
    const std::optional<mts::AssetManifest> manifest =
        mts::AssetManifest::Parse(mts::BuildAssetManifestBlob({&sourceEntry, 1}));
    REQUIRE(manifest.has_value());

    mts::AssetCache cache(&*manifest, dir.path);
    CHECK(cache.Load(id) == nullptr);
    CHECK(cache.Load(id) == nullptr);
}

TEST_CASE("AssetCache Load rejects a blob that does not match its manifest entry", "[assets][cache]")
{
    ScratchDir dir;
    const mts::AssetId id = mts::MakeAssetId("mismatch.raw");
    WriteFile(dir.path / "mismatch.blob", mts::BuildAssetBlob(1, 1, MakeContent("payload")));

    const mts::AssetManifestSourceEntry sourceEntry{{id, 2, 1}, "mismatch.blob"};
    const std::optional<mts::AssetManifest> manifest =
        mts::AssetManifest::Parse(mts::BuildAssetManifestBlob({&sourceEntry, 1}));
    REQUIRE(manifest.has_value());

    mts::AssetCache cache(&*manifest, dir.path);
    CHECK(cache.Load(id) == nullptr);
}

TEST_CASE("AssetCache and its entries are move-only", "[assets][cache]")
{
    // AssetCacheEntry::view.content spans that entry's own raw buffer, so a copy
    // would allocate a fresh buffer and leave the view aliasing the original
    STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<mts::AssetCacheEntry>);
    STATIC_REQUIRE_FALSE(std::is_copy_assignable_v<mts::AssetCacheEntry>);
    STATIC_REQUIRE(std::is_move_constructible_v<mts::AssetCacheEntry>);

    STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<mts::AssetCache>);
    STATIC_REQUIRE_FALSE(std::is_copy_assignable_v<mts::AssetCache>);
    STATIC_REQUIRE(std::is_move_constructible_v<mts::AssetCache>);
}
