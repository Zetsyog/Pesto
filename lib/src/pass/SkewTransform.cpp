#include <pesto/pass/SkewTransform.hpp>
#include <cctype>
#include <sstream>
#include <set>

namespace pesto::pass {

// Tokenize the RHS of "name = <expr>" into signed iterator terms.
// Grammar (linear, integer): term (('+'|'-') term)*
//   term := [int '*'?] iterator
SkewTarget SkewTransform::parseLine(const std::string &line) {
    auto eq = line.find('=');
    if (eq == std::string::npos)
        throw std::runtime_error("skew: missing '=' in \"" + line + "\"");

    std::string lhs = line.substr(0, eq);
    std::string rhs = line.substr(eq + 1);

    auto trim = [](std::string s) {
        size_t a = s.find_first_not_of(" \t");
        size_t b = s.find_last_not_of(" \t");
        return (a == std::string::npos) ? std::string() : s.substr(a, b - a + 1);
    };

    SkewTarget t;
    t.name = trim(lhs);
    if (t.name.empty())
        throw std::runtime_error("skew: empty target name in \"" + line + "\"");

    std::string expr = rhs;
    int sign = +1;
    size_t i = 0;
    auto skipws = [&]() { while (i < expr.size() && std::isspace((unsigned char)expr[i])) i++; };

    bool expectTerm = true;
    while (i < expr.size()) {
        skipws();
        if (i >= expr.size()) break;

        char c = expr[i];
        if (c == '+' || c == '-') {
            if (expectTerm)
                throw std::runtime_error("skew: unexpected '" + std::string(1,c) + "' in \"" + line + "\"");
            sign = (c == '-') ? -1 : +1;
            i++;
            expectTerm = true;
            continue;
        }

        int coeff = 1;
        bool hasCoeff = false;
        if (std::isdigit((unsigned char)c)) {
            int val = 0;
            while (i < expr.size() && std::isdigit((unsigned char)expr[i])) {
                val = val * 10 + (expr[i] - '0');
                i++;
            }
            coeff = val;
            hasCoeff = true;
            skipws();
            if (i < expr.size() && expr[i] == '*') { i++; skipws(); }
        }

        skipws();
        if (i >= expr.size() || !(std::isalpha((unsigned char)expr[i]) || expr[i] == '_')) {
            if (hasCoeff)
                throw std::runtime_error("skew: bare constant not allowed in \"" + line + "\"");
            throw std::runtime_error("skew: expected iterator in \"" + line + "\"");
        }
        std::string name;
        while (i < expr.size() && (std::isalnum((unsigned char)expr[i]) || expr[i] == '_')) {
            name += expr[i]; i++;
        }

        t.coeffs[name] += sign * coeff;
        sign = +1;
        expectTerm = false;
    }

    if (expectTerm)
        throw std::runtime_error("skew: trailing operator in \"" + line + "\"");
    if (t.coeffs.empty())
        throw std::runtime_error("skew: empty expression in \"" + line + "\"");

    for (auto it = t.coeffs.begin(); it != t.coeffs.end(); ) {
        if (it->second == 0) it = t.coeffs.erase(it);
        else ++it;
    }
    return t;
}

SkewTransform SkewTransform::parse(const std::vector<std::string> &lines) {
    SkewTransform tr;
    for (const auto &l : lines)
        tr.targets_.push_back(parseLine(l));
    return tr;
}

std::vector<std::string> SkewTransform::originalIterators() const {
    std::set<std::string> seen;
    std::vector<std::string> order;
    for (const auto &t : targets_)
        for (const auto &kv : t.coeffs)
            if (seen.insert(kv.first).second)
                order.push_back(kv.first);
    return order;
}

} // namespace pesto::pass

