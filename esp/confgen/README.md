# confgen
 
`confgen` is the system we use to generate the config.cpp parser. It essentially generates implementations for header files ending with `.cfg.h` based
on control comments.

There are a couple of ways that `confgen` can be instructed to provide data to you:

- directly, in a ram variable
- indirectly, through a function that receives config
- lazily, by generating a function that reads the data

## Comment syntax

All `confgen` comments start

```
//!cfg: <something>
```

The first word of a confgen is the desired type, either `holds`, `receives`, `requests`. The next block, after a space, is the _path specification_, similar in
style to `jq`.

Specifically, the path syntax is

```ebnf

path: ('.' atom)+

atom: IDENTIFIER
    | QUOTED_IDENTIFIER
    | atom '[' array_index ']' -> named_array
    | '[' array_index ']' -> anon_array
    | variable

variable: '$' IDENTIFIER

array_index: NUMBER
           | variable

```

For example:

```
.ssid  ->  { "ssid": <something> }

.weather.apikey -> {"weather": {"apikey": <something>}}

.ttc.entries[0].name -> {"ttc": {"entries": [{"name": 0}]}}

.ttc.entries[$n].$var -> all subentries
```

"variables" are only allowed for indirect/lazy callbacks for making them more generic, becoming function parameters. The sole exception is if assigning into an array type, in which case
the variable `$i` (or `$j` and onwards for multi-dimensional arrays) are available to denote the entries of the array by default. If ommited, they are added automatically.

### Additional tags

Other things can be added as comma separated tags after the main declaration, such as explicit datatypes or defaults.

```
, default <some value>
```

### Annotating types

Structure members can be annotated, in which case the entire structure becomes eligible to receive entire objects. This syntax does not allow lazy loading, but it _does_ allow indirect
values. Defaults are not always required, if not present values will not be modified, but if present they will override.
