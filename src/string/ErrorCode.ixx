export module aspire.string.errorcode;

import std;

export namespace aspire::string
{
    enum class ErrorCode : std::uint8_t
    {
        None = 0,
        InvalidArgument = 1,
        OutOfRange = 2,
        Overflow = 3,
        Underflow = 4,
        UnknownError = 5
    };
}
