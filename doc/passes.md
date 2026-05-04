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

Here is the list of the passes already existing in the pesto library.

### ParamContextPass

Add a minimal value to all parameter of each `pesto::ScopNest`. The value must be passed in the constructor.

### PlutoAutoTransformPass

Run pluto on each `pesto::ScopNest` to apply only the auto transform i.e similar to the command:

```bash
polycc --noparallel --nounroll --noprevector --notile
```

### PlutoTilingPass

Run pluto on each `pesto::ScopNest` to apply only a tiling transformation i.e similar to the command:

```bash
polycc --noparallel --nounroll --noprevector --tile --nodiamond-tile
```

### GenClastPass

This pass will start cloog code generation and store the CloogAST as metadata for each ScopNest.
This allows pass to manipulate CloogAST for their optimization.
After this pass, no more transformation over the OpenScop representation should be done as code generation will ignore them.

### ClastParamTiling *(WIP)*

### CloogCodegenPass

Code generation step. This pass will simply call cloog to generate a c program.
If the `pesto::GenClastPass` was called (to be more precise, if the `pesto::ScopNest` has a `pesto::ClastMetadata`) the generated CloogAST will be fed to Cloog. Else cloog will generate code from the OpenScop.

See [Cloog](https://github.com/periscop/cloog)

## Write your own pass

Pesto passes are very simple. There is one pass base class: `pesto::pass::IPass`.

A pass is very simple, it has a name, and will perform transformations over a `pesto::Program`.
The `pesto::pass::IPass::apply(pesto::Program &prog)` will be called to apply those transformations. This method cannot be override. But it will call three ineteral methods that you can modify at your ease.
Then for each `pesto::OslScop` of the program the `pesto::pass::IPass::applyScop(pesto::OslScop &nest);` will be called.

Below is an example of a pass:

```c++
class FooPass : public pesto::pass::IPass {
  public:
    FooPass();
    virtual ~FooPass() {};

    virtual void applyScop(pesto::OslScop &nest) {
        log(std::cerr) << " running foo pass on " << nest.getId() << std::endl;
        osl_scop_p scop = nest.getScop();
        // do some work on this scop...
    };
};
```
