# Pesto &#x1F90C; (not sure of the name)

Polyhedral flExible loop-neST Optimizer (PESTO)


```bash
./pesto <input_file> [-h,--help] [-v,--version]
```

This tool allows to easily apply polyhedral transformation on C code.

It is divided in two parts : the command-line interface, and the library.

## CLI interface

Using cli

## JSON interface

You can pass a json config file to the CLI interface.

```bash
pesto --config opt.json 2mm.c -o 2mm.pesto.c
```

### opt.json

```json
{
    "options": {
        "input-format": "c"
    },
    "passes":[
        {
            "uuid": "pluto",
            "options": {
                "pluto": {
                    "tile": false,
                    "parallel": false,
                    "prevector": false,
                    "silent": true
                }
            }
        },
        {
            "uuid": "split-nested-loops"
        },
        {
            "uuid": "pluto",
            "options": {
                "pluto": {
                    "tile": true,
                    "parallel": false,
                    "prevector": false,
                    "silent": true
                }
            }
        },
        {
            "uuid": "clast-gen"
        },
        {
            "uuid": "param-tiling"
        },
        {
            "uuid": "codegen-cloog"
        }
    ]
}
```
