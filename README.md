# Compiling query engine

An in-memory SQL engine that turns each query into C++, compiles it, and runs the result. A database-systems course project: the value types follow the HyPer runtime, and the planner uses the produce/consume style of a compiling database.

A query is parsed into an operator tree, predicate pushdown attaches filters and join keys, and the tree emits a C++ function. That function is compiled against this source tree and loaded with `dlopen`.

## SQL shell

Build with the commands in [Build](#build). Then, from the repository root, start the shell:

```bash
./build/imlabdb
```

A `>` prompt appears. Type a statement and finish it with a semicolon. The result is printed underneath, and the prompt comes back. Ctrl-D quits. The first `copy` or `select` takes about a second, because that statement is compiled. On macOS, `ld` may warn that oneTBB was built for a newer OS. The warning is harmless.

```text
> create table foo (a integer not null);
Table foo created.
> copy foo from 'examples/foo.tbl';
> select a from foo;
a
1
2
3
>
```

`examples/foo.tbl` is `1`, `2`, `3`. `examples/bar.tbl` is `2`, `4`.

## Example

```sql
create table foo (a integer not null);
create table bar (b integer not null);
copy foo from 'examples/foo.tbl';
copy bar from 'examples/bar.tbl';
select * from foo f, bar b where f.a = b.b;
```

`examples/foo.tbl` holds `1`, `2`, `3`. `examples/bar.tbl` holds `2`, `4`. The engine builds a hash table on `foo.a`, probes it with `bar.b`, and prints:

```text
b, a
2, 2
```

Type that at the `>` prompt. `SELECT *` lists columns in alias order. Fields are separated by a comma and a space.

The emitted function has this shape. The hash table name is `hashTable_` plus the operator's position in the plan, so a repeated plan emits the same function.

```cpp
extern "C" void execute(imlab::Database& db) {
    std::cout << "b" << ", " << "a" << std::endl;
    LazyMultiMap<Key<imlab::Value>, std::vector<imlab::Value>> hashTable_0;

    auto& foo = db.getTable("foo");
    for (size_t i = 0; i < foo.getRows().size(); ++i) {
        auto foo_a = foo.getRows()[i][foo.getColumnIndices().at("a")];
        hashTable_0.insert({Key<imlab::Value>(foo_a), {foo_a}});
    }
    hashTable_0.finalize();

    auto& bar = db.getTable("bar");
    for (size_t i = 0; i < bar.getRows().size(); ++i) {
        auto bar_b = bar.getRows()[i][bar.getColumnIndices().at("b")];
        auto matches = hashTable_0.equal_range(Key<imlab::Value>(bar_b));
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

Requires CMake, Ninja, Bison 3, Flex, oneTBB, and GoogleTest. On macOS the system Bison is too old. Homebrew installs the six tools, and the CMake file finds the keg-only Bison and Flex:

```bash
brew install cmake ninja bison flex tbb googletest
```

From the repository root:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target imlabdb tester bench
./build/tester
./build/bench
```

`./build/tester` passing means the build works. Then run `./build/imlabdb` and type SQL at the `>` prompt, as in [SQL shell](#sql-shell). `./build/bench` reprints the timings below.

## Performance

`./build/bench` times compilation and execution separately. Result rows are formatted and discarded, so the execute time includes producing the output and excludes writing it to a terminal. `rows` counts input rows.

```text
query                   rows    compile_ms    execute_ms    rows_per_s   cache  note
scan                   10000        1109.9           1.6       6400684    miss  cold
scan                   10000           0.0           0.8      12119376     hit  warm
scan                 1000000        1063.7          69.4      14406956    miss  cold
filter                 10000        1066.4           0.1      78457217    miss  cold
filter               1000000        1073.4          11.2      89202422    miss  cold
join                   20000        1566.6           3.1       6517133    miss  10k x 10k
join                  200000        1503.3          29.6       6752052    miss  100k x 100k
join-large-build      202000        1500.5          35.6       5668743    miss  build forced to large
join-small-build      202000        1453.8           5.0      40799154    miss  build forced to small
join-auto-build       202000           0.0           4.7      43234377     hit  build=small
groupby               200000        1398.1           6.9      29164973    miss  100 groups
```

Compiling a query takes about 1.1–1.6 seconds. Running it, at these sizes, takes under 70 milliseconds. The second scan compiles nothing because the generated function was already loaded. A filter is faster than a scan of the same table because it returns one row. Building the 2,000-row side of a join instead of the 200,000-row side drops execution from 36 ms to 5 ms, and the automatic choice does that. It was a cache hit because that plan is the same function as the small-build join.

## Limits

Predicates are equalities. Joins are comma joins. `GROUP BY` supports integer columns, `COUNT(*)`, and `SUM` of an integer column. Rows live in memory. Scans run as parallel morsels and merge in input order. A primary-key equality probes a hash index. There are no secondary indexes. A repeated plan reuses its compiled library.
