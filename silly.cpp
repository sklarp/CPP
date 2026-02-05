/*
// Build+search the most syllable-efficient spoken form for a number (computed fresh each run).
// Compile: g++ -O2 -pthread silly.cpp -o silly
// Run: ./silly 27 --quiet
*/

#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <thread>
#include <mutex>

using std::string;
using std::vector;

static constexpr int pemdas_count = 6;

struct OneName { string card; int cardSyl; string ord; int ordSyl; };
struct TenName { string card; int cardSyl; string ord; int ordSyl; };
struct LargeName { string word; int syl; int base; int zeroesAdd; };

struct Entry {
    int value{};
    vector<int> syllables;
    vector<string> names;
    vector<string> equations;
    int original{};   // base spoken syllables for the plain number
    int zeroes{};
    int digits{};
    int nonzero{};
    bool auto_pass{};
};

struct BaseOut {
    int n_syl;
    string n_name;
    int frac_syl;
    string frac_name;
    int zeroes;
    int digits;
};

struct UnaryOp {
    string id;
    int syllables;
    string text;
    int value;
    int pemdas_input;
    int pemdas_result;
};

struct BinaryOp {
    string id;
    int syllables;
    string text;
    string suffix;
    int pemdas_left;
    int pemdas_right;
    int pemdas_result;
};

static vector<Entry> number_names;

// Data tables
static const vector<OneName> one_names = {
    {"zero",2,"zeroeth",2},
    {"one",1,"first",1},
    {"two",1,"second",2},
    {"three",1,"third",1},
    {"four",1,"fourth",1},
    {"five",1,"fifth",1},
    {"six",1,"sixth",1},
    {"seven",2,"seventh",2},
    {"eight",1,"eighth",1},
    {"nine",1,"ninth",1},
    {"ten",1,"tenth",1},
    {"eleven",3,"eleventh",3},
    {"twelve",1,"twelfth",1},
    {"thirteen",2,"thirteenth",2},
    {"fourteen",2,"fourteenth",2},
    {"fifteen",2,"fifteenth",2},
    {"sixteen",2,"sixteenth",2},
    {"seventeen",3,"seventeenth",3},
    {"eighteen",2,"eighteenth",2},
    {"nineteen",2,"nineteenth",2},
};

static const vector<TenName> ten_names = {
    {"",0,"",0},
    {"",0,"",0},
    {"twenty",2,"twentieth",3},
    {"thirty",2,"thirtieth",3},
    {"forty",2,"fortieth",3},
    {"fifty",2,"fiftieth",3},
    {"sixty",2,"sixtieth",3},
    {"seventy",3,"seventieth",4},
    {"eighty",2,"eightieth",3},
    {"ninety",2,"ninetieth",3},
};

static const vector<LargeName> large_names = {
    {"hundred", 2, 100, 2},
    {"thousand",2, 1000,3},
    {"million", 2, 1000000,6},
    {"billion", 2, 1000000000,9},
};

// exponent formatting
static const vector<string> superscripts = {
    "⁰","¹","²","³","⁴","⁵","⁶","⁷","⁸","⁹",
    "¹⁰","¹¹","¹²","¹³","¹⁴","¹⁵","¹⁶","¹⁷","¹⁸","¹⁹",
    "²⁰","²¹","²²","²³"
};

// -------- small parallel helpers --------
static int default_threads() {
    unsigned hc = std::thread::hardware_concurrency();
    return (hc == 0 ? 4 : (int)hc);
}

template <class Func>
static void parallel_for_chunks(int start, int end, int threads, Func fn) {
    if (end <= start) return;
    if (threads <= 1) {
        fn(start, end);
        return;
    }

    int total = end - start;
    int chunk = (total + threads - 1) / threads;

    vector<std::thread> ts;
    ts.reserve(threads);

    for (int t = 0; t < threads; ++t) {
        int b = start + t * chunk;
        int e = std::min(end, b + chunk);
        if (b >= e) break;
        ts.emplace_back([=, &fn]() { fn(b, e); });
    }
    for (auto& th : ts) th.join();
}

