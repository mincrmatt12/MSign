# confgen
 
`confgen` is the system we use to generate the config.cpp parser. It essentially generates implementations for header files ending with `.cfg.h` based
on control comments.

There are a couple of ways that `confgen` can be instructed to provide data to you:

- directly, in a ram variable (`holds`)
- indirectly, through a function that receives config (`receives`)
- lazily, by generating a function that reads the data (`loads`)

## Comment syntax

All `confgen` comments start

```
//!cfg: <something>
```

The first word of a confgen is the desired type, either `holds`, `receives`, or `loads`. The next block, after a space, is the _path specification_, similar in
style to `jq`.

Specifically, the path syntax is

```ebnf

path: ("." atom)+

atom: CNAME -> path_element
	| CNAME "[" array_index "]" -> named_array_element
	| "[" array_index "]" -> anon_array_element
	| variable -> wildcard_element
	| CNAME "!size" -> special_size_element

variable: "$" CNAME

?array_index: INT
		   | variable

```

For example:

```
.ssid  ->  { "ssid": <something> }

.weather.apikey -> {"weather": {"apikey": <something>}}

.ttc.entries[0].name -> {"ttc": {"entries": [{"name": 0}]}}

.ttc.entries[$n].$var -> all subentries (passed to function as a size_t and const char *)

.ttc.entries!size -> length of entries array
```

Variables have three uses:
- For arrays, to denote which axes of the target variable correspond to axes of json arrays. These use names `$i - $k`, and if omitted will be assumed to refer to the
  last element of the path
- For `receives`, to specify wildcards which correspond to function arguments (by name).
- For `loads`, to specify dynamic matches. They are the exact inverse of the behaviour for `receives`; calling a generated loader with some arguments
will load the value that a similar receiver would get for those same arguments.

`holds` annotates a variable and specifies that the config data at the given path should be placed in it. The variable should be declared
`extern`, as the config generator will create the actual storage.

`receives` annotates a function that will be called with config data (and possible values of variables). The values are passed to whichever parameter
is named `value` -- it does not need to be in any specific location.

`loads` annotates a function that will have its implementation generated such that it loads some entry from the config, as denoted by the path. Its arguments
can be used as variables in said path to choose different things to load at runtime.

### Additional tags

Other things can be added as comma separated tags after the main declaration, such as explicit datatypes or defaults.

```
, default <some value>
, max $i = 0
, result $hi
```

- `default` sets a default for a value. Defaults are only allowed for loaders/global variables and should only be basic constants (not a complex expression). Arrays
can be initialized by space separating the desired values. The `$i - $k` variables can be used to refer to array indices.
- `max` sets a maximum for a given variable in wildcard/array filling mode. By default, it's inferred from the dimensions of the targeted variable,
but for `receives` this is impossible and so can be assigned manually
- `result` specifies a named argument to a loader which should be a `bool&` and which is set to true/false depending on if the load succeeded.

### Annotating types

Structure members can be annotated, in which case the entire structure becomes eligible for being used as a target variable. Note that currently you cannot have a function
receive an entire object (although a loader _can_ generate one). Structure function members can also be used as receive targets.

The paths used are relative to wherever the structure is used, for example

```c++
struct Thing {
   //!cfg: .a
   int a = 4;
   //!cfg: .bs
   int bs[3]{};
}

//!cfg: .some.path
extern Thing thingy;
```

which will load from `.some.path.a` and `.some.path.bs[0-2]`.

Structures are default-constructed, however you can also provide explicit defaults for fields too. These will currently _not_ be applied
in loaders, though.

Types are only searched for annotations in `.cfg.h` files, not anything included from one. To search an additional file, use the `//!cfg: include <file>` notation
which works relative to the current file (like a quoted `#include`).


