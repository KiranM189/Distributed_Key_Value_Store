#include "kvstore.hpp"
#include <iostream>
#include <cstdlib>  // for std::atoi

int main(int argc, char* argv[]) {
    int mem_size = 0;

    if (argc == 2) {
        mem_size = std::atoi(argv[1]);
        if (mem_size <= 0) {
            std::cerr << "Invalid memory size. Must be a positive integer.\n";
            return 1;
        }
    } else {
        mem_size = 1024 * 1024;
        std::cout << "No memory size provided. Using default size: " << mem_size << " bytes (1 MB)\n";
    }


    KvStore& kv = KvStore::get_instance(mem_size);

    int choice;
    while (true) {
        std::cout << "\n1. Find inside Shared Mem\n";
        std::cout << "2. Delete inside Shared Mem\n";
        std::cout << "3. Update inside Shared Mem\n";
        std::cout << "4. Insert inside Shared Mem\n";
        std::cout << "5. Exit\n";
        std::cout << "Enter your choice: ";
        std::cin >> choice;

        if (std::cin.fail()) {
            std::cin.clear(); // Clear the fail flag
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Discard bad input
            std::cout << "Invalid input. Please enter a number between 1 and 5.\n";
            continue;
        }

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


//g++ -o kvstore main.cpp kvstore.cpp -I/home/zhenkar/HPE/boost_1_87_0/boost -L/home/zhenkar/HPE/boost_1_87_0/stage/lib