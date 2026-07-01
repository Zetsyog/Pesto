# ParamTilingPass

## PassBuilder configuration

Full and detailed json configuration optionss for this pass with default values.

```json
{
    "passes": [
        ...
        {
            "uuid": "param-tiling",
            "gen-trahrhe": true,
            "trahrhe-use-dichotomy": true,
            "bound-compute-method": "default", 
            "types": [
                "algebraic",
            ],
            "boundCorrectionStrategy": "no-correction",
            "volumeCheck": {
                "strategy": "no-check",
                "trigger": "never",
                "threshold": "DIV0 * 10"
            },
            "clast": {
                "optLastLevel": -1,
                "optFirstLevel": 0,
                "stopLevel": -1,
                "simpleHulls": false,
                "firstUnrollDimension": -1,
                "spreadEqualities": true,
                "backtrack": false,
                "codegenContext": -1
            }
        },
        ...
    ]
}
```

Detailed description of parameters.

### `uuid`

Unique identifier of the pass. Case sensitive.

### `gen-trahrhe`

Whether the program should call the trahrhe software to generate header C files.
Those definition files contains the trahrhe functions used to compute tile bounds. They are mandatory so the program can be compiled.
The only valid reason to set this to false, is if the header files already exists, or if they are computed by another mean.

### `trahrhe-use-dichotomy`

If true, trahrhe functions will do a dichotomic search over the ranking polynomials to find the coordinates of a given rank.
If false, ranking polynomials will be inverted and the header file will contains exact trahrhe expressions. Maxima calls create a slower code generation.

### `bound-compute-method`

Set how loop bounds will be computed. Default value is `fill-last`. Value must be either `default`, `fill-last` or `reduce-control`.
If we are looking at the `i` tile of the `N`-th dimension, with `i` going from `[0,Dn[` with `Dn` the number of tiles (i.e. the divider). It should have a target volume `Vn`
Its lower bound is called `lbi` and its upper bound is `ubi`

With the `fill-last` setting, we have
`lbi = trahrhe_i(max(1, i * Vn))` with i in `[0,Dn[`
and `ubi = trahrhe_i(min(Ehrhart(N), (i+1) * Vn)) - 1` with i in `[0,Dn - 1[` and
`ubi = ubN` when `i` equals `Dn`. `ubN` is the maximum value of this dimension.
`default` is strictly equivalent to `fill-last`.

To keep it simple, the `fill-last` method cut the tiles one unit before the target volume is reached, for each tiles. The last tile will be a bit bigger than the others, so the whole iteration domain is covered.
_TODO: plots_

The `reduce-control` method is the opposite.
`lbi = trahrhe_i(max(1, i * Vn)) + !!i` with i in `[0,Dn[` and
`ubi = trahrhe_i(min(Ehrhart(N), (i+1) * Vn))`
This way only the lower bound of the first tile will be at 0.

### `types`

_Array of string_. Default is `["algebraic"]`.
This array will define the type of tiling that will be done.
Accepted values are `algebraic`, `rectangular` and `last-dim-rect`.
This define for each tiled dimension the tiling type. If there is more tiled dimensions than items in the array, the last element will be used.
The `last-dim-rect` will perform algebraic tiling to all dimensions except for dimension that is the inner most in the inner loop nest. This last dimension will be tiled with a rectangular tiling.

### `boundCorrectionStrategy`

_String_. Default is `no-correction`.
Allowed values: `no-correction`, `correct`, `shout-and-continue`, `shout-and-die`.

This options defines the behavior of the generated code if the computed tile bounds are not correct. Incorrect bounds are common to happen when the tile volume is too small compared to the divider.
This can be easily detected by comparing the upper and lower bounds of the tile.
Having a lower bound greater than the upper bound is an incorrect tile.

The `no-correction` strategy will not do any check, and will generate code that might crash or produce incorrect results.
All the other strategies add control code to chec the tile bounds.

The `shout-and-continue` strategy will print a warning message and continue the execution of the program. This might lead to incorrect results or crash later.

The `shout-and-die` strategy will print a warning message and exit immediately the program.

The `correct` strategy will try to correct the tile bounds by setting the upper bound to the lower bound. This does NOT guarantee that the program will be correct. But it should avoid some crashes.
This option should not be used in production code, but only for debugging purposes.

### `volumeCheck`

_Object_
Example:

```json
    ...
    "volumeCheck": {
        "trigger": "first-outer-iterator-dependency",
        "strategy": "dynamic-tile-until-threshold",
        "threshold": "2000"
    },
    ...

```

Allowed `trigger` values :

- `never`: volume check is never triggered
- `always`: volume check strategy is applied at every depth of each loop nest
- `first-outer-iterator-dependency`: the volume check strategy will only be applied to the outermost for that depend on an untiled outer iterator

Allowed `strategy` values:

- `no-check`: no volume check is done. _Might lead to incorrect program, or crash later_
- `check-and-die`: if volume is lower than threshold, then shout and exit program
- `check-and-warn`: if volume is lower than threshold, then warn. Program execution continues. _Might lead to incorrect program, or crash later_
- `no-tile-until-threshold`: no tiling will be done until the volume reaches the given `threshold`
- `dynamic-tile-until-threshold`: when target volume is under `threshold`, it will increase progressively the divider until it reaches the required volume.

### Deprecated options

- `trahrhe-mask`: legacy trahrhe option
- `pluto`: legacy pluto pass options. The pluto pass is no longer integrated in the ParamTilingPass. It must now be called by the user before the ParamTilingPass.