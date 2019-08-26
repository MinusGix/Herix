#include <iostream>
#include "types.hpp"
#include "herix.hpp"

int main () {
    test_editstorage();
    Herix h = Herix(std::filesystem::current_path() / "test_files/text_file.txt", 24, 8);

    return 0;
}

