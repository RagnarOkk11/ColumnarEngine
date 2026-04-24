//
// Created by ragnarokk on 30.03.2026.
//

#ifndef COLUMNAR_ENGINE_MACRO_H
#define COLUMNAR_ENGINE_MACRO_H

// USE THIS EVERYWHERE FOR SWITCHING
// Прости, Герман, но я не хочу везде исправлять переборы типов, если новый добавился...
#define FOR_EACH_COLUMN_TYPE(M)                   \
    M(INT16, "INT16", Int16Column)                \
    M(INT32, "INT32", Int32Column)                \
    M(INT64, "INT64", Int64Column)                \
    M(INT128, "INT128", Int128Column)             \
    M(FLOAT, "FLOAT", FloatColumn)                \
    M(DOUBLE, "DOUBLE", DoubleColumn)             \
    M(LONGDOUBLE, "LONGDOUBLE", LongDoubleColumn) \
    M(CHAR, "CHAR", CharColumn)                   \
    M(STRING, "STRING", StringColumn)             \
    M(DATE, "DATE", DateColumn)                   \
    M(TIMESTAMP, "TIMESTAMP", TimestampColumn)

#define ASSERT(cond)                                                                          \
    do {                                                                                      \
        if (!(cond)) {                                                                        \
            throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) + \
                                     ": condition '" + #cond + "' is not satistfied");        \
        }                                                                                     \
    } while (false)

#define ASSERT_WITH_MESSAGE(cond, message)                                                    \
    do {                                                                                      \
        if (!(cond)) {                                                                        \
            throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) + \
                                     ": condition '" + #cond +                                \
                                     "' is not satistfied: " + (message));                    \
        }                                                                                     \
    } while (false)

#define THROW_NOT_IMPLEMENTED                                                         \
    throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) + \
                             ": not implemented")

#define THROW_RUNTIME_ERROR(msg) \
    throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) + ": " + (msg))

#endif  // COLUMNAR_ENGINE_MACRO_H
