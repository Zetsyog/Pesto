# Pluto Pass

The Pluto pass runs Pluto scheduling and transformation on OpenScop IR.

Pass UUID: `pluto`

## PassBuilder configuration

Configuration is done with a pass entry containing `uuid: "pluto"` and an optional `options` object.

```json
{
    "passes": [
        {
        "uuid": "pluto",
            "options": {
                "tile": true,
                "parallel": true,
                "prevector": true,
                "silent": true,
                "fuse": "smart",
                "tile-sizes": [32, 32]
            }
        }
    ]
}
```

If `options` is omitted, the pass uses Pluto defaults from the pass implementation and Pluto itself.

## Option value types

- Boolean options must be JSON booleans.
- Integer options must be JSON integers.
- `fuse` must be a string.
- `tile-sizes` must be an array of integers.

Invalid types raise `std::invalid_argument`.

## Supported boolean options

The following option keys are accepted as booleans:

- `tile`
- `intratileopt`
- `diamondtile`
- `per_cc_obj`
- `dynschedule`
- `dynschedule_graph_old`
- `dyn_trans_deps_tasks`
- `fulldiamondtile`
- `parallel`
- `innerpar`
- `unrolljam`
- `prevector`
- `rar`
- `delayed_cut`
- `hybridcut`
- `codegen_context`
- `scancount`
- `forceparallel`
- `multipar`
- `second_level_tile`
- `find_tile_sizes`
- `identity`
- `bee`
- `isldep`
- `candldep`
- `isldepaccesswise`
- `isldepcoalesce`
- `lastwriter`
- `nodepbound`
- `scalpriv`
- `pipsolve`
- `islsolve`
- `glpk`
- `gurobi`
- `lp`
- `dfp`
- `ilp`
- `lpcolour`
- `scc_cluster`
- `iss`
- `flic`

## Supported integer options

The following option keys are accepted as integers:

- `ft`
- `lt`
- `ufactor`
- `coeff_bound`
- `cache_size`
- `data_element_size`

## Special options

### `fuse`

`fuse` is a string with these recognized values:

- `none`
- `maximal`
- `smart`
- `typed`
- `hybrid`

Unknown string values are ignored with a warning.

### `tile-sizes`

`tile-sizes` is an array of integers. It sets both Pluto tile size storage and length.

Example:

```json
{
    "uuid": "pluto",
    "options": {
        "tile": true,
        "tile-sizes": [64, 32, 16]
    }
}
```

## Aliases handled by the pass builder

The following keys are accepted and mapped to multiple internal options:

- `silent`: sets both `silent` and `quiet`
- `debug`: sets both `debug` and `moredebug`

## Accepted but intentionally ignored keys

These keys are recognized only to emit warnings and are not applied by this pass builder:

- `out_file`
- `cloogf`
- `cloogl`
- `cloogsh`
- `cloogbacktrack`
- `readscop`
- `pet`

## Unknown keys

Any unknown option key emits a warning and is ignored.

## Notes

- The pass operates on OpenScop IR.
- Pluto is run in place on the scop.
- `tile-sizes` defaults to `[32]` in the Pluto wrapper when not overridden.
