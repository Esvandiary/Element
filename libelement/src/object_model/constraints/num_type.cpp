#include "num_type.hpp"

//SELF
#include "object_model/compilation_context.hpp"
#include "object_model/declarations/declaration.hpp"
#include "object_model/error.hpp"

using namespace element;

object_const_shared_ptr num_type::index(const compilation_context& context,
                                        const identifier& name,
                                        const source_information& source_info) const
{
    if (!cached)
    {
        cached_declaration = context.get_global_scope()->find(num_type::name, false);
        cached = true;
    }

    if (!cached_declaration)
        return std::make_shared<const error>("tried to index bool, but bool was not found", ELEMENT_ERROR_NOT_FOUND, source_info);

    return cached_declaration->index(context, name, source_info);
}

bool num_type::is_constant() const
{
    return true;
}