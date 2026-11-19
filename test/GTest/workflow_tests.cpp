#include <gtest/gtest.h>

#include <pesto/Options.hpp>
#include <pesto/PassManager.hpp>
#include <pesto/Pluto.hpp>
#include <pesto/ir/OpenScopIR.hpp>
#include <pesto/pass/SkewPass.hpp>
#include <pesto/pass/clast/GenClast.hpp>

namespace {

std::unique_ptr<pesto::Options> parse(std::vector<std::string> args) {
	std::vector<char *> argv;
	for (auto &arg : args) {
		argv.push_back(arg.data());
	}
	argv.push_back(nullptr);
	return pesto::Options::build(static_cast<int>(args.size()), argv.data());
}

TEST(Options, HelpAndVersionDoNotRequireAnInput) {
	EXPECT_TRUE(parse({"pesto", "--help"})->printUsage());
	EXPECT_TRUE(parse({"pesto", "--version"})->printVersion());
	EXPECT_FALSE(parse({"pesto"})->isValid());
}

TEST(Options, RejectsInvalidArguments) {
	EXPECT_THROW(parse({"pesto", "--unknown", "input.c"}), std::invalid_argument);
	EXPECT_THROW(parse({"pesto", "-o"}), std::invalid_argument);
	EXPECT_THROW(parse({"pesto", "first.c", "second.c"}), std::invalid_argument);
}

TEST(Options, RepeatedParsesHaveIndependentFlags) {
	auto first = parse({"pesto", "--debug", "--readscop", "first.scop"});
	EXPECT_TRUE(first->debug());
	EXPECT_TRUE(first->readScop());
	first.reset();
	auto second = parse({"pesto", "--debug", "second.c"});
	EXPECT_TRUE(second->debug());
	EXPECT_FALSE(second->readScop());
	EXPECT_EQ(second->outputPath(), "second.pesto.c");
	EXPECT_FALSE(parse({"pesto", "third.c"})->debug());
}

class TestIR : public pesto::ir::IR {
  public:
	const std::string &id() const override {
		static const std::string name = "test-ir";
		return name;
	}
};

class RecordingPass : public pesto::pass::IPass {
  public:
	RecordingPass(bool &called, std::string accepted)
		: IPass("recording"), called_(called), accepted_(std::move(accepted)) {}
	std::set<std::string> inputIR() const override { return {accepted_}; }
	pesto::Bundle apply(pesto::Bundle &&bundle) override {
		called_ = true;
		return std::move(bundle);
	}

  private:
	bool &called_;
	std::string accepted_;
};

TEST(PassManager, RejectsIncompatibleIRBeforeApplyingPass) {
	bool called = false;
	pesto::PassManager pm;
	pm.addPass(std::make_unique<RecordingPass>(called, "different-ir"));
	EXPECT_THROW(pm.run(pesto::Bundle(std::make_unique<TestIR>())), std::runtime_error);
	EXPECT_FALSE(called);
}

TEST(PassManager, RunsCompatiblePass) {
	bool called = false;
	pesto::PassManager pm;
	pm.addPass(std::make_unique<RecordingPass>(called, "test-ir"));
	auto result = pm.run(pesto::Bundle(std::make_unique<TestIR>()));
	EXPECT_TRUE(called);
	EXPECT_EQ(result.ir_id(), "test-ir");
}

TEST(PassManager, RejectsNullPass) {
	pesto::PassManager pm;
	EXPECT_THROW(pm.addPass(nullptr), std::invalid_argument);
}

TEST(SkewPass, AcceptsTheOpenScopIRIdentifier) {
	pesto::pass::SkewPass pass;
	EXPECT_EQ(pass.inputIR().count(pesto::ir::OpenScopIR::getID()), 1u);
	EXPECT_EQ(pass.outputIR(), pesto::ir::OpenScopIR::getID());
}

TEST(GenClast, RejectsStatementsWithoutBodies) {
	auto *scop = osl_scop_malloc();
	scop->statement = osl_statement_malloc();
	pesto::Bundle bundle(std::make_unique<pesto::ir::OpenScopIR>(scop));
	pesto::pass::GenClastPass pass;
	EXPECT_THROW(pass.apply(std::move(bundle)), std::runtime_error);
}

TEST(PlutoOptions, RejectsInvalidTileSizesWithoutChangingTheConfiguration) {
	pesto::PlutoWrapper pluto;
	pluto.setTileSizes({1, 3, 64});
	EXPECT_THROW(pluto.setTileSizes({0}), std::invalid_argument);
	EXPECT_THROW(pluto.setTileSizes({-1, 3}), std::invalid_argument);
	EXPECT_THROW(pluto.setTileSizes(std::vector<int>{}), std::invalid_argument);
	EXPECT_THROW(pluto.pushTileSize(0), std::invalid_argument);
	EXPECT_EQ(pluto.getOptions().tile_sizes_len, 3);
	EXPECT_EQ(pluto.getOptions().tile_sizes[0], 1);
	EXPECT_EQ(pluto.getOptions().tile_sizes[1], 3);
	EXPECT_EQ(pluto.getOptions().tile_sizes[2], 64);
}

} // namespace
