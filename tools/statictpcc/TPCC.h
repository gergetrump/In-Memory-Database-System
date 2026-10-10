//---------------------------------------------------------------------------
// IMLAB
//---------------------------------------------------------------------------
#ifndef TOOLS_TPCC_H
#define TOOLS_TPCC_H
//---------------------------------------------------------------------------
#include "imlab/types/Types.h"
#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

#ifndef IMLAB_DATA_DIR
#define IMLAB_DATA_DIR "data"
#endif

//---------------------------------------------------------------------------
namespace statictpcc {
//---------------------------------------------------------------------------
enum Relation : uint8_t {
    kWarehouse,
    kDistrict,
    kCustomer,
    kHistory,
    kNewOrder,
    kOrder,
    kOrderLine,
    kItem,
    kStock,
};
//---------------------------------------------------------------------------

struct Warehouse {
    imlab::Integer w_id;
    imlab::Varchar<10> w_name;
    imlab::Varchar<20> w_street_1;
    imlab::Varchar<20> w_street_2;
    imlab::Varchar<20> w_city;
    imlab::Char<2> w_state;
    imlab::Char<9> w_zip;
    imlab::Numeric<4, 4> w_tax;
    imlab::Numeric<12, 2> w_ytd;
};

struct District {
    imlab::Integer d_id;
    imlab::Integer d_w_id;
    imlab::Varchar<10> d_name;
    imlab::Varchar<20> d_street_1;
    imlab::Varchar<20> d_street_2;
    imlab::Varchar<20> d_city;
    imlab::Char<2> d_state;
    imlab::Char<9> d_zip;
    imlab::Numeric<4, 4> d_tax;
    imlab::Numeric<12, 2> d_ytd;
    imlab::Integer d_next_o_id;
};

struct Customer {
    imlab::Integer c_id;
    imlab::Integer c_d_id;
    imlab::Integer c_w_id;
    imlab::Varchar<16> c_first;
    imlab::Char<2> c_middle;
    imlab::Varchar<16> c_last;
    imlab::Varchar<20> c_street_1;
    imlab::Varchar<20> c_street_2;
    imlab::Varchar<20> c_city;
    imlab::Char<2> c_state;
    imlab::Char<9> c_zip;
    imlab::Char<16> c_phone;
    imlab::Timestamp c_since;
    imlab::Char<2> c_credit;
    imlab::Numeric<12, 2> c_credit_lim;
    imlab::Numeric<4, 4> c_discount;
    imlab::Numeric<12, 2> c_balance;
    imlab::Numeric<12, 2> c_ytd_payment;
    imlab::Numeric<4, 0> c_payment_cnt;
    imlab::Numeric<4, 0> c_delivery_cnt;
    imlab::Varchar<500> c_data;
};

struct History {
    imlab::Integer h_c_id;
    imlab::Integer h_c_d_id;
    imlab::Integer h_c_w_id;
    imlab::Integer h_d_id;
    imlab::Integer h_w_id;
    imlab::Timestamp h_date;
    imlab::Numeric<6, 2> h_amount;
    imlab::Varchar<24> h_data;
};

struct NewOrderEntry {
    imlab::Integer no_o_id;
    imlab::Integer no_d_id;
    imlab::Integer no_w_id;
};

struct Order {
    imlab::Integer o_id;
    imlab::Integer o_d_id;
    imlab::Integer o_w_id;
    imlab::Integer o_c_id;
    imlab::Timestamp o_entry_d;
    imlab::Integer o_carrier_id;
    imlab::Numeric<2, 0> o_ol_cnt;
    imlab::Numeric<1, 0> o_all_local;
};

struct OrderLine {
    imlab::Integer ol_o_id;
    imlab::Integer ol_d_id;
    imlab::Integer ol_w_id;
    imlab::Integer ol_number;
    imlab::Integer ol_i_id;
    imlab::Integer ol_supply_w_id;
    imlab::Timestamp ol_delivery_d;
    imlab::Numeric<2, 0> ol_quantity;
    imlab::Numeric<6, 2> ol_amount;
    imlab::Char<24> ol_dist_info;
};

struct Item {
    imlab::Integer i_id;
    imlab::Integer i_im_id;
    imlab::Varchar<24> i_name;
    imlab::Numeric<5, 2> i_price;
    imlab::Varchar<50> i_data;
};

struct Stock {
    imlab::Integer s_i_id;
    imlab::Integer s_w_id;
    imlab::Numeric<4, 0> s_quantity;
    imlab::Char<24> s_dist_01;
    imlab::Char<24> s_dist_02;
    imlab::Char<24> s_dist_03;
    imlab::Char<24> s_dist_04;
    imlab::Char<24> s_dist_05;
    imlab::Char<24> s_dist_06;
    imlab::Char<24> s_dist_07;
    imlab::Char<24> s_dist_08;
    imlab::Char<24> s_dist_09;
    imlab::Char<24> s_dist_10;
    imlab::Numeric<8, 0> s_ytd;
    imlab::Numeric<4, 0> s_order_cnt;
    imlab::Numeric<4, 0> s_remote_cnt;
    imlab::Varchar<50> s_data;
};

struct TupleHash {
    size_t operator()(const std::tuple<imlab::Integer, imlab::Integer>& key) const {
        return std::get<0>(key).hash()
        ^ std::get<1>(key).hash() << 1;
    }
    size_t operator()(const std::tuple<imlab::Integer, imlab::Integer, imlab::Integer>& key) const {
        return std::get<0>(key).hash()
        ^ std::get<1>(key).hash() << 1
        ^ std::get<2>(key).hash() << 2;
    }
    size_t operator()(const std::tuple<imlab::Integer, imlab::Integer, imlab::Integer, imlab::Integer>& key) const {
        return std::get<0>(key).hash()
        ^ std::get<1>(key).hash() << 1
        ^ std::get<2>(key).hash() << 2
        ^ std::get<3>(key).hash() << 3;
    }
    size_t operator()(const std::tuple<imlab::Integer, imlab::Integer, imlab::Integer, imlab::Integer, imlab::Integer>& key) const {
        return std::get<0>(key).hash()
        ^ std::get<1>(key).hash() << 1
        ^ std::get<2>(key).hash() << 2
        ^ std::get<3>(key).hash() << 3
        ^ std::get<4>(key).hash() << 4;
    }
};

//---------------------------------------------------------------------------
class TPCC {

