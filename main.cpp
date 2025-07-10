#include "OrderType.h"
#include "Orderbook.h"
#include "OrderModify.h"
#include "Usings.h"

#include <iostream>
#include <memory>
#include <limits>

void ShowMenu() {
    std::cout << "\n\033[1;34m===== Orderbook CLI =====\033[0m\n";
    std::cout << "1. Add Order\n";
    std::cout << "2. Cancel Order\n";
    std::cout << "3. Modify Order\n";
    std::cout << "4. Print Orderbook 📊\n";
    std::cout << "5. Print Market Stats 📈\n";
    std::cout << "6. Exit 🚪\n";
    std::cout << "Select an option: ";
}

OrderType SelectOrderType() {
    std::cout << "Select Order Type:\n";
    std::cout << "1. GoodTillCancel\n";
    std::cout << "2. FillAndKill\n";
    std::cout << "3. FillOrKill\n";
    std::cout << "4. GoodForDay\n";
    std::cout << "5. Market\n";
    int choice;
    std::cin >> choice;
    switch (choice) {
        case 1: return OrderType::GoodTillCancel;
        case 2: return OrderType::FillAndKill;
        case 3: return OrderType::FillOrKill;
        case 4: return OrderType::GoodForDay;
        case 5: return OrderType::Market;
        default:
            std::cout << "Invalid choice, defaulting to GoodTillCancel.\n";
            return OrderType::GoodTillCancel;
    }
}

Side SelectSide() {
    std::cout << "Select Side:\n1. Buy\n2. Sell\n";
    int side;
    std::cin >> side;
    return (side == 1) ? Side::Buy : Side::Sell;
}

void ClearInputBuffer() {
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

int main() {
    Orderbook ob;
    OrderId nextOrderId = 1;

    while (true) {
        ShowMenu();
        int option;
        std::cin >> option;
        ClearInputBuffer();

        if (option == 1) {
            // Add Order
            auto type = SelectOrderType();
            auto side = SelectSide();

            std::cout << "Enter Price: ";
            Price price;
            std::cin >> price;

            std::cout << "Enter Quantity: ";
            Quantity quantity;
            std::cin >> quantity;


            auto start = std::chrono::high_resolution_clock::now();
            auto order = std::make_shared<Order>(type, nextOrderId, side, price, quantity);
            auto trades = ob.AddOrder(order);
            auto end = std::chrono::high_resolution_clock::now();

            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

            std::cout << "\033[1;32mOrder Placed. Order ID: " << nextOrderId
                      << " (Time taken: " << duration << " μs)\033[0m\n";
            ++nextOrderId;
            // std::cout << "\033[1;32mOrder Placed. Order ID: " << nextOrderId << "\033[0m\n";
            // ++nextOrderId;

        } else if (option == 2) {
            // Cancel Order
            std::cout << "Enter Order ID to cancel: ";
            OrderId id;
            std::cin >> id;


            auto start = std::chrono::high_resolution_clock::now();
            ob.CancelOrder(id);

            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

            std::cout << "\033[1;31mOrder Cancelled (if it existed). (Time: " << duration << " μs)\033[0m\n";
            // std::cout << "\033[1;31mOrder Cancelled (if it existed).\033[0m\n";

        } else if (option == 3) {
            // Modify Order
            std::cout << "Enter Order ID to modify: ";
            OrderId id;
            std::cin >> id;

            auto side = SelectSide();
            std::cout << "Enter New Price: ";
            Price price;
            std::cin >> price;

            std::cout << "Enter New Quantity: ";
            Quantity quantity;
            std::cin >> quantity;

            OrderModify modify{id, side, price, quantity};



            auto start = std::chrono::high_resolution_clock::now();
            ob.ModifyOrder(modify);
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

            std::cout << "\033[1;33mOrder Modified (if it existed). (Time: " << duration << " μs)\033[0m\n";
            // std::cout << "\033[1;33mOrder Modified (if it existed).\033[0m\n";

        } else if (option == 4) {
            // Print Orderbook
            std::cout << "\n\033[1;36m========== ORDERBOOK ==========\033[0m\n";
            ob.PrintOrderbook();
            std::cout << "\033[1;36m===============================\033[0m\n";

        } else if (option == 5) {
            // Print Market Stats
            ob.PrintMarketStats();

        } else if (option == 6) {
            std::cout << "Exiting...\n";
            break;

        } else {
            std::cout << "Invalid option. Please try again.\n";
        }
    }

    return 0;
}
