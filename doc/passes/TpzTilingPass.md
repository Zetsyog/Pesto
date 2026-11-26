# TpzTilingPass

Trapezoidal tiling (`isl-tpz-tiling`).

```json
{
    "passes": [
        ...
        {
            "uuid": "isl-tpz-tiling",
            "useMinVolume": false,
            "tileReduction": true,
            "clast": { "codegenContext": 0 }
        },
        ...
    ]
}
```

### `clast`

Options of the clast generation run before tiling, see
[GenClastPass](GenClastPass.md). They are applied on top of this pass's presets
(`optFirstLevel: -1`, `optLastLevel: 1`, `codegenContext: 0`), so only the options
to change need to be given.

`codegenContext` used to be hardcoded to "every parameter `>= 10`". It now
defaults to `>= 0`. For the old behaviour, set
`"clast": {"codegenContext": 10}`.
