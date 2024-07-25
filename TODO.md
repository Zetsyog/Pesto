# Todolist

- [ ] This list
- [ ] Find a new name
- [ ] Better build
- [ ] Lib structure
- [ ] Tool
- [ ] Namespace everything
- [ ] Some refactor ? Better code architecture ?
- [ ] Tests

## Example transform.json

```json
{
    "options": {
        "output-format": "c", // osl, or c
        "input-format": "osl", // osl, or c
        "opt-osl": true,
        "opt-clast": true
    },
    "passes": {
        "osl": [
            {
                "uuid": "pluto-pass",
                "pluto-opt": {
                    "tile": false,
                    "parallel": false,
                    "prevector": false
                },
                "filters": [
                    "$nest_depth >= 2",
                ]
            },
            {
                "uuid": "split-loop-nest",
            },
            {
                "uuid": "pluto-pass",
                "pluto-opt": {
                    "tile": true,
                    "parallel": false,
                    "prevector": false
                }
            }
        ],
        "clast": [
            {
                "uuid": "param-tiling"
            }
        ]
    }
}
```
