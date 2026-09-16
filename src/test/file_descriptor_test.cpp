#include <include/file_descriptor.h>

#include <sstream>
#include <string>
#include <type_traits>
#include <utility>


static_assert(std::is_same_v<
        decltype(bcpp::to_underlying_fundamental(std::declval<bcpp::system::file_descriptor const &>())),
        bcpp::system::file_descriptor::value_type>);


int main()
{
    int descriptors[2];
    if (::pipe(descriptors) != 0)
        return 1;

    bcpp::system::file_descriptor reader{descriptors[0]};
    bcpp::system::file_descriptor writer{descriptors[1]};
    auto const & readOnly = reader;

    if ((bcpp::system::to_underlying(readOnly) != descriptors[0]) ||
            (bcpp::to_underlying_fundamental(readOnly) != descriptors[0]) ||
            (bcpp::to_underlying_fundamental(writer) != descriptors[1]))
        return 2;

    std::ostringstream output;
    output << readOnly;
    if (output.str() != std::to_string(descriptors[0]))
        return 3;

    // Extraction borrows the handle; it must leave the descriptor usable.
    char sent = 'x';
    char received{};
    if ((::write(bcpp::to_underlying_fundamental(writer), &sent, 1) != 1) ||
            (::read(bcpp::to_underlying_fundamental(readOnly), &received, 1) != 1) ||
            (received != sent))
        return 4;
}
