#pragma once 

namespace CORE {

    enum class ParseState {
        PARSING_REQUEST_LINE,
        PARSING_HEADERS,
        PARSING_BODY,
        COMPLETE,
        ERROR
    };

    class HTTPParser {
        // TODO parse this shit
    };

} // namespace CORE