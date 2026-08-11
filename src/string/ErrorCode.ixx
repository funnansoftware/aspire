export module stream.string.errorcode;

export namespace stream::string
{
    enum class ErrorCode
    {
        None = 0,
        InvalidArgument = 1,
        OutOfRange = 2,
        Overflow = 3,
        Underflow = 4,
        UnknownError = 5
    };
}