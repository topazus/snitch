#include "snitch/snitch_cli.hpp"

#include "snitch/snitch_console.hpp"
#include "snitch/snitch_error_handling.hpp"
#include "snitch/snitch_string_utility.hpp"

#include <algorithm> // for std::find

namespace snitch::impl { namespace {
using namespace std::literals;

constexpr std::size_t max_arg_names = 2;

namespace argument_type {
enum type { optional = 0b00, mandatory = 0b01, repeatable = 0b10 };
}

struct expected_argument {
    small_vector<std::string_view, max_arg_names> names;
    std::optional<std::string_view>               value_name;
    std::string_view                              description;
    argument_type::type                           type = argument_type::optional;
};

using expected_arguments = small_vector<expected_argument, max_command_line_args>;

struct parser_settings {
    bool with_color = true;
};

std::string_view extract_executable(std::string_view path) noexcept {
    if (auto folder_end = path.find_last_of("\\/"); folder_end != path.npos) {
        path.remove_prefix(folder_end + 1);
    }
    if (auto extension_start = path.find_last_of('.'); extension_start != path.npos) {
        path.remove_suffix(path.size() - extension_start);
    }

    return path;
}

bool is_option(const expected_argument& e) noexcept {
    return !e.names.empty();
}

bool is_option(const cli::argument& a) noexcept {
    return !a.name.empty();
}

bool has_value(const expected_argument& e) noexcept {
    return e.value_name.has_value();
}

bool is_mandatory(const expected_argument& e) noexcept {
    return (e.type & argument_type::mandatory) != 0;
}

bool is_repeatable(const expected_argument& e) noexcept {
    return (e.type & argument_type::repeatable) != 0;
}

std::optional<cli::input> parse_arguments(
    int                       argc,
    const char* const         argv[],
    const expected_arguments& expected,
    const expected_arguments& ignored,
    const parser_settings&    settings = parser_settings{}) noexcept {

    std::optional<cli::input> ret(std::in_place);
    ret->executable = extract_executable(argv[0]);

    auto& args = ret->arguments;
    bool  bad  = false;

    // Check validity of inputs
    small_vector<bool, max_command_line_args> expected_found;
    for (const auto& e : expected) {
        expected_found.push_back(false);

        if (is_option(e)) {
            if (e.names.size() == 1) {
                if (!e.names[0].starts_with('-')) {
                    terminate_with("option name must start with '-' or '--'");
                }
            } else {
                if (!(e.names[0].starts_with('-') && e.names[1].starts_with("--"))) {
                    terminate_with("option names must be given with '-' first and '--' second");
                }
            }
        } else {
            if (!has_value(e)) {
                terminate_with("positional argument must have a value name");
            }
        }
    }

    // Parse
    for (int argi = 1; argi < argc; ++argi) {
        std::string_view arg(argv[argi]);

        if (arg.starts_with('-')) {
            // Options start with dashes.
            bool found = false;

            for (std::size_t arg_index = 0; arg_index < expected.size(); ++arg_index) {
                const auto& e = expected[arg_index];

                if (!is_option(e)) {
                    continue;
                }

                if (std::find(e.names.cbegin(), e.names.cend(), arg) == e.names.cend()) {
                    continue;
                }

                found = true;

                if (expected_found[arg_index] && !is_repeatable(e)) {
                    cli::print(
                        make_colored("error:", settings.with_color, color::error),
                        " duplicate command line argument '", arg, "'\n");
                    bad = true;
                    break;
                }

                expected_found[arg_index] = true;

                if (has_value(e)) {
                    if (argi + 1 == argc) {
                        cli::print(
                            make_colored("error:", settings.with_color, color::error),
                            " missing value '<", *e.value_name, ">' for command line argument '",
                            arg, "'\n");
                        bad = true;
                        break;
                    }

                    argi += 1;
                    args.push_back(cli::argument{
                        e.names.back(), e.value_name, {std::string_view(argv[argi])}});
                } else {
                    args.push_back(cli::argument{e.names.back()});
                }

                break;
            }

            if (!found) {
                cli::print(
                    make_colored("warning:", settings.with_color, color::warning),
                    " unknown command line argument '", arg, "'\n");
            }

            // Not a supported argument; figure out if this is a known argument (e.g. from Catch2)
            // and whether we need to ignore the next item if it is an option.
            for (std::size_t arg_index = 0; arg_index < ignored.size(); ++arg_index) {
                const auto& e = ignored[arg_index];

                if (std::find(e.names.cbegin(), e.names.cend(), arg) == e.names.cend()) {
                    continue;
                }

                if (has_value(e)) {
                    argi += 1;
                }

                break;
            }
        } else {
            // If no dash, this is a positional argument.
            bool found = false;

            for (std::size_t arg_index = 0; arg_index < expected.size(); ++arg_index) {
                const auto& e = expected[arg_index];

                if (is_option(e)) {
                    continue;
                }

                if (expected_found[arg_index] && !is_repeatable(e)) {
                    continue;
                }

                found = true;

                args.push_back(cli::argument{""sv, e.value_name, {arg}});
                expected_found[arg_index] = true;
                break;
            }

            if (!found) {
                cli::print(
                    make_colored("error:", settings.with_color, color::error),
                    " too many positional arguments\n");
                bad = true;
            }
        }
    }

    for (std::size_t arg_index = 0; arg_index < expected.size(); ++arg_index) {
        const auto& e = expected[arg_index];
        if (!expected_found[arg_index] && is_mandatory(e)) {
            if (!is_option(e)) {
                cli::print(
                    make_colored("error:", settings.with_color, color::error),
                    " missing positional argument '<", *e.value_name, ">'\n");
            } else {
                cli::print(
                    make_colored("error:", settings.with_color, color::error), " missing option '<",
                    e.names.back(), ">'\n");
            }
            bad = true;
        }
    }

    if (bad) {
        ret.reset();
    }

    return ret;
}

struct print_help_settings {
    bool with_color = true;
};

void print_help(
    std::string_view           program_name,
    std::string_view           program_description,
    const expected_arguments&  expected,
    const print_help_settings& settings = print_help_settings{}) noexcept {

    // Print program description
    cli::print(make_colored(program_description, settings.with_color, color::highlight2), "\n");

    // Print command line usage example
    cli::print(make_colored("Usage:", settings.with_color, color::pass), "\n");
    cli::print("  ", program_name);
    if (std::any_of(expected.cbegin(), expected.cend(), [](auto& e) { return is_option(e); })) {
        cli::print(" [options...]");
    }

    for (const auto& e : expected) {
        if (!is_option(e)) {
            if (!is_mandatory(e) && !is_repeatable(e)) {
                cli::print(" [<", *e.value_name, ">]");
            } else if (is_mandatory(e) && !is_repeatable(e)) {
                cli::print(" <", *e.value_name, ">");
            } else if (!is_mandatory(e) && is_repeatable(e)) {
                cli::print(" [<", *e.value_name, ">...]");
            } else if (is_mandatory(e) && is_repeatable(e)) {
                cli::print(" <", *e.value_name, ">...");
            } else {
                terminate_with("unhandled argument type");
            }
        }
    }

    cli::print("\n\n");

    // List arguments
    small_string<max_message_length> heading;
    for (const auto& e : expected) {
        heading.clear();

        bool success = true;
        if (is_option(e)) {
            if (e.names[0].starts_with("--")) {
                success = success && append(heading, "    ");
            }

            success = success && append(heading, e.names[0]);

            if (e.names.size() == 2) {
                success = success && append(heading, ", ", e.names[1]);
            }

            if (has_value(e)) {
                success = success && append(heading, " <", *e.value_name, ">");
            }
        } else {
            success = success && append(heading, "<", *e.value_name, ">");
        }

        if (!success) {
            truncate_end(heading);
        }

        cli::print(
            "  ", make_colored(heading, settings.with_color, color::highlight1), " ", e.description,
            "\n");
    }
}

// clang-format off
constexpr expected_arguments expected_args = {
    {{"-l", "--list-tests"},    {},                         "List tests by name"},
    {{"--list-tags"},           {},                         "List tags by name"},
    {{"--list-tests-with-tag"}, {"tag"},                    "List tests by name with a given tag"},
    {{"--list-reporters"},      {},                         "List available test reporters (see --reporter)"},
    {{"-r", "--reporter"},      {"reporter[::key=value]*"}, "Choose which reporter to use to output the test results"},
    {{"-v", "--verbosity"},     {"quiet|normal|high|full"}, "Define how much gets sent to the standard output"},
    {{"-o", "--out"},           {"path"},                   "Saves output to a file given as 'path'"},
    {{"--color"},               {"always|default|never"},   "Enable/disable color in output"},
    {{"--colour-mode"},         {"ansi|default|none"},      "Enable/disable color in output (for compatibility with Catch2)"},
    {{"-h", "--help"},          {},                         "Print help"},
    {{},                        {"test regex"},             "A regex to select which test cases to run", argument_type::repeatable}};

// For compatibility with Catch2; unused.
// This is used just to swallow the argument and its parameters.
// The argument will still be reported as unknown.
constexpr expected_arguments ignored_args = {
    {{"-s", "--success"},           {}, ""},
    {{"-b", "--break"},             {}, ""},
    {{"-e", "--nothrow"},           {}, ""},
    {{"-i", "--invisibles"},        {}, ""},
    {{"-n", "--name"},              {}, ""},
    {{"-a", "--abort"},             {}, ""},
    {{"-x", "--abortx"},            {"x"}, ""},
    {{"-w", "--warn"},              {"x"}, ""},
    {{"-d", "--durations"},         {"x"}, ""},
    {{"-D", "--min-duration"},      {"x"}, ""},
    {{"-f", "--input-file"},        {"x"}, ""},
    {{"-#", "--filenames-as-tags"}, {"x"}, ""},
    {{"-c", "--section"},           {"x"}, ""},
    {{"--list-listeners"},          {}, ""},
    {{"--order"},                   {"x"}, ""},
    {{"--rng-seed"},                {"x"}, ""},
    {{"--libidentify"},             {}, ""},
    {{"--wait-for-keypress"},       {"x"}, ""},
    {{"--shard-count"},             {"x"}, ""},
    {{"--shard-index"},             {"x"}, ""},
    {{"--allow-running-no-tests"},  {}, ""}};
// clang-format on

constexpr bool with_color_default = SNITCH_DEFAULT_WITH_COLOR == 1;

constexpr const char* program_description =
    "Test runner (snitch v" SNITCH_FULL_VERSION " | compatible with Catch2 v3.4.0)";
}} // namespace snitch::impl

