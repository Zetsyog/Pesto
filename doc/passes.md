# Passes

Like a standard compiler transformation are applied to code via a pass system.
To do so you need to create a instance of a `pesto::PassManager`

Then you can register passes that will be applied to a program.

```c++
#include <pesto/Pesto.hpp>
// ...

pesto::PassManager pm;

pm.addPass(std::make_unique(pesto::ParamContextPass(10)));
pm.addPass(std::make_unique<pesto::PlutoAutoTransformPass>());
pm.addPass(std::make_unique<pesto::CloogCodegenPass>("prefix"));
```

Be careful, passes will be applied in the order they are registered.

`pesto::PassManager` can apply its passes only on a instance of `pesto::PolyProgram`. The `pesto::PolyProgram` can contain one or multiple `pesto::ScopNest`.
The `pesto::ScopNest` is an abstraction of an Openscop representation of a program (see [OpenScop](https://icps.u-strasbg.fr/~bastoul/development/openscop/index.html))
Each pass can apply transformation on each `pesto::ScopNest`.
Each `pesto::ScopNest` contains an `osl_scop_p` and can store arbitrary data, set by the different passes, called metadata.

See the following code to create a sample `pesto::PolyProgram`

```c++
#include <pesto/Pesto.hpp>
// ...
osl_scop_p scop; // should be initialized somewhere
// ...

std::unique_ptr<pesto::PolyProgram> prog = pesto::PolyProgramBuilder().addScop(scop).build();
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

    std::unique_ptr<pesto::PolyProgram> polyProgram = pesto::PolyProgramBuilder().addScop(scop).build();

    osl_scop_free(scop);

    pesto::PassManager pestoPM;

    // register passes
    pestoPM.addPass(std::make_unique<pesto::ParamContextPass>(10));
    pestoPM.addPass(std::make_unique<pesto::PlutoAutoTransformPass>());
    pestoPM.addPass(std::make_unique<pesto::SplitNestedLoopsPass>());
    pestoPM.addPass(std::make_unique<pesto::PlutoTilingPass>());
    pestoPM.addPass(std::make_unique<pesto::CloogCodegenPass>("prefix"));

    pestoPM.runPasses(*polyProgram.get());

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

Pesto passes are very simple. There are two main interfaces: `pesto::IPolyhedralPass` and `pesto::IPolySingleNestPass`.
Note that the second one inherits from the first one.

A pass is very simple, it has a name, and will perform transformations over a `pesto::PolyProgram`.
The `pesto::IPolyhedralPass::run(pesto::PolyProgram &prog)` will be called to apply those transformations.
The `pesto::IPolySingleNestPass::runSingleNest(pesto::ScopNest &nest)` is very similar but is called for each `pesto::ScopNest` of the program.

Below is an example of a pass:

```c++
class FooPass : public IPolySingleNestPass {
  public:
    FooPass() : IPolySingleNestPass("pass-foo") {};
    virtual ~FooPass() {};

    virtual void runSingleNest(ScopNest &nest) {
        log(std::cerr) << " running foo pass on " << nest.getId() << std::endl;
        osl_scop_p scop = nest.getScop();
        // do some work on this scop...
    };
};
```
