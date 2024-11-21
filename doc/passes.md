# Passes

Like a standard compiler transformation are applied to code via a pass system.
To do so you need to create a instance of a `pesto::PassManager`

Then you can register passes that will be applied to a program.

```c++
#include <pesto/Pesto.hpp>
// ...

pesto::PassManager pm;

pm.addPass(std::make_unique<pesto::ParamContextPass>(10));
pm.addPass(std::make_unique<pesto::PlutoAutoTransformPass>());
pm.addPass(std::make_unique<pesto::CloogCodegenPass>("prefix"));
```

Be careful, passes will be applied in the order they are registered.

`pesto::PassManager` can apply its passes only on a instance of `pesto::Program`. The `pesto::Program` can contain one or multiple `pesto::OslScop`.
The `pesto::OslScop` is an abstraction of an Openscop representation of a program (see [OpenScop](https://icps.u-strasbg.fr/~bastoul/development/openscop/index.html))
Each pass can apply transformation on each `pesto::OslScop`.
A `pesto::OslScop` is an abstraction of `osl_scop_p`. It can store additional arbitrary data, set by the different passes, called metadata.

See the following code to create a sample `pesto::Program`

```c++
#include <pesto/Pesto.hpp>
// ...
osl_scop_p scop; // should be initialized somewhere at your conveniance
// ...

std::unique_ptr<pesto::Program> prog = pesto::Progam::Builder().addScop(scop).build();
```

Below is a full example that reads an openscop file and generate code after some pesto optimizations.

```c++

#include <osl/osl.h>
#include <stdio.h>

#include <pesto/Pesto.hpp>

int main() {
    FILE *in = fopen("source.scop", "r");
    osl_scop_p scop = osl_scop_read(in);
    fclose(in);

    std::unique_ptr<pesto::Program> program = pesto::Program::Builder().addScop(scop).build();

    // scop has been copied by builder, we can free memory
    osl_scop_free(scop);

    pesto::PassManager passManager;

    // register passes
    passManager.addPass(std::make_unique<pesto::ParamContextPass>(10));
    passManager.addPass(std::make_unique<pesto::PlutoAutoTransformPass>());
    passManager.addPass(std::make_unique<pesto::SplitNestedLoopsPass>());
    passManager.addPass(std::make_unique<pesto::PlutoTilingPass>());
    passManager.addPass(std::make_unique<pesto::CloogCodegenPass>("prefix"));

    // apply passes
    passManager.runPasses(*program.get());

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

### SplitNestedLoopsPass

This pass will try to split each `pesto::ScopNest` of the program into multiple `pesto::ScopNest`. To do so it will check if the first dimension of the scattering function is scalar. If so, it will create a new scop for each value of this dimension.

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
The `pesto::pass::IPass::hookBegin(pesto::Program &prog)` will be called first, before any computation.
Then for each `pesto::OslScop` of the program the `pesto::pass::IPass::applyScop(pesto::OslScop &nest);` will be called.
Finally the `pesto::pass::IPass::hookEnd(pesto::Program &prog)` method is called.

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
