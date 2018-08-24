﻿#include "jsonpp.hpp"
#include <cctype>
#include <cstdlib>

namespace json {
    
    parse_exception::parse_exception(const std::string& msg)
        : std::runtime_error(msg) {}

    Value* Object::getValue(const std::string& name)
    {
        auto it = _values.find(name);
        if (it == _values.end()) {
            return nullptr;
        }
        return _values[name].get();
    }

    void Object::addValue(const std::string& name, std::unique_ptr<Value> value)
    {
        _values.emplace(name, std::move(value));
    }

    String::String(const std::string& value)
        : _value(value) {}

    std::string String::getValue() const
    {
        return _value;
    }

    void Array::addValue(std::unique_ptr<Value> value)
    {
        _values.emplace_back(std::move(value));
    }

    size_t Array::size() const
    {
        return _values.size();
    }

    std::unique_ptr<Object> parse(const std::string& text)
    {
        detail::Lexer lexer(text);
        detail::Parser parser(lexer);
        return parser.parse();
    }

    namespace detail {

        template <class... Args>
        static std::string format(const char* fmt, Args&&... args)
        {
            auto len = std::snprintf(nullptr, 0, fmt, std::forward<Args>(args)...) + 1;
            std::vector<char> buf(len, '\0');
            std::snprintf(buf.data(), len, fmt, std::forward<Args>(args)...);
            return buf.data();
        }

        Token::Token(TokenType type, const std::string& value, int line, int pos)
            : type(type), value(value), line(line), pos(pos) {}
        
        Lexer::Lexer(const std::string& text)
            : _cursor(0), _text(text), _line(1), _pos(1) {}

        bool Lexer::isDoneReading() const
        {
            return _cursor >= _text.size();
        }

        Token Lexer::lexString()
        {
            std::string str;
            bool endQuoteFound = false;
            while (!endQuoteFound && !isDoneReading()) {
                char c = _text[_cursor++];
                if (c == '\"') {
                    endQuoteFound = true;
                } else {
                    str += c;
                }
            }

            if (!endQuoteFound) {
                throw parse_exception("Terminating \" for string not found!");
            }

            return {TokenType::STRING, str, _line, _pos };
        }


        Token Lexer::getToken() 
        {
            // skip white space and account for lines and position in the line
            while (std::isspace(_text[_cursor]) && !isDoneReading()) {
                if (_text[_cursor] == '\n') {
                    ++_line;
                    _pos = 1;
                } else {
                    ++_pos;
                }
                ++_cursor;
            }

            while (!isDoneReading()) {
                char c = _text[_cursor++];
                if (c == '{') {
                    return {TokenType::LBRACE, "{", _line, _pos};
                } else if (c == '}') {
                    return {TokenType::RBRACE, "}", _line, _pos };
                } else if (c == '\"') {
                    return lexString();
                } else if (c == ':') {
                    return {TokenType::COLON, ":", _line, _pos };
                } else if (c == ',') {
                    return {TokenType::COMMA, ",", _line, _pos };
                } else if (c == '[') {
                    return { TokenType::LBRACKET, "[", _line, _pos };
                } else if (c == ']') {
                    return { TokenType::RBRACKET, "]", _line, _pos };
                } else {
                    throw parse_exception(c + " is not a valid token!");
                }
            }
            return {TokenType::NONE, ""};
        }

        void Parser::raiseError(const std::string& expected)
        {
            throw parse_exception(json::detail::format("Expecting '%s' at line %d:%d but got '%s' instead!", expected.c_str(), currentToken.line, currentToken.pos, currentToken.value.c_str()));
        }

        std::unique_ptr<Object> Parser::parseObject()
        {
            currentToken = lexer.getToken();
            if (currentToken.type != detail::TokenType::LBRACE) {
                raiseError("{");
            }

            currentToken = lexer.getToken();

            // an empty object
            if (currentToken.type == detail::TokenType::RBRACE) {
                return std::make_unique<Object>();
            }

            auto obj = parseValueList();

            // closing brace
            if (currentToken.type != detail::TokenType::RBRACE) {
                raiseError("}");
            }

            return obj;
        }

        std::unique_ptr<Array> Parser::parseArray()
        {
            currentToken = lexer.getToken();
            // empty array
            if (currentToken.type == TokenType::RBRACKET) {
                return std::make_unique<Array>();
            }

            auto arr = std::make_unique<Array>();
            bool isList = false;
            do {
                arr->addValue(parseValue());

                currentToken = lexer.getToken();
                if (currentToken.type == TokenType::COMMA) {
                    isList = true;
                    currentToken = lexer.getToken(); // eat the comma
                } else {
                    isList = false;
                }
            } while (isList);

            if (currentToken.type != TokenType::RBRACKET) {
                raiseError("]");
            }

            return arr;
        }

        std::unique_ptr<Value> Parser::parseValue()
        {
            if (currentToken.type == detail::TokenType::STRING) {
                return std::make_unique<json::String>(currentToken.value);
            } else if (currentToken.type == detail::TokenType::LBRACKET) {
                return parseArray();
            } else {
                raiseError("<value>");
            }
            return nullptr;
        }

        std::unique_ptr<json::Object> Parser::parseValueList()
        {
            auto obj = std::make_unique<Object>();
            bool isList = false;
            do {
                // name
                if (currentToken.type != detail::TokenType::STRING) {
                    raiseError("<string>");
                }
                auto name = currentToken.value;

                // colon
                currentToken = lexer.getToken();
                if (currentToken.type != detail::TokenType::COLON) {
                    raiseError(":");
                }
                currentToken = lexer.getToken(); // eat the colon

                // value
                auto value = parseValue();
                obj->addValue(name, std::move(value));

                // if there is a comma, we continue parsing the list
                // otherwise, we are at the end of the name/value pairs
                currentToken = lexer.getToken();
                if (currentToken.type == detail::TokenType::COMMA) {
                    isList = true;
                    currentToken = lexer.getToken(); // eat the comma
                } else {
                    isList = false;
                }

            } while (isList);
            return obj;
        }

        std::unique_ptr<Object> Parser::parse()
        {
            return parseObject();
        }

        Parser::Parser(Lexer lexer)
            : lexer(lexer) {}
    }
}