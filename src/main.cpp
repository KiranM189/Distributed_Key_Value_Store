// main.cpp
#include "kvstore.hpp"
#include <iostream>
#include <limits>

int main() {
    KvStore& kv = KvStore::get_instance();

    int choice;
    while (true) {
        std::cout << "\n1. Find inside Shared Mem\n";
        std::cout << "2. Delete inside Shared Mem\n";
        std::cout << "3. Update inside Shared Mem\n";
        std::cout << "4. Insert inside Shared Mem\n";
        std::cout << "5. Exit\n";
        std::cout << "Enter your choice: ";
        std::cin >> choice;

        int key;
        std::string value;

        switch (choice) {
            case 1: {
                std::cout << "Enter the key to find its value: ";
                std::cin >> key;
                kv.Find(key);
                break;
            }
            case 2: {
                std::cout << "Enter the key to delete: ";
                std::cin >> key;
                kv.Delete(key);
                break;
            }
            case 3: {
                std::cout << "Enter the key you want to update: ";
                std::cin >> key;
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::cout << "Enter the new value: ";
                std::getline(std::cin, value);
                kv.Update(key, value);
                break;
            }
            case 4: {
                std::cout << "Enter the key you want to insert: ";
                std::cin >> key;
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::cout << "Enter the value: ";
                std::getline(std::cin, value);
                kv.Insert(key, value);
                break;
            }
            case 5: {
                std::cout << "Exiting program.\n";
                return 0;
            }
            default:
                std::cout << "Invalid Choice. Try again.\n";
        }
    }
}