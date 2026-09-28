#ifndef WMA_EXCEPTIONS_WMA_EXCEPTION_HPP
#define WMA_EXCEPTIONS_WMA_EXCEPTION_HPP

#include <stdexcept>
#include <string>

namespace wma
{

/**
 * @brief Base exception class for wma library
 */
class WMAException : public std::runtime_error
{
  public:
    explicit WMAException(const std::string &message) : std::runtime_error("wma Error: " + message)
    {
    }

    explicit WMAException(const char *message) : std::runtime_error(std::string("wma Error: ") + message)
    {
    }
};

/**
 * @brief Window-specific exception
 */
class WindowException : public WMAException
{
  public:
    explicit WindowException(const std::string &message) : WMAException("Window Error: " + message)
    {
    }
};

/**
 * @brief Input-specific exception
 */
class InputException : public WMAException
{
  public:
    explicit InputException(const std::string &message) : WMAException("Input Error: " + message)
    {
    }
};

/**
 * @brief Graphics API-specific exception
 */
class GraphicsException : public WMAException
{
  public:
    explicit GraphicsException(const std::string &message) : WMAException("Graphics Error: " + message)
    {
    }
};

/**
 * @brief Audio-specific exception
 *
 * Thrown only when an audio device cannot be *constructed* — a backend that
 * was not compiled into this build. A device that merely fails to open (no
 * hardware, device busy, format refused) reports WmaCode::Error instead, so
 * createAudioDevice() can degrade to the next backend rather than unwind.
 */
class AudioException : public WMAException
{
  public:
    explicit AudioException(const std::string &message) : WMAException("Audio Error: " + message)
    {
    }
};

} // namespace wma

#endif // WMA_EXCEPTIONS_WMA_EXCEPTION_HPP
