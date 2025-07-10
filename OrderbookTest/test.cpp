#include "../Orderbook.h"
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>
#include <tuple>

enum class ActionType {
    Add,
    Modify,
    Cancel
};

struct Information {
    ActionType type_;
    OrderType orderType_;
    Side side_;
    Price price_;
    Quantity quantity_;
    OrderId orderId_;
};


struct Result {
    std::size_t allCount_;
    std::size_t bidCount_;
    std::size_t askCount_;
};

using Informations = std::vector<Information>;

struct InputHandler {
private:
    std::uint64_t ToNumber(const std::string_view& str) const {
        std::int64_t value{};
        std::from_chars(str.data(), str.data() + str.size(), value);
        if (value < 0)
            throw std::invalid_argument("negatives not allowed");
        return static_cast<std::uint64_t>(value);
    }

    std::vector<std::string_view> Split(const std::string_view& str, char delimiter) const {
        std::vector<std::string_view> columns;
        std::size_t start{}, end{};

        while ((end = str.find(delimiter, start)) != std::string::npos) {
            columns.push_back(str.substr(start, end - start));
            start = end + 1;
        }
        columns.push_back(str.substr(start));
        return columns;
    }

    bool TryParseResult(const std::string_view& str, Result& result) const {
        if (str[0] != 'R') return false;
        auto tokens = Split(str, ' ');
        if (tokens.size() != 4) return false;

        result.allCount_ = ToNumber(tokens[1]);
        result.bidCount_ = ToNumber(tokens[2]);
        result.askCount_ = ToNumber(tokens[3]);
        return true;
    }

    Side ParseSide(std::string_view str) const {
        if (str == "B") return Side::Buy;
        if (str == "S") return Side::Sell;
        throw std::invalid_argument("invalid side");
    }

    OrderType ParseOrderType(std::string_view str) const {
        if (str == "FillAndKill") return OrderType::FillAndKill;
        if (str == "GoodTillCancel") return OrderType::GoodTillCancel;
        if (str == "GoodForDay") return OrderType::GoodForDay;
        if (str == "FillOrKill") return OrderType::FillOrKill;
        if (str == "Market") return OrderType::Market;
        throw std::invalid_argument("invalid order type");
    }

    Price ParsePrice(std::string_view str) const {
        if (str.empty()) throw std::invalid_argument("empty price");
        return ToNumber(str);
    }

    Quantity ParseQuantity(std::string_view str) const {
        if (str.empty()) throw std::invalid_argument("empty quantity");
        return ToNumber(str);
    }

    OrderId ParseOrderId(std::string_view str) const {
        if (str.empty()) throw std::invalid_argument("empty order ID");
        return ToNumber(str);
    }

    bool TryParseInformation(const std::string_view& str, Information& info) const {
        auto tokens = Split(str, ' ');
        if (tokens.empty()) return false;

        char action = tokens[0][0];
        switch (action) {
            case 'A':
                if (tokens.size() != 6) return false;
                info.type_ = ActionType::Add;
                info.side_ = ParseSide(tokens[1]);
                info.orderType_ = ParseOrderType(tokens[2]);
                info.price_ = ParsePrice(tokens[3]);
                info.quantity_ = ParseQuantity(tokens[4]);
                info.orderId_ = ParseOrderId(tokens[5]);
                return true;
            case 'M':
                if (tokens.size() != 5) return false;
                info.type_ = ActionType::Modify;
                info.orderId_ = ParseOrderId(tokens[1]);
                info.side_ = ParseSide(tokens[2]);
                info.price_ = ParsePrice(tokens[3]);
                info.quantity_ = ParseQuantity(tokens[4]);
                return true;
            case 'C':
                if (tokens.size() != 2) return false;
                info.type_ = ActionType::Cancel;
                info.orderId_ = ParseOrderId(tokens[1]);
                return true;
            default:
                return false;
        }
    }

public:
    std::tuple<Informations, Result> GetInformations(const std::filesystem::path& file) const {
        Informations infos;
        std::ifstream input(file);
        std::string line;

        while (std::getline(input, line)) {
            if (line.empty()) continue;

            if (line[0] == 'R') {
                Result result;
                if (!TryParseResult(line, result))
                    throw std::logic_error("Invalid result format");
                return {infos, result};
            } else {
                Information info;
                if (!TryParseInformation(line, info))
                    throw std::logic_error("Invalid update format");
                infos.push_back(info);
            }
        }
        throw std::logic_error("No result line found");
    }
};

// -------------------- Google Test Fixture -----------------------

class OrderbookTestsFixture : public ::testing::TestWithParam<const char*> {
protected:
    static inline std::filesystem::path TestFolderPath = std::filesystem::current_path() / "OrderbookTest" / "TestFiles";
};

TEST_P(OrderbookTestsFixture, OrderbookTestSuite) {
    const auto file = OrderbookTestsFixture::TestFolderPath / GetParam();
    InputHandler handler;
    auto [updates, result] = handler.GetInformations(file);

    Orderbook orderbook;

    auto GetOrder = [](const Information& info) {
        return std::make_shared<Order>(
            info.orderType_,
            info.orderId_,
            info.side_,
            info.price_,
            info.quantity_
        );
    };

    auto GetOrderModify = [](const Information& info) {
        return OrderModify{
            info.orderId_,
            info.side_,
            info.price_,
            info.quantity_,
        };
    };

    for (const auto& update : updates) {
        switch (update.type_) {
            case ActionType::Add:
                orderbook.AddOrder(GetOrder(update));
                break;
            case ActionType::Modify:
                orderbook.ModifyOrder(GetOrderModify(update));
                break;
            case ActionType::Cancel:
                orderbook.CancelOrder(update.orderId_);
                break;
        }
    }

    const auto& infos = orderbook.GetOrderInfos();
    ASSERT_EQ(orderbook.Size(), result.allCount_);
    ASSERT_EQ(infos.GetBids().size(), result.bidCount_);
    ASSERT_EQ(infos.GetAsks().size(), result.askCount_);
}

INSTANTIATE_TEST_SUITE_P(
    AllTests,
    OrderbookTestsFixture,
    ::testing::Values(
        "Match_GoodTillCancel.txt",
        "Match_FillAndKill.txt",
        "Match_FillOrKill_Hit.txt",
        "Match_FillOrKill_Miss.txt",
        "Cancel_Success.txt",
        "Modify_Side.txt",
        "Match_Market.txt"
    )
);
