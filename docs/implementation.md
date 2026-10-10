# Implementation

## Pipeline

`imlabdb` reads stdin until a line contains `;`, then parses the whole accumulated statement.

1. Flex and Bison build an AST.
2. Semantic analysis checks that tables and columns exist and builds an operator tree.
3. `QueryStatement::run` runs predicate pushdown, then `prepare`, then `produce`.
4. `CodeGenHelper` writes the generated C++ to `/tmp/code.cc`, compiles it against this source tree and `libimlab`, and calls `execute` through `dlopen`. The shared library is named from a hash of the generated source and kept for the process. A later query that emits the same source skips the compiler. Operator names use a counter that resets for each query, so the same plan produces the same source.

`COPY` uses the same compile-and-load path. The generated function reads the file and inserts rows.

A separate `tpcc` shell loads the static TPC-C tables and runs `NewOrder`. `LOAD tpcc FROM <directory>` reads `tpcc_*.tbl` from the directory after `FROM`.

## Operator tree

A query becomes a tree of operators. Each one implements `prepare` and the produce/consume interface:

- `produce` emits the loop or pipeline that generates tuples.
- `consume` emits the code that runs for one tuple from a child.

`SELECT` without a `WHERE` clause is a scan (or a join of scans) under a `Print`. `FROM a, b` is an `InnerJoin` of two scans. A parenthesized `FROM` item is a subquery scan. `Print` is always the root of a query and writes the result rows.

`prepare` walks the tree top-down and decides which columns each operator must keep. Code generation then only names those columns.

## Predicate pushdown

Pushdown runs before `prepare`. Filters start as `Select` operators above the join that `FROM` built. Each `Select` asks its child to take the predicate:

- A predicate whose columns all come from the left input is pushed onto the left. The same rule applies on the right.
- A predicate that touches both inputs is attached to the join as its condition.
- A predicate that reaches a scan stays there, unless it equates every primary-key column to a literal. That scan then probes the primary-key index. A predicate that reaches another `Select` stays there.
- A `Select` that successfully pushed its predicate is removed.

For `select * from foo f, bar b where f.a = b.b`, the equality uses both sides, so it becomes the join condition. A predicate such as `f.a = 20` in the same `WHERE` clause is pushed onto the `foo` scan and does not run after the join.

## Scans

A scan reads the table in morsels of at most 1024 rows. `tbb::parallel_for` runs those morsels on several threads. Each morsel keeps its own rows, and the morsels are then merged from first to last, so the consumer sees input order. A smaller table uses morsels of half its row count, and at least one row. A primary-key lookup does not scan.

## Hash join

The generated join is a hash join. The left input is the build side: each row is inserted into a `LazyMultiMap` under the join key. The right input is the probe side: each row looks up `equal_range` and runs the join predicate on the matches. The hash chains keep insertion order, so join matches come out in build-side order. The map is built once per query, in the generated function, and dropped when `execute` returns.

After predicate pushdown, a join whose right input has fewer rows than its left input swaps the two inputs. The estimate is the row count of the underlying scan, including rows a filter will reject. A side whose size is not a plain scan is left where it is. The equality is then oriented so the build key belongs to the new left input. `./build/bench` can turn the pass off to time both orientations.

## Primary-key index

`CREATE TABLE` can name a primary key. `insertRow`, which `COPY` calls, hashes those columns and rejects a duplicate. A `WHERE` clause that equates every primary-key column to a literal is absorbed by the scan and probes that hash index. Any other predicate still reads the morsels and filters them.

## Hash aggregation

`GROUP BY` is a pipeline breaker above the scan, filter, or join. Integer group keys go into an `unordered_map`. `COUNT(*)` and `SUM` of an integer column update a state per group. After the input is consumed, the groups are sorted by key and emitted. `SUM` accumulates in 64 bits and throws if the result does not fit in the 32-bit integer type. An aggregate without `GROUP BY` is one group. An empty `SUM` returns 0. `GROUP BY` and aggregates in a subquery are rejected.

## Benchmark

`./build/bench` loads integer tables in memory, then runs a scan, a filter, a join, both join orientations, and a grouped `COUNT`/`SUM`. It records compile time and execute time from `QueryStatement`. The numbers in the README are one run of that binary on this machine.

## SQL subset

```text
CREATE TABLE name (
    column type NOT NULL,
    ...
    [, PRIMARY KEY (column, ...)]
);

COPY name FROM 'file' [DELIMITER '|'];

SELECT * | column, ... | COUNT(*) | SUM(column)
FROM table [alias] | (subquery) [alias], ...
[WHERE condition AND ...]
[GROUP BY column, ...];
```

Column types are `integer`, `char(n)`, `varchar(n)`, `numeric(precision, scale)`, and `timestamp`. Every column is `NOT NULL`. The copy delimiter defaults to `|`.

A condition is `column = column` or `column = literal`. Literals are integers and strings. A comma in `FROM` is an inner join. An equality that references both inputs becomes the join key.

`COUNT(*)` and `SUM` accept integer columns. A selected column next to an aggregate has to appear in `GROUP BY`. `count`, `sum`, `group`, and `by` are reserved words.