static BaseOut base_syllables(int n) {
    if (n < 20) {
        return { one_names[n].cardSyl, one_names[n].card,
                 one_names[n].ordSyl, one_names[n].ord,
                 0, 1 };
    } else if (n < 100) {
        int n_mod = n % 10;
        int n_div = n / 10;

        if (n_mod == 0) {
            const auto& t = ten_names[n_div];
            return { t.cardSyl, t.card, t.ordSyl, t.ord, 1, 2 };
        }

        const auto& mod = number_names[n_mod];
        const auto& t = ten_names[n_div];

        int syl_card = t.cardSyl + mod.syllables[1];
        string name_card = t.card + "-" + mod.names[1];

        int syl_frac = t.cardSyl + mod.syllables[0];
        string name_frac = t.card + "-" + mod.names[0];

        return { syl_card, name_card, syl_frac, name_frac, 0, 2 };
    }

    int large_index = 0;
    while (large_index + 1 < (int)large_names.size() && large_names[large_index + 1].base <= n) {
        large_index++;
    }

    const auto& L = large_names[large_index];
    int n_mod = n % L.base;
    int n_div = n / L.base;

    if (n_mod == 0) {
        const auto& div = number_names[n_div];
        return {
            div.syllables[1] + L.syl,
            div.names[1] + " " + L.word,
            div.syllables[1] + L.syl,
            div.names[1] + " " + L.word + "th",
            L.zeroesAdd + div.zeroes,
            L.zeroesAdd + div.digits
        };
    }

    string connect_word = " ";
    int connect_syll = 0;

    const auto& div = number_names[n_div];
    const auto& mod = number_names[n_mod];

    return {
        div.syllables[1] + L.syl + connect_syll + mod.syllables[1],
        div.names[1] + " " + L.word + connect_word + mod.names[1],
        div.syllables[1] + L.syl + connect_syll + mod.syllables[0],
        div.names[1] + " " + L.word + connect_word + mod.names[0],
        mod.zeroes,
        L.zeroesAdd + div.digits
    };
}

static std::pair<double,double> get_first_extremes(const string& id, int min_missing, int max_number) {
    if (id == "²") return { std::pow((double)min_missing, 1.0/2.0), std::pow((double)max_number, 1.0/2.0) };
    if (id == "³") return { std::pow((double)min_missing, 1.0/3.0), std::pow((double)max_number, 1.0/3.0) };
    if (id == "+") return { 6.0, (double)max_number - 1.0 };
    if (id == "*") return { 2.0, std::pow((double)max_number, 0.5) };
    if (id == "-") return { (double)min_missing + 1.0, (double)max_number };
    if (id == "/" || id == "fraction") return { (double)min_missing * 2.0, (double)max_number };
    if (id == "^") return { 2.0, std::pow((double)max_number, 0.2) };
    return {0.0, 0.0};
}

static std::pair<double,double> get_second_extremes(const string& id, int min_missing, int max_number, int left_value) {
    if (id == "+") return { 1.0, (double)std::min(left_value, max_number - left_value) };
    if (id == "*") return { (double)std::max(left_value, (int)std::ceil((double)min_missing / left_value)), (double)max_number / left_value };
    if (id == "-") return { 1.0, (double)(left_value - min_missing) };
    if (id == "/" || id == "fraction") return { 2.0, (double)left_value / 2.0 };
    if (id == "^") return { 5.0, std::log((double)max_number) / std::log((double)left_value) };
    return {0.0, 0.0};
}

static std::pair<long long,bool> get_output(const string& id, long long left_value, long long right_value = 0) {
    if (id == "²") return { left_value * left_value, true };
    if (id == "³") return { left_value * left_value * left_value, true };
    if (id == "^") {
        long long out = 1;
        for (long long i = 0; i < right_value; ++i) out *= left_value;
        return { out, true };
    }
    if (id == "+") return { left_value + right_value, true };
    if (id == "*") return { left_value * right_value, true };
    if (id == "-") return { left_value - right_value, true };
    if (id == "/" || id == "fraction") {
        if (right_value != 0 && left_value % right_value == 0) return { left_value / right_value, true };
        return {0, false};
    }
    return {0, false};
}

