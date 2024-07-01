# Attila project

## Specifications

- MUST perform parametric rectangular tiling
- MUST perform parametric algebraic tiling
- MUST perform parametric hybrid rectangular / algebraic
- MUST generate parallel code
- JSON configuration

## Examples

## Workflow

### Why custom AST ?

Mainly because of the split pass.
Split the problem into multiple scops we know independent.
We will do our optimization passes (or not) on each individual scop.
And then generate the code.
The split pass can be disabled.

## Name

SILYCON
FlexIble PoLYhedral COmpiler