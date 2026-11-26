# ParamContextPass

Adds a constraint on the global parameters to the scop context. It runs on the
OpenScop IR, so every later pass sees the constraint: Pluto, the tiling passes
and code generation. The generated code is only correct for parameter values
that satisfy it.

```json
{
    "passes": [
        { "uuid": "param-context", "value": "N > 10 and M > 25" },
        ...
    ]
}
```

### `value`

Required. Same forms as [GenClastPass's `codegenContext`](GenClastPass.md#codegencontext):
`null`, an integer lower bound for every parameter, an object of per-parameter
lower bounds (`{"N": 11}`, others `>= 0`), or an affine constraint in isl syntax
(`"N > 10 and M >= N"`).

To constrain only code generation, set `codegenContext` on `codegen-cloog`, or
in the `clast` option of `param-tiling` and `isl-tpz-tiling`, instead.