static void number_names_generator(int leave_point, int max_number, bool show_progress) {
    number_names.clear();
    number_names.reserve(max_number + 1);

    int max_syllables = 0;

    // Base fill (kept sequential because base_syllables reads earlier number_names)
    for (int n = 0; n <= max_number; ++n) {
        BaseOut b = base_syllables(n);
        int adj_zeroes = b.zeroes;
        if (adj_zeroes > 3) adj_zeroes = (adj_zeroes / 3) * 3;

        Entry e;
        e.value = n;
        e.syllables.assign(pemdas_count, b.n_syl);
        e.names.assign(pemdas_count, b.n_name);
        e.equations.assign(pemdas_count, std::to_string(n));

        e.syllables[0] = b.frac_syl;
        e.names[0] = b.frac_name;

        e.original = b.n_syl;
        e.zeroes = adj_zeroes;
        e.digits = b.digits;
        e.nonzero = b.digits - b.zeroes;
        e.auto_pass = ((n % 100 < 20 && n % 100 > 0) || b.zeroes < 1 || b.digits < 3);

        number_names.push_back(std::move(e));
        max_syllables = std::max(max_syllables, b.n_syl);
    }

    // Special-case: "halve"
    if (max_number >= 2) {
        number_names[2].syllables[0] = 1;
        number_names[2].names[0] = "halve";
    }

    vector<vector<vector<int>>> syllable_key;
    syllable_key.resize(1);
    syllable_key[0].resize(pemdas_count);

    vector<UnaryOp> unary = {
        {"²", 1, " squared", 2, 2, 2},
        {"³", 1, " cubed",   3, 2, 2},
    };

    vector<BinaryOp> binary = {
        {"+", 1, " plus ",  "", 5, 5, 5},
        {"*", 1, " times ", "", 3, 4, 4},
        {"*", 1, " times ", "", 3, 3, 3},
        {"-", 2, " minus ", "", 5, 4, 5},
        {"/", 2, " over ",  "", 3, 2, 4},
        {"fraction", 0, " ", "s", 2, 0, 2},
        {"^", 2, " to the ", "", 2, 0, 2},
    };

    int min_missing = 1;

    // striped locks for number_names updates
    static constexpr int LOCK_STRIPES = 8192;
    vector<std::mutex> stripes(LOCK_STRIPES);
    auto lock_for = [&](int out) -> std::mutex& { return stripes[(unsigned)out % LOCK_STRIPES]; };

    int threads = default_threads();

    for (int s = 1; s <= max_syllables; ++s) {
        if (show_progress) {
            std::cout << "searching " << s << " syllables, at " << min_missing << "\n";
        }

        syllable_key.resize(s + 1);
        syllable_key[s].assign(pemdas_count, {});

        // ---- Fill syllable_key[s][u] in parallel ----
        vector<vector<vector<int>>> locals(threads, vector<vector<int>>(pemdas_count));
        parallel_for_chunks(min_missing, max_number + 1, threads, [&](int b, int e) {
            // map this worker to an index by hashing the thread id is annoying;
            // instead: use a small per-call static counter is also annoying.
            // Simple approach: allocate by chunk-start index -> deterministic worker slot:
            int slot = (b - min_missing) * threads / std::max(1, (max_number + 1 - min_missing));
            if (slot < 0) slot = 0;
            if (slot >= threads) slot = threads - 1;

            auto& local = locals[slot];
            for (int n = b; n < e; ++n) {
                for (int u = 0; u < pemdas_count; ++u) {
                    int cur = number_names[n].syllables[u];
                    if (cur < s) break;
                    if (cur == s) {
                        local[u].push_back(n);
                    } else if (u > 0) {
                        break;
                    }
                }
            }
        });

        for (int u = 0; u < pemdas_count; ++u) {
            for (int t = 0; t < threads; ++t) {
                auto& v = locals[t][u];
                if (!v.empty()) {
                    syllable_key[s][u].insert(syllable_key[s][u].end(), v.begin(), v.end());
                }
            }
            std::sort(syllable_key[s][u].begin(), syllable_key[s][u].end());
            syllable_key[s][u].erase(std::unique(syllable_key[s][u].begin(), syllable_key[s][u].end()),
                                     syllable_key[s][u].end());
        }

        // ---- Binary ops (parallel over left_list chunks) ----
        for (const auto& op : binary) {
            auto [min_left, max_left] = get_first_extremes(op.id, min_missing, max_number);

            for (int left_syl = 0; left_syl < s - op.syllables; ++left_syl) {
                const auto& left_list = syllable_key[left_syl][op.pemdas_left];
                if (left_list.empty()) continue;

                int right_syl = s - op.syllables - left_syl;
                if (right_syl < 0) continue;
                const auto& right_list = syllable_key[right_syl][op.pemdas_right];
                if (right_list.empty()) continue;

                // thread-local new outs per pemdas u
                vector<vector<vector<int>>> newouts_locals(threads, vector<vector<int>>(pemdas_count));

                parallel_for_chunks(0, (int)left_list.size(), threads, [&](int bi, int ei) {
                    int slot = bi * threads / std::max(1, (int)left_list.size());
                    if (slot < 0) slot = 0;
                    if (slot >= threads) slot = threads - 1;

                    auto& newouts = newouts_locals[slot];

                    for (int idx = bi; idx < ei; ++idx) {
                        int left_value = left_list[idx];
                        if (left_value < min_left) continue;
                        if (left_value > max_left) break;

                        auto [min_right, max_right] = get_second_extremes(op.id, min_missing, max_number, left_value);

                        for (int right_value : right_list) {
                            if (right_value < min_right) continue;
                            if (right_value > max_right) break;

                            if (op.id == "fraction") {
                                const auto& L = number_names[left_value];
                                const auto& R = number_names[right_value];
                                if (!L.auto_pass &&
                                    right_value != 2 &&
                                    L.zeroes >= R.digits &&
                                    (L.nonzero > 1 || R.nonzero > 1) &&
                                    L.names[1] == L.names[2]) {
                                    continue;
                                }
                            }

                            if (op.id == "^" && (size_t)right_value >= superscripts.size()) {
                                continue;
                            }

                            auto [out_ll, ok] = get_output(op.id, left_value, right_value);
                            if (!ok) continue;
                            if (out_ll < 0 || out_ll > max_number) continue;
                            int out = (int)out_ll;

                            string new_name =
                                number_names[left_value].names[op.pemdas_left] +
                                op.text +
                                number_names[right_value].names[op.pemdas_right] +
                                op.suffix;

                            string new_equation = number_names[left_value].equations[op.pemdas_left];

                            if (op.id == "^") {
                                if (new_equation == number_names[left_value].equations[1]) {
                                    new_equation = new_equation + " " + superscripts[right_value];
                                } else {
                                    new_equation = "(" + new_equation + ") " + superscripts[right_value];
                                }
                            } else {
                                if (op.id == "fraction") new_equation += " / ";
                                else new_equation += " " + op.id + " ";
                                new_equation += number_names[right_value].equations[op.pemdas_right];
                            }

                            // commit updates under a stripe lock
                            {
                                std::lock_guard<std::mutex> g(lock_for(out));
                                for (int u = op.pemdas_result; u < pemdas_count; ++u) {
                                    if (number_names[out].syllables[u] >= s) {
                                        number_names[out].names[u] = new_name;
                                        number_names[out].equations[u] = new_equation;

                                        if (number_names[out].syllables[u] > s) {
                                            number_names[out].syllables[u] = s;
                                            newouts[u].push_back(out);
                                        }
                                    }
                                }
                            }
                        }
                    }
                });

                // merge new outs into syllable_key[s][u]
                for (int u = 0; u < pemdas_count; ++u) {
                    for (int t = 0; t < threads; ++t) {
                        auto& v = newouts_locals[t][u];
                        if (!v.empty()) {
                            syllable_key[s][u].insert(syllable_key[s][u].end(), v.begin(), v.end());
                        }
                    }
                    std::sort(syllable_key[s][u].begin(), syllable_key[s][u].end());
                    syllable_key[s][u].erase(std::unique(syllable_key[s][u].begin(), syllable_key[s][u].end()),
                                             syllable_key[s][u].end());
                }
            }
        }

        // ---- Unary ops (parallel over input list chunks) ----
        for (const auto& op : unary) {
            if (s <= op.syllables) continue;

            auto [min_val, max_val] = get_first_extremes(op.id, min_missing, max_number);
            int in_syl = s - op.syllables;
            if (in_syl < 0) continue;

            const auto& in_list = syllable_key[in_syl][op.pemdas_input];
            if (in_list.empty()) continue;

            vector<vector<vector<int>>> newouts_locals(threads, vector<vector<int>>(pemdas_count));

            parallel_for_chunks(0, (int)in_list.size(), threads, [&](int bi, int ei) {
                int slot = bi * threads / std::max(1, (int)in_list.size());
                if (slot < 0) slot = 0;
                if (slot >= threads) slot = threads - 1;

                auto& newouts = newouts_locals[slot];

                for (int idx = bi; idx < ei; ++idx) {
                    int input_value = in_list[idx];
                    if (input_value < min_val) continue;
                    if (input_value > max_val) break;

                    auto [out_ll, ok] = get_output(op.id, input_value);
                    if (!ok) continue;
                    if (out_ll < 0 || out_ll > max_number) continue;
                    int out = (int)out_ll;

                    string new_name = number_names[input_value].names[op.pemdas_input] + op.text;

                    string new_equation = number_names[input_value].equations[op.pemdas_input];
                    if (new_equation == number_names[input_value].equations[1]) {
                        new_equation = new_equation + " " + op.id;
                    } else {
                        new_equation = "(" + new_equation + ") " + op.id;
                    }

                    {
                        std::lock_guard<std::mutex> g(lock_for(out));
                        for (int u = op.pemdas_result; u < pemdas_count; ++u) {
                            if (number_names[out].syllables[u] >= s) {
                                number_names[out].names[u] = new_name;
                                number_names[out].equations[u] = new_equation;

                                if (number_names[out].syllables[u] > s) {
                                    number_names[out].syllables[u] = s;
                                    newouts[u].push_back(out);
                                }
                            }
                        }
                    }
                }
            });

            for (int u = 0; u < pemdas_count; ++u) {
                for (int t = 0; t < threads; ++t) {
                    auto& v = newouts_locals[t][u];
                    if (!v.empty()) {
                        syllable_key[s][u].insert(syllable_key[s][u].end(), v.begin(), v.end());
                    }
                }
                std::sort(syllable_key[s][u].begin(), syllable_key[s][u].end());
                syllable_key[s][u].erase(std::unique(syllable_key[s][u].begin(), syllable_key[s][u].end()),
                                         syllable_key[s][u].end());
            }
        }

        // Advance min_missing
        while (min_missing <= leave_point && number_names[min_missing].syllables.back() <= s) {
            min_missing++;
        }
        if (min_missing > leave_point) break;
    }
}

