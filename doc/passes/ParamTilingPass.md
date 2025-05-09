# ParamTilingPass

## PassBuilder configuration

Example json configuration for this pass

```json
{
    "passes": [
        ...
        {
            "uuid": "param-tiling",
            "gen-trahrhe": true, // default is true
            "trahrhe-use-dichotomy": true, // default is true
            "bound-compute-method": "default", // must be either "default", "fill-last" or "reduce-control"
            "types": [
                "algebraic",
                "algebraic",
                "rectangular"
            ],
            "tile-size-hint": 32 // default is 32
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

This array will define the type of tiling that will be done.
Values of the array are string. They should be either `algebraic` or `rectangular`.
Default is `["algebraic"]`
This define for each tiled dimension the tiling type. If there is more tiled dimensions than items in the array, the last element will be used.

### `tile-size-hint`

_Advanced use_
This pass is working on the CloogAST. It is looking in the AST for the loops that have been tiled.
This parameter gives a hint on the tile size that was used so the algorithm can find tiled dimensions.
Default value is `32`.

### `boundCorrectionStrategy`

_String_
Allowed values: `no-correction`, `correct`, `shout-and-continue`, `shout-and-die`

### `volumeCheck`

_Object_
Example:

```json
    ...
    "volumeCheck": {
        "trigger": "first-outter-iterator-dependency",
        "strategy": "dynamic-tile-until-threshold",
        "threshold": "2000"
    },
    ...

```

Allowed `trigger` values :

- `never`: volume check is never triggered
- `always`: volume check strategy is applied at every depth of each loop nest
- `first-outter-iterator-dependency`: the volume check strategy will only be applied to the outtermost for that depend on an untiled outter iterator

Allowed `strategy` values:

- `no-check`: no volume check is done
- `check-and-die`: if volume is lower than threshold, then shout and exit program
- `check-and-warn`: if volume is lower than threshold, then warn. Program execution continues. _Might leader to uncorrect program, or crash later_
- `no-tile-until-threshold`: no tiling will be done until the volume reaches the given `threshold`
- `dynamic-tile-until-threshold`: when target volume is under `threshold`, it will increase progressively the divider until it reaches the required volume.
