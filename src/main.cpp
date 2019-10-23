#include <iostream>
#include <filesystem>
#include "types.hpp"
#include "herix.hpp"

int main () {
    HerixLib::test_editstorage();
    HerixLib::Herix h = HerixLib::Herix(
        std::filesystem::current_path() / "test_files/text_file.txt",
        true,
        2,
        24,
        8
    );

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
    std::cout << "Redo, should do nothing:\n";
    h.redo();
    std::cout << "0: " << unsigned(h.read(0).value()) << ", 1: " << unsigned(h.read(1).value()) << ", 2: " << unsigned(h.read(2).value()) << "\n";
    h.undo();
    std::cout << "Undo\n";
    std::cout << "0: " << unsigned(h.read(0).value()) << ", 1: " << unsigned(h.read(1).value()) << ", 2: " << unsigned(h.read(2).value()) << "\n";
    h.undo();
    std::cout << "Undo\n";
    std::cout << "0: " << unsigned(h.read(0).value()) << ", 1: " << unsigned(h.read(1).value()) << ", 2: " << unsigned(h.read(2).value()) << "\n";

    std::cout << "File size: " << h.getFileSize() << "\nFile End: " << h.getFileEnd() << "\n";
    std::cout << "File start: " << h.getStartPosition() << "\n";

    return 0;
}

