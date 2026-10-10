# Compiling query engine

An in-memory SQL engine that turns each query into C++, compiles it, and runs the result. A database-systems course project: the value types follow the HyPer runtime, and the planner uses the produce/consume style of a compiling database.

A query is parsed into an operator tree, predicate pushdown attaches filters and join keys, and the tree emits a C++ function. That function is compiled against this source tree and loaded with `dlopen`.

## Example

```sql
create table foo (a integer not null);
create table bar (b integer not null);
copy foo from 'foo.tbl';
copy bar from 'bar.tbl';
select * from foo f, bar b where f.a = b.b;
```

`foo` holds `1`, `2`, `3`. `bar` holds `2`, `4`. The engine builds a hash table on `foo.a`, probes it with `bar.b`, and prints:

```text
b, a
2, 2
```

`SELECT *` lists columns in alias order. Fields are separated by a comma and a space.

The emitted function has this shape. The real hash table name is `hashTable_` plus the operator address, so that suffix changes every run.

```cpp
extern "C" void execute(imlab::Database& db) {
    std::cout << "b" << ", " << "a" << std::endl;
    LazyMultiMap<Key<imlab::Value>, std::vector<imlab::Value>> hashTable_join;

    auto& foo = db.getTable("foo");
    for (size_t i = 0; i < foo.getRows().size(); ++i) {
        auto foo_a = foo.getRows()[i][foo.getColumnIndices().at("a")];
        hashTable_join.insert({Key<imlab::Value>(foo_a), {foo_a}});
    }
    hashTable_join.finalize();

    auto& bar = db.getTable("bar");
    for (size_t i = 0; i < bar.getRows().size(); ++i) {
        auto bar_b = bar.getRows()[i][bar.getColumnIndices().at("b")];
        auto matches = hashTable_join.equal_range(Key<imlab::Value>(bar_b));
        for (auto match = matches.first; match != matches.second; ++match) {
            auto foo_a = match->value_[0];
            if (foo_a.getInteger() == bar_b.getInteger()) {
                std::cout << bar_b.getInteger() << ", " << foo_a.getInteger() << std::endl;
            }
        }
    }
}
```

`foo` is the build side. `bar` is the probe side. The `if` is the join predicate after pushdown. How the tree is built, how pushdown moves that predicate, and which SQL is accepted are in [Implementation](docs/implementation.md).

## Build

Requires CMake, Ninja, Bison 3, Flex, oneTBB, and GoogleTest.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target imlabdb tester
./build/tester
./build/imlabdb
```

On macOS, Homebrew Bison and Flex are keg-only. The CMake file searches the usual Homebrew prefixes.

## Limits

Equality predicates and comma joins only. Rows live in memory. Each query is compiled on its own. There are no secondary indexes.
