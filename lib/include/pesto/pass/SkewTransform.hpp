#pragma once
#include <string>
#include <vector>
#include <map>
#include <stdexcept>

namespace pesto::pass {

// A single target row: newName = sum_k coeff[k] * origIterator[k]
// Convention: NEW index expressed in terms of the ORIGINAL iterators.
struct SkewTarget {
    std::string name;                    // e.g. "x"
    std::map<std::string, int> coeffs;   // e.g. {"i": -1, "j": 1}  for  x = j - i
    int constant = 0;                    // additive constant, e.g. -1 for  x = L - 1
};

class SkewTransform {
public:
    // Parse one line "x = j - i" into a SkewTarget.
    // Accepts: integer coeffs, +/- , iterator names. No additive constant,
    // no non-linear terms. Throws std::runtime_error on malformed input.
    static SkewTarget parseLine(const std::string &line);

    // Parse the whole transform (a list of lines).
    static SkewTransform parse(const std::vector<std::string> &lines);
    
    // Build a transform directly from targets, bypassing the string parser
    // (which rejects additive constants). Used by skew's automatic detection.
    static SkewTransform fromTargets(std::vector<SkewTarget> t) {
        SkewTransform st;
        st.targets_ = std::move(t);
        return st;
    }

    const std::vector<SkewTarget> &targets() const { return targets_; }

    // The ordered list of original iterators referenced anywhere.
    std::vector<std::string> originalIterators() const;

private:
    std::vector<SkewTarget> targets_;
};

} // namespace pesto::pass

