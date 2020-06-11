#pragma once

#include <string>

#include "typeutil.hpp"

namespace element
{
    struct compiled_expression;
    struct identifier_expression;
    struct indexing_expression;
    struct call_expression;

    struct element_object
	{
		virtual ~element_object() = default;
    	
        [[nodiscard]] virtual std::string to_string() const = 0;

        //todo: some kind of component architecture?
        [[nodiscard]] virtual const element_object* index(const indexing_expression*) const { return nullptr; };
        [[nodiscard]] virtual const element_object* call(const call_expression*) const { return nullptr; };
    };

 //   struct error : element_object
	//{
 //       static const object_model_id type_id;
 //   	
 //       std::string message;

 //       explicit error(std::string message)
 //   		: element_object(nullptr, type_id)
 //   		, message{std::move(message)}
 //       {
 //       }
 //   };
}
