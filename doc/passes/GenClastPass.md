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
            "codegenContext": 0
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

Default is `0`. Constraint on the global parameters, added to the scop context
before code generation. Cloog then drops the guards it implies, so **the
generated code is only correct for parameter values that satisfy it**. Accepted
forms:

| Value | Meaning |
| --- | --- |
| `null` | no constraint |
| integer `v` | every parameter `P >= v` |
| `{"N": 11, "M": 26}` | `N >= 11`, `M >= 26`; parameters not listed get `>= 0` |
| `"N > 10 and M > 25 and M >= N"` | any affine constraint on the parameters, in isl syntax |

The string form must be convex (no `or`) and must not need existential
variables (no `%`, no `floor`). Unknown parameter names and unsatisfiable
constraints are errors.

`-1` is the deprecated spelling of `null`: it is still read as "no constraint"
and logs a warning. Use the [`param-context`](ParamContextPass.md) pass instead
to make the constraint visible to every pass of the pipeline, not only to code
generation.

### `stopLevel`

*Integer*. Default is `-1`.
The level of the loop nest to stop the code generation. Value of `-1` means infinity.