    friend void NewOrder(TPCC&, imlab::Integer, imlab::Integer, imlab::Integer, imlab::Integer, std::array<imlab::Integer, 15>&, std::array<imlab::Integer, 15>&, std::array<imlab::Integer, 15>&, imlab::Timestamp);

    public:
    std::vector<Customer> customer_rows;
    std::vector<Warehouse> warehouse_rows;
    std::vector<District> district_rows;
    std::vector<Order> order_rows;
    std::vector<OrderLine> orderline_rows;
    std::vector<Item> item_rows;
    std::vector<Stock> stock_rows;
    std::vector<NewOrderEntry> neworder_rows;
    std::vector<History> history_rows;

    std::unordered_map<std::tuple<imlab::Integer, imlab::Integer, imlab::Integer>, imlab::Integer, TupleHash> customer_ids;
    std::unordered_map<imlab::Integer, imlab::Integer> warehouse_ids;
    std::unordered_map<std::tuple<imlab::Integer, imlab::Integer>, imlab::Integer, TupleHash> district_ids;
    std::unordered_map<std::tuple<imlab::Integer, imlab::Integer, imlab::Integer>, imlab::Integer, TupleHash> order_ids;
    std::unordered_map<std::tuple<imlab::Integer, imlab::Integer, imlab::Integer, imlab::Integer>, imlab::Integer, TupleHash> orderline_ids;
    std::unordered_map<imlab::Integer, imlab::Integer> item_ids;
    std::unordered_map<std::tuple<imlab::Integer, imlab::Integer>, imlab::Integer, TupleHash> stock_ids;
    std::unordered_map<std::tuple<imlab::Integer, imlab::Integer, imlab::Integer>, imlab::Integer, TupleHash> neworder_ids;
    std::unordered_map<std::tuple<imlab::Integer, imlab::Integer, imlab::Integer, imlab::Integer, imlab::Integer>, imlab::Integer, TupleHash> history_ids;

    // Load the TPCC data.
    // Call these methods with std::ifstreams to load your data.
    void LoadCustomer(std::istream& in);
    void LoadDistrict(std::istream& in);
    void LoadHistory(std::istream& in);
    void LoadItem(std::istream& in);
    void LoadNewOrder(std::istream& in);
    void LoadOrder(std::istream& in);
    void LoadOrderLine(std::istream& in);
    void LoadStock(std::istream& in);
    void LoadWarehouse(std::istream& in);

    // Call the load methods from above to load the full TPCC data
    void LoadTPCC(const std::string& directory = IMLAB_DATA_DIR);

    // Returns the number of tuples in a relation.
    template <statictpcc::Relation>
    size_t Size();

};
//---------------------------------------------------------------------------
void NewOrder(TPCC&, imlab::Integer, imlab::Integer, imlab::Integer, imlab::Integer, std::array<imlab::Integer, 15>&, std::array<imlab::Integer, 15>&, std::array<imlab::Integer, 15>&, imlab::Timestamp);
//---------------------------------------------------------------------------
} // namespace statictpcc
//---------------------------------------------------------------------------
#endif //TOOLS_TPCC_H
// ---------------------------------------------------------------------------
