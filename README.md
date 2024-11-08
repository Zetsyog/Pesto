# Pesto &#x1F90C; (not sure of the name)

Polyhedral flExible loop-neST Optimizer (PESTO)

```bash
./pesto <input_file> [-h,--help] [-v,--version]
```

This tool allows to easily apply polyhedral transformation on C code.

It is divided in two parts : the command-line interface, and the library.

## Build

Dependencies: see pluto and trahrhe dependencies.
nlohmann/json library is also required.
cmake is required

Then you can build the tool by running the following commands :

```bash
mkdir build
cd build
cmake ..
make -j
```

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
    "passes": [
        {
            "uuid": "pluto",
            "options": {
                "tile": false,
                "parallel": false,
                "prevector": false,
                "silent": true
            }
        },
        {
            "uuid": "split-nested-loops"
        },
        {
            "uuid": "param-tiling",
            "gen-trahrhe": false,
            "trahrhe-mask": "d",
            "pluto": {
                "options": {
                    "tile": true,
                    "parallel": true,
                    "prevector": false,
                    "intratileopt": true,
                    "silent": false
                }
            }
        },
        {
            "uuid": "codegen-cloog"
        }
    ]
}
```

## Download and install trahrhe

```bash
(
    rm -rf trahrhe
    mkdir trahrhe
    cd trahrhe || exit 1
    wget https://webpages.gitlabpages.inria.fr/trahrhe/trahrhe_packages/trahrhe-4.2.zip
    unzip trahrhe-4.2.zip
    rm trahrhe-4.2.zip
    make
)
```
