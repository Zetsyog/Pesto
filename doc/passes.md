# Passes

Like in a standard compiler transformations are applied to code via a pass system.
To do so you need to create a instance of a `pesto::PassManager`

Passes interact with a `pesto::Bundle` object. A `pesto::Bundle` is an abstraction of a generic intermediate representation (IR) of a program.
It contains one IR and can hold additional metadata that can be used by the different passses. A pass can read and write this metadata to communicate with other passes.
A pass can alter the IR of the bundle, and even change the IR type. For example, a pass can take as input a bundle with an OpenScop IR and output a bundle with a CloogAST IR.

The following code snippet shows how to create a pass and apply it to a bundle.


```c++
#include <pesto/Pesto.hpp>
// ...
osl_scop_p scop; // should be initialized somewhere at your conveniance
// ...
pesto::Bundle bundle(std::make_unique<pesto::ir::OpenScopIR>(scop)); // create a bundle with an OpenScop IR

pesto::pass::PlutoTilingPass pass;

bundle = pass.apply(std::move(bundle)); // apply the pass to the bundle
// now the bundle contains a tiled OpenScop IR

```

Multiple passes can be applied sequentially by using a `pesto::PassManager` that will take care of applying the passes in the correct order and managing the bundles.
Passes have an ir type that they expect as input and an ir type that they output. The pass manager will check those types to make sure the passes are applied in the correct order.
A pass can accept multiple ir types as input, in this case the pass manager will check if the current bundle ir type is one of the accepted types.
It is the pass responsibility to check the type of the bundle ir and cast it to the correct type before applying transformations. But the pass manager ensure that the bundle ir type is correct before applying the pass.
When using a pass without the pass manager it is the user responsibility to check the ir type of the bundle before applying the pass.

Below is a full example that reads an openscop file and generate code after some pesto optimizations.

```c++

#include <osl/osl.h>
#include <stdio.h>

#include <pesto/Pesto.hpp>

int main() {
    FILE *in = fopen("source.scop", "r");
    osl_scop_p scop = osl_scop_read(in);
    fclose(in);

    pesto::Bundle bundle(std::make_unique<pesto::ir::OpenScopIR>(scop));

    pesto::PassManager passManager;

    // register passes
    passManager.addPass(std::make_unique<pesto::ParamContextPass>(10));
    passManager.addPass(std::make_unique<pesto::PlutoAutoTransformPass>());
    passManager.addPass(std::make_unique<pesto::PlutoTilingPass>());
    passManager.addPass(std::make_unique<pesto::CloogCodegenPass>());

    // apply passes
    bundle = passManager.run(std::move(bundle));

    return 0;
}
```

## Existing passes

*WIP*

## Write your own pass


