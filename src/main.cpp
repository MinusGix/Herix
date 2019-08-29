#include <iostream>
#include "types.hpp"
#include "herix.hpp"

int main () {
    test_editstorage();
    Herix h = Herix(std::filesystem::current_path() / "test_files/text_file.txt", 24, 8);

    std::cout << "0: " << unsigned(h.read(0).value()) << ", 1: " << unsigned(h.read(1).value()) << ", 2: " << unsigned(h.read(2).value()) << "\n";
    h.edit(0, 5);
    std::cout << "0: " << unsigned(h.read(0).value()) << ", 1: " << unsigned(h.read(1).value()) << ", 2: " << unsigned(h.read(2).value()) << "\n";
    h.edit(1, 6);
    std::cout << "0: " << unsigned(h.read(0).value()) << ", 1: " << unsigned(h.read(1).value()) << ", 2: " << unsigned(h.read(2).value()) << "\n";
    h.edit(2, 7);
    std::cout << "0: " << unsigned(h.read(0).value()) << ", 1: " << unsigned(h.read(1).value()) << ", 2: " << unsigned(h.read(2).value()) << "\n";
    h.undo();
    std::cout << "Undo\n";
    std::cout << "0: " << unsigned(h.read(0).value()) << ", 1: " << unsigned(h.read(1).value()) << ", 2: " << unsigned(h.read(2).value()) << "\n";
    h.undo();
    std::cout << "Undo\n";
    std::cout << "0: " << unsigned(h.read(0).value()) << ", 1: " << unsigned(h.read(1).value()) << ", 2: " << unsigned(h.read(2).value()) << "\n";
    h.edit(1, 9);
    std::cout << "0: " << unsigned(h.read(0).value()) << ", 1: " << unsigned(h.read(1).value()) << ", 2: " << unsigned(h.read(2).value()) << "\n";
    std::cout << "Redo\n";
    h.redo();
    std::cout << "0: " << unsigned(h.read(0).value()) << ", 1: " << unsigned(h.read(1).value()) << ", 2: " << unsigned(h.read(2).value()) << "\n";


    return 0;
}

