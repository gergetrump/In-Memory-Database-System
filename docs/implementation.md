# Implementation

## Pipeline

`imlabdb` reads stdin until a line contains `;`, then parses the whole accumulated statement.

1. Flex and Bison build an AST.
2. Semantic analysis checks that tables and columns exist and builds an operator tree.
3. `QueryStatement::run` runs predicate pushdown, then `prepare`, then `produce`.
4. `CodeGenHelper` writes the generated C++ to `/tmp/code.cc`, compiles it to `/tmp/code.so` against this source tree and `libimlab`, and calls `execute` through `dlopen`.

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
- A predicate that reaches a scan, or another `Select`, stays there.
- A `Select` that successfully pushed its predicate is removed.

For `select * from foo f, bar b where f.a = b.b`, the equality uses both sides, so it becomes the join condition. A predicate such as `f.a = 20` in the same `WHERE` clause is pushed onto the `foo` scan and does not run after the join.

## Hash join

The generated join is a hash join. The left input is the build side: each row is inserted into a `LazyMultiMap` under the join key. The right input is the probe side: each row looks up `equal_range` and runs the join predicate on the matches.

Rows are scanned in input order, and the hash chains keep that order, so matches come out in build-side order. The map is built once per query, in the generated function, and dropped when `execute` returns.

## SQL subset

```text
CREATE TABLE name (
    column type NOT NULL,
    ...
    [, PRIMARY KEY (column, ...)]
);

COPY name FROM 'file' [DELIMITER '|'];

SELECT * | column, ...
FROM table [alias] | (subquery) [alias], ...
[WHERE condition AND ...];
```

Column types are `integer`, `char(n)`, `varchar(n)`, `numeric(precision, scale)`, and `timestamp`. Every column is `NOT NULL`. The copy delimiter defaults to `|`.

A condition is `column = column` or `column = literal`. Literals are integers and strings. A comma in `FROM` is an inner join. An equality that references both inputs becomes the join key.
