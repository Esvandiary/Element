#pragma once

//STD
#include <string>
#include <memory>
#include <utility>
#include <vector>
#include <cassert>
#include <optional>

//SELF
#include "port.hpp"
#include "fwd.hpp"
#include "ast/fwd.hpp"
#include "source_information.hpp"

struct element_expression;
struct element_interpreter_ctx;

namespace element
{
    class call_stack
    {
    public:
        struct frame
        {
            const declaration* function;
            std::vector<std::shared_ptr<const object>> compiled_arguments;
        };

        frame& push(const declaration* function, std::vector<std::shared_ptr<const object>> compiled_arguments);
        void pop();

        [[nodiscard]] bool is_recursive(const declaration* declaration) const;
        [[nodiscard]] std::shared_ptr<error> build_recursive_error(
            const declaration* decl,
            const compilation_context& context,
            const source_information& source_info);

        //todo: private
        std::vector<frame> frames;
    };

    class capture_stack
    {
    public:
        struct frame
        {
            const declaration* function;
            std::vector<std::shared_ptr<const object>> compiled_arguments;
        };

        capture_stack() = default;
        capture_stack(const declaration* function, const call_stack& calls);

        void push(const declaration* function, std::vector<std::shared_ptr<const object>> compiled_arguments);
        void pop();
        [[nodiscard]] std::shared_ptr<const object> find(const scope* s, const identifier& name,
                                                         const compilation_context& context,
                                                         const source_information& source_info);

        //todo: private
        std::vector<frame> frames;
    };

    class compilation_context
    {
    private:
        const scope* global_scope;

    public:
        explicit compilation_context(const scope* scope, element_interpreter_ctx* interpreter);

        [[nodiscard]] const scope* get_global_scope() const { return global_scope; }
        [[nodiscard]] const element_log_ctx* get_logger() const;

        mutable call_stack calls;
        mutable capture_stack captures;
        mutable source_information source_info;

        element_interpreter_ctx* interpreter;
    };

    class object
    {
    public:
        virtual ~object() = default;

        [[nodiscard]] virtual std::string typeof_info() const = 0;
        [[nodiscard]] virtual std::string to_code(int depth) const = 0;
        [[nodiscard]] virtual bool matches_constraint(const compilation_context& context, const constraint* constraint) const;
        [[nodiscard]] virtual const constraint* get_constraint() const { return nullptr; };

        [[nodiscard]] virtual std::shared_ptr<const object> index(const compilation_context& context,
                                                                  const identifier& name,
                                                                  const source_information& source_info) const;
        [[nodiscard]] virtual std::shared_ptr<const object> call(const compilation_context& context,
                                                                 std::vector<std::shared_ptr<const object>> compiled_args,
                                                                 const source_information& source_info) const;
        [[nodiscard]] virtual std::shared_ptr<const object> compile(const compilation_context& context,
                                                                    const source_information& source_info) const;

        [[nodiscard]] virtual const std::vector<port>& get_inputs() const { static std::vector<port> empty; return empty; };
        [[nodiscard]] virtual const scope* get_scope() const { return nullptr; };
        [[nodiscard]] virtual const std::optional<port>& get_output() const { static std::optional<port> empty; return empty; };
        
        [[nodiscard]] virtual std::shared_ptr<const element_expression> to_expression() const { return nullptr; };

        source_information source_info;

    protected:
        object() = default;
    };

    class error final : public object, public std::enable_shared_from_this<error>
    {
    public:
        explicit error(std::string message, element_result code, source_information src_info)
            : message{ std::move(message) }
            , code(code)
        {
            source_info = std::move(src_info);
        }

        [[nodiscard]] std::string typeof_info() const override;
        [[nodiscard]] std::string to_code(int depth) const override;

        [[nodiscard]] std::shared_ptr<const object> index(const compilation_context& context, const identifier&,
                                                          const source_information& source_info) const override { return shared_from_this(); };
        [[nodiscard]] std::shared_ptr<const object> call(const compilation_context& context,
                                                         std::vector<std::shared_ptr<const object>> compiled_args,
                                                         const source_information& source_info) const override { return shared_from_this(); };
        [[nodiscard]] std::shared_ptr<const object> compile(const compilation_context& context,
                                                            const source_information& source_info) const override { return shared_from_this(); };

        [[nodiscard]] element_result get_result() const;
        [[nodiscard]] const std::string& get_message() const;
        [[nodiscard]] element_log_message get_log_message() const;

        [[nodiscard]] bool matches_constraint(const compilation_context& context, const constraint* constraint) const override { return true; };

        element_result log_once(const element_log_ctx* logger) const;

    private:
        std::string message;
        element_result code = ELEMENT_ERROR_UNKNOWN;
        mutable bool logged = false;
    };

    bool valid_call(const compilation_context& context, const declaration* declarer, const std::vector<std::shared_ptr<const object>>&
                    compiled_args);
    std::shared_ptr<error> build_error_for_invalid_call(const compilation_context& context, const declaration* declarer, const std::vector<std::shared_ptr<const object>>&
                                                        compiled_args);
    std::shared_ptr<const object> index_type(const declaration* type, std::shared_ptr<const object> instance,
                                             const compilation_context& context, const identifier& name,
                                             const source_information& source_info);
 }
