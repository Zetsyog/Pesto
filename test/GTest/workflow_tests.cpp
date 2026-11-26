#include <gtest/gtest.h>

#include <pesto/Logging.hpp>
#include <pesto/Options.hpp>
#include <pesto/ParamConstraint.hpp>
#include <pesto/PassManager.hpp>
#include <pesto/Pluto.hpp>
#include <pesto/ir/OpenScopIR.hpp>
#include <pesto/pass/SkewPass.hpp>
#include <pesto/pass/clast/GenClast.hpp>
#include <pesto/passbuilder/ParamConstraintJson.hpp>

#include <osl/generic.h>
#include <osl/macros.h>
#include <osl/relation.h>
#include <osl/strings.h>
#include <osl/util.h>

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


// A scop with parameters N and M and an unconstrained context.
osl_scop_p makeParamScop() {
	osl_scop_p scop = osl_scop_malloc();
	scop->context = osl_relation_pmalloc(osl_util_get_precision(), 0, 4);
	osl_relation_set_attributes(scop->context, 0, 0, 0, 2);
	osl_relation_set_type(scop->context, OSL_TYPE_CONTEXT);
	char *names[] = {(char *)"N", (char *)"M", nullptr};
	osl_strings_p strings = osl_strings_malloc();
	for (char **n = names; *n; ++n) {
		osl_strings_add(strings, *n);
	}
	scop->parameters = osl_generic_shell(strings, osl_strings_interface());
	return scop;
}

// Evaluates every context row at (N, M).
bool contextHolds(osl_scop_p scop, long n, long m) {
	osl_relation_p rel = scop->context;
	for (int r = 0; r < rel->nb_rows; r += 1) {
		long v = osl_int_get_si(rel->precision, rel->m[r][1]) * n + osl_int_get_si(rel->precision, rel->m[r][2]) * m +
				 osl_int_get_si(rel->precision, rel->m[r][3]);
		bool eq = osl_int_zero(rel->precision, rel->m[r][0]);
		if (eq ? v != 0 : v < 0) {
			return false;
		}
	}
	return true;
}

struct ScopGuard {
	osl_scop_p scop = makeParamScop();
	~ScopGuard() { osl_scop_free(scop); }
};

TEST(ParamConstraint, DefaultIsNonNegativeParameters) {
	ScopGuard g;
	pesto::applyParamConstraint(g.scop, pesto::ParamConstraint());
	EXPECT_TRUE(contextHolds(g.scop, 0, 0));
	EXPECT_FALSE(contextHolds(g.scop, -1, 0));
	EXPECT_FALSE(contextHolds(g.scop, 0, -1));
}

TEST(ParamConstraint, NoneLeavesContextUntouched) {
	ScopGuard g;
	pesto::applyParamConstraint(g.scop, pesto::ParamConstraint::none());
	EXPECT_EQ(g.scop->context->nb_rows, 0);
}

TEST(ParamConstraint, PerParameterBoundsDefaultOthersToZero) {
	ScopGuard g;
	pesto::applyParamConstraint(g.scop, pesto::ParamConstraint(pesto::ParamConstraint::PerParam{{"N", 11}}));
	EXPECT_TRUE(contextHolds(g.scop, 11, 0));
	EXPECT_FALSE(contextHolds(g.scop, 10, 0));
	EXPECT_FALSE(contextHolds(g.scop, 11, -1));
}

TEST(ParamConstraint, IslExpression) {
	ScopGuard g;
	pesto::applyParamConstraint(g.scop, pesto::ParamConstraint(std::string("N > 10 and M > 25 and M >= N")));
	EXPECT_TRUE(contextHolds(g.scop, 11, 26));
	EXPECT_FALSE(contextHolds(g.scop, 10, 26));
	EXPECT_FALSE(contextHolds(g.scop, 11, 25));
	EXPECT_FALSE(contextHolds(g.scop, 30, 29));
	// negative values are expressible now
	ScopGuard h;
	pesto::applyParamConstraint(h.scop, pesto::ParamConstraint(std::string("N = -3")));
	EXPECT_TRUE(contextHolds(h.scop, -3, -100));
	EXPECT_FALSE(contextHolds(h.scop, -2, 0));
}

TEST(ParamConstraint, RejectsUnsupportedConstraints) {
	using PC = pesto::ParamConstraint;
	for (const PC &c : {PC(PC::PerParam{{"K", 1}}), PC(std::string("K > 3")), PC(std::string("N < 0 or N > 5")),
						PC(std::string("N % 2 = 0")), PC(std::string("N >")), PC(std::string("N > 3 and N < 2"))}) {
		ScopGuard g;
		EXPECT_THROW(pesto::applyParamConstraint(g.scop, c), std::invalid_argument) << c.str();
	}
}

TEST(ParamConstraint, ParsesJsonForms) {
	auto logger = pesto::log::get_logger("test");
	using pesto::builder::parseParamConstraint;
	EXPECT_TRUE(parseParamConstraint(nullptr, "k", *logger).isNone());
	EXPECT_TRUE(parseParamConstraint(-1, "k", *logger).isNone());
	EXPECT_EQ(std::get<int>(parseParamConstraint(10, "k", *logger).spec), 10);
	EXPECT_EQ(std::get<int>(parseParamConstraint(-2, "k", *logger).spec), -2);
	auto per = parseParamConstraint(json{{"N", 11}, {"M", 26}}, "k", *logger);
	EXPECT_EQ((std::get<pesto::ParamConstraint::PerParam>(per.spec)), (pesto::ParamConstraint::PerParam{{"N", 11}, {"M", 26}}));
	EXPECT_EQ(std::get<std::string>(parseParamConstraint("N > 10", "k", *logger).spec), "N > 10");
	EXPECT_THROW(parseParamConstraint(true, "k", *logger), std::invalid_argument);
	EXPECT_THROW(parseParamConstraint(json{{"N", "x"}}, "k", *logger), std::invalid_argument);
	EXPECT_THROW(parseParamConstraint(json::array({1}), "k", *logger), std::invalid_argument);
}

TEST(GenClast, DefaultCodegenContextIsZero) {
	pesto::pass::GenClastPass pass;
	EXPECT_EQ(std::get<int>(pass.codegenContext.spec), 0);
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
