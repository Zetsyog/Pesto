# GenClastPass

## PassBuilder configuration

Full and detailed json configuration optionss for this pass with default values.

```json
{
    "passes": [
        ...
        {
            "uuid": "gen-clast",
            "optLastLevel": -1,
            "optFirstLevel": 0,
            "stopLevel": -1,
            "simpleHulls": false,
            "firstUnrollDimension": -1,
            "spreadEqualities": true,
            "backtrack": false,
            "codegenContext": -1
        },
        ...
    ]
}
```

Most options are explained in [Cloog's User Manual](http://www.bastoul.net/cloog/pages/download/cloog.pdf)

### `uuid`

Unique identifier of the pass. Case sensitive.

### `optLastLevel`

*Integer*. Default is `-1`.
The last level of the loop nest to optimize. Value of `-1` means infinity, i.e. all levels will be optimized.
Corresponds to the `-l` option of Cloog.

### `optFirstLevel`

*Integer*. Default is `0`.
The first level of the loop nest to optimize. Value of `-1` means infinity, i.e. no level will be optimized.
Corresponds to the `-f` option of Cloog.

### `simpleHulls`

*Boolean*. Default is `false`.
Whether to simplify convex hulls or not

### `firstUnrollDimension`

*Integer*. Default is `-1`.
The dimension of the first loop to unroll.
`-1` means infinity, i.e. no unrolling.

### `spreadEqualities`

*Boolean*. Default is `true`.
Whether to spread complex equalities or not.
Corresponds to the `-esp` option of Cloog.

### `backtrack`

*Boolean*. Default is `false`.
Whether to perform backtrack in Quillere's algorithm or not.
Corresponds to the `-backtrack` option of Cloog.

### `codegenContext`

*Integer*. Default is `-1`.
Bound the parameters to a minimum value.
`-1` means no bound.

### `stopLevel`

*Integer*. Default is `-1`.
The level of the loop nest to stop the code generation. Value of `-1` means infinity.
