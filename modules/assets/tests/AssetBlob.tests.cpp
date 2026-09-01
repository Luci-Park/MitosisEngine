#include <assets/AssetBlob.h>

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <string_view>

namespace
{
    std::vector<std::byte> MakeContent(std::string_view text)
    {
        std::vector<std::byte> bytes(text.size());
        std::memcpy(bytes.data(), text.data(), text.size());
        return bytes;
    }
}

TEST_CASE("BuildAssetBlob and ParseAssetBlob round trip", "[assets][blob]")
{
    const std::vector<std::byte> content = MakeContent("hello blob");
    const std::vector<std::byte> blob = mts::BuildAssetBlob(42, 3, content);

    const std::optional<mts::AssetBlobView> view = mts::ParseAssetBlob(blob);
    REQUIRE(view.has_value());
    CHECK(view->header.magic == mts::kAssetBlobMagic);
    CHECK(view->header.formatVersion == mts::kAssetBlobFormatVersion);
    CHECK(view->header.typeTag == 42);
    CHECK(view->header.contentVersion == 3);
    CHECK(view->header.contentSize == content.size());
    REQUIRE(view->content.size() == content.size());
    CHECK(std::memcmp(view->content.data(), content.data(), content.size()) == 0);
}

TEST_CASE("ParseAssetBlob round trips empty content", "[assets][blob]")
{
    const std::vector<std::byte> blob = mts::BuildAssetBlob(1, 1, {});
    const std::optional<mts::AssetBlobView> view = mts::ParseAssetBlob(blob);
    REQUIRE(view.has_value());
    CHECK(view->content.empty());
}

TEST_CASE("ParseAssetBlob rejects a buffer smaller than the header", "[assets][blob]")
{
    const std::vector<std::byte> tooSmall(4);
    CHECK_FALSE(mts::ParseAssetBlob(tooSmall).has_value());
}

TEST_CASE("ParseAssetBlob rejects a bad magic", "[assets][blob]")
{
    std::vector<std::byte> blob = mts::BuildAssetBlob(1, 1, MakeContent("data"));
    blob[0] = std::byte{0};
    CHECK_FALSE(mts::ParseAssetBlob(blob).has_value());
}

TEST_CASE("ParseAssetBlob rejects content that does not match the stored hash", "[assets][blob]")
{
    std::vector<std::byte> blob = mts::BuildAssetBlob(1, 1, MakeContent("data"));
    blob.back() ^= std::byte{0xFF};
    CHECK_FALSE(mts::ParseAssetBlob(blob).has_value());
}

TEST_CASE("ParseAssetBlob rejects a truncated content region", "[assets][blob]")
{
    std::vector<std::byte> blob = mts::BuildAssetBlob(1, 1, MakeContent("data"));
    blob.pop_back();
    CHECK_FALSE(mts::ParseAssetBlob(blob).has_value());
}
