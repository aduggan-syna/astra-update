#include <iostream>
#include "astra_update.hpp"

int main() {
    AstraUpdate update{"/home/aduggan/astra_boot", "/home/aduggan/eMMCimg"};

    int ret = update.Update();
    if (ret < 0) {
        std::cerr << "Error running update" << std::endl;
        return 1;
    }

    return 0;
}