namespace snitch::cli {
function_ref<void(std::string_view) noexcept> console_print = &snitch::impl::stdout_print;

void print_help(std::string_view program_name) noexcept {
    print_help(
        program_name, impl::program_description, impl::expected_args,
        {.with_color = impl::with_color_default});
}

std::optional<cli::input> parse_arguments(int argc, const char* const argv[]) noexcept {
    std::optional<cli::input> ret_args = parse_arguments(
        argc, argv, impl::expected_args, impl::ignored_args,
        {.with_color = impl::with_color_default});

    if (!ret_args) {
        print("\n");
        print_help(argv[0]);
    }

    return ret_args;
}

std::optional<cli::argument> get_option(const cli::input& args, std::string_view name) noexcept {
    std::optional<cli::argument> ret;

    auto iter = std::find_if(args.arguments.cbegin(), args.arguments.cend(), [&](const auto& arg) {
        return arg.name == name;
    });

    if (iter != args.arguments.cend()) {
        ret = *iter;
    }

    return ret;
}

std::optional<cli::argument>
get_positional_argument(const cli::input& args, std::string_view name) noexcept {
    std::optional<cli::argument> ret;

    auto iter = std::find_if(args.arguments.cbegin(), args.arguments.cend(), [&](const auto& arg) {
        return !impl::is_option(arg) && arg.value_name == name;
    });

    if (iter != args.arguments.cend()) {
        ret = *iter;
    }

    return ret;
}

void for_each_positional_argument(
    const cli::input&                                    args,
    std::string_view                                     name,
    const function_ref<void(std::string_view) noexcept>& callback) noexcept {

    auto iter = args.arguments.cbegin();
    while (iter != args.arguments.cend()) {
        iter = std::find_if(iter, args.arguments.cend(), [&](const auto& arg) {
            return !impl::is_option(arg) && arg.value_name == name;
        });

        if (iter != args.arguments.cend()) {
            callback(*iter->value);
            ++iter;
        }
    }
}
} // namespace snitch::cli
