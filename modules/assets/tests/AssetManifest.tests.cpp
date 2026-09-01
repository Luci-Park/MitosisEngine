#include <assets/AssetManifest.h>

#include <assets/AssetBlob.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("AssetManifest build and parse round trip", "[assets][manifest]")
{
    const mts::AssetId first = mts::MakeAssetId("a/one.raw");
    const mts::AssetId second = mts::MakeAssetId("a/two.raw");

    const mts::AssetManifestSourceEntry entries[]{
        {{first, 1, 1}, "cooked/one.blob"},
        {{second, 2, 1}, "cooked/two.blob"},
    };

    const std::vector<std::byte> blob = mts::BuildAssetManifestBlob(entries);
    const std::optional<mts::AssetManifest> manifest = mts::AssetManifest::Parse(blob);
    REQUIRE(manifest.has_value());
    CHECK(manifest->Count() == 2);

    const mts::AssetManifestEntry *foundFirst = manifest->Find(first);
    REQUIRE(foundFirst != nullptr);
    CHECK(foundFirst->typeTag == 1);
    CHECK(manifest->PathOf(*foundFirst) == "cooked/one.blob");

    const mts::AssetManifestEntry *foundSecond = manifest->Find(second);
    REQUIRE(foundSecond != nullptr);
    CHECK(foundSecond->typeTag == 2);
    CHECK(manifest->PathOf(*foundSecond) == "cooked/two.blob");
}

TEST_CASE("AssetManifest Find returns nullptr for an unknown id", "[assets][manifest]")
{
    const std::vector<std::byte> blob = mts::BuildAssetManifestBlob({});
    const std::optional<mts::AssetManifest> manifest = mts::AssetManifest::Parse(blob);
    REQUIRE(manifest.has_value());
    CHECK(manifest->Find(mts::MakeAssetId("missing")) == nullptr);
}

TEST_CASE("AssetManifest Parse rejects a blob with the wrong type tag", "[assets][manifest]")
{
    const std::vector<std::byte> blob = mts::BuildAssetBlob(999, 1, {});
    CHECK_FALSE(mts::AssetManifest::Parse(blob).has_value());
}

TEST_CASE("AssetManifest rejects an unsupported content version", "[assets][manifest]")
{
    const mts::AssetManifestSourceEntry entries[]{
        {{mts::MakeAssetId("a/one.raw"), 1, 1}, "cooked/one.blob"},
    };

    // a well-formed manifest body, re-wrapped with a content version this build
    // does not know: the entry layout it describes may not be the current one
    const std::vector<std::byte> current = mts::BuildAssetManifestBlob(entries);
    const std::optional<mts::AssetBlobView> view = mts::ParseAssetBlob(current);
    REQUIRE(view.has_value());

    const std::vector<std::byte> future = mts::BuildAssetBlob(
        mts::kAssetManifestTypeTag, mts::kAssetManifestContentVersion + 1, view->content);

    CHECK_FALSE(mts::AssetManifest::Parse(future).has_value());
}

TEST_CASE("AssetManifest rejects duplicate asset ids", "[assets][manifest]")
{
    const mts::AssetId id = mts::MakeAssetId("a/one.raw");

    // what an id collision looks like on disk: silently keeping the first would
    // make the second asset permanently unreachable
    const mts::AssetManifestSourceEntry entries[]{
        {{id, 1, 1}, "cooked/one.blob"},
        {{id, 1, 1}, "cooked/two.blob"},
    };

    const std::vector<std::byte> blob = mts::BuildAssetManifestBlob(entries);
    CHECK_FALSE(mts::AssetManifest::Parse(blob).has_value());
}