static void print_usage() {
    std::cout <<
        "Usage: saynum <number> [--quiet] [--show name|equation|both|all]\n"
        "Example: ./saynum 27 --quiet --show both\n";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    long long n_ll = 0;
    try {
        n_ll = std::stoll(argv[1]);
    } catch (...) {
        std::cerr << "Invalid number.\n";
        return 1;
    }
    if (n_ll < 0) {
        std::cerr << "Only non-negative integers are supported.\n";
        return 1;
    }
    if (n_ll > 2000000) {
        std::cerr << "Refusing: number too large for a fresh full recompute in reasonable time.\n";
        std::cerr << "Try <= 2,000,000 or remove this guard in the source.\n";
        return 1;
    }
    int n = (int)n_ll;

    bool quiet = false;
    string show = "both";

    for (int i = 2; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "--quiet") quiet = true;
        else if (arg == "--show" && i + 1 < argc) {
            show = argv[++i];
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            print_usage();
            return 1;
        }
    }

    number_names_generator(n, n, !quiet);

    const auto& e = number_names[n];
    const string& name = e.names.back();
    const string& eq = e.equations.back();
    int best_syl = e.syllables.back();
    int orig_syl = e.original;

    auto diff_suffix = [&]() -> string {
        return " (from " + std::to_string(orig_syl) + " to " + std::to_string(best_syl) + " syllies)";
    };

    if (show == "name") {
        std::cout << n << " -> " << name << diff_suffix() << "\n";
    } else if (show == "equation") {
        std::cout << n << " -> " << eq << diff_suffix() << "\n";
    } else if (show == "both") {
        std::cout << n << " -> " << name << " (" << eq << ")" << diff_suffix() << "\n";
    } else if (show == "all") {
        std::cout << "number: " << n << "\n";
        std::cout << "name: " << name << "\n";
        std::cout << "equation: " << eq << "\n";
        std::cout << "syllables: " << best_syl << "\n";
        std::cout << "original syllables: " << orig_syl << "\n";
    } else {
        std::cerr << "Invalid --show option.\n";
        return 1;
    }

    return 0;
}
