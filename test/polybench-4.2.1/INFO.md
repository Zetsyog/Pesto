# Polybench for pesto

This folder contains some of the polybench benchmarks adapted for a pesto usage.

## Working benchmarks

| Benchmark name | Status |
| -------------- | ------ |
| datamining/correlation | OK (nointratile-opt) |
| datamining/covariance | OK (nointratile-opt) |
| linear-algebra/blas/gemm       | OK (nointratile-opt) |
| linear-algebra/blas/gemver     | OK (nointratile-opt) |
| linear-algebra/blas/gesummv    | OK (nointratile-opt) |
| linear-algebra/blas/symm       | no tiling available |
| linear-algebra/blas/syr2k      | OK (nointratile-opt) |
| linear-algebra/blas/syrk       | OK (nointratile-opt) |
| linear-algebra/blas/trmm       | OK (nointratile-opt) |
| linear-algebra/kernels/2mm     | OK (nointratile-opt) |
| linear-algebra/kernels/3mm     | OK (nointratile-opt) |
| linear-algebra/kernels/atax    | OK (nointratile-opt) |
| linear-algebra/kernels/bicg    | OK (nointratile-opt) |
| linear-algebra/kernels/doitgen | FAIL |
| linear-algebra/kernels/mvt      | FAIL |
| stencils/adi | not tested |
| stencils/fdtd-2d | not tested |
| stencils/heat-3d | not tested |
| stencils/jacobi-1d | not tested |
| stencils/jacobi-2d | not tested |
| stencils/seidel-2d | not tested |

## Credit
