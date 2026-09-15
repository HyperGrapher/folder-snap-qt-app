#pragma once

#include <stdexcept>

#include <QString>

namespace foldersnap
{
enum class ErrorCode
{
    InvalidPath,
    InvalidIdentifier,
    InvalidRule,
    InvalidData,
    UnsupportedSchema,
    SizeLimit,
    Io,
    MissingPayload
};

// Core entry points throw this value; application job boundaries translate it to UI errors.
class DomainError final : public std::runtime_error
{
  public:
    DomainError(ErrorCode code, const QString &message, int ruleLine = 0)
        : std::runtime_error(message.toStdString()), m_code(code), m_message(message),
          m_ruleLine(ruleLine)
    {
    }
    [[nodiscard]] ErrorCode code() const
    {
        return m_code;
    }
    [[nodiscard]] const QString &message() const
    {
        return m_message;
    }
    [[nodiscard]] int ruleLine() const
    {
        return m_ruleLine;
    }

  private:
    ErrorCode m_code;
    QString m_message;
    int m_ruleLine;
};
} // namespace foldersnap
