import std;
#include "framework/experiment.hpp"
#include "runtime_config.hpp"
#include <Standard_Failure.hxx>
#include <Standard_Version.hxx>

namespace geometry {
void RunContext::publish(StepResult step) {
    steps.push_back(std::move(step));
}
void RunContext::log(std::string message) {
    logs.push_back(std::move(message));
}

ParameterValues defaults(const Experiment &experiment) {
    ParameterValues values;
    for (const auto &spec : experiment.parameters)
        values.emplace(spec.key, spec.default_value);
    return values;
}
std::string value_text(const ParameterValue &value) {
    return std::visit(
        [](const auto &item) -> std::string {
            using T = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<T, std::string>)
                return item;
            else if constexpr (std::is_same_v<T, bool>)
                return item ? "true" : "false";
            else {
                std::ostringstream stream;
                stream << std::setprecision(17) << item;
                return stream.str();
            }
        },
        value);
}
void validate(const Experiment &experiment, const ParameterValues &values) {
    if (values.size() != experiment.parameters.size())
        throw std::invalid_argument("Parameter count mismatch");
    for (const auto &spec : experiment.parameters) {
        auto found = values.find(spec.key);
        if (found == values.end() || found->second.index() != spec.default_value.index())
            throw std::invalid_argument("Missing or wrong type: " + spec.key);
        std::visit(
            [&](const auto &value) {
                using T = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<T, double> || std::is_same_v<T, int>) {
                    if (!std::isfinite(static_cast<double>(value)) ||
                        (spec.minimum && value < *spec.minimum) ||
                        (spec.maximum && value > *spec.maximum))
                        throw std::invalid_argument("Parameter outside allowed range: " + spec.key);
                } else if constexpr (std::is_same_v<T, std::string>) {
                    if (std::find(spec.choices.begin(), spec.choices.end(), value) ==
                        spec.choices.end())
                        throw std::invalid_argument("Unknown choice: " + spec.key);
                }
            },
            found->second);
    }
}
void set_parameter(const Experiment &experiment, ParameterValues &values, const std::string &key,
                   const std::string &text) {
    auto spec = std::find_if(experiment.parameters.begin(), experiment.parameters.end(),
                             [&](const auto &item) { return item.key == key; });
    if (spec == experiment.parameters.end())
        throw std::invalid_argument("Unknown parameter: " + key);
    ParameterValue parsed;
    std::visit(
        [&](const auto &sample) {
            using T = std::decay_t<decltype(sample)>;
            if constexpr (std::is_same_v<T, std::string>)
                parsed = text;
            else if constexpr (std::is_same_v<T, bool>) {
                if (text == "true")
                    parsed = true;
                else if (text == "false")
                    parsed = false;
                else
                    throw std::invalid_argument("Boolean must be true or false: " + key);
            } else {
                T number{};
                auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), number);
                if (error != std::errc{} || end != text.data() + text.size() || text.empty())
                    throw std::invalid_argument("Invalid number: " + key);
                parsed = number;
            }
        },
        spec->default_value);
    auto candidate = values;
    candidate[key] = parsed;
    validate(experiment, candidate);
    values = std::move(candidate);
}
RunContext execute(const Experiment &experiment, const ParameterValues &values) {
    RunContext context;
    try {
        validate(experiment, values);
        experiment.run(values, context);
        context.status = RunStatus::Succeeded;
    } catch (const Standard_Failure &error) {
        context.status = RunStatus::Failed;
        context.error = error.what();
    } catch (const std::exception &error) {
        context.status = RunStatus::Failed;
        context.error = error.what();
    } catch (...) {
        context.status = RunStatus::Failed;
        context.error = "Unknown experiment exception";
    }
    context.log("Run " + status_text(context.status) +
                (context.error.empty() ? "" : ": " + context.error));
    return context;
}
std::string status_text(RunStatus status) {
    switch (status) {
    case RunStatus::Running:
        return "running";
    case RunStatus::Succeeded:
        return "succeeded";
    case RunStatus::Failed:
        return "failed";
    }
    return "unknown";
}
std::string occt_version() {
    return OCC_VERSION_STRING_EXT;
}
void initialize_runtime() {
    const auto resources = std::filesystem::path(GEOMETRY_OCCT_RESOURCES);
    for (const auto &[key, relative] : std::vector<std::pair<const char *, const char *>>{
             {"CSF_ShadersDirectory", "Shaders"},
             {"CSF_SHMessage", "SHMessage"},
             {"CSF_XSMessage", "XSMessage"},
             {"CSF_MDTVTexturesDirectory", "Textures"}}) {
        if (!std::getenv(key))
            ::setenv(key, (resources / relative).c_str(), 0);
    }
}
} // namespace geometry
