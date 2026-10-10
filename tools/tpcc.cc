#include "statictpcc/TPCC.h"
#include "imlab/types/Types.h"

#include <array>
#include <iostream>
#include <random>
//---------------------------------------------------------------------------
using namespace imlab;
//---------------------------------------------------------------------------
namespace {
//---------------------------------------------------------------------------
// A function to return a seeded random number generator.
inline std::mt19937& generator() {
    // the generator will only be seeded once (per thread) since it's static
    static thread_local std::mt19937 gen(std::random_device{}());
    return gen;
}
//---------------------------------------------------------------------------
// A function to generate integers in the range [min, max]
template <typename T, std::enable_if_t<std::is_integral_v<T>>* = nullptr>
T rand(T min = std::numeric_limits<T>::min(), T max = std::numeric_limits<T>::max()) {
    std::uniform_int_distribution<T> dist(min, max);
    return dist(generator());
}
//---------------------------------------------------------------------------
// A function to generate floats in the range [min, max)
template <typename T, std::enable_if_t<std::is_floating_point_v<T>>* = nullptr>
T rand(T min = std::numeric_limits<T>::min(), T max = std::numeric_limits<T>::max()) {
    std::uniform_real_distribution<T> dist(min, max);
    return dist(generator());
}
//---------------------------------------------------------------------------
const int32_t kWarehouses = 5;
//---------------------------------------------------------------------------
// Uniform random number
int32_t URand(int32_t min, int32_t max) {
    return rand<int32_t>(min, max);
}
//---------------------------------------------------------------------------
// Uniform random number
int32_t URandExcept(int32_t min, int32_t max, int32_t v) {
    if (max <= min) {
        return min;
    }
    int32_t r = rand<int32_t>(min, max - 1);
    return (r >= v) ? (r + 1) : r;
}
//---------------------------------------------------------------------------
// Non-uniform random number
int32_t NURand(int32_t A, int32_t x, int32_t y) {
    return (((URand(0, A) | URand(x, y)) + 42) % (y - x + 1)) + x;
}
//---------------------------------------------------------------------------
// Place a random order
void RandomNewOrder(auto& fn, auto& data) {
    Timestamp now(0);
    auto w_id = Integer(URand(1, kWarehouses));
    auto d_id = Integer(URand(1, 10));
    auto c_id = Integer(NURand(1023, 1, 3000));
    auto ol_cnt = Integer(URand(5, 15));

    std::array<Integer, 15> supware;
    std::array<Integer, 15> itemid;
    std::array<Integer, 15> qty;
    for (auto i = Integer(0); i < ol_cnt; i += Integer(1)) {
        supware[i.value] = (URand(1, 100) > 1) ? w_id : Integer(URandExcept(1, kWarehouses, w_id.value));
        itemid[i.value] = Integer(NURand(8191, 1, 100000));
        qty[i.value] = Integer(URand(1, 10));
    }

    fn(data, w_id, d_id, c_id, ol_cnt, supware, itemid, qty, now);
}
//---------------------------------------------------------------------------
} // namespace
//---------------------------------------------------------------------------
void REPL(statictpcc::TPCC& data) {
    std::string line;
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) {
            break;
        }
        if (line.empty()) {
            continue;
        }
        if (line.find("LOAD tpcc FROM") == 0) {
            const std::string prefix = "LOAD tpcc FROM";
            std::string directory = line.substr(prefix.size());
            const auto first = directory.find_first_not_of(" \t");
            if (first == std::string::npos) {
                std::cerr << "Invalid input" << '\n';
                continue;
            }
            directory.erase(0, first);
            const auto last = directory.find_last_not_of(" \t\r");
            if (last != std::string::npos) {
                directory.erase(last + 1);
            }
            data.LoadTPCC(directory);
        } else if (line.find("EXECUTE neworder") == 0) {
            RandomNewOrder(statictpcc::NewOrder, data);
            std::cout << "Number of orders (tuples): " << data.order_rows.size() << '\n';
        } else {
            std::cerr << "Invalid input" << '\n';
        }
    }
}

int main() {
    statictpcc::TPCC data;
    REPL(data);
    return 0;
}
//---------------------------------------------------------------------------